#include "cmakebuilder.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QTimer>

#include "./ui_cmakebuilder.h"

#if defined(_WIN32)
#include <windows.h>
std::string u8_to_ansi(const std::string& str)
{
    int wideCharLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (wideCharLen <= 0) {
        return "";
    }
    std::wstring wideStr(wideCharLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wideStr[0], wideCharLen);
    int gbkLen = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (gbkLen <= 0) {
        return "";
    }
    std::string gbkStr(gbkLen, '\0');
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &gbkStr[0], gbkLen, nullptr, nullptr);

    gbkStr.resize(gbkLen - 1);
    return gbkStr;
}
std::string ansi_to_u8(const std::string& str)
{
    int wideCharLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    if (wideCharLen <= 0) {
        return "";
    }
    std::wstring wideStr(wideCharLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideCharLen);

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) {
        return "";
    }
    std::string utf8Str(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &utf8Str[0], utf8Len, nullptr, nullptr);

    utf8Str.resize(utf8Len - 1);
    return utf8Str;
}
QString code_handle(const QString& str)
{
    auto s = str.toStdString();
    s = u8_to_ansi(s);
    return QString::fromStdString(s);
}
#endif

constexpr auto SL = "----------------------------------------------";

CmakeBuilder::CmakeBuilder(QWidget* parent) : QDialog(parent), ui(new Ui::CmakeBuilder)
{
    ui->setupUi(this);

    config_ = new BuilderConfig(this);

    InitData();
    LoadConfig();
    BaseInit();

    setWindowTitle("cmakeBuilder v1.1.1");
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);

    auto s = config_->getSize();
    if (s.first != 0 && s.second != 0) {
        resize(s.first, s.second);
    }
}

CmakeBuilder::~CmakeBuilder()
{
    delete ui;
}

void CmakeBuilder::closeEvent(QCloseEvent* event)
{
    QSize currentSize = this->size();
    int width = currentSize.width();
    int height = currentSize.height();
    if (width > 0 && height > 0) {
        config_->setSize(width, height);
    }
    auto cur = ui->cbProject->currentText();
    if (!cur.isEmpty()) {
        config_->SetCurUse(ui->cbProject->currentText());
    }
    QWidget::closeEvent(event);
}

void CmakeBuilder::InitData()
{
    process_ = new QProcess(this);

    modes_ = {"All", "Debug", "Release"};

    InitTab();

    auto configDir = QDir::homePath() + "/.config/cmakeBuilder";
    QDir dir(configDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::information(this, "提示", "创建配置文件目录失败。");
            return;
        }
    }

    config_->setConfigDir(configDir + "/config.json");
    config_->setConfigSizeDir(configDir + "/size.json");
    config_->setConfigUseDir(configDir + "/curuse.json");

    ui->cbProject->setEditable(true);
    ui->cbProject->setMinimumWidth(150);
    ui->edCMake->setFocusPolicy(Qt::ClickFocus);
    ui->pedOutput->setReadOnly(true);

    // ui->btnConfig->setStyleSheet("background-color: red;");
    // ui->btnBuild->setStyleSheet("background-color: blue;");

    connect(ui->btnStart, &QPushButton::clicked, this, &CmakeBuilder::StartExe);
    connect(this, &CmakeBuilder::sigPrint, this, [this](const QString& msg) { Print(msg); });
    connect(process_, &QProcess::readyReadStandardOutput, this, &CmakeBuilder::onProcessReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &CmakeBuilder::onProcessReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &CmakeBuilder::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &CmakeBuilder::onProcessError);
    connect(ui->btnSaveConfig, &QPushButton::clicked, this, &CmakeBuilder::SaveCur);
    connect(ui->btnLoadConfig, &QPushButton::clicked, this, &CmakeBuilder::SimpleLoad);
    connect(ui->btnDelConfig, &QPushButton::clicked, this, [this]() {
        int ret = QMessageBox::question(this, "确认操作", "确定要删除吗？");
        if (ret != QMessageBox::Yes) {
            return;
        }
        auto key = ui->cbProject->currentText();
        if (config_->DelData(key)) {
            QMessageBox::information(this, "提示", "已删除");
            int index = ui->cbProject->findText(key);
            if (index >= 0) {
                ui->cbProject->removeItem(index);
            }
            if (ui->cbProject->count() > 0) {
                ui->cbProject->setCurrentIndex(0);
            }
            config_->SetCurUse(ui->cbProject->currentText());
        }
    });
    connect(ui->btnClearEnv, &QPushButton::clicked, this, [this]() { ui->edVcEnv->clear(); });
    connect(this, &CmakeBuilder::sigEnableBtn, this, [this](bool enable) {
        if (enable) {
            EnableBtn();
        } else {
            DisableBtn();
        }
    });

    connect(ui->btnClear, &QPushButton::clicked, this, [this]() {
        int ret = QMessageBox::question(this, "确认操作", "确定要清空CMake配置吗？");
        if (ret != QMessageBox::Yes) {
            return;
        }
        Print("开始清空...");
        auto buildDir = ui->edBuildDir->text().trimmed();
        QDir dir(buildDir);
        if (dir.exists()) {
            if (dir.removeRecursively()) {
                Print("已清空旧模式的构建目录: " + buildDir);
                QDir().mkpath(buildDir);
                ui->cbTarget->clear();
            } else {
                Print("错误: 无法清空构建目录: " + buildDir);
            }
        }
    });

    connect(ui->btnCancel, &QPushButton::clicked, this, &CmakeBuilder::terminalProcess);
}

void CmakeBuilder::LoadConfig()
{
    QVector<QString> keys;
    if (!config_->GetAllKeys(keys)) {
        return;
    }
    QString curuse;
    if (!config_->GetCurUse(curuse)) {
        return;
    }
    if (!keys.contains(curuse)) {
        return;
    }
    OneConfig o;
    if (!config_->GetData(curuse, o)) {
        return;
    }
    SetUi(o);
    ui->cbProject->clear();
    for (auto& item : keys) {
        ui->cbProject->addItem(item);
    }
    ui->cbProject->setCurrentText(curuse);
}

bool CmakeBuilder::SimpleLoad()
{
    QString key = ui->cbProject->currentText();
    OneConfig o;
    if (!config_->GetData(key, o)) {
        return false;
    }
    SetUi(o);
    return true;
}

OneConfig CmakeBuilder::ReadUi()
{
    OneConfig o;
    o.cmakePath = ui->edCMake->text().trimmed().replace('\\', '/');
    o.sourceDir = ui->edSource->text().trimmed().replace('\\', '/');
    o.buildDir = ui->edBuildDir->text().trimmed().replace('\\', '/');
    o.vcEnv = ui->edVcEnv->text().trimmed().replace('\\', '/');
    o.arg = ui->edArg->text().trimmed();
    o.additonArgs.clear();

    for (int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        AddArgItem item;
        item.name = ui->tableWidget->item(i, 0)->text().trimmed();
        item.type = ui->tableWidget->item(i, 1)->text().trimmed();
        item.mode = ui->tableWidget->item(i, 2)->text().trimmed();
        item.value = ui->tableWidget->item(i, 3)->text().trimmed().replace('\\', '/');
        o.additonArgs.append(item);
    }
    return o;
}

void CmakeBuilder::SetUi(const OneConfig& o)
{
    // 设置基本路径
    ui->edCMake->setText(o.cmakePath);
    ui->edSource->setText(o.sourceDir);
    ui->edBuildDir->setText(o.buildDir);
    ui->edVcEnv->setText(o.vcEnv);
    ui->edArg->setText(o.arg);
    clearTable();

    for (int i = 0; i < o.additonArgs.count(); ++i) {
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(o.additonArgs[i].name));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(o.additonArgs[i].type));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(o.additonArgs[i].mode));
        ui->tableWidget->setItem(row, 3, new QTableWidgetItem(o.additonArgs[i].value));
        // 为Type列设置下拉框
        setTypeComboBox(row, QStringList::fromVector(typeOptions_));
        setModeComboBox(row, QStringList::fromVector(modes_));
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
    if (!config_->SaveData(o)) {
        QMessageBox::information(this, "提示", "保存失败");
        return;
    }
    config_->SetCurUse(o.key);
    QMessageBox::information(this, "提示", "已保存。");
}

void CmakeBuilder::terminalProcess()
{
    // process_->terminate();
    Print("强制终止cmake执行...");
    process_->kill();
}

