#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class Config;

struct ProtonRelease {
    QString name;
    QString filename;
    QString url;
};

class ProtonManager
{
public:
    explicit ProtonManager(Config *store);

    QString rootPath() const;
    QStringList installed() const;       // compatible builds, newest first
    QStringList installedAll() const;    // every build folder, newest first
    bool isCompatible(const QString &name) const;
    QString latestInstalled() const;
    QString resolve(const QString &name) const; // "GE-Proton Latest" -> newest installed
    QString pathFor(const QString &name) const;
    QString executable(const QString &name, const QString &binary = QStringLiteral("proton")) const;

    QVector<ProtonRelease> fetchReleases(QString *error = nullptr) const;

    using ProgressCallback = std::function<void(qint64 received, qint64 total, const QString &phase)>;
    bool download(const ProtonRelease &release, ProgressCallback progress, QString *error);

    bool remove(const QString &name, QString *error);

private:
    Config *m_store;
};
