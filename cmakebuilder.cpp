#include "cmakebuilder.h"

#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

#include "./ui_cmakebuilder.h"

constexpr auto SL = "----------------------------------------------";

CmakeBuilder::CmakeBuilder(QWidget* parent) : QDialog(parent), ui(new Ui::CmakeBuilder)
{
    ui->setupUi(this);
    InitData();
    LoadConfig();
    BaseInit();
    setWindowTitle("cmakeBuilder v1.0");
}

CmakeBuilder::~CmakeBuilder()
{
    delete ui;
}

void CmakeBuilder::InitData()
{
    process_ = new QProcess(this);
    auto configDir = QDir::homePath() + "/.config/cmakeBuilder";
    QDir dir(configDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::information(this, "提示", "创建配置文件目录失败。");
            return;
        }
    }
    configFile_ = configDir + "/config.json";
    configUse_ = configDir + "/curuse.json";

    ui->cbProject->setEditable(true);
    ui->cbProject->setMinimumWidth(150);
    ui->edCMake->setFocusPolicy(Qt::ClickFocus);

    connect(process_, &QProcess::readyReadStandardOutput, this, &CmakeBuilder::onProcessReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &CmakeBuilder::onProcessReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &CmakeBuilder::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &CmakeBuilder::onProcessError);
    connect(ui->btnSaveConfig, &QPushButton::clicked, this, &CmakeBuilder::SaveCur);
    connect(ui->btnLoadConfig, &QPushButton::clicked, this, &CmakeBuilder::SimpleLoad);
    connect(ui->btnAddArg, &QPushButton::clicked, this, &CmakeBuilder::newArg);
    connect(ui->btnDelArg, &QPushButton::clicked, this, &CmakeBuilder::delArg);
    connect(ui->btnReBuild, &QPushButton::clicked, this, &CmakeBuilder::cmakeReBuild);
    connect(ui->btnDelConfig, &QPushButton::clicked, this, [this]() {
        auto key = ui->cbProject->currentText();
        if (DelData(key)) {
            int index = ui->cbProject->findText(key);
            if (index >= 0) {
                ui->cbProject->removeItem(index);
            }
            if (ui->cbProject->count() > 0) {
                ui->cbProject->setCurrentIndex(0);
            }
        }
    });
}

void CmakeBuilder::LoadConfig()
{
    QVector<QString> keys;
    if (!GetAllKeys(keys)) {
        return;
    }
    QString curuse;
    if (!GetCurUse(curuse)) {
        return;
    }
    if (!keys.contains(curuse)) {
        return;
    }
    OneConfig o;
    if (!GetData(curuse, o)) {
        return;
    }
    SetUi(o);
    ui->cbProject->clear();
    for (const auto& item : keys) {
        ui->cbProject->addItem(item);
    }
    ui->cbProject->setCurrentText(curuse);
}

bool CmakeBuilder::SimpleLoad()
{
    QString key = ui->cbProject->currentText();
    OneConfig o;
    if (!GetData(key, o)) {
        return false;
    }
    SetUi(o);
    return true;
}

bool CmakeBuilder::SaveData(const OneConfig& config)
{
    if (config.key.isEmpty()) {
        QMessageBox::warning(nullptr, "警告", "配置键为空，无法保存");
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
            QMessageBox::information(nullptr, "成功", QString("配置保存成功！\n键值：%1").arg(config.key));
            return true;
        }

    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "错误", QString("保存配置时发生错误：\n%1").arg(e.what()));
    }

    return false;
}

bool CmakeBuilder::GetData(const QString& key, OneConfig& config)
{
    if (key.isEmpty()) {
        QMessageBox::warning(nullptr, "警告", "配置键为空");
        return false;
    }

    try {
        json j;
        if (!loadJsonFromFile(j, configFile_)) {
            QMessageBox::information(nullptr, "提示", "配置文件不存在或读取失败");
            return false;
        }

        if (!j.contains(key.toStdString())) {
            QMessageBox::information(nullptr, "提示", QString("未找到键值为 '%1' 的配置").arg(key));
            return false;
        }

        config = jsonToConfig(j[key.toStdString()]);
        return true;

    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "错误", QString("加载配置时发生错误：\n%1").arg(e.what()));
        return false;
    }
}

bool CmakeBuilder::DelData(const QString& key)
{
    if (key.isEmpty()) {
        QMessageBox::warning(nullptr, "警告", "配置键为空，无法删除");
        return false;
    }

    if (!QFile::exists(configFile_)) {
        QMessageBox::information(nullptr, "提示", "配置文件不存在");
        return false;
    }

    try {

        json j;

        if (!loadJsonFromFile(j, configFile_)) {
            QMessageBox::critical(nullptr, "错误", "无法读取配置文件");
            return false;
        }

        if (!j.contains(key.toStdString())) {
            QMessageBox::information(nullptr, "提示", QString("未找到键值为 '%1' 的配置").arg(key));
            return false;
        }

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(nullptr, "确认删除", QString("确定要删除配置 '%1' 吗？").arg(key),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return false;
        }

        QString currentKey;
        if (GetCurUse(currentKey) && currentKey == key) {
            QMessageBox::information(nullptr, "提示", "不能删除当前正在使用的配置，请先切换其他配置");
            return false;
        }

        j.erase(key.toStdString());

        if (saveJsonToFile(j, configFile_)) {
            QMessageBox::information(nullptr, "成功", QString("配置 '%1' 删除成功").arg(key));
            return true;
        } else {
            QMessageBox::critical(nullptr, "错误", "保存配置文件失败");
            return false;
        }

    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "错误", QString("删除配置时发生错误：\n%1").arg(e.what()));
        return false;
    }
}

bool CmakeBuilder::SetCurUse(const QString& key)
{
    if (key.isEmpty()) {
        QMessageBox::warning(nullptr, "警告", "配置键为空");
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
        QMessageBox::critical(nullptr, "错误", QString("设置当前配置时发生错误：\n%1").arg(e.what()));
    }

    return false;
}

bool CmakeBuilder::GetCurUse(QString& key)
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
        QMessageBox::critical(nullptr, "错误", QString("获取当前配置时发生错误：\n%1").arg(e.what()));
        key.clear();
        return false;
    }
}

OneConfig CmakeBuilder::ReadUi()
{
    OneConfig o;
    o.cmakePath = ui->edCMake->text().trimmed().replace('\\', '/');
    o.sourceDir = ui->edSource->text().trimmed().replace('\\', '/');
    o.buildDir = ui->edBuildDir->text().trimmed().replace('\\', '/');
    o.curUseArg = ui->cbAdditionArg->currentText();
    o.additonArgs.clear();
    for (int i = 0; i < ui->cbAdditionArg->count(); ++i) {
        o.additonArgs.append(ui->cbAdditionArg->itemText(i));
    }
    return o;
}

void CmakeBuilder::SetUi(const OneConfig& o)
{
    // 设置基本路径
    ui->edCMake->setText(o.cmakePath);
    ui->edSource->setText(o.sourceDir);
    ui->edBuildDir->setText(o.buildDir);

    // 清空下拉框并添加所有选项
    ui->cbAdditionArg->clear();
    for (const QString& arg : o.additonArgs) {
        ui->cbAdditionArg->addItem(arg);
    }

    // 设置当前选中的参数
    int index = ui->cbAdditionArg->findText(o.curUseArg);
    if (index >= 0) {
        ui->cbAdditionArg->setCurrentIndex(index);
    } else if (ui->cbAdditionArg->count() > 0) {
        // 如果找不到匹配项，选择第一项
        ui->cbAdditionArg->setCurrentIndex(0);
    }
}

void CmakeBuilder::SaveCur(bool isNotice)
{
    QString key = ui->cbProject->currentText().trimmed();
    if (key.isEmpty()) {
        QMessageBox::information(this, "提示", "输入配置名称");
        return;
    }
    auto o = ReadUi();
    o.key = key;
    if (!SaveData(o)) {
        QMessageBox::information(this, "提示", "保存失败");
        return;
    }
    SetCurUse(o.key);
}

bool CmakeBuilder::GetAllKeys(QVector<QString>& keys)
{
    keys.clear();

    if (!QFile::exists(configFile_)) {
        return false;
    }

    try {
        json j;
        if (!loadJsonFromFile(j, configFile_)) {
            QMessageBox::critical(nullptr, "错误", "无法读取配置文件");
            return false;
        }

        // 遍历JSON对象的所有键
        for (auto it = j.begin(); it != j.end(); ++it) {
            keys.append(QString::fromStdString(it.key()));
        }

        if (keys.isEmpty()) {
            QMessageBox::information(nullptr, "提示", "配置文件中没有找到任何配置");
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "错误", QString("读取配置键值时发生错误：\n%1").arg(e.what()));
        return false;
    }
}

void CmakeBuilder::newArg()
{
    QInputDialog dialog(this);
    dialog.setWindowTitle("输入");
    dialog.setLabelText("要新建参数项:");
    dialog.setOkButtonText("确定");
    dialog.setCancelButtonText("取消");
    auto size = dialog.minimumSizeHint();
    size.setWidth(size.width() + 200);
    dialog.setFixedSize(size);

    if (dialog.exec() == QDialog::Accepted) {
        QString newArg = dialog.textValue().trimmed();
        if (!newArg.isEmpty()) {
            int index = ui->cbAdditionArg->findText(newArg);
            if (index == -1) {
                ui->cbAdditionArg->addItem(newArg);
                ui->cbAdditionArg->setCurrentText(newArg);
            } else {
                ui->cbAdditionArg->setCurrentIndex(index);
            }
        } else {
            QMessageBox::warning(this, "警告", "输入内容不能为空");
        }
    }
}

void CmakeBuilder::delArg()
{
    QString currentText = ui->cbAdditionArg->currentText();
    if (currentText.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择要删除的参数项");
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除", QString("确定要删除参数项 '%1' 吗？").arg(currentText),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        int currentIndex = ui->cbAdditionArg->currentIndex();
        ui->cbAdditionArg->removeItem(currentIndex);
        if (ui->cbAdditionArg->count() > 0) {
            if (currentIndex >= ui->cbAdditionArg->count()) {
                ui->cbAdditionArg->setCurrentIndex(ui->cbAdditionArg->count() - 1);
            } else {
                ui->cbAdditionArg->setCurrentIndex(currentIndex);
            }
        }

        QMessageBox::information(this, "成功", "参数项删除成功");
    }
}

// 私有辅助函数
bool CmakeBuilder::loadJsonFromFile(json& j, const QString& filename)
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

bool CmakeBuilder::saveJsonToFile(const json& j, const QString& filename)
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

OneConfig CmakeBuilder::jsonToConfig(const json& j)
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

    // 处理一维数组 additonArgs
    if (j.contains("additonArgs")) {
        json argsArray = j["additonArgs"];
        for (const auto& arg : argsArray) {
            config.additonArgs.append(QString::fromStdString(arg));
        }
    }

    return config;
}

json CmakeBuilder::configToJson(const OneConfig& config)
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

    // 处理一维数组 additonArgs
    json argsArray = json::array();
    for (const QString& arg : config.additonArgs) {
        argsArray.push_back(arg.toStdString());
    }
    j["additonArgs"] = argsArray;

    return j;
}

void CmakeBuilder::BaseInit()
{
    ui->cbType->addItem("Ninja");
    ui->cbType->setCurrentText(0);

    ui->cbMode->addItem("Debug");
    ui->cbMode->addItem("Release");
    ui->cbMode->setCurrentIndex(0);

    ui->cbTarget->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->cbMode->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->cbType->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    connect(ui->btnConfig, &QPushButton::clicked, this, &CmakeBuilder::cmakeConfigTest);
    connect(ui->btnBuild, &QPushButton::clicked, this, &CmakeBuilder::cmakeBuild);
    connect(ui->btnAddCmake, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "选择 CMake 可执行文件", QDir::homePath(),
                                                        "CMake 文件 (cmake cmake.exe);;所有文件 (*.*)");

        if (!fileName.isEmpty()) {
            QFileInfo fileInfo(fileName);
            QString baseName = fileInfo.fileName().toLower();

            if (baseName == "cmake" || baseName == "cmake.exe") {
                ui->edCMake->setText(QDir::toNativeSeparators(fileName));
            } else {
                QMessageBox::warning(this, "警告", "选择的文件不是有效的 CMake 可执行文件");
            }
        }
    });

    connect(ui->btnSelSource, &QPushButton::clicked, this, [this]() {
        QString dirPath = QFileDialog::getExistingDirectory(this, "选择源码目录", QDir::homePath(),
                                                            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (!dirPath.isEmpty()) {
            // 检查目录是否包含CMakeLists.txt
            QDir sourceDir(dirPath);
            bool hasCMakeLists = sourceDir.exists("CMakeLists.txt");

            if (hasCMakeLists) {
                ui->edSource->setText(QDir::toNativeSeparators(dirPath));
            } else {
                QMessageBox::StandardButton reply =
                    QMessageBox::question(this, "确认选择", "选择的目录中没有找到 CMakeLists.txt 文件，是否继续使用此目录？",
                                          QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    ui->edSource->setText(QDir::toNativeSeparators(dirPath));
                }
            }
        }
    });

    connect(ui->btnSelBuildDir, &QPushButton::clicked, this, [this]() {
        QString dirPath = QFileDialog::getExistingDirectory(this, "选择构建目录", QDir::homePath(),
                                                            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (!dirPath.isEmpty()) {
            QDir buildDir(dirPath);

            if (!buildDir.exists()) {
                ui->edBuildDir->setText(QDir::toNativeSeparators(dirPath));
            } else {
                buildDir.setFilter(QDir::NoDotAndDotDot | QDir::AllEntries);
                if (buildDir.count() == 0) {
                    ui->edBuildDir->setText(QDir::toNativeSeparators(dirPath));
                } else {
                    ui->edBuildDir->setText("");
                }
            }
        }
    });
}

void CmakeBuilder::cmakeConfig()
{
    configRet_ = false;
    std::shared_ptr<void> recv(nullptr, [this](void*) { EnableBtn(); });

    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->edCMake->text().trimmed();
    auto target = ui->cbTarget->currentText();
    auto mode = ui->cbMode->currentText();
    curTarget_ = ui->cbTarget->currentText();

    if (process_->state() == QProcess::Running) {
        Print("CMake 进程正在运行，请等待完成...", true);
        return;
    }

    if (!curType_.isEmpty() && ui->cbMode->currentText() != curType_) {
        Print("模式已变更，重新执行配置。");
        QDir dir(buildDir);
        if (dir.exists()) {
            if (dir.removeRecursively()) {
                Print("已清空旧模式的构建目录: " + buildDir);
                QDir().mkpath(buildDir);
            } else {
                Print("错误: 无法清空构建目录: " + buildDir);
            }
        }
    }

    ui->pedOutput->clear();
    QDir dir(buildDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            Print("错误：无法创建构建目录 " + buildDir, true);
            return;
        }
    }
    process_->setWorkingDirectory(buildDir);
    QStringList arguments;
    arguments << "-S" << ui->edSource->text().trimmed();
    arguments << "-B" << buildDir;
    arguments << "-G" << ui->cbType->currentText();
    arguments << ui->cbAdditionArg->currentText();

    // 添加其他常用选项
    QString buildType = "-DCMAKE_BUILD_TYPE=" + mode;
    arguments << buildType;

    Print("开始执行 CMake 配置...");
    Print("命令: " + cmake + " " + arguments.join(" "));
    Print("工作目录: " + buildDir);
    Print(SL);

    DisableBtn();
    process_->start(cmake, arguments);

    if (!process_->waitForStarted(20000)) {
        Print("错误：启动 CMake 进程超时", true);
        return;
    }
    QTimer::singleShot(1000, [=]() {
        QString buildNinjaPath = buildDir + "/build.ninja";
        if (QFile::exists(buildNinjaPath)) {
            auto ret = getTarget();
            ui->cbTarget->clear();
            ui->cbTarget->addItem("all");
            for (const auto& item : ret) {
                ui->cbTarget->addItem(item);
            }
            if (!curTarget_.isEmpty()) {
                auto index = ui->cbTarget->findText(curTarget_);
                if (index >= 0) {
                    ui->cbTarget->setCurrentIndex(index);
                } else {
                    ui->cbTarget->setCurrentIndex(0);
                }
            }
            curTarget_ = ui->cbTarget->currentText();
            curType_ = ui->cbMode->currentText();
            configRet_ = true;
        } else {
            Print("错误：未找到 build.ninja 文件", true);
        }
    });
}

void CmakeBuilder::cmakeConfigTest()
{
    QString envBat = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017/Professional\\VC\\Auxiliary\\Build\\vcvars64.bat";

    if (!QFile::exists(envBat)) {
        QMessageBox::critical(this, "错误", "VC环境脚本不存在:\n" + envBat);
        return;
    }

    // 获取配置参数
    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->edCMake->text().trimmed();
    auto sourceDir = ui->edSource->text().trimmed();
    auto generator = ui->cbType->currentText();
    auto mode = ui->cbMode->currentText();

    // 验证参数
    if (buildDir.isEmpty() || cmake.isEmpty() || sourceDir.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先设置必要的路径");
        return;
    }

    if (process_->state() == QProcess::Running) {
        QMessageBox::information(this, "提示", "CMake 进程正在运行，请等待完成...");
        return;
    }

    Print("=== 开始VC环境配置测试 ===");
    Print("获取VC环境变量...");

    // 1. 首先获取VC环境变量
    QProcessEnvironment vsEnv = getVCEnvironment(envBat);
    if (vsEnv.isEmpty()) {
        Print("错误：无法获取VC环境变量", true);
        return;
    }

    Print("VC环境变量获取成功");

    // 2. 创建构建目录
    QDir dir(buildDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            Print("错误：无法创建构建目录 " + buildDir, true);
            return;
        }
    }

    process_->setWorkingDirectory(buildDir);

    // 3. 构建CMake参数
    QStringList arguments;
    arguments << "-S" << sourceDir;
    arguments << "-B" << buildDir;
    arguments << "-G" << generator;
    arguments << "-DCMAKE_BUILD_TYPE=" + mode;

    // 添加附加参数
    QString additionArg = ui->cbAdditionArg->currentText();
    if (!additionArg.isEmpty()) {
        arguments << additionArg;
    }

    Print("命令: " + cmake + " " + arguments.join(" "));
    Print("工作目录: " + buildDir);
    Print(SL);

    DisableBtn();

    // 4. 设置进程环境（关键步骤）
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(vsEnv);   // 添加VC环境变量
    process_->setProcessEnvironment(env);

    // 5. 启动CMake进程
    process_->start(cmake, arguments);

    if (!process_->waitForStarted(20000)) {
        Print("错误：启动配置进程超时", true);
        EnableBtn();
        return;
    }

    Print("配置进程已启动...");
}

QProcessEnvironment CmakeBuilder::getVCEnvironment(const QString& vcvarsPath)
{
    QProcess process;
    process.setProgram("cmd.exe");
    process.setArguments({"/c", "call", vcvarsPath, "&&", "set"});
    Print("获取VC环境变量: " + vcvarsPath);
    process.start();
    
    if (!process.waitForFinished(15000)) {
        Print("错误：进程执行超时");
        return QProcessEnvironment();
    }
    
    if (process.exitCode() != 0) {
        Print("错误：VC环境脚本执行失败");
        return QProcessEnvironment();
    }
    
    QString allOutput = QString::fromLocal8Bit(process.readAllStandardOutput());
    
    if (allOutput.isEmpty()) {
        Print("错误：没有获取到输出");
        return QProcessEnvironment();
    }
    Print("成功获取VC环境变量");
    return parseEnvironmentOutput(allOutput);
}