void CmakeBuilder::InitTab()
{
    // 设置列数
    ui->tableWidget->setColumnCount(4);

    // 设置表头
    QStringList headers;
    headers << "名称" << "类型" << "应用范围" << "值";
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    // 设置列宽策略
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setColumnWidth(0, 200);
    ui->tableWidget->setColumnWidth(1, 100);
    ui->tableWidget->setColumnWidth(2, 100);
    // Value列自动填充剩余空间

    // 设置选择行为
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // 设置编辑策略
    ui->tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    // 允许用户排序
    ui->tableWidget->setSortingEnabled(true);

    // 设置行高
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(25);

    // 隐藏垂直表头（可选）
    ui->tableWidget->verticalHeader()->setVisible(false);

    // 设置交替行颜色
    ui->tableWidget->setAlternatingRowColors(true);

    // 添加右键菜单
    ui->tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, this, &CmakeBuilder::onTableContextMenu);

    typeOptions_ = {"STRING", "PATH", "BOOL", "FILEPATH", "INTERNAL"};
}

void CmakeBuilder::StartExe()
{
    QString basePath = ui->edBuildDir->text();
    QString relativePath = ui->cbTarget->currentText();

    basePath = basePath.trimmed();
    relativePath = relativePath.trimmed();

    if (basePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "构建目录路径为空！");
        return;
    }

    QDir baseDir(basePath);
    if (!baseDir.exists()) {
        QMessageBox::warning(this, "错误", QString("构建目录不存在：\n%1").arg(basePath));
        return;
    }

    if (relativePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "没有要启动的程序！");
        return;
    }

    QString fullPath = QDir::cleanPath(baseDir.absoluteFilePath(relativePath));

    QFileInfo fileInfo(fullPath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "错误", QString("目标程序不存在：\n%1").arg(fullPath));
        return;
    }

    QString additionArg = ui->edArg->text().trimmed();
    QString cmd = "cmd.exe";
    QStringList args;

    QString fullCommand = QDir::toNativeSeparators(fullPath);
    if (!additionArg.isEmpty()) {
        fullCommand += " " + additionArg;
    }

    if (fullCommand.contains(" ")) {
        fullCommand = "" + fullCommand + "";
    }

    args << "/c" << "start" << "cmd.exe" << "/k" << fullCommand;
    QProcess::startDetached(cmd, args, fileInfo.absolutePath());
}

void CmakeBuilder::BaseInit()
{
    ui->cbType->addItem("Ninja");
    ui->cbType->setCurrentText(0);

    for (int i = 0; i < modes_.size(); ++i) {
        if (modes_[i] == "All") {
            continue;
        }
        ui->cbMode->addItem(modes_[i]);
    }
    ui->cbMode->setCurrentIndex(0);

    ui->cbTarget->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->cbMode->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->cbType->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    connect(ui->btnConfig, &QPushButton::clicked, this, &CmakeBuilder::cmakeConfigWithVCEnv);
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
            ui->edBuildDir->setText(QDir::toNativeSeparators(dirPath));
        }
    });
    // ui->btnCancel->setEnabled(false);
}

void CmakeBuilder::cmakeConfig()
{
    ui->pedOutput->clear();
    configRet_ = false;

    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->edCMake->text().trimmed();
    auto target = ui->cbTarget->currentText();
    auto mode = ui->cbMode->currentText();
    curTarget_ = ui->cbTarget->currentText();
    ui->cbTarget->clear();

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

    QDir dir(buildDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            Print("错误：无法创建构建目录 " + buildDir, true);
            return;
        }
    }
    process_->setWorkingDirectory(buildDir);
    buildFile_ = buildDir + "/build.ninja";

    // 3. 构建CMake参数
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    QStringList arguments;
    arguments << "-S" << ui->edSource->text().trimmed();
    arguments << "-B" << buildDir;
    arguments << "-G" << ui->cbType->currentText();
    arguments << "-DCMAKE_BUILD_TYPE=" + mode;
    arguments << "-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE";
    arguments << "-Wno-dev";
    arguments << "--no-warn-unused-cli";

    QStringList additionalArgs = QStringList::fromVector(getAddArgsFromTable(env));
    if (!additionalArgs.isEmpty()) {
        arguments << additionalArgs;
        Print("生成的CMake参数: " + additionalArgs.join(" "));
    }

    Print("开始执行 CMake 配置...");
    Print("命令: " + cmake + " " + arguments.join(" "));
    Print("工作目录: " + buildDir);
    Print(SL);

    DisableBtn();

    process_->setProcessEnvironment(env);
    process_->start(cmake, arguments);

    currentTaskName_ = "config";
    if (!process_->waitForStarted(20000)) {
        Print("错误：启动 CMake 进程超时", true);
        return;
    }
}

