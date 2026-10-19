#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

// Small, forgiving INI reader/writer. The original launcher stores raw
// key=value pairs (including backslashes in the environment string), so we
// deliberately avoid QSettings' escaping rules.
class IniFile
{
public:
    struct Section {
        QString name;
        QStringList keyOrder;
        QMap<QString, QString> values;
    };

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;

    bool hasSection(const QString &section) const;
    bool hasKey(const QString &section, const QString &key) const;

    QString value(const QString &section, const QString &key,
                  const QString &defaultValue = QString()) const;
    bool boolValue(const QString &section, const QString &key, bool defaultValue = false) const;
    int intValue(const QString &section, const QString &key, int defaultValue = 0) const;
    qint64 longLongValue(const QString &section, const QString &key, qint64 defaultValue = 0) const;

    void setValue(const QString &section, const QString &key, const QString &value);
    void setBool(const QString &section, const QString &key, bool value);
    void removeKey(const QString &section, const QString &key);
    void removeSection(const QString &section);

    QStringList sections() const;
    QStringList keys(const QString &section) const;
    QMap<QString, QString> sectionValues(const QString &section) const;
    void setSectionValues(const QString &section, const QMap<QString, QString> &values);

    void renameSection(const QString &oldName, const QString &newName);

private:
    int sectionIndex(const QString &section) const;
    int createSection(const QString &section);

    QVector<Section> m_sections;
};
