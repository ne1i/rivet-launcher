#include "Utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

bool copyDirectoryRecursively(const QString &source, const QString &destination, QString *error)
{
    QDir sourceDir(source);
    if (!sourceDir.exists()) {
        if (error)
            *error = QStringLiteral("Source directory does not exist: %1").arg(source);
        return false;
    }
    if (!QDir().mkpath(destination)) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(destination);
        return false;
    }

    const auto entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                                     | QDir::Hidden,
                                                 QDir::DirsFirst);
    for (const QFileInfo &entry : entries) {
        const QString target = destination + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            if (!copyDirectoryRecursively(entry.absoluteFilePath(), target, error))
                return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            if (error)
                *error = QStringLiteral("Could not copy %1 to %2").arg(entry.absoluteFilePath(), target);
            return false;
        }
    }
    return true;
}

} // namespace

namespace Utils {

ProcessResult runProcess(const QString &program, const QStringList &arguments,
                         const QString &workingDirectory, const QProcessEnvironment &environment,
                         int timeoutMs)
{
    ProcessResult result;

    QProcess process;
    if (!workingDirectory.isEmpty())
        process.setWorkingDirectory(workingDirectory);
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(program, arguments);

    if (!process.waitForStarted(30000)) {
        result.failedToStart = true;
        result.error = process.errorString();
        return result;
    }

    if (!process.waitForFinished(timeoutMs)) {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(3000);
    }

    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    if (result.exitCode < 0 && !result.timedOut)
        result.error = process.errorString();
    return result;
}

bool which(const QString &program)
{
    if (program.contains(QLatin1Char('/')))
        return QFileInfo(program).isExecutable();
    return !QStandardPaths::findExecutable(program).isEmpty();
}

QString humanDuration(qint64 seconds)
{
    if (seconds < 60)
        return QStringLiteral("0 min");
    if (seconds < 3600) {
        const qint64 minutes = qRound64(double(seconds) / 60.0);
        return QStringLiteral("%1 min").arg(minutes);
    }
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (minutes == 0)
        return QStringLiteral("%1 h").arg(hours);
    return QStringLiteral("%1 h %2 min").arg(hours).arg(minutes);
}

QString humanBytes(qint64 bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = double(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 1).arg(units[unit]);
}

QString formatEnvironmentValue(const QString &value)
{
    return value;
}

QStringList splitArguments(const QString &commandLine)
{
    return QProcess::splitCommand(commandLine);
}

bool copyResource(const QString &resource, const QString &destination, QString *error)
{
    QFile input(resource);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not read bundled resource %1").arg(resource);
        return false;
    }
    QDir().mkpath(QFileInfo(destination).absolutePath());
    QFile output(destination);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2").arg(destination, output.errorString());
        return false;
    }
    const QByteArray data = input.readAll();
    if (output.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2").arg(destination, output.errorString());
        return false;
    }
    return true;
}

bool copyFile(const QString &source, const QString &destination, QString *error)
{
    QDir().mkpath(QFileInfo(destination).absolutePath());
    QFile::remove(destination);
    if (!QFile::copy(source, destination)) {
        if (error)
            *error = QStringLiteral("Could not copy %1 to %2").arg(source, destination);
        return false;
    }
    return true;
}

bool moveDirectoryContents(const QString &from, const QString &to, QString *error)
{
    QDir fromDir(from);
    if (!fromDir.exists())
        return true;
    if (!QDir().mkpath(to)) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(to);
        return false;
    }

    const auto entries = fromDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                                   | QDir::Hidden);
    for (const QFileInfo &entry : entries) {
        const QString target = to + QLatin1Char('/') + entry.fileName();
        if (QFileInfo::exists(target)) {
            if (error)
                *error = QStringLiteral("Destination already contains %1").arg(entry.fileName());
            return false;
        }
        if (!QFile::rename(entry.absoluteFilePath(), target)) {
            // Different filesystem: copy then remove.
            if (entry.isDir()) {
                if (!copyDirectoryRecursively(entry.absoluteFilePath(), target, error))
                    return false;
                if (!removeRecursively(entry.absoluteFilePath(), error))
                    return false;
            } else {
                if (!QFile::copy(entry.absoluteFilePath(), target)) {
                    if (error)
                        *error = QStringLiteral("Could not move %1").arg(entry.absoluteFilePath());
                    return false;
                }
                QFile::remove(entry.absoluteFilePath());
            }
        }
    }
    return true;
}

bool removeRecursively(const QString &path, QString *error)
{
    QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return true;
    if (info.isDir() && !info.isSymLink()) {
        QDir dir(path);
        if (!dir.removeRecursively()) {
            if (error)
                *error = QStringLiteral("Could not remove %1").arg(path);
            return false;
        }
        return true;
    }
    if (!QFile::remove(path)) {
        if (error)
            *error = QStringLiteral("Could not remove %1").arg(path);
        return false;
    }
    return true;
}

bool isPathInside(const QString &child, const QString &parent)
{
    const QString childPath = QDir(child).absolutePath();
    QString parentPath = QDir(parent).absolutePath();
    if (!parentPath.endsWith(QLatin1Char('/')))
        parentPath += QLatin1Char('/');
    return childPath == QDir(parent).absolutePath() || childPath.startsWith(parentPath);
}

QString desktopExecQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    escaped.replace(QLatin1Char('`'), QLatin1String("\\`"));
    escaped.replace(QLatin1Char('$'), QLatin1String("\\$"));
    escaped.replace(QLatin1Char('%'), QLatin1String("%%"));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString cleanArchiveBaseName(const QString &fileName)
{
    QString base = QFileInfo(fileName).completeBaseName();
    static const QRegularExpression partSuffix(QStringLiteral("\\.part\\d+$"),
                                                QRegularExpression::CaseInsensitiveOption);
    base.remove(partSuffix);
    return base;
}

} // namespace Utils
