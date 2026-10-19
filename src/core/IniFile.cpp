#include "IniFile.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

bool IniFile::load(const QString &path, QString *error)
{
    m_sections.clear();

    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Could not open %1: %2").arg(path, file.errorString());
        return false;
    }

    Section *current = nullptr;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine());
        if (line.endsWith(QLatin1Char('\n')))
            line.chop(1);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(';'))
            || trimmed.startsWith(QLatin1Char('#')))
            continue;

        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            const QString name = trimmed.mid(1, trimmed.size() - 2).trimmed();
            const int index = createSection(name);
            current = &m_sections[index];
            continue;
        }

        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0)
            continue;

        if (!current) {
            const int index = createSection(QStringLiteral("General"));
            current = &m_sections[index];
        }

        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1);
        if (key.isEmpty())
            continue;

        if (!current->values.contains(key))
            current->keyOrder.append(key);
        current->values.insert(key, value);
    }

    return true;
}

bool IniFile::save(const QString &path, QString *error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    bool first = true;
    for (const Section &section : m_sections) {
        if (!first)
            out << '\n';
        first = false;
        out << '[' << section.name << "]\n";
        for (const QString &key : section.keyOrder) {
            const auto it = section.values.constFind(key);
            if (it == section.values.constEnd())
                continue;
            out << key << '=' << it.value() << '\n';
        }
    }

    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Could not save %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

int IniFile::sectionIndex(const QString &section) const
{
    for (int i = 0; i < m_sections.size(); ++i) {
        if (m_sections[i].name == section)
            return i;
    }
    return -1;
}

int IniFile::createSection(const QString &section)
{
    const int existing = sectionIndex(section);
    if (existing >= 0)
        return existing;
    Section s;
    s.name = section;
    m_sections.append(s);
    return m_sections.size() - 1;
}

bool IniFile::hasSection(const QString &section) const { return sectionIndex(section) >= 0; }

bool IniFile::hasKey(const QString &section, const QString &key) const
{
    const int index = sectionIndex(section);
    return index >= 0 && m_sections[index].values.contains(key);
}

QString IniFile::value(const QString &section, const QString &key,
                       const QString &defaultValue) const
{
    const int index = sectionIndex(section);
    if (index < 0)
        return defaultValue;
    const auto it = m_sections[index].values.constFind(key);
    return it == m_sections[index].values.constEnd() ? defaultValue : it.value();
}

bool IniFile::boolValue(const QString &section, const QString &key, bool defaultValue) const
{
    if (!hasKey(section, key))
        return defaultValue;
    const QString raw = value(section, key).trimmed().toLower();
    if (raw.isEmpty() || raw == QLatin1String("0") || raw == QLatin1String("false")
        || raw == QLatin1String("no") || raw == QLatin1String("off"))
        return false;
    if (raw == QLatin1String("1") || raw == QLatin1String("true")
        || raw == QLatin1String("yes") || raw == QLatin1String("on"))
        return true;
    return defaultValue;
}

int IniFile::intValue(const QString &section, const QString &key, int defaultValue) const
{
    bool ok = false;
    const int parsed = value(section, key).trimmed().toInt(&ok);
    return ok ? parsed : defaultValue;
}

qint64 IniFile::longLongValue(const QString &section, const QString &key, qint64 defaultValue) const
{
    bool ok = false;
    const qint64 parsed = value(section, key).trimmed().toLongLong(&ok);
    return ok ? parsed : defaultValue;
}

void IniFile::setValue(const QString &section, const QString &key, const QString &value)
{
    const int index = createSection(section);
    Section &s = m_sections[index];
    if (!s.values.contains(key))
        s.keyOrder.append(key);
    s.values.insert(key, value);
}

void IniFile::setBool(const QString &section, const QString &key, bool value)
{
    setValue(section, key, value ? QStringLiteral("1") : QString());
}

void IniFile::removeKey(const QString &section, const QString &key)
{
    const int index = sectionIndex(section);
    if (index < 0)
        return;
    m_sections[index].keyOrder.removeAll(key);
    m_sections[index].values.remove(key);
}

void IniFile::removeSection(const QString &section)
{
    const int index = sectionIndex(section);
    if (index >= 0)
        m_sections.remove(index);
}

QStringList IniFile::sections() const
{
    QStringList result;
    result.reserve(m_sections.size());
    for (const Section &section : m_sections)
        result.append(section.name);
    return result;
}

QStringList IniFile::keys(const QString &section) const
{
    const int index = sectionIndex(section);
    return index < 0 ? QStringList() : m_sections[index].keyOrder;
}

QMap<QString, QString> IniFile::sectionValues(const QString &section) const
{
    const int index = sectionIndex(section);
    return index < 0 ? QMap<QString, QString>() : m_sections[index].values;
}

void IniFile::setSectionValues(const QString &section, const QMap<QString, QString> &values)
{
    const int index = createSection(section);
    Section &s = m_sections[index];
    s.values = values;
    s.keyOrder = values.keys();
}

void IniFile::renameSection(const QString &oldName, const QString &newName)
{
    const int index = sectionIndex(oldName);
    if (index < 0)
        return;
    m_sections[index].name = newName;
}
