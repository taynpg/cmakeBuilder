#include "cmakebuilder.h"

#include <QDir>

#include "./ui_cmakebuilder.h"

CmakeBuilder::CmakeBuilder(QWidget* parent) : QDialog(parent), ui(new Ui::CmakeBuilder)
{
    ui->setupUi(this);
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &CmakeBuilder::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &CmakeBuilder::onProcessReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &CmakeBuilder::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &CmakeBuilder::onProcessError);
    TestInit();
}

CmakeBuilder::~CmakeBuilder()
{
    delete ui;
}

void CmakeBuilder::TestInit()
{
    ui->cbCMake->addItem("C:/Program Files/CMake/bin/cmake.exe");
    ui->cbCMake->setCurrentIndex(0);
    ui->cbSource->addItem("D:/Demo/untitled");
    ui->cbSource->setCurrentIndex(0);
    ui->edBuildDir->setText("D:/Demo/untitled/build");

    ui->cbType->addItem("Ninja");
    ui->cbType->setCurrentText(0);

    ui->cbMode->addItem("Debug");
    ui->cbMode->addItem("Release");
    ui->cbMode->setCurrentIndex(0);

    ui->cbTarget->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->cbMode->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->cbType->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    connect(ui->btnConfig, &QPushButton::clicked, this, &CmakeBuilder::TestConfig);
    connect(ui->btnBuild, &QPushButton::clicked, this, &CmakeBuilder::TestBuild);
}

void CmakeBuilder::TestConfig()
{
    auto sourceDir = ui->cbSource->currentText();
    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->cbCMake->currentText();
    QString type = ui->cbType->currentText();
    auto mode = ui->cbMode->currentText();

    // 检查进程是否正在运行
    if (m_process->state() == QProcess::Running) {
        appendOutput("CMake 进程正在运行，请等待完成...", true);
        return;
    }

    // 清空输出
    ui->pedOutput->clear();

    // 确保构建目录存在
    QDir dir(buildDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            appendOutput("错误：无法创建构建目录 " + buildDir, true);
            return;
        }
    }

    // 设置工作目录
    m_process->setWorkingDirectory(buildDir);

    // 准备 CMake 参数
    QStringList arguments;
    arguments << "-S" << sourceDir;
    arguments << "-B" << buildDir;
    arguments << "-G" << type;

    // 添加其他常用选项
    QString buildType = "-DCMAKE_BUILD_TYPE=" + mode;
    arguments << buildType;

    appendOutput("开始执行 CMake 配置...");
    appendOutput("命令: " + cmake + " " + arguments.join(" "));
    appendOutput("工作目录: " + buildDir);
    appendOutput("----------------------------------------");

    // 禁用按钮，防止重复点击
    ui->btnConfig->setEnabled(false);

    // 启动进程
    m_process->start(cmake, arguments);

    if (!m_process->waitForStarted(5000)) {
        appendOutput("错误：启动 CMake 进程超时", true);
        ui->btnConfig->setEnabled(true);
        return;
    }

    auto ret = getTarget();
    ui->cbTarget->clear();
    ui->cbTarget->addItem("all");
    for (const auto& item : ret) {
        ui->cbTarget->addItem(item);
    }
    ui->cbTarget->setCurrentIndex(0);
}

void CmakeBuilder::TestBuild()
{
    auto buildDir = ui->edBuildDir->text().trimmed();
    auto cmake = ui->cbCMake->currentText();
    auto target = ui->cbTarget->currentText();
    auto mode = ui->cbMode->currentText();

    // 检查进程是否正在运行
    if (m_process->state() == QProcess::Running) {
        appendOutput("CMake 进程正在运行，请等待完成...", true);
        return;
    }

    // 检查构建目录是否存在
    if (!QDir(buildDir).exists()) {
        appendOutput("错误：构建目录不存在，请先执行配置", true);
        return;
    }

    // 检查 CMakeCache.txt 是否存在（确保已配置）
    if (!QFile::exists(buildDir + "/CMakeCache.txt")) {
        appendOutput("错误：项目未配置，请先执行 CMake 配置", true);
        return;
    }

    // 清空输出
    ui->pedOutput->clear();

    // 设置工作目录
    m_process->setWorkingDirectory(buildDir);

    // 准备 CMake 构建参数
    QStringList arguments;
    arguments << "--build" << buildDir;
    
    // 添加构建配置
    arguments << "--config" << mode;

    auto retTarget = QFileInfo(target).baseName();
    arguments << "--target" << retTarget;
    
    // 添加并行构建选项（可选）
    arguments << "--parallel";
    
    // 添加详细输出（可选）
    // arguments << "--verbose";

    appendOutput("开始执行 CMake 构建...");
    appendOutput("命令: " + cmake + " " + arguments.join(" "));
    appendOutput("目标: " + target);
    appendOutput("配置: " + mode);
    appendOutput("工作目录: " + buildDir);
    appendOutput("----------------------------------------");

    // 禁用按钮，防止重复点击
    ui->btnConfig->setEnabled(false);
    ui->btnBuild->setEnabled(false);

    // 启动进程
    m_process->start(cmake, arguments);

    if (!m_process->waitForStarted(5000)) {
        appendOutput("错误：启动 CMake 构建进程超时", true);
        ui->btnConfig->setEnabled(true);
        ui->btnBuild->setEnabled(true);
        return;
    }
}

void CmakeBuilder::onProcessReadyRead()
{
    // 处理标准输出
    QByteArray outputData = m_process->readAllStandardOutput();
    if (!outputData.isEmpty()) {
        static QString stdoutBuffer;
        stdoutBuffer.append(QString::fromLocal8Bit(outputData));

        // 使用 QTextStream 按行读取
        QTextStream stream(&stdoutBuffer);
        QString line;

        while (stream.readLineInto(&line)) {
            if (!line.isEmpty()) {
                appendOutput(line, false);
            }
        }

        // 保存剩余的不完整行
        stdoutBuffer = stream.readAll();
    }

    // 处理标准错误（类似）
    QByteArray errorData = m_process->readAllStandardError();
    if (!errorData.isEmpty()) {
        static QString stderrBuffer;
        stderrBuffer.append(QString::fromLocal8Bit(errorData));

        QTextStream stream(&stderrBuffer);
        QString line;

        while (stream.readLineInto(&line)) {
            if (!line.isEmpty()) {
                appendOutput(line, true);
            }
        }

        stderrBuffer = stream.readAll();
    }
}

void CmakeBuilder::appendOutput(const QString& text, bool isError)
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

    appendOutput("----------------------------------------");

    if (exitStatus == QProcess::NormalExit) {
        if (exitCode == 0) {
            appendOutput("CMake 执行成功！");
            appendOutput("----------------------------------------");
        } else {
            appendOutput("CMake 指令执行完成，但有警告或错误", true);
            appendOutput("退出码: " + QString::number(exitCode), true);
            appendOutput("----------------------------------------");
        }
    } else {
        appendOutput("CMake 进程异常退出", true);
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

    appendOutput("错误: " + errorMsg, true);
    ui->btnConfig->setEnabled(true);
}