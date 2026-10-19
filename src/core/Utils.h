#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace Utils {

struct ProcessResult {
    int exitCode = -1;
    QString stdOut;
    QString stdErr;
    bool timedOut = false;
    bool failedToStart = false;
    QString error;
    bool ok() const { return !failedToStart && !timedOut && exitCode == 0; }
};

ProcessResult runProcess(const QString &program,
                         const QStringList &arguments,
                         const QString &workingDirectory = QString(),
                         const QProcessEnvironment &environment = QProcessEnvironment::systemEnvironment(),
                         int timeoutMs = 120000);

bool which(const QString &program);

QString humanDuration(qint64 seconds);
QString humanBytes(qint64 bytes);
QString formatEnvironmentValue(const QString &value);

QStringList splitArguments(const QString &commandLine);

bool copyResource(const QString &resource, const QString &destination, QString *error = nullptr);
bool copyFile(const QString &source, const QString &destination, QString *error = nullptr);

// Moves every child of `from` into `to`. Handles cross-device moves by falling
// back to a recursive copy. Both directories are created when missing.
bool moveDirectoryContents(const QString &from, const QString &to, QString *error = nullptr);

bool removeRecursively(const QString &path, QString *error = nullptr);
bool isPathInside(const QString &child, const QString &parent);

QString desktopExecQuote(const QString &value);
QString cleanArchiveBaseName(const QString &fileName);

} // namespace Utils
