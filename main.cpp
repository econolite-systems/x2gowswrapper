#include <QCoreApplication>
#include "x2gowswrapper.h"

int main(int argc, char** argv)
{
    QCoreApplication* app=new QCoreApplication(argc, argv);
    new X2GoWsWrapper();
    return app->exec();
}
