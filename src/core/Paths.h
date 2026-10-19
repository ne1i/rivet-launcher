#pragma once

#include <QString>

namespace Paths {

// XDG-aware application locations.
QString configDir();    // $XDG_CONFIG_HOME/rivet
QString configFile();   // $XDG_CONFIG_HOME/rivet/config.json
QString dataDir();      // $XDG_DATA_HOME/rivet
QString iconsDir();     // $XDG_DATA_HOME/rivet/icons
QString coversDir();    // $XDG_DATA_HOME/rivet/covers

QString downloadsDir();
QString installsDir();
QString defaultProtonsDir();
QString defaultPrefixesDir();

QString applicationsDir();
QString desktopDir();
QString documentsDir();

QString appExecutable();
QString bundledFile(const QString &name);

QString uniqueIconPath(const QString &extension = QString());
QString uniqueCoverPath(const QString &extension = QString());

void ensureConfigDirs();
void ensureDir(const QString &path);

} // namespace Paths
