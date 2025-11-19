#include "config.h"

#include <QDateTime>
#include <QFile>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ConfigPrivate
{
public:
    bool loadJsonFromFile(json& j, const QString& filename);
    bool saveJsonToFile(const json& j, const QString& filename);
    OneConfig jsonToConfig(const json& j);
    json configToJson(const OneConfig& config);
    bool setSize(int w, int h);
    std::pair<int, int> getSize();
    bool SaveData(const OneConfig& config);
    bool GetData(const QString& key, OneConfig& config);
    bool GetCurUse(QString& key);
    bool SetCurUse(const QString& key);
    bool DelData(const QString& key);
    bool GetAllKeys(QVector<QString>& keys);
    void SetError(const QString& msg);

public:
    QString configFile_;
    QString configUse_;
    QString configSize_;
    QString errMsg_;
};

void ConfigPrivate::SetError(const QString& msg)
{
    errMsg_ = msg;
}

bool ConfigPrivate::setSize(int w, int h)
{
    if (w <= 0 || h <= 0) {
        SetError("错误：窗口尺寸必须大于0");
        return false;
    }

    if (configSize_.isEmpty()) {
        SetError("错误：配置文件路径未设置");
        return false;
    }

    try {
        json j;

        // 如果配置文件已存在，读取现有内容
        if (QFile::exists(configSize_)) {
            if (!loadJsonFromFile(j, configSize_)) {
                SetError("警告：无法读取现有配置文件，将创建新文件");
            }
        }

        // 设置窗口尺寸
        j["window_size"] = {{"width", w},
                            {"height", h},
                            {"last_modified", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString()}};

        // 保存到文件
        if (saveJsonToFile(j, configSize_)) {
            // SetError(QString("窗口尺寸已保存: %1 x %2").arg(w).arg(h));
            return true;
        } else {
            SetError("错误：保存窗口尺寸失败");
            return false;
        }

    } catch (const std::exception& e) {
        SetError(QString("错误：设置窗口尺寸时发生异常: %1").arg(e.what()));
        return false;
    }
}

std::pair<int, int> ConfigPrivate::getSize()
{
    if (configSize_.isEmpty()) {
        SetError("错误：配置文件路径未设置");
        return {0, 0};
    }

    if (!QFile::exists(configSize_)) {
        SetError("配置文件不存在，返回默认尺寸");
        return {0, 0};
    }

    try {
        json j;
        if (!loadJsonFromFile(j, configSize_)) {
            SetError("错误：无法读取配置文件");
            return {0, 0};
        }

        // 检查是否存在窗口尺寸配置
        if (!j.contains("window_size")) {
            SetError("配置文件中未找到窗口尺寸设置");
            return {0, 0};
        }

        json windowSize = j["window_size"];

        // 验证字段存在性
        if (!windowSize.contains("width") || !windowSize.contains("height")) {
            SetError("错误：窗口尺寸配置不完整");
            return {0, 0};
        }

        int width = windowSize.value("width", 0);
        int height = windowSize.value("height", 0);

        if (width <= 0 || height <= 0) {
            SetError("警告：配置中的窗口尺寸无效");
            return {0, 0};
        }
        return {width, height};

    } catch (const std::exception& e) {
        SetError(QString("错误：读取窗口尺寸时发生异常: %1").arg(e.what()));
        return {0, 0};
    }
}

// 私有辅助函数
bool ConfigPrivate::loadJsonFromFile(json& j, const QString& filename)
{
    if (!QFile::exists(filename)) {
        j = json::object();   // 返回空JSON对象
        return true;
    }

    std::ifstream inFile(filename.toStdString());
    if (!inFile.is_open()) {
        return false;
    }

    try {
        inFile >> j;
        inFile.close();
        return true;
    } catch (...) {
        return false;
    }
}

bool ConfigPrivate::saveJsonToFile(const json& j, const QString& filename)
{
    std::ofstream outFile(filename.toStdString());
    if (!outFile.is_open()) {
        return false;
    }

    try {
        outFile << j.dump(4);   // 美化输出，4空格缩进
        outFile.close();
        return true;
    } catch (...) {
        return false;
    }
}

OneConfig ConfigPrivate::jsonToConfig(const json& j)
{
    OneConfig config;

    config.key = QString::fromStdString(j.value("key", ""));
    config.cmakePath = QString::fromStdString(j.value("cmakePath", ""));
    config.sourceDir = QString::fromStdString(j.value("sourceDir", ""));
    config.buildDir = QString::fromStdString(j.value("buildDir", ""));
    config.curMode = QString::fromStdString(j.value("curMode", ""));
    config.curTarget = QString::fromStdString(j.value("curTarget", ""));
    config.curType = QString::fromStdString(j.value("curType", ""));
    config.curUseArg = QString::fromStdString(j.value("curUseArg", ""));
    config.vcEnv = QString::fromStdString(j.value("vcEnv", ""));

    // 处理一维数组 additonArgs
    if (j.contains("additonArgs")) {
        json argsArray = j["additonArgs"];
        for (const auto& arg : argsArray) {
            config.additonArgs.append(QString::fromStdString(arg));
        }
    }

    return config;
}

json ConfigPrivate::configToJson(const OneConfig& config)
{
    json j;

    j["key"] = config.key.toStdString();
    j["cmakePath"] = config.cmakePath.toStdString();
    j["sourceDir"] = config.sourceDir.toStdString();
    j["buildDir"] = config.buildDir.toStdString();
    j["curMode"] = config.curMode.toStdString();
    j["curTarget"] = config.curTarget.toStdString();
    j["curType"] = config.curType.toStdString();
    j["curUseArg"] = config.curUseArg.toStdString();
    j["vcEnv"] = config.vcEnv.toStdString();

    // 处理一维数组 additonArgs
    json argsArray = json::array();
    for (const QString& arg : config.additonArgs) {
        argsArray.push_back(arg.toStdString());
    }
    j["additonArgs"] = argsArray;

    return j;
}

bool ConfigPrivate::SaveData(const OneConfig& config)
{
    if (config.key.isEmpty()) {
        SetError("配置键为空，无法保存");
        return false;
    }

    try {
        json j;

        // 读取现有配置文件
        loadJsonFromFile(j, configFile_);

        // 更新或添加配置
        j[config.key.toStdString()] = configToJson(config);

        // 保存到文件
        if (saveJsonToFile(j, configFile_)) {
            return true;
        }
        return false;

    } catch (const std::exception& e) {
        SetError(QString("保存配置时发生错误：\n%1").arg(e.what()));
        return false;
    }
}

bool ConfigPrivate::GetData(const QString& key, OneConfig& config)
{
    if (key.isEmpty()) {
        SetError("配置键为空");
        return false;
    }

    try {
        json j;
        if (!loadJsonFromFile(j, configFile_)) {
            SetError("配置文件不存在或读取失败");
            return false;
        }

        if (!j.contains(key.toStdString())) {
            SetError(QString("未找到键值为 '%1' 的配置").arg(key));
            return false;
        }

        config = jsonToConfig(j[key.toStdString()]);
        return true;

    } catch (const std::exception& e) {
        SetError(QString("加载配置时发生错误：\n%1").arg(e.what()));
        return false;
    }
}

bool ConfigPrivate::DelData(const QString& key)
{
    if (key.isEmpty()) {
        SetError("配置键为空，无法删除");
        return false;
    }

    if (!QFile::exists(configFile_)) {
        SetError("配置文件不存在");
        return false;
    }

    try {

        json j;

        if (!loadJsonFromFile(j, configFile_)) {
            SetError("无法读取配置文件");
            return false;
        }

        if (!j.contains(key.toStdString())) {
            SetError(QString("未找到键值为 '%1' 的配置").arg(key));
            return false;
        }

        // QString currentKey;
        // if (GetCurUse(currentKey) && currentKey == key) {
        //     SetError("不能删除当前正在使用的配置，请先切换其他配置");
        //     return false;
        // }

        j.erase(key.toStdString());

        if (saveJsonToFile(j, configFile_)) {
            SetError(QString("配置 '%1' 删除成功").arg(key));
            return true;
        } else {
            SetError("保存配置文件失败");
            return false;
        }

    } catch (const std::exception& e) {
        SetError(QString("删除配置时发生错误：\n%1").arg(e.what()));
        return false;
    }
}

bool ConfigPrivate::SetCurUse(const QString& key)
{
    if (key.isEmpty()) {
        SetError("配置键为空");
        return false;
    }

    try {
        // 验证配置是否存在
        OneConfig config;
        if (!GetData(key, config)) {
            return false;
        }

        json j;
        j["current_config"] = key.toStdString();
        j["last_modified"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString();

        if (saveJsonToFile(j, configUse_)) {
            return true;
        }

    } catch (const std::exception& e) {
        SetError(QString("设置当前配置时发生错误：\n%1").arg(e.what()));
    }

    return false;
}

bool ConfigPrivate::GetCurUse(QString& key)
{
    try {
        json j;
        if (!loadJsonFromFile(j, configUse_)) {
            key.clear();
            return false;
        }

        if (!j.contains("current_config")) {
            key.clear();
            return false;
        }

        key = QString::fromStdString(j["current_config"]);
        return true;

    } catch (const std::exception& e) {
        SetError(QString("获取当前配置时发生错误：\n%1").arg(e.what()));
        key.clear();
        return false;
    }
}

bool ConfigPrivate::GetAllKeys(QVector<QString>& keys)
{
    keys.clear();

    if (!QFile::exists(configFile_)) {
        return false;
    }

    try {
        json j;
        if (!loadJsonFromFile(j, configFile_)) {
            SetError("无法读取配置文件");
            return false;
        }

        // 遍历JSON对象的所有键
        for (auto it = j.begin(); it != j.end(); ++it) {
            keys.append(QString::fromStdString(it.key()));
        }

        if (keys.isEmpty()) {
            SetError("配置文件中没有找到任何配置");
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        SetError(QString("读取配置键值时发生错误：\n%1").arg(e.what()));
        return false;
    }
}

BuilderConfig::BuilderConfig(QObject* parent) : QObject(parent)
{
    p_ = new ConfigPrivate();
}

BuilderConfig::~BuilderConfig()
{
    delete p_;
}

void BuilderConfig::setConfigDir(const QString& d)
{
    p_->configFile_ = d;
}

void BuilderConfig::setConfigSizeDir(const QString& d)
{
    p_->configSize_ = d;
}

void BuilderConfig::setConfigUseDir(const QString& d)
{
    p_->configUse_ = d;
}

bool BuilderConfig::GetAllKeys(QVector<QString>& keys)
{
    return p_->GetAllKeys(keys);
}

bool BuilderConfig::setSize(int w, int h)
{
    auto r = p_->setSize(w, h);
    if (!r) {
        emit sigMsg(p_->errMsg_);
    }
    return r;
}

std::pair<int, int> BuilderConfig::getSize()
{
    return p_->getSize();
}

bool BuilderConfig::SaveData(const OneConfig& config)
{
    auto r = p_->SaveData(config);
    if (!r) {
        emit sigMsg(p_->errMsg_);
    }
    return r;
}

bool BuilderConfig::GetData(const QString& key, OneConfig& config)
{
    auto r = p_->GetData(key, config);
    if (!r) {
        emit sigMsg(p_->errMsg_);
    }
    return r;
}

bool BuilderConfig::GetCurUse(QString& key)
{
    auto r = p_->GetCurUse(key);
    if (!r) {
        emit sigMsg(p_->errMsg_);
    }
    return r;
}

bool BuilderConfig::SetCurUse(const QString& key)
{
    auto r = p_->SetCurUse(key);
    if (!r) {
        emit sigMsg(p_->errMsg_);
    }
    return r;
}

bool BuilderConfig::DelData(const QString& key)
{
    auto r = p_->DelData(key);
    if (!r) {
        emit sigMsg(p_->errMsg_);
    }
    return r;
}
