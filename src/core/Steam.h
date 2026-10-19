#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace Steam {

QString installPath();
bool isInstalled();
bool isRunning();

QString loginUsersPath();
qint64 loginUsersModified();

QString overlayRenderer32();
QString overlayRenderer64();

// Returns the Steam Linux Runtime "run" script suitable for the given
// GE-Proton installation, or an empty string when it is not installed.
QString findRuntime(const QString &protonPath);

QStringList libraryFolders();

} // namespace Steam
