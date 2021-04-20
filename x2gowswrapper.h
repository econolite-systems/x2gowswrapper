#include <QObject>
#include <QTextStream>
#include <QProcess>

class QProcess;
class QFile;
class X2GoWsWrapper: public QObject
{
    Q_OBJECT
public:
    X2GoWsWrapper();
private:
    void cleanAndExit();
    bool isSshRunning();
    void readSettings();
    QString sshTunnelPid;
    QString wsPort;
    QString localPort;
    QTextStream* log;
    QTextStream* err;
    QProcess *websockify;
    QString ssl_cert;
    QString ssl_key;
    bool ssl_only;

private slots:
    void initialize();
    void checkSSHprocess();
    void wsFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void wsErr();
    void wsOut();
};
