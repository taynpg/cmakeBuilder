#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>


struct AddArgItem {
    QString name;
    QString type;
    QString value;
    QString mode;
};

struct OneConfig {
    QString key;
    QString cmakePath;
    QString sourceDir;
    QString buildDir;
    QString curMode;
    QString curTarget;
    QString curType;
    QString vcEnv;
    QVector<AddArgItem> additonArgs;
};

class ConfigPrivate;
class BuilderConfig : public QObject
{
    Q_OBJECT

public:
    BuilderConfig(QObject* parent = nullptr);
    ~BuilderConfig();

public:
    void setConfigDir(const QString& d);
    void setConfigSizeDir(const QString& d);
    void setConfigUseDir(const QString& d);

public:
    bool GetAllKeys(QVector<QString>& keys);
    bool setSize(int w, int h);
    std::pair<int, int> getSize();
    bool SaveData(const OneConfig& config);
    bool GetData(const QString& key, OneConfig& config);
    bool GetCurUse(QString& key);
    bool SetCurUse(const QString& key);
    bool DelData(const QString& key);

Q_SIGNALS:
    void sigMsg(const QString& msg);

private:
    ConfigPrivate* p_{};
};

#endif