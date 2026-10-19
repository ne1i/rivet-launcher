#include "ProtonManager.h"

#include "Config.h"
#include "Http.h"
#include "Utils.h"

#include <QCoreApplication>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrl>
#include <algorithm>

namespace {

bool isArm()
{
    const QString cpu = QSysInfo::currentCpuArchitecture();
    return cpu == QLatin1String("arm64") || cpu == QLatin1String("aarch64")
        || QSysInfo::buildCpuArchitecture() == QLatin1String("arm64");
}

qint64 protonTimestamp(const QString &path)
{
    const QFileInfo info(path + QStringLiteral("/proton"));
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
}

int elfMachine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return -1;
    const QByteArray header = file.read(20);
    if (header.size() < 20 || header.at(0) != char(0x7f) || header.at(1) != 'E'
        || header.at(2) != 'L' || header.at(3) != 'F')
        return -1;
    const bool littleEndian = quint8(header.at(5)) == 1;
    const auto *bytes = reinterpret_cast<const quint8 *>(header.constData());
    const quint16 machine = littleEndian
        ? quint16(bytes[18] | (bytes[19] << 8))
        : quint16((bytes[18] << 8) | bytes[19]);
    return int(machine);
}

} // namespace

ProtonManager::ProtonManager(Config *store)
    : m_store(store)
{
}

QString ProtonManager::rootPath() const
{
    const QString path = m_store ? m_store->protonsPath() : QString();
    return path.isEmpty() ? QStringLiteral("~/.local/share/rivet/protons") : path;
}

QStringList ProtonManager::installed() const
{
    QStringList result;
    for (const QString &name : installedAll()) {
        if (isCompatible(name))
            result.append(name);
    }
    return result;
}

QStringList ProtonManager::installedAll() const
{
    QStringList result;
    const QDir root(rootPath());
    if (!root.exists())
        return result;

    const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (QFileInfo(entry.absoluteFilePath() + QStringLiteral("/proton")).isFile())
            result.append(entry.fileName());
    }

    std::sort(result.begin(), result.end(), [this](const QString &a, const QString &b) {
        return protonTimestamp(pathFor(a)) > protonTimestamp(pathFor(b));
    });
    return result;
}

bool ProtonManager::isCompatible(const QString &name) const
{
    const QString base = pathFor(name) + QStringLiteral("/files/bin/");
    QString binary = base + QStringLiteral("wineserver");
    if (!QFileInfo(binary).isFile())
        binary = base + QStringLiteral("wine64");
    if (!QFileInfo(binary).isFile())
        binary = base + QStringLiteral("wine");

    const int machine = elfMachine(binary);
    if (machine < 0)
        return false;

    const QString cpu = QSysInfo::currentCpuArchitecture();
    int expected = 0;
    if (cpu == QLatin1String("x86_64") || cpu == QLatin1String("amd64"))
        expected = 62; // EM_X86_64
    else if (cpu == QLatin1String("arm64") || cpu == QLatin1String("aarch64"))
        expected = 183; // EM_AARCH64
    else if (cpu.startsWith(QLatin1String("i")) || cpu == QLatin1String("x86"))
        expected = 3; // EM_386

    if (expected == 0)
        return true;
    return machine == expected;
}

QString ProtonManager::latestInstalled() const
{
    const QStringList list = installed();
    return list.isEmpty() ? QString() : list.first();
}

QString ProtonManager::resolve(const QString &name) const
{
    if (name.isEmpty() || name == QLatin1String("GE-Proton Latest"))
        return latestInstalled();
    if (QFileInfo(pathFor(name) + QStringLiteral("/proton")).isFile()
        && isCompatible(name))
        return name;
    return {};
}

QString ProtonManager::pathFor(const QString &name) const
{
    if (name.isEmpty())
        return {};
    return rootPath() + QLatin1Char('/') + name;
}

QString ProtonManager::executable(const QString &name, const QString &binary) const
{
    const QString resolved = resolve(name);
    if (resolved.isEmpty())
        return {};
    const QString path = binary == QLatin1String("proton")
        ? pathFor(resolved) + QStringLiteral("/proton")
        : pathFor(resolved) + QStringLiteral("/files/bin/") + binary;
    return QFileInfo(path).isFile() ? path : QString();
}

QVector<ProtonRelease> ProtonManager::fetchReleases(QString *error) const
{
    QVector<ProtonRelease> releases;

    QString body;
    QString requestError;
    if (!Http::getString(QStringLiteral("https://api.github.com/repos/gloriouseggroll/proton-ge-custom/releases"),
                         &body, &requestError, 30000)) {
        if (error)
            *error = requestError;
        return releases;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error)
            *error = QStringLiteral("Could not parse the GitHub response: %1")
                         .arg(parseError.errorString());
        return releases;
    }

    const bool arm = isArm();
    for (const QJsonValue &value : document.array()) {
        const QJsonObject release = value.toObject();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty())
            continue;

        for (const QJsonValue &assetValue : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = assetValue.toObject();
            const QString contentType = asset.value(QStringLiteral("content_type")).toString();
            const QString assetName = asset.value(QStringLiteral("name")).toString();
            const QString url = asset.value(QStringLiteral("browser_download_url")).toString();

            const bool gzip = contentType == QLatin1String("application/gzip")
                || contentType == QLatin1String("application/x-gtar");
            const bool assetIsArm = assetName.contains(QStringLiteral("aarch64"),
                                                       Qt::CaseInsensitive);
            if (!gzip || assetIsArm != arm || url.isEmpty())
                continue;

            releases.append(ProtonRelease{tag, assetName, url});
            break;
        }
    }

    return releases;
}

bool ProtonManager::download(const ProtonRelease &release, ProgressCallback progress, QString *error)
{
    if (release.url.isEmpty() || release.name.isEmpty()) {
        if (error)
            *error = QStringLiteral("Invalid Proton release information");
        return false;
    }

    QDir().mkpath(rootPath());
    const QString target = pathFor(release.name);
    if (QFileInfo(target + QStringLiteral("/proton")).isFile()) {
        if (error)
            *error = QStringLiteral("%1 is already installed").arg(release.name);
        return false;
    }

    if (!Utils::which(QStringLiteral("tar"))) {
        if (error)
            *error = QStringLiteral("tar was not found. Install it with your package manager.");
        return false;
    }

    const QString archive = rootPath() + QLatin1Char('/') + release.filename;
    if (progress)
        progress(0, 0, QStringLiteral("Downloading %1").arg(release.name));

    if (!Http::download(release.url, archive, error,
                        [&](qint64 received, qint64 total) {
                            if (progress)
                                progress(received, total, QStringLiteral("Downloading %1").arg(release.name));
                        })) {
        QFile::remove(archive);
        return false;
    }

    const QString temporary = rootPath() + QStringLiteral("/.rivet-extract-")
        + QString::number(QCoreApplication::applicationPid());
    Utils::removeRecursively(temporary);
    QDir().mkpath(temporary);

    if (progress)
        progress(0, 0, QStringLiteral("Unpacking %1").arg(release.name));

    const Utils::ProcessResult result = Utils::runProcess(
        QStringLiteral("tar"), {QStringLiteral("-xzf"), archive, QStringLiteral("-C"), temporary},
        rootPath(), QProcessEnvironment::systemEnvironment(), 15 * 60 * 1000);

    QFile::remove(archive);

    if (!result.ok()) {
        Utils::removeRecursively(temporary);
        if (error)
            *error = QStringLiteral("Could not unpack the Proton archive: %1")
                         .arg(result.stdErr.trimmed().isEmpty() ? result.error : result.stdErr.trimmed());
        return false;
    }

    const QDir tempDir(temporary);
    const QStringList entries = tempDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QString extracted;
    if (entries.size() == 1)
        extracted = temporary + QLatin1Char('/') + entries.first();
    else
        extracted = temporary;

    if (QFileInfo(target).exists())
        Utils::removeRecursively(target);

    if (!QDir().rename(extracted, target)) {
        // Cross-device or permissions: fall back to a copy.
        QString moveError;
        if (!Utils::moveDirectoryContents(extracted, target, &moveError)) {
            Utils::removeRecursively(temporary);
            if (error)
                *error = QStringLiteral("Could not install Proton: %1").arg(moveError);
            return false;
        }
    }

    Utils::removeRecursively(temporary);

    if (!QFileInfo(target + QStringLiteral("/proton")).isFile()) {
        if (error)
            *error = QStringLiteral("The archive did not contain a valid Proton build");
        Utils::removeRecursively(target);
        return false;
    }

    QFile::setPermissions(target + QStringLiteral("/proton"),
                          QFile::permissions(target + QStringLiteral("/proton"))
                              | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);

    if (progress)
        progress(1, 1, QStringLiteral("Installed %1").arg(release.name));
    return true;
}

bool ProtonManager::remove(const QString &name, QString *error)
{
    const QString target = pathFor(name);
    if (!QFileInfo(target).isDir()) {
        if (error)
            *error = QStringLiteral("%1 is not installed").arg(name);
        return false;
    }
    return Utils::removeRecursively(target, error);
}
