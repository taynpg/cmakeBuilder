#ifndef CMAKEBUILDER_H
#define CMAKEBUILDER_H

#include <QDialog>
#include <QFuture>
#include <QFutureWatcher>
#include <QProcess>
#include <QtConcurrent>

#include "config.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CmakeBuilder;
}
QT_END_NAMESPACE

class CmakeBuilder : public QDialog
{
    Q_OBJECT

public:
    CmakeBuilder(QWidget* parent = nullptr);
    ~CmakeBuilder();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void InitData();
    void LoadConfig();
    bool SimpleLoad();
    OneConfig ReadUi();
    void SetUi(const OneConfig& o);
    void SaveCur(bool isNotice);
    void newArg();
    void delArg();

public:
    void BaseInit();
    void cmakeConfig();
    void cmakeConfigWithVCEnv();
    void cmakeBuild();
    void checkBuildNinjaFile(int attempt = 0);
    void onVCEnvReady(QProcessEnvironment vsEnv);
    void onBuildNinjaChanged(const QString& path);

    QProcessEnvironment getVCEnvironment(const QString& vcvarsPath);
    QProcessEnvironment parseEnvironmentOutput(const QString& output);

    void DisableBtn();
    void EnableBtn();

Q_SIGNALS:
    void sigPrint(const QString& msg);
    void sigEnableBtn(bool enable);
    void processBuildNinja();

public Q_SLOTS:
    void Print(const QString& text, bool isError = false);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess* process_;
    QVector<QString> getTarget();

private:
    BuilderConfig* config_{};
    QString curType_;
    QString curTarget_;
    QString curVcEnv_;
    QString curEnvBatFile_;
    QString buildFile_;
    QProcessEnvironment curEnvValue_;
    bool configRet_;

private:
    Ui::CmakeBuilder* ui;
};
#endif   // CMAKEBUILDER_H
