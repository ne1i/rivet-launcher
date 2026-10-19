#include "FixParser.h"

#include "Config.h"
#include "Http.h"
#include "IniFile.h"
#include "Paths.h"
#include "Utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace {

const QRegularExpression &candidateRegex()
{
    static const QRegularExpression regex(
        QStringLiteral("^(?:(?:emp|custom)\\.dll|win.*\\.dll|(?:online|steam).*\\.(?:dll|ini|json)"
                       "|eos.*\\.dll|epicfix.*\\.dll|(?:winmm|dlllist)\\.txt|launch_data\\.of.*)$"),
        QRegularExpression::CaseInsensitiveOption);
    return regex;
}

void collectFiles(const QString &root, QStringList &files, int depth = 0)
{
    if (depth > 16)
        return;
    QDir dir(root);
    const auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
                                               | QDir::Hidden);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        if (entry.isDir()) {
            if (name.compare(QLatin1String("OFME Prefix"), Qt::CaseInsensitive) == 0
                || name == QLatin1String(".git") || name == QLatin1String("node_modules"))
                continue;
            if (entry.isSymLink())
                continue;
            collectFiles(entry.absoluteFilePath(), files, depth + 1);
        } else if (candidateRegex().match(name).hasMatch()) {
            files.append(entry.absoluteFilePath());
        }
    }
}

bool containsDll(const QString &overrides, const QString &dll)
{
    return overrides.contains(dll + QLatin1Char('='), Qt::CaseInsensitive);
}

void addOverride(QString &overrides, const QString &dll, const QString &value)
{
    if (dll.isEmpty() || containsDll(overrides, dll))
        return;
    overrides += dll + QLatin1Char('=') + value + QLatin1Char(';');
}

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString findNewtonsoft(const QString &root)
{
    static const QRegularExpression nameRegex(QStringLiteral("^Newtonsoft\\.Json\\.dll$"),
                                               QRegularExpression::CaseInsensitiveOption);
    QDirIterator iterator(root, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (nameRegex.match(QFileInfo(path).fileName()).hasMatch())
            return path;
    }
    return {};
}

int steamfixBits(const QString &fileName)
{
    if (fileName.contains(QLatin1String("32")))
        return 32;
    if (fileName.contains(QLatin1String("64")))
        return 64;
    return 0;
}

} // namespace

FixInfo FixParser::parse(const QString &root, QString *error)
{
    FixInfo info;
    if (!QFileInfo(root).isDir()) {
        if (error)
            *error = QStringLiteral("Game directory does not exist: %1").arg(root);
        return info;
    }

    QStringList files;
    collectFiles(root, files);

    for (const QString &filePath : files) {
        const QFileInfo file(filePath);
        const QString fileName = file.fileName();
        const QString lowerName = fileName.toLower();

        if (lowerName == QLatin1String("winmm.txt") || lowerName == QLatin1String("dlllist.txt")) {
            const QStringList lines = readAll(filePath).split(QLatin1Char('\n'));
            for (QString line : lines) {
                line = line.trimmed();
                if (line.isEmpty())
                    continue;
                line.replace(QLatin1Char('\\'), QLatin1Char('/'));
                const QFileInfo dllInfo(line);
                if (dllInfo.suffix().compare(QLatin1String("dll"), Qt::CaseInsensitive) != 0)
                    continue;
                addOverride(info.overrides, dllInfo.completeBaseName().toLower(), QStringLiteral("n"));
            }
            continue;
        }

        if (lowerName == QLatin1String("onlinefix.ini") || lowerName == QLatin1String("steamfix.ini")) {
            IniFile ini;
            QString iniError;
            if (!ini.load(filePath, &iniError)) {
                info.warnings.append(iniError);
                continue;
            }

            QString real = ini.value(QStringLiteral("OnlineFix Linux"), QStringLiteral("RealAppId"));
            if (real.isEmpty())
                real = ini.value(QStringLiteral("Main"), QStringLiteral("RealAppId"));
            const QString fake = ini.value(QStringLiteral("Main"), QStringLiteral("FakeAppId"));
            if (!real.isEmpty())
                info.realAppId = real;
            if (!fake.isEmpty())
                info.fakeAppId = fake;

            bool changed = false;
            if (lowerName == QLatin1String("steamfix.ini")
                && !ini.hasSection(QStringLiteral("OnlineFix Linux"))) {
                ini.setValue(QStringLiteral("OnlineFix Linux"), QStringLiteral("RealAppId"), real);
                ini.setValue(QStringLiteral("Main"), QStringLiteral("RealAppId"), fake);
                info.freeTpPatched = true;
                changed = true;
            }

            if (ini.hasKey(QStringLiteral("Misc"), QStringLiteral("ExtraProtection"))) {
                ini.setValue(QStringLiteral("Misc"), QStringLiteral("ExtraProtection"),
                             QStringLiteral("false"));
                changed = true;
            }

            if (changed && !ini.save(filePath, &iniError))
                info.warnings.append(iniError);
            continue;
        }

        if (lowerName.startsWith(QLatin1String("steamfix"))
            && lowerName.endsWith(QLatin1String(".dll"))) {
            info.fixPath = file.absolutePath();
        }

        if ((lowerName.startsWith(QLatin1String("launch_data.of"))
             || lowerName == QLatin1String("onlinefix.json"))
            && QFileInfo(file.absolutePath() + QStringLiteral("/Launcher.exe")).isFile()
            && !QFileInfo(file.absolutePath() + QStringLiteral("/Newtonsoft.Json.dll")).isFile()) {
            const QString existing = findNewtonsoft(root);
            const QString target = file.absolutePath() + QStringLiteral("/Newtonsoft.Json.dll");
            bool patched = false;
            if (!existing.isEmpty() && existing != target) {
                patched = QFile::link(existing, target);
                if (!patched)
                    patched = Utils::copyFile(existing, target);
            } else if (existing.isEmpty()) {
                patched = Utils::copyResource(QStringLiteral(":/assets/Newtonsoft.Json.dll"), target);
            }
            if (patched)
                info.photonPatched = true;
            else
                info.warnings.append(QStringLiteral("Could not install Newtonsoft.Json.dll"));
            continue;
        }

        if (!lowerName.endsWith(QLatin1String(".dll")))
            continue;

        const QString stem = file.completeBaseName().toLower();
        if (stem.startsWith(QLatin1String("win")))
            addOverride(info.overrides, stem, QStringLiteral("n,b"));
        else
            addOverride(info.overrides, stem, QStringLiteral("n"));
    }

    if (info.overrides.endsWith(QLatin1Char(';')))
        info.overrides.chop(1);

    return info;
}

