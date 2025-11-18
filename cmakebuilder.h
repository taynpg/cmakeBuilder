#ifndef CMAKEBUILDER_H
#define CMAKEBUILDER_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class CmakeBuilder; }
QT_END_NAMESPACE

class CmakeBuilder : public QDialog
{
    Q_OBJECT

public:
    CmakeBuilder(QWidget *parent = nullptr);
    ~CmakeBuilder();

private:
    Ui::CmakeBuilder *ui;
};
#endif // CMAKEBUILDER_H
