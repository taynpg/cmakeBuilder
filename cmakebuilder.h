#ifndef CMAKEBUILDER_H
#define CMAKEBUILDER_H

#include <QDialog>
#include <QProcess>

QT_BEGIN_NAMESPACE
namespace Ui { class CmakeBuilder; }
QT_END_NAMESPACE

class CmakeBuilder : public QDialog
{
    Q_OBJECT

public:
    CmakeBuilder(QWidget *parent = nullptr);
    ~CmakeBuilder();

public:
    void TestInit();
    void TestConfig();

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
    void appendOutput(const QString &text, bool isError = false);

private:
    Ui::CmakeBuilder *ui;
};
#endif // CMAKEBUILDER_H