QString FixParser::fetchBanner(const QString &appId, QString *error)
{
    if (appId.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("The Steam AppID is empty");
        return {};
    }

    Paths::ensureDir(Paths::coversDir());
    const QString destination = Paths::coversDir() + QLatin1Char('/') + appId.trimmed()
        + QStringLiteral(".jpg");
    const QString url = QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/header.jpg")
                            .arg(appId.trimmed());

    if (!Http::download(url, destination, error)) {
        QFile::remove(destination);
        return {};
    }
    return destination;
}

bool FixParser::isNoOfllPathEnabled(const QString &fixPath)
{
    if (fixPath.isEmpty())
        return false;
    const QDir dir(fixPath);
    if (!dir.exists())
        return false;
    static const QRegularExpression suffix(QStringLiteral("\\.noofllpath$"),
                                           QRegularExpression::CaseInsensitiveOption);
    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (suffix.match(entry.fileName()).hasMatch())
            return true;
    }
    return false;
}

bool FixParser::setNoOfllPath(const QString &fixPath, bool enable, QString *error)
{
    if (fixPath.isEmpty() || !QFileInfo(fixPath).isDir()) {
        if (error)
            *error = QStringLiteral("The fix directory is unknown. Re-add the game or set it in the game settings.");
        return false;
    }

    static const QRegularExpression dllRegex(QStringLiteral("^steamfix(32|64)\\.dll$"),
                                              QRegularExpression::CaseInsensitiveOption);
    const QDir dir(fixPath);
    QList<QFileInfo> dlls;
    for (const QFileInfo &entry : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (dllRegex.match(entry.fileName()).hasMatch())
            dlls.append(entry);
    }

    if (dlls.isEmpty()) {
        if (error)
            *error = QStringLiteral("Could not find steamfix32.dll or steamfix64.dll in the fix directory.");
        return false;
    }

    if (enable) {
        for (const QFileInfo &dll : dlls) {
            const QString backup = dll.absoluteFilePath() + QStringLiteral(".noofllpath");
            if (QFileInfo(backup).exists())
                continue;
            if (!QFile::rename(dll.absoluteFilePath(), backup)) {
                if (error)
                    *error = QStringLiteral("Could not back up %1").arg(dll.fileName());
                return false;
            }
            const int bits = steamfixBits(dll.fileName());
            const QString resource = bits == 32 ? QStringLiteral(":/assets/ftpPath32.dll")
                                                : QStringLiteral(":/assets/ftpPath64.dll");
            if (!Utils::copyResource(resource, dll.absoluteFilePath())) {
                QFile::rename(backup, dll.absoluteFilePath());
                if (error)
                    *error = QStringLiteral("Could not install the replacement DLL");
                return false;
            }
        }
    } else {
        for (const QFileInfo &dll : dlls) {
            const QString backup = dll.absoluteFilePath() + QStringLiteral(".noofllpath");
            if (!QFileInfo(backup).exists())
                continue;
            QFile::remove(dll.absoluteFilePath());
            if (!QFile::rename(backup, dll.absoluteFilePath())) {
                if (error)
                    *error = QStringLiteral("Could not restore %1").arg(dll.fileName());
                return false;
            }
        }
    }

    return true;
}