void CmakeBuilder::onBuildNinjaChanged(const QString& path)
{
    if (QFile::exists(buildFile_)) {
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
}

void CmakeBuilder::cmakeConfigWithVCEnv()
{
    QString envBat = ui->edVcEnv->text().trimmed();
    if (envBat.isEmpty()) {
        cmakeConfig();
        return;
    }

    if (!QFile::exists(envBat)) {
        QMessageBox::critical(this, "错误", "VC环境脚本不存在:\n" + envBat);
        return;
    }

    ui->pedOutput->clear();
    configRet_ = false;

    // 获取配置参数
    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->edCMake->text().trimmed();
    auto sourceDir = ui->edSource->text().trimmed();
    auto generator = ui->cbType->currentText();

    curTarget_ = ui->cbTarget->currentText();
    ui->cbTarget->clear();

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

    DisableBtn();
    buildFile_ = buildDir + "/build.ninja";
    curEnvBatFile_ = envBat;
    QFuture<QProcessEnvironment> future = QtConcurrent::run([&]() { return getVCEnvironment(curEnvBatFile_); });

    QFutureWatcher<QProcessEnvironment>* watcher = new QFutureWatcher<QProcessEnvironment>(this);
    connect(watcher, &QFutureWatcher<QProcessEnvironment>::finished, this, [this, watcher]() {
        onVCEnvReady();
        watcher->deleteLater();
    });

    watcher->setFuture(future);
}

void CmakeBuilder::onVCEnvReady()
{
    Print("VC环境变量获取成功");

    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->edCMake->text().trimmed();
    auto sourceDir = ui->edSource->text().trimmed();
    auto generator = ui->cbType->currentText();
    auto mode = ui->cbMode->currentText();

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
    arguments << "-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE";
    arguments << "-Wno-dev";
    arguments << "--no-warn-unused-cli";

    QStringList additionalArgs = QStringList::fromVector(getAddArgsFromTable(curEnvValue_));
    if (!additionalArgs.isEmpty()) {
        arguments << additionalArgs;
        Print("生成的CMake参数: " + additionalArgs.join(" "));
    }

    Print("命令: " + cmake + " " + arguments.join(" "));
    Print("工作目录: " + buildDir);
    Print(SL);

    DisableBtn();
    process_->start(cmake, arguments);

    currentTaskName_ = "config";
    if (!process_->waitForStarted(5000)) {
        Print("错误：启动配置进程超时", true);
        return;
    }
}

QProcessEnvironment CmakeBuilder::getVCEnvironment(const QString& vcvarsPath)
{
    if (!curVcEnv_.isEmpty() && curVcEnv_ == vcvarsPath) {
        return curEnvValue_;
    }

    QProcess process;
    process.setProgram("cmd.exe");
    process.setArguments({"/c", "call", vcvarsPath, "&&", "set"});
    sigPrint("获取VC环境变量: " + vcvarsPath);
    process.start();

    if (!process.waitForFinished(15000)) {
        sigPrint("错误：进程执行超时");
        return QProcessEnvironment();
    }

    if (process.exitCode() != 0) {
        sigPrint("错误：VC环境脚本执行失败");
        return QProcessEnvironment();
    }

    QString allOutput = QString::fromLocal8Bit(process.readAllStandardOutput());

    if (allOutput.isEmpty()) {
        sigPrint("错误：没有获取到输出");
        return QProcessEnvironment();
    }
    sigPrint("成功获取VC环境变量");
    curVcEnv_ = vcvarsPath;
    curEnvValue_ = parseEnvironmentOutput(allOutput);
    process_->setProcessEnvironment(curEnvValue_);
    return curEnvValue_;
}

QProcessEnvironment CmakeBuilder::parseEnvironmentOutput(const QString& output)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList lines = output.split('\n');

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty())
            continue;

        int equalsIndex = trimmedLine.indexOf('=');
        if (equalsIndex > 0) {
            QString varName = trimmedLine.left(equalsIndex).trimmed();
            QString varValue = trimmedLine.mid(equalsIndex + 1).trimmed();
            env.insert(varName, varValue);
        }
    }

    // QStringList keys = env.keys();
    // keys.sort();
    // sigPrint("环境变量数量: " + QString::number(keys.size()));
    // for (const QString& key : keys) {
    //     QString value = env.value(key);
    //     sigPrint(key + " = " + value);
    // }

    return env;
}

