#include "Config.h"

#include "IniFile.h"
#include "Paths.h"
#include "Utils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

constexpr int kSchemaVersion = 1;
constexpr const char *kFormat = "rivet";

QString legacyConfigDir()
{
    return QDir::homePath() + QStringLiteral("/.config/OFME-Linux");
}

QString legacyDataDir()
{
    return QDir::homePath() + QStringLiteral("/.local/share/OnlineFix Linux Launcher");
}

QJsonObject mapToJson(const QMap<QString, QString> &values)
{
    QJsonObject object;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!it.key().trimmed().isEmpty())
            object.insert(it.key(), it.value());
    }
    return object;
}

QMap<QString, QString> jsonToMap(const QJsonObject &object)
{
    QMap<QString, QString> values;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        values.insert(it.key(), it.value().toString());
    return values;
}

QMap<QString, QString> parseLegacyEnvironment(const QString &stored)
{
    QMap<QString, QString> result;
    if (stored.trimmed().isEmpty())
        return result;

    const QString separator = QString(4, QLatin1Char('\\'));
    if (!stored.contains(separator)) {
        const int index = stored.indexOf(QStringLiteral("===="));
        if (index > 0)
            result.insert(stored.left(index).trimmed(), stored.mid(index + 4));
        return result;
    }

    const QStringList entries = stored.split(separator);
    for (const QString &entry : entries) {
        const int index = entry.indexOf(QStringLiteral("===="));
        if (index <= 0)
            continue;
        result.insert(entry.left(index).trimmed(), entry.mid(index + 4));
    }
    return result;
}

QJsonObject gameToJson(const Game &game)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), game.name);
    object.insert(QStringLiteral("executable"), game.executable);
    object.insert(QStringLiteral("game_dir"), game.mainPath);
    object.insert(QStringLiteral("prefix"), game.prefixPath);
    object.insert(QStringLiteral("proton"), game.proton.isEmpty()
                                                 ? QStringLiteral("GE-Proton Latest")
                                                 : game.proton);

    if (!game.banner.isEmpty())
        object.insert(QStringLiteral("cover"), game.banner);
    if (!game.icon.isEmpty())
        object.insert(QStringLiteral("icon"), game.icon);
    if (!game.fixPath.isEmpty())
        object.insert(QStringLiteral("fix_dir"), game.fixPath);
    if (!game.overrides.isEmpty())
        object.insert(QStringLiteral("dll_overrides"), game.overrides);

    QJsonObject args;
    if (!game.argsBefore.isEmpty())
        args.insert(QStringLiteral("before"), game.argsBefore);
    if (!game.argsAfter.isEmpty())
        args.insert(QStringLiteral("after"), game.argsAfter);
    if (!args.isEmpty())
        object.insert(QStringLiteral("args"), args);

    if (!game.environment.isEmpty())
        object.insert(QStringLiteral("environment"), mapToJson(game.environment));

    QJsonObject steam;
    if (!game.steamId.isEmpty())
        steam.insert(QStringLiteral("app_id"), game.steamId);
    if (!game.fakeSteamId.isEmpty())
        steam.insert(QStringLiteral("overlay_app_id"), game.fakeSteamId);
    if (!steam.isEmpty())
        object.insert(QStringLiteral("steam"), steam);

    object.insert(QStringLiteral("time_played"), double(game.timeSpent));

    QJsonObject features;
    features.insert(QStringLiteral("steam_overlay"), game.steamOverlay);
    features.insert(QStringLiteral("steam_runtime"), game.steamRuntime);
    features.insert(QStringLiteral("wined3d"), game.wined3d);
    features.insert(QStringLiteral("native_wayland"), game.nativeWayland);
    object.insert(QStringLiteral("features"), features);

    return object;
}

Game gameFromJson(const QJsonObject &object)
{
    Game game;
    game.name = object.value(QStringLiteral("name")).toString();
    game.executable = object.value(QStringLiteral("executable")).toString();
    game.mainPath = object.value(QStringLiteral("game_dir")).toString();
    game.prefixPath = object.value(QStringLiteral("prefix")).toString();
    game.proton = object.value(QStringLiteral("proton")).toString();
    if (game.proton.isEmpty())
        game.proton = QStringLiteral("GE-Proton Latest");
    game.banner = object.value(QStringLiteral("cover")).toString();
    game.icon = object.value(QStringLiteral("icon")).toString();
    game.fixPath = object.value(QStringLiteral("fix_dir")).toString();
    game.overrides = object.value(QStringLiteral("dll_overrides")).toString();

    const QJsonObject args = object.value(QStringLiteral("args")).toObject();
    game.argsBefore = args.value(QStringLiteral("before")).toString();
    game.argsAfter = args.value(QStringLiteral("after")).toString();
    game.environment = jsonToMap(object.value(QStringLiteral("environment")).toObject());

    const QJsonObject steam = object.value(QStringLiteral("steam")).toObject();
    game.steamId = steam.value(QStringLiteral("app_id")).toString();
    game.fakeSteamId = steam.value(QStringLiteral("overlay_app_id")).toString();

    game.timeSpent = qint64(object.value(QStringLiteral("time_played"))
                                .toVariant()
                                .toLongLong());

    const QJsonObject features = object.value(QStringLiteral("features")).toObject();
    game.steamOverlay = features.value(QStringLiteral("steam_overlay")).toBool();
    game.steamRuntime = features.value(QStringLiteral("steam_runtime")).toBool();
    game.wined3d = features.value(QStringLiteral("wined3d")).toBool();
    game.nativeWayland = features.value(QStringLiteral("native_wayland")).toBool();
    return game;
}

QString migrateAsset(const QString &path, const QString &oldDirectory, const QString &newDirectory)
{
    if (path.isEmpty())
        return path;
    const QFileInfo source(path);
    if (!source.isFile() || !source.absolutePath().startsWith(oldDirectory))
        return path;

    Paths::ensureDir(newDirectory);
    const QString destination = newDirectory + QLatin1Char('/') + source.fileName();
    if (!QFileInfo::exists(destination) && !QFile::copy(path, destination))
        return path;
    return QFileInfo::exists(destination) ? destination : path;
}

} // namespace

QString Game::effectivePrefixPath() const
{
    if (!prefixPath.trimmed().isEmpty())
        return prefixPath;
    return QFileInfo(executable).absolutePath() + QStringLiteral("/OFME Prefix");
}

QString Game::winePrefixPath() const
{
    return effectivePrefixPath() + QStringLiteral("/pfx");
}

QString Game::workingDirectory() const
{
    if (!mainPath.trimmed().isEmpty())
        return mainPath;
    return QFileInfo(executable).absolutePath();
}

Config::Config() = default;

void Config::reset()
{
    m_games.clear();
    m_settings = ConfigSettings{};
    m_lastError.clear();
    m_migrated = false;
    m_promptDismissed = false;
}

bool Config::load(QString *error)
{
    return reload(error);
}

bool Config::reload(QString *error)
{
    reset();
    Paths::ensureConfigDirs();

    if (QFileInfo::exists(Paths::configFile())) {
        if (!readJson(Paths::configFile(), &m_lastError)) {
            if (error)
                *error = m_lastError;
            return false;
        }
        return true;
    }

    if (!writeJson(Paths::configFile(), &m_lastError)) {
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

bool Config::readJson(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not open %1: %2").arg(path, file.errorString());
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("Could not parse %1: %2").arg(path, parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)) {
        if (error)
            *error = QStringLiteral("%1 is not a Rivet configuration").arg(path);
        return false;
    }
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version <= 0 || version > kSchemaVersion) {
        if (error)
            *error = QStringLiteral("Unsupported config version %1 in %2").arg(version).arg(path);
        return false;
    }

    const QJsonObject migration = root.value(QStringLiteral("migration")).toObject();
    m_migrated = migration.value(QStringLiteral("legacy_migrated")).toBool();
    m_promptDismissed = migration.value(QStringLiteral("prompt_dismissed")).toBool();

    const QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
    m_settings.defaultProton = settings.value(QStringLiteral("default_proton"))
                                   .toString(QStringLiteral("GE-Proton Latest"));
    if (m_settings.defaultProton.isEmpty())
        m_settings.defaultProton = QStringLiteral("GE-Proton Latest");
    m_settings.noSteamRequest = settings.value(QStringLiteral("no_steam_request")).toBool();
    m_settings.fullscreen = settings.value(QStringLiteral("fullscreen")).toBool();
    m_settings.downloadsSource = settings.value(QStringLiteral("downloads_source")).toString();

    const QJsonObject paths = settings.value(QStringLiteral("paths")).toObject();
    m_settings.downloadsPath = paths.value(QStringLiteral("downloads")).toString();
    m_settings.installsPath = paths.value(QStringLiteral("installs")).toString();
    m_settings.prefixesPath = paths.value(QStringLiteral("prefixes")).toString();
    m_settings.protonsPath = paths.value(QStringLiteral("protons")).toString();

    const QJsonObject newGames = settings.value(QStringLiteral("new_games")).toObject();
    m_settings.useWayland = newGames.value(QStringLiteral("native_wayland")).toBool();
    m_settings.useWined3d = newGames.value(QStringLiteral("wined3d")).toBool();

    m_games.clear();
    const QJsonArray games = root.value(QStringLiteral("games")).toArray();
    for (const QJsonValue &value : games) {
        Game game = gameFromJson(value.toObject());
        if (game.name.isEmpty() || !hasGame(game.name))
            m_games.append(game);
    }
    return true;
}

bool Config::writeJson(const QString &path, QString *error) const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QLatin1String(kFormat));
    root.insert(QStringLiteral("version"), kSchemaVersion);

    QJsonObject migration;
    migration.insert(QStringLiteral("legacy_migrated"), m_migrated);
    migration.insert(QStringLiteral("prompt_dismissed"), m_promptDismissed);
    root.insert(QStringLiteral("migration"), migration);

    QJsonObject settings;
    settings.insert(QStringLiteral("default_proton"),
                    m_settings.defaultProton.isEmpty() ? QStringLiteral("GE-Proton Latest")
                                                       : m_settings.defaultProton);
    settings.insert(QStringLiteral("no_steam_request"), m_settings.noSteamRequest);
    settings.insert(QStringLiteral("fullscreen"), m_settings.fullscreen);
    settings.insert(QStringLiteral("downloads_source"), m_settings.downloadsSource);

    QJsonObject paths;
    paths.insert(QStringLiteral("downloads"), m_settings.downloadsPath);
    paths.insert(QStringLiteral("installs"), m_settings.installsPath);
    paths.insert(QStringLiteral("prefixes"), m_settings.prefixesPath);
    paths.insert(QStringLiteral("protons"), m_settings.protonsPath);
    settings.insert(QStringLiteral("paths"), paths);

    QJsonObject newGames;
    newGames.insert(QStringLiteral("native_wayland"), m_settings.useWayland);
    newGames.insert(QStringLiteral("wined3d"), m_settings.useWined3d);
    settings.insert(QStringLiteral("new_games"), newGames);
    root.insert(QStringLiteral("settings"), settings);

    QJsonArray games;
    for (const Game &game : m_games)
        games.append(gameToJson(game));
    root.insert(QStringLiteral("games"), games);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = QStringLiteral("Could not save %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool Config::save(QString *error)
{
    Paths::ensureConfigDirs();
    if (!writeJson(Paths::configFile(), &m_lastError)) {
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

QStringList Config::gameNames() const
{
    QStringList names;
    for (const Game &game : m_games)
        names.append(game.name);
    return names;
}

int Config::gameCount() const { return m_games.size(); }

bool Config::hasGame(const QString &name) const
{
    for (const Game &game : m_games) {
        if (game.name == name)
            return true;
    }
    return false;
}

Game Config::game(const QString &name) const
{
    for (const Game &game : m_games) {
        if (game.name == name)
            return game;
    }
    return {};
}

bool Config::saveGame(const Game &game, QString *error)
{
    if (game.name.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("The game name is empty");
        if (error)
            *error = m_lastError;
        return false;
    }

    bool replaced = false;
    for (Game &existing : m_games) {
        if (existing.name == game.name) {
            existing = game;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        m_games.append(game);

    if (!save(error)) {
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

bool Config::renameGame(const QString &oldName, const QString &newName, QString *error)
{
    if (oldName == newName)
        return true;
    if (!hasGame(oldName)) {
        m_lastError = QStringLiteral("Game does not exist: %1").arg(oldName);
        if (error)
            *error = m_lastError;
        return false;
    }
    if (hasGame(newName)) {
        m_lastError = QStringLiteral("A game named \"%1\" already exists").arg(newName);
        if (error)
            *error = m_lastError;
        return false;
    }

    for (Game &game : m_games) {
        if (game.name == oldName) {
            game.name = newName;
            break;
        }
    }
    if (!save(error)) {
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

bool Config::removeGame(const QString &name, QString *error)
{
    for (int i = 0; i < m_games.size(); ++i) {
        if (m_games[i].name == name) {
            m_games.remove(i);
            if (!save(error)) {
                if (error)
                    *error = m_lastError;
                return false;
            }
            return true;
        }
    }
    m_lastError = QStringLiteral("Game does not exist: %1").arg(name);
    if (error)
        *error = m_lastError;
    return false;
}

void Config::addPlayTime(const QString &gameName, qint64 seconds)
{
    if (seconds <= 0)
        return;
    for (Game &game : m_games) {
        if (game.name == gameName) {
            game.timeSpent += seconds;
            QString error;
            if (!save(&error))
                qWarning("Could not save play time: %s", qPrintable(error));
            return;
        }
    }
}

qint64 Config::playTime(const QString &gameName) const
{
    return game(gameName).timeSpent;
}

QString Config::downloadsPath() const
{
    return m_settings.downloadsPath.isEmpty() ? Paths::downloadsDir()
                                              : m_settings.downloadsPath;
}

QString Config::protonsPath() const
{
    return m_settings.protonsPath.isEmpty() ? Paths::defaultProtonsDir()
                                            : m_settings.protonsPath;
}

QString Config::prefixesPath() const
{
    return m_settings.prefixesPath.isEmpty() ? Paths::defaultPrefixesDir()
                                             : m_settings.prefixesPath;
}

bool Config::hasLegacyConfig()
{
    return QFileInfo::exists(legacyConfigDir() + QStringLiteral("/Games.ini"))
        || QFileInfo::exists(legacyConfigDir() + QStringLiteral("/Launcher.ini"));
}

bool Config::migrationNeeded() const
{
    return hasLegacyConfig() && !m_migrated && !m_promptDismissed;
}

bool Config::declineMigrationPrompt(bool neverShowAgain, QString *error)
{
    m_promptDismissed = neverShowAgain;
    if (!save(error)) {
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

bool Config::migrateLegacy(QString *error)
{
    const QString oldConfig = legacyConfigDir();
    const QString oldData = legacyDataDir();
    const bool hadGames = !m_games.isEmpty();

    IniFile gamesIni;
    IniFile launcherIni;
    QString iniError;
    if (!gamesIni.load(oldConfig + QStringLiteral("/Games.ini"), &iniError)
        || !launcherIni.load(oldConfig + QStringLiteral("/Launcher.ini"), &iniError)) {
        if (error)
            *error = iniError;
        return false;
    }

    const QString user = QStringLiteral("User Settings");
    if (!hadGames) {
        m_settings.defaultProton = launcherIni.value(user, QStringLiteral("defaultProton"),
                                                     QStringLiteral("GE-Proton Latest"));
        if (m_settings.defaultProton.isEmpty())
            m_settings.defaultProton = QStringLiteral("GE-Proton Latest");
        m_settings.noSteamRequest = launcherIni.boolValue(user, QStringLiteral("noSteamRequest"));
        m_settings.fullscreen = launcherIni.boolValue(user, QStringLiteral("fullscreen"));
        m_settings.useWayland = launcherIni.boolValue(user, QStringLiteral("gamesUsesWayland"));
        m_settings.useWined3d = launcherIni.boolValue(user, QStringLiteral("gamesUsesWined3d"));
        m_settings.downloadsPath = launcherIni.value(user, QStringLiteral("downloadsPath"));
        m_settings.installsPath = launcherIni.value(user, QStringLiteral("installsPath"));
        m_settings.downloadsSource = launcherIni.value(user, QStringLiteral("downloadsSource"));

        // Reuse the old Proton/prefix roots when they exist: they can contain many
        // gigabytes and the per-game prefix paths already point there.
        m_settings.protonsPath = launcherIni.value(user, QStringLiteral("protonsPath"));
        if (m_settings.protonsPath.isEmpty() && QFileInfo(oldData + QStringLiteral("/protons")).isDir())
            m_settings.protonsPath = oldData + QStringLiteral("/protons");
        m_settings.prefixesPath = launcherIni.value(user, QStringLiteral("prefixesPath"));
        if (m_settings.prefixesPath.isEmpty() && QFileInfo(oldData + QStringLiteral("/prefixes")).isDir())
            m_settings.prefixesPath = oldData + QStringLiteral("/prefixes");
    }

    const QString oldIcons = oldConfig + QStringLiteral("/icons");
    const QString oldBanners = oldConfig + QStringLiteral("/banners");

    for (const QString &name : gamesIni.sections()) {
        if (name == user)
            continue;

        Game game;
        game.name = name;
        game.executable = gamesIni.value(name, QStringLiteral("executable"));
        game.mainPath = gamesIni.value(name, QStringLiteral("mainPath"));
        game.prefixPath = gamesIni.value(name, QStringLiteral("prefixPath"));
        game.proton = gamesIni.value(name, QStringLiteral("proton"),
                                     QStringLiteral("GE-Proton Latest"));
        game.banner = gamesIni.value(name, QStringLiteral("banner"));
        game.icon = gamesIni.value(name, QStringLiteral("icon"));
        game.overrides = gamesIni.value(name, QStringLiteral("overrides"));
        game.fixPath = gamesIni.value(name, QStringLiteral("fixPath"));
        game.argsBefore = gamesIni.value(name, QStringLiteral("argsBefore"));
        game.argsAfter = gamesIni.value(name, QStringLiteral("argsAfter"));
        game.environment = parseLegacyEnvironment(gamesIni.value(name, QStringLiteral("environment")));
        game.fakeSteamId = gamesIni.value(name, QStringLiteral("fakeSteamID"));
        game.steamId = gamesIni.value(name, QStringLiteral("steamID"));
        game.steamOverlay = gamesIni.boolValue(name, QStringLiteral("steamOverlay"));
        game.steamRuntime = gamesIni.boolValue(name, QStringLiteral("steamRuntime"));
        game.wined3d = gamesIni.boolValue(name, QStringLiteral("wined3d"));
        game.nativeWayland = gamesIni.boolValue(name, QStringLiteral("nativeWayland"));
        game.timeSpent = gamesIni.longLongValue(name, QStringLiteral("timeSpent"));

        if (hasGame(game.name))
            continue;

        game.icon = migrateAsset(game.icon, oldIcons, Paths::iconsDir());
        game.banner = migrateAsset(game.banner, oldBanners, Paths::coversDir());
        m_games.append(game);
    }

    m_migrated = true;
    m_promptDismissed = true;
    if (!save(error)) {
        if (error)
            *error = m_lastError;
        return false;
    }

    qInfo("Rivet migrated %d games from %s", int(m_games.size()), qPrintable(oldConfig));
    return true;
}
