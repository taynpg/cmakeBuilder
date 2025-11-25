#include "cmakebuilder.h"

#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QTimer>

#include "./ui_cmakebuilder.h"

constexpr auto SL = "----------------------------------------------";

CmakeBuilder::CmakeBuilder(QWidget* parent) : QDialog(parent), ui(new Ui::CmakeBuilder)
{
    ui->setupUi(this);

    config_ = new BuilderConfig(this);

    InitData();
    LoadConfig();
    BaseInit();

    setWindowTitle("cmakeBuilder v1.0.5");
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

    ui->btnConfig->setStyleSheet("background-color: #e8f5e8;");
    ui->btnBuild->setStyleSheet("background-color: #e3f2fd;");

    connect(this, &CmakeBuilder::sigPrint, this, [this](const QString& msg) { Print(msg); });
    connect(process_, &QProcess::readyReadStandardOutput, this, &CmakeBuilder::onProcessReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &CmakeBuilder::onProcessReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &CmakeBuilder::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &CmakeBuilder::onProcessError);
    connect(ui->btnSaveConfig, &QPushButton::clicked, this, &CmakeBuilder::SaveCur);
    connect(ui->btnLoadConfig, &QPushButton::clicked, this, &CmakeBuilder::SimpleLoad);
    connect(ui->btnAddArg, &QPushButton::clicked, this, &CmakeBuilder::newArg);
    connect(ui->btnDelArg, &QPushButton::clicked, this, &CmakeBuilder::delArg);
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
    connect(this, &CmakeBuilder::processBuildNinja, this, [this]() { onBuildNinjaChanged(""); });
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
    for (const auto& item : keys) {
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
    ui->edVcEnv->setText(o.vcEnv);

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
    if (!config_->SaveData(o)) {
        QMessageBox::information(this, "提示", "保存失败");
        return;
    }
    config_->SetCurUse(o.key);
    QMessageBox::information(this, "提示", "已保存。");
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

void CmakeBuilder::terminalProcess()
{
    //process_->terminate();
    Print("强制终止cmake执行...");
    process_->kill();
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
    ui->btnCancel->setEnabled(false);
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

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    process_->setProcessEnvironment(env);
    process_->start(cmake, arguments);

    if (!process_->waitForStarted(20000)) {
        Print("错误：启动 CMake 进程超时", true);
        return;
    }
    checkBuildNinjaFile();
}

void CmakeBuilder::checkBuildNinjaFile(int attempt)
{
    const int maxAttempts = 50;   // 最多尝试50次（10秒）
    const int interval = 200;     // 每次间隔200ms
    if (QFile::exists(buildFile_)) {
        emit processBuildNinja();
        return;
    }
    if (attempt < maxAttempts) {
        QTimer::singleShot(interval, this, [this, attempt]() { checkBuildNinjaFile(attempt + 1); });
        return;
    }
    sigPrint("错误：等待 build.ninja 文件超时");
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
        onVCEnvReady(watcher->result());
        watcher->deleteLater();
    });

    watcher->setFuture(future);
}

void CmakeBuilder::onVCEnvReady(QProcessEnvironment vsEnv)
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

    // 添加附加参数
    QString additionArg = ui->cbAdditionArg->currentText();
    if (!additionArg.isEmpty()) {
        arguments << additionArg;
    }

    Print("命令: " + cmake + " " + arguments.join(" "));
    Print("工作目录: " + buildDir);
    Print(SL);

    DisableBtn();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(vsEnv);
    process_->setProcessEnvironment(env);
    process_->start(cmake, arguments);

    if (!process_->waitForStarted(5000)) {
        Print("错误：启动配置进程超时", true);
        return;
    }
    checkBuildNinjaFile();
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
    return curEnvValue_;
}

QProcessEnvironment CmakeBuilder::parseEnvironmentOutput(const QString& output)
{
    QProcessEnvironment env;
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

    return env;
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
    ui->btnAddArg->setEnabled(false);
    ui->btnDelArg->setEnabled(false);
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
    ui->cbAdditionArg->setEnabled(false);
    //ui->btnCancel->setEnabled(true);
}

void CmakeBuilder::EnableBtn()
{
    ui->btnBuild->setEnabled(true);
    ui->btnConfig->setEnabled(true);
    ui->btnAddCmake->setEnabled(true);
    ui->btnSelSource->setEnabled(true);
    ui->btnSelBuildDir->setEnabled(true);
    ui->btnAddArg->setEnabled(true);
    ui->btnDelArg->setEnabled(true);
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
    ui->cbAdditionArg->setEnabled(true);
    //ui->btnCancel->setEnabled(false);
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
