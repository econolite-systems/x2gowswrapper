/*
 * Copyright © 2021 Oleksandr Shneyder <o.shneyder@phoca-gmbh.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "x2gowswrapper.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QSettings>

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>


X2GoWsWrapper::X2GoWsWrapper()
{
    QTimer::singleShot(10,this, SLOT(initialize()));
}

void X2GoWsWrapper::readSettings()
{
    QSettings st("/etc/x2go/x2gows/x2gows.options",QSettings::IniFormat);
    bool stOk=true;
    if(st.status()!=QSettings::NoError)
    {
        stOk=false;
        qDebug()<<"can't read settings files, proceed with default values";
    }

    QString logdir=st.value("log_dir","/var/log/x2gows").toString();
    ssl_cert=st.value("ssl_cert","").toString();
    ssl_key=st.value("ssl_key","").toString();
    ssl_only=st.value("ssl_only",false).toBool();

    QFile* efile=new QFile(logdir+"/"+sshTunnelPid+"_"+localPort+"_"+wsPort+".err", this);
    if (!efile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug()<<"can't open for writing:"<<efile->fileName();
    }
    err=new QTextStream(efile);

    QFile* lfile=new QFile(logdir+"/"+sshTunnelPid+"_"+localPort+"_"+wsPort+".log", this);
    if (!lfile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug()<<"can't open for writing:"<<lfile->fileName();
    }
    log=new QTextStream(lfile);
    if(!stOk)
    {
        *err<<"can't read settings files, proceed with default values"<<endl;
    }

}

void X2GoWsWrapper::initialize()
{
    QStringList args=QCoreApplication::arguments();
    if(args.size()!=4)
    {
        qDebug()<<"wrong number of arguments:"<<args.size();
        QCoreApplication::exit(-1);
        return;
    }
    sshTunnelPid=args[1];
    localPort=args[2];
    wsPort=args[3];

    readSettings();


    *log<<"SSH pid: "<<sshTunnelPid<<" local port: "<<localPort<<" WS port: "<<wsPort<<endl;
    QTimer* tr=new QTimer(this);
    connect(tr, SIGNAL(timeout()), this, SLOT(checkSSHprocess()));
    tr->start(2000);

    QStringList procargs;

    procargs<<"--verbose";
    if(ssl_cert.length() && ssl_key.length())
    {
        procargs<<"--cert"<< ssl_cert <<"--key"<< ssl_key;
    }
    else
    {
        *err<<"warning, starting websockify without SSL support"<<endl;
    }
    if(ssl_only)
    {
        procargs<<"--ssl-only";
    }
    procargs<<":"+wsPort<< ":"+localPort;
    websockify=new QProcess(this);

    connect(websockify, SIGNAL(finished(int, QProcess::ExitStatus)),this, SLOT(wsFinished(int, QProcess::ExitStatus)));
    connect(websockify, SIGNAL(readyReadStandardError()),this, SLOT(wsErr()));
    connect(websockify, SIGNAL(readyReadStandardOutput()),this, SLOT(wsOut()));

    websockify->start("websockify",procargs,QIODevice::ReadOnly);
    *log<<"start websockify with args "<<procargs.join(" ")<<endl;
}

bool X2GoWsWrapper::isSshRunning()
{
    if(!QFile::exists("/proc/"+sshTunnelPid+"/cmdline"))
    {
        return false;
    }
    return true;
}


void X2GoWsWrapper::checkSSHprocess()
{
    if(!isSshRunning())
    {
        *err<<"Tunnel process with pid "<<sshTunnelPid<<" doesn't exist anymore"<<endl;
        cleanAndExit();
    }
}

void X2GoWsWrapper::cleanAndExit()
{
    QDir d;
    d.remove("/tmp/x2gows/"+wsPort);
    d.remove("/tmp/x2gows/"+localPort);

    if(websockify->state()==QProcess::Running)
    {
        *log<<"Terminating websockify"<<endl;
        websockify->terminate();
    }

    if(isSshRunning())
    {
        QProcess ps;
        ps.start("ps", QStringList()<<"ax"<<"o"<<"pid,ppid,command");
        ps.waitForFinished();
        QStringList processes=QString(ps.readAllStandardOutput()).split('\n');
        *log<<"Terminating tunnel with pid "<<sshTunnelPid<<endl;
        kill(sshTunnelPid.toUInt(), SIGTERM);
        foreach (QString pr,processes)
        {
            if(pr.split(" ",QString::SkipEmptyParts).length()<3)
                continue;
            if(pr.split(" ",QString::SkipEmptyParts)[1]==sshTunnelPid && pr.indexOf("ssh")!=-1)
            {
                QString childPid=pr.split(" ",QString::SkipEmptyParts)[0];
                *log<<"terminating child process "<<childPid<<" : "<<pr.split(" ",QString::SkipEmptyParts)[2]<<endl;
                kill(childPid.toUInt(), SIGTERM);
            }
        }

    }
    QCoreApplication::exit(0);
}

void X2GoWsWrapper::wsFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if(exitStatus != QProcess::NormalExit)
    {
        *err<<"websockify terminated with code "<<exitCode<<endl;
    }
    else
    {
        *log<<"websockify terminated with code "<<exitCode<<endl;
    }
    cleanAndExit();
}

void X2GoWsWrapper::wsErr()
{
    QString errs=websockify->readAllStandardError();
    *err<<errs<<endl;
    if((errs.indexOf("Client closed connection")!=-1 )||(errs.indexOf("Closed target")!=-1))
    {
        *log<<"Client closed connection, terminating..."<<endl;
        cleanAndExit();
    }
}

void X2GoWsWrapper::wsOut()
{
    *log<<websockify->readAllStandardOutput()<<endl;
}
