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
    void terminalProcess();
    void InitTab();

public:
    void BaseInit();
    void cmakeConfig();
    void cmakeConfigWithVCEnv();
    void cmakeBuild();
    void onVCEnvReady();
    void onBuildNinjaChanged(const QString& path);

    QProcessEnvironment getVCEnvironment(const QString& vcvarsPath);
    QProcessEnvironment parseEnvironmentOutput(const QString& output);
    QString expandEnvVar(const QProcessEnvironment& env, const QString& str);

    void onTableContextMenu(const QPoint& pos);
    void setTypeComboBox(int row, const QStringList& options);
    void setModeComboBox(int row, const QStringList& options);
    void addTableRow();
    void deleteTableRow();
    void clearTable();
    QVector<QString> getAddArgsFromTable(const QProcessEnvironment& env);

    void DisableBtn();
    void EnableBtn();
    void StartExe();

Q_SIGNALS:
    void sigPrint(const QString& msg);
    void sigEnableBtn(bool enable);

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
    QString currentTaskName_;
    QProcessEnvironment curEnvValue_;
    bool configRet_;
    QVector<QString> typeOptions_;
    QVector<QString> modes_;

private:
    Ui::CmakeBuilder* ui;
};
#endif   // CMAKEBUILDER_H
