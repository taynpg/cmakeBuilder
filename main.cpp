#include "cmakebuilder.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CmakeBuilder w;
    w.show();
    return a.exec();
}
