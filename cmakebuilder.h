#ifndef CMAKEBUILDER_H
#define CMAKEBUILDER_H

#include <QDialog>
#include <QProcess>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

QT_BEGIN_NAMESPACE
namespace Ui {
class CmakeBuilder;
}
QT_END_NAMESPACE

struct OneConfig {
    QString key;
    QString cmakePath;
    QString sourceDir;
    QString buildDir;
    QString curMode;
    QString curTarget;
    QString curType;
    QString curUseArg;
    QVector<QString> additonArgs;
};

class CmakeBuilder : public QDialog
{
    Q_OBJECT

public:
    CmakeBuilder(QWidget* parent = nullptr);
    ~CmakeBuilder();

private:
    void InitData();
    void LoadConfig();
    bool SimpleLoad();
    bool SaveData(const OneConfig& config);
    bool GetData(const QString& key, OneConfig& config);
    bool DelData(const QString& key);
    bool SetCurUse(const QString& key);
    bool GetCurUse(QString& key);
    OneConfig ReadUi();
    void SetUi(const OneConfig& o);
    void SaveCur(bool isNotice);
    bool GetAllKeys(QVector<QString>& keys);
    void newArg();
    void delArg();

public:
    void BaseInit();
    void cmakeConfig();
    void cmakeBuild();
    void cmakeReBuild();

    void DisableBtn();
    void EnableBtn();

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

    bool loadJsonFromFile(json& j, const QString& filename);
    bool saveJsonToFile(const json& j, const QString& filename);
    OneConfig jsonToConfig(const json& j);
    json configToJson(const OneConfig& config);

private:
    QProcess* process_;
    void Print(const QString& text, bool isError = false);
    QVector<QString> getTarget();

private:
    QString configFile_;
    QString configUse_;
    QString curType_;
    QString curTarget_;
    bool configRet_;

private:
    Ui::CmakeBuilder* ui;
};
#endif   // CMAKEBUILDER_H