QString CmakeBuilder::expandEnvVar(const QProcessEnvironment& env, const QString& str)
{
    QString result = str;

    // 展开 %VAR% 格式的环境变量（Windows）
    QRegularExpression winRe("%([^%]+)%");
    QRegularExpressionMatchIterator winIt = winRe.globalMatch(str);
    while (winIt.hasNext()) {
        QRegularExpressionMatch match = winIt.next();
        QString varName = match.captured(1);
        if (env.contains(varName)) {
            result.replace("%" + varName + "%", env.value(varName));
        }
    }

    // 展开 $VAR 或 ${VAR} 格式的环境变量（Unix）
    QRegularExpression unixRe("\\$(?:{([^}]+)}|(\\w+))");
    QRegularExpressionMatchIterator unixIt = unixRe.globalMatch(str);
    while (unixIt.hasNext()) {
        QRegularExpressionMatch match = unixIt.next();
        QString varName = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        if (env.contains(varName)) {
            if (match.captured(0).startsWith("${")) {
                result.replace("${" + varName + "}", env.value(varName));
            } else {
                result.replace("$" + varName, env.value(varName));
            }
        }
    }

    return result;
}

void CmakeBuilder::onTableContextMenu(const QPoint& pos)
{
    QMenu menu(this);

    QAction* addAction = menu.addAction("添加参数");
    QAction* deleteAction = menu.addAction("删除参数");
    menu.addSeparator();
    QAction* clearAction = menu.addAction("清空所有");

    // 只有选中行时才启用删除操作
    deleteAction->setEnabled(ui->tableWidget->currentRow() >= 0);

    QAction* selectedAction = menu.exec(ui->tableWidget->viewport()->mapToGlobal(pos));

    if (selectedAction == addAction) {
        addTableRow();
    } else if (selectedAction == deleteAction) {
        deleteTableRow();
    } else if (selectedAction == clearAction) {
        clearTable();
    }
}

void CmakeBuilder::setModeComboBox(int row, const QStringList& options)
{
    QComboBox* comboBox = new QComboBox();
    comboBox->addItems(options);
    comboBox->setEditable(false);

    // 获取当前单元格的值
    QTableWidgetItem* modeItem = ui->tableWidget->item(row, 2);
    QString currentType = "Debug";   // 默认值

    if (modeItem && !modeItem->text().isEmpty()) {
        currentType = modeItem->text();
    }

    // 设置下拉框当前选中项
    int index = comboBox->findText(currentType);
    if (index >= 0) {
        comboBox->setCurrentIndex(index);
    } else {
        comboBox->setCurrentIndex(0);   // 如果找不到，默认选第一个
    }

    ui->tableWidget->setCellWidget(row, 2, comboBox);

    // 连接信号，当下拉框选择改变时更新单元格内容
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, row, comboBox](int index) {
        if (row < ui->tableWidget->rowCount()) {
            QTableWidgetItem* item = ui->tableWidget->item(row, 2);
            if (!item) {
                item = new QTableWidgetItem();
                ui->tableWidget->setItem(row, 2, item);
            }
            item->setText(comboBox->currentText());
        }
    });
}

void CmakeBuilder::setTypeComboBox(int row, const QStringList& options)
{
    QComboBox* comboBox = new QComboBox();
    comboBox->addItems(options);
    comboBox->setEditable(false);

    // 获取当前单元格的值
    QTableWidgetItem* typeItem = ui->tableWidget->item(row, 1);
    QString currentType = "STRING";   // 默认值

    if (typeItem && !typeItem->text().isEmpty()) {
        currentType = typeItem->text();
    }

    // 设置下拉框当前选中项
    int index = comboBox->findText(currentType);
    if (index >= 0) {
        comboBox->setCurrentIndex(index);
    } else {
        comboBox->setCurrentIndex(0);   // 如果找不到，默认选第一个
    }

    ui->tableWidget->setCellWidget(row, 1, comboBox);

    // 连接信号，当下拉框选择改变时更新单元格内容
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, row, comboBox](int index) {
        if (row < ui->tableWidget->rowCount()) {
            QTableWidgetItem* item = ui->tableWidget->item(row, 1);
            if (!item) {
                item = new QTableWidgetItem();
                ui->tableWidget->setItem(row, 1, item);
            }
            item->setText(comboBox->currentText());
        }
    });
}

void CmakeBuilder::addTableRow()
{
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);

    ui->tableWidget->setItem(row, 0, new QTableWidgetItem(""));
    ui->tableWidget->setItem(row, 1, new QTableWidgetItem("STRING"));
    ui->tableWidget->setItem(row, 2, new QTableWidgetItem("Debug"));
    ui->tableWidget->setItem(row, 3, new QTableWidgetItem(""));

    setTypeComboBox(row, QStringList::fromVector(typeOptions_));
    setModeComboBox(row, QStringList::fromVector(modes_));
}

void CmakeBuilder::deleteTableRow()
{
    int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        ui->tableWidget->removeRow(currentRow);
    }
}

void CmakeBuilder::clearTable()
{
    ui->tableWidget->setRowCount(0);
}

QVector<QString> CmakeBuilder::getAddArgsFromTable(const QProcessEnvironment& env)
{
    QVector<QString> args;

    auto curMode = ui->cbMode->currentText();

    for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
        QTableWidgetItem* nameItem = ui->tableWidget->item(row, 0);
        QTableWidgetItem* typeItem = ui->tableWidget->item(row, 1);
        QTableWidgetItem* modeItem = ui->tableWidget->item(row, 2);
        QTableWidgetItem* valueItem = ui->tableWidget->item(row, 3);

        // 跳过空行
        if (!nameItem || nameItem->text().isEmpty()) {
            continue;
        }

        if (modeItem->text() != "All" && modeItem->text() != curMode) {
            continue;
        }

        QString name = nameItem->text().trimmed();
        QString value = valueItem ? valueItem->text().trimmed() : "";

        // 展开环境变量
        value = expandEnvVar(env, value);

        // 构建 CMake 参数
        if (!value.isEmpty()) {
            QString arg = "-D" + name + ":" + typeItem->text() + "=" + value;
            args << arg;
        }
    }

    return args;
}

void CmakeBuilder::cmakeBuild()
{
    if (ui->cbTarget->currentText().isEmpty()) {
        QMessageBox::information(this, "提示", "请先执行CMake配置");
        return;
    }

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

void CmakeBuilder::DisableBtn()
{
    ui->btnBuild->setEnabled(false);
    ui->btnConfig->setEnabled(false);
    ui->btnAddCmake->setEnabled(false);
    ui->btnSelSource->setEnabled(false);
    ui->btnSelBuildDir->setEnabled(false);
    ui->btnClearEnv->setEnabled(false);
    ui->btnClear->setEnabled(false);
    ui->btnDelConfig->setEnabled(false);
    ui->btnLoadConfig->setEnabled(false);
    ui->btnSaveConfig->setEnabled(false);
    ui->cbProject->setEnabled(false);
    ui->cbType->setEnabled(false);
    ui->cbMode->setEnabled(false);
    ui->cbTarget->setEnabled(false);
    ui->edBuildDir->setEnabled(false);
    ui->edSource->setEnabled(false);
    ui->edBuildDir->setEnabled(false);
    ui->edVcEnv->setEnabled(false);
    ui->edCMake->setEnabled(false);
    // ui->btnCancel->setEnabled(true);
}

void CmakeBuilder::EnableBtn()
{
    ui->btnBuild->setEnabled(true);
    ui->btnConfig->setEnabled(true);
    ui->btnAddCmake->setEnabled(true);
    ui->btnSelSource->setEnabled(true);
    ui->btnSelBuildDir->setEnabled(true);
    ui->btnClearEnv->setEnabled(true);
    ui->btnClear->setEnabled(true);
    ui->btnDelConfig->setEnabled(true);
    ui->btnLoadConfig->setEnabled(true);
    ui->btnSaveConfig->setEnabled(true);
    ui->cbProject->setEnabled(true);
    ui->cbType->setEnabled(true);
    ui->cbMode->setEnabled(true);
    ui->cbTarget->setEnabled(true);
    ui->edBuildDir->setEnabled(true);
    ui->edSource->setEnabled(true);
    ui->edBuildDir->setEnabled(true);
    ui->edVcEnv->setEnabled(true);
    ui->edCMake->setEnabled(true);
    // ui->btnCancel->setEnabled(false);
}

void CmakeBuilder::onProcessReadyRead()
{
    QByteArray outputData = process_->readAllStandardOutput();
    if (!outputData.isEmpty()) {
        static QString stdoutBuffer;
        stdoutBuffer.append(QString::fromLocal8Bit(outputData));

        // 手动按换行符分割
        int newlinePos;
        while ((newlinePos = stdoutBuffer.indexOf('\n')) != -1) {
            QString line = stdoutBuffer.left(newlinePos).trimmed();
            stdoutBuffer = stdoutBuffer.mid(newlinePos + 1);

            if (!line.isEmpty()) {
#if defined(_WIN32)
                line = code_handle(line);
#endif
                Print(line, false);
            }
        }

        // stdoutBuffer 中保留的是不完整的行
    }

    // 处理标准错误（和标准输出一样的方案3）
    QByteArray errorData = process_->readAllStandardError();
    if (!errorData.isEmpty()) {
        static QString stderrBuffer;
        stderrBuffer.append(QString::fromLocal8Bit(errorData));

        // 手动按换行符分割处理
        int newlinePos;
        while ((newlinePos = stderrBuffer.indexOf('\n')) != -1) {
            QString line = stderrBuffer.left(newlinePos).trimmed();
            stderrBuffer = stderrBuffer.mid(newlinePos + 1);

            if (!line.isEmpty()) {
#if defined(_WIN32)
                line = code_handle(line);
#endif
                Print(line, true);   // 第二个参数为true，表示错误输出
            }
        }

        // stderrBuffer 自动保留不完整的行
    }
}

void CmakeBuilder::Print(const QString& text, bool isError)
{
    if (text.isEmpty()) {
        return;
    }

    QString processedText = text;
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    QTextCursor cursor = ui->pedOutput->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat timeFormat;
    timeFormat.setForeground(QBrush(QColor(128, 128, 128)));   // 灰色时间戳
    timeFormat.setFontWeight(QFont::Normal);
    cursor.setCharFormat(timeFormat);
    cursor.insertText("[" + timestamp + "] ");

    // 2. 插入内容文本
    QTextCharFormat contentFormat;
    // if (isError) {
    //     contentFormat.setForeground(QBrush(QColor(200, 0, 0)));   //
    //     深红色错误信息 contentFormat.setFontWeight(QFont::Bold);
    // } else {
    contentFormat.setForeground(QBrush(QColor(0, 0, 0)));   // 黑色普通信息
    contentFormat.setFontWeight(QFont::Normal);
    //}
    cursor.setCharFormat(contentFormat);
    cursor.insertText(processedText + "\n");

    // 自动滚动到底部
    // ui->pedOutput->ensureCursorVisible();
    QScrollBar* vScrollBar = ui->pedOutput->verticalScrollBar();
    vScrollBar->setValue(vScrollBar->maximum());
}

QVector<QString> CmakeBuilder::getTarget()
{
    QVector<QString> targetFiles;
    QFile file(buildFile_);

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
    auto r = targetFiles = QSet<QString>(targetFiles.begin(), targetFiles.end()).values().toVector();
    return targetFiles;
}

void CmakeBuilder::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 读取剩余的输出
    onProcessReadyRead();

    Print(SL);

    auto afterFinish = [this]() {
        std::shared_ptr<void> r(nullptr, [this](void*) { currentTaskName_.clear(); });
        if (currentTaskName_ == "config") {
            onBuildNinjaChanged("");
        }
    };

    if (exitStatus == QProcess::NormalExit) {
        if (exitCode == 0) {
            Print("CMake 执行成功！");
            Print(SL);
            afterFinish();
        } else {
            Print("CMake 指令执行完成，但有警告或错误。", true);
            Print("退出码: " + QString::number(exitCode), true);
            Print(SL);
        }
    } else {
        Print("CMake 进程异常退出", true);
    }

    EnableBtn();
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
