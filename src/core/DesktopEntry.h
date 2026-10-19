#pragma once

#include <QString>

namespace DesktopEntry {

QString fileNameFor(const QString &gameName);
QString filePath(const QString &directory, const QString &gameName);
bool exists(const QString &directory, const QString &gameName);

QString content(const QString &gameName, const QString &iconPath);

bool create(const QString &directory, const QString &gameName, const QString &iconPath,
            QString *error = nullptr);
bool remove(const QString &directory, const QString &gameName);
bool replaceIcon(const QString &directory, const QString &gameName, const QString &iconPath);

} // namespace DesktopEntry