QProcessEnvironment CmakeBuilder::parseEnvironmentOutput(const QString& output)
{
    QProcessEnvironment env;
    QStringList lines = output.split('\n');
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;
        
        int equalsIndex = trimmedLine.indexOf('=');
        if (equalsIndex > 0) {
            QString varName = trimmedLine.left(equalsIndex).trimmed();
            QString varValue = trimmedLine.mid(equalsIndex + 1).trimmed();
            env.insert(varName, varValue);
        }
    }
    
    return env;
}

void CmakeBuilder::cmakeBuild()
{
    if (ui->cbTarget->currentText().isEmpty()) {
        QMessageBox::information(this, "提示", "请先执行CMake配置");
        return;
    }

    std::shared_ptr<void> recv(nullptr, [this](void*) { EnableBtn(); });

    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->edCMake->text().trimmed();
    auto target = ui->cbTarget->currentText();
    auto mode = ui->cbMode->currentText();

    if (process_->state() == QProcess::Running) {
        Print("CMake 进程正在运行，请等待完成...", true);
        return;
    }

    if (!QDir(buildDir).exists()) {
        Print("错误：构建目录不存在，请先执行配置", true);
        return;
    }

    if (!QFile::exists(buildDir + "/CMakeCache.txt")) {
        Print("错误：项目未配置，请先执行 CMake 配置", true);
        return;
    }

    ui->pedOutput->clear();
    process_->setWorkingDirectory(buildDir);

    QStringList arguments;
    arguments << "--build" << buildDir;
    arguments << "--config" << mode;

    auto retTarget = QFileInfo(target).baseName();
    arguments << "--target" << retTarget;
    arguments << "--parallel";

    Print("开始执行 CMake 构建...");
    Print("命令: " + cmake + " " + arguments.join(" "));
    Print("目标: " + target);
    Print("配置: " + mode);
    Print("工作目录: " + buildDir);
    Print(SL);

    DisableBtn();
    process_->start(cmake, arguments);

    if (!process_->waitForStarted(5000)) {
        Print("错误：启动 CMake 构建进程超时", true);
        return;
    }
}

void CmakeBuilder::cmakeReBuild()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认重新构建",
                                                              "确定要重新执行 CMake 配置吗？\n"
                                                              "这将清空构建目录并重新生成构建文件。",
                                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        Print("用户取消重新构建");
        return;
    }

    Print("重新构建开始...");
    auto buildDir = ui->edBuildDir->text().trimmed();
    QDir dir(buildDir);
    if (dir.exists()) {
        if (dir.removeRecursively()) {
            Print("已清空旧模式的构建目录: " + buildDir);
            QDir().mkpath(buildDir);
        } else {
            Print("错误: 无法清空构建目录: " + buildDir);
        }
    }
    cmakeConfig();
    QTimer::singleShot(1500, [this]() {
        if (!configRet_) {
            return;
        }
        cmakeBuild();
    });
}

void CmakeBuilder::DisableBtn()
{
    ui->btnBuild->setEnabled(false);
    ui->btnReBuild->setEnabled(false);
    ui->btnConfig->setEnabled(false);
}

void CmakeBuilder::EnableBtn()
{
    ui->btnBuild->setEnabled(true);
    ui->btnReBuild->setEnabled(true);
    ui->btnConfig->setEnabled(true);
}

