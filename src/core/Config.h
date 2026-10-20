#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct Game {
    QString name;
    QString executable;
    QString mainPath;
    QString prefixPath;
    QString proton;
    QString banner;
    QString icon;
    QString overrides;
    QString fixPath;
    QString argsBefore;
    QString argsAfter;
    QMap<QString, QString> environment;
    QString fakeSteamId;
    QString steamId;
    bool steamOverlay = false;
    bool steamRuntime = false;
    bool wined3d = false;
    bool nativeWayland = false;
    qint64 timeSpent = 0;

    QString effectivePrefixPath() const;
    QString winePrefixPath() const;
    QString workingDirectory() const;
    bool isValid() const { return !name.isEmpty() && !executable.isEmpty(); }
};

struct ConfigSettings {
    QString defaultProton = QStringLiteral("GE-Proton Latest");
    bool noSteamRequest = false;
    bool fullscreen = false;
    bool useWayland = false;
    bool useWined3d = false;
    QString downloadsPath;
    QString installsPath;
    QString prefixesPath;
    QString protonsPath;
    QString downloadsSource;
};

// Rivet's configuration: one versioned JSON document at
// $XDG_CONFIG_HOME/rivet/config.json. It is intentionally not compatible with
// the old Games.ini/Launcher.ini format; Config::load() converts the old files
// once when no Rivet config exists yet.
class Config
{
public:
    Config();

    bool load(QString *error = nullptr);
    bool reload(QString *error = nullptr);
    bool save(QString *error = nullptr);

    static bool hasLegacyConfig();
    bool migrationNeeded() const;
    bool migrateLegacy(QString *error = nullptr);
    bool declineMigrationPrompt(bool neverShowAgain, QString *error = nullptr);

    QStringList gameNames() const;
    QVector<Game> games() const { return m_games; }
    int gameCount() const;
    bool hasGame(const QString &name) const;

    Game game(const QString &name) const;
    bool saveGame(const Game &game, QString *error = nullptr);
    bool renameGame(const QString &oldName, const QString &newName, QString *error = nullptr);
    bool removeGame(const QString &name, QString *error = nullptr);

    void addPlayTime(const QString &game, qint64 seconds);
    qint64 playTime(const QString &game) const;

    const ConfigSettings &settings() const { return m_settings; }
    ConfigSettings &settings() { return m_settings; }

    QString defaultProton() const { return m_settings.defaultProton; }
    void setDefaultProton(const QString &value) { m_settings.defaultProton = value; }

    bool noSteamRequest() const { return m_settings.noSteamRequest; }
    void setNoSteamRequest(bool value) { m_settings.noSteamRequest = value; }

    bool fullscreen() const { return m_settings.fullscreen; }
    void setFullscreen(bool value) { m_settings.fullscreen = value; }

    bool gamesUseWayland() const { return m_settings.useWayland; }
    void setGamesUseWayland(bool value) { m_settings.useWayland = value; }

    bool gamesUseWined3d() const { return m_settings.useWined3d; }
    void setGamesUseWined3d(bool value) { m_settings.useWined3d = value; }

    QString downloadsPath() const;
    void setDownloadsPath(const QString &value) { m_settings.downloadsPath = value; }

    QString installsPath() const { return m_settings.installsPath; }
    void setInstallsPath(const QString &value) { m_settings.installsPath = value; }

    QString protonsPath() const;
    void setProtonsPath(const QString &value) { m_settings.protonsPath = value; }

    QString prefixesPath() const;
    void setPrefixesPath(const QString &value) { m_settings.prefixesPath = value; }

    QString downloadsSource() const { return m_settings.downloadsSource; }
    void setDownloadsSource(const QString &value) { m_settings.downloadsSource = value; }

    bool migratedFromLegacy() const { return m_migrated; }
    QString lastError() const { return m_lastError; }

private:
    bool readJson(const QString &path, QString *error);
    bool writeJson(const QString &path, QString *error) const;
    void reset();

    QVector<Game> m_games;
    ConfigSettings m_settings;
    QString m_lastError;
    bool m_migrated = false;
    bool m_promptDismissed = false;
    bool m_migrationStatePresent = false;
};