void CmakeBuilder::onProcessReadyRead()
{
    // 处理标准输出
    QByteArray outputData = process_->readAllStandardOutput();
    if (!outputData.isEmpty()) {
        static QString stdoutBuffer;
        stdoutBuffer.append(QString::fromLocal8Bit(outputData));

        // 使用 QTextStream 按行读取
        QTextStream stream(&stdoutBuffer);
        QString line;

        while (stream.readLineInto(&line)) {
            if (!line.isEmpty()) {
                Print(line, false);
            }
        }

        // 保存剩余的不完整行
        stdoutBuffer = stream.readAll();
    }

    // 处理标准错误（类似）
    QByteArray errorData = process_->readAllStandardError();
    if (!errorData.isEmpty()) {
        static QString stderrBuffer;
        stderrBuffer.append(QString::fromLocal8Bit(errorData));

        QTextStream stream(&stderrBuffer);
        QString line;

        while (stream.readLineInto(&line)) {
            if (!line.isEmpty()) {
                Print(line, true);
            }
        }

        stderrBuffer = stream.readAll();
    }
}

void CmakeBuilder::Print(const QString& text, bool isError)
{
    if (text.isEmpty()) {
        return;
    }

    QString processedText = text;

    // 处理多种换行情况
    if (!processedText.endsWith('\n') && !processedText.endsWith('\r') && !processedText.endsWith(QChar::LineFeed)) {
        processedText += '\n';
    }

    // 获取当前时间
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    QTextCursor cursor = ui->pedOutput->textCursor();
    cursor.movePosition(QTextCursor::End);

    // 1. 先插入时间戳（灰色）
    QTextCharFormat timeFormat;
    timeFormat.setForeground(QBrush(QColor(128, 128, 128)));   // 灰色时间戳
    timeFormat.setFontWeight(QFont::Normal);
    cursor.setCharFormat(timeFormat);
    cursor.insertText("[" + timestamp + "] ");

    // 2. 插入内容文本
    QTextCharFormat contentFormat;
    if (isError) {
        contentFormat.setForeground(QBrush(QColor(200, 0, 0)));   // 深红色错误信息
        contentFormat.setFontWeight(QFont::Bold);
    } else {
        contentFormat.setForeground(QBrush(QColor(0, 0, 0)));   // 黑色普通信息
        contentFormat.setFontWeight(QFont::Normal);
    }
    cursor.setCharFormat(contentFormat);
    cursor.insertText(processedText);

    // 自动滚动到底部
    ui->pedOutput->ensureCursorVisible();
}

QVector<QString> CmakeBuilder::getTarget()
{
    QVector<QString> targetFiles;

    QString ninjaFile = ui->edBuildDir->text() + "/build.ninja";
    QFile file(ninjaFile);

    if (!file.open(QIODevice::ReadOnly)) {
        return targetFiles;
    }

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    // 使用正则表达式匹配所有 TARGET_FILE = 行
    QRegularExpression regex(R"(TARGET_FILE\s*=\s*([^\s]+))");
    QRegularExpressionMatchIterator matches = regex.globalMatch(content);

    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        QString targetFile = match.captured(1).trimmed();
        targetFiles.append(targetFile);
    }
    targetFiles = QSet<QString>(targetFiles.begin(), targetFiles.end()).values();
    return targetFiles;
}

void CmakeBuilder::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 读取剩余的输出
    onProcessReadyRead();

    Print(SL);

    if (exitStatus == QProcess::NormalExit) {
        if (exitCode == 0) {
            Print("CMake 执行成功！");
            Print(SL);
        } else {
            Print("CMake 指令执行完成，但有警告或错误。", true);
            Print("退出码: " + QString::number(exitCode), true);
            Print(SL);
        }
    } else {
        Print("CMake 进程异常退出", true);
    }

    // 重新启用按钮
    ui->btnConfig->setEnabled(true);
}

void CmakeBuilder::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "进程启动失败，请检查 CMake 路径是否正确";
        break;
    case QProcess::Crashed:
        errorMsg = "CMake 进程崩溃";
        break;
    case QProcess::Timedout:
        errorMsg = "进程执行超时";
        break;
    case QProcess::WriteError:
        errorMsg = "写入错误";
        break;
    case QProcess::ReadError:
        errorMsg = "读取错误";
        break;
    default:
        errorMsg = "未知错误";
    }

    Print("错误: " + errorMsg, true);
    ui->btnConfig->setEnabled(true);
}