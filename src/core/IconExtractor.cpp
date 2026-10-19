#include "IconExtractor.h"

#include "Paths.h"
#include "Utils.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QTemporaryDir>

namespace {

QString largestFile(const QString &directory)
{
    QString best;
    qint64 bestSize = -1;
    QDirIterator iterator(directory, QDir::Files | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        const QString suffix = info.suffix().toLower();
        // Skip metadata / text files that 7z may extract alongside icons.
        if (suffix == QLatin1String("txt") || suffix == QLatin1String("xml")
            || suffix == QLatin1String("json"))
            continue;
        if (info.size() > bestSize) {
            bestSize = info.size();
            best = path;
        }
    }
    return best;
}

bool convertToPng(const QString &source, const QString &destination, QString *error)
{
    if (!Utils::which(QStringLiteral("ffmpeg"))) {
        if (error)
            *error = QStringLiteral("ffmpeg was not found. Install it to extract icons from .exe files.");
        return false;
    }
    const Utils::ProcessResult result = Utils::runProcess(
        QStringLiteral("ffmpeg"),
        {QStringLiteral("-y"), QStringLiteral("-loglevel"), QStringLiteral("error"),
         QStringLiteral("-i"), source, QStringLiteral("-frames:v"), QStringLiteral("1"),
         destination},
        {}, QProcessEnvironment::systemEnvironment(), 60000);

    if (!result.ok() || !QFileInfo(destination).isFile()) {
        if (error)
            *error = QStringLiteral("ffmpeg could not convert the icon: %1")
                         .arg(result.stdErr.trimmed());
        return false;
    }
    return true;
}

} // namespace

QString IconExtractor::extractFromExecutable(const QString &executable, QString *error)
{
    if (!QFileInfo(executable).isFile()) {
        if (error)
            *error = QStringLiteral("The executable does not exist: %1").arg(executable);
        return {};
    }

    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        if (error)
            *error = QStringLiteral("Could not create a temporary directory");
        return {};
    }

    QString iconSource;
    bool usedIcoextract = false;

    if (Utils::which(QStringLiteral("icoextract"))) {
        const QString icoPath = temporary.path() + QStringLiteral("/icon.ico");
        const Utils::ProcessResult result = Utils::runProcess(
            QStringLiteral("icoextract"), {executable, icoPath}, temporary.path(),
            QProcessEnvironment::systemEnvironment(), 60000);
        if (result.ok() && QFileInfo(icoPath).size() > 0) {
            iconSource = icoPath;
            usedIcoextract = true;
        }
    }

    if (iconSource.isEmpty()) {
        QString sevenZip;
        for (const QString &candidate : {QStringLiteral("7z"), QStringLiteral("7za"),
                                         QStringLiteral("7zz")}) {
            if (Utils::which(candidate)) {
                sevenZip = candidate;
                break;
            }
        }
        if (sevenZip.isEmpty()) {
            if (error)
                *error = QStringLiteral("Neither icoextract nor 7z was found. Install one of them to extract icons.");
            return {};
        }

        const QString extractDir = temporary.path() + QStringLiteral("/sevenzip");
        QDir().mkpath(extractDir);
        const Utils::ProcessResult result = Utils::runProcess(
            sevenZip, {QStringLiteral("-y"), QStringLiteral("x"), executable,
                       QStringLiteral(".rsrc/ICON"), QStringLiteral("-o") + extractDir},
            extractDir, QProcessEnvironment::systemEnvironment(), 60000);
        if (!result.ok()) {
            if (error)
                *error = QStringLiteral("7z could not extract the icon: %1")
                             .arg(result.stdErr.trimmed());
            return {};
        }
        iconSource = largestFile(extractDir);
        if (iconSource.isEmpty()) {
            if (error)
                *error = QStringLiteral("No icon was found inside %1").arg(QFileInfo(executable).fileName());
            return {};
        }
    }

    const QString suffix = QFileInfo(iconSource).suffix().toLower();
    if (usedIcoextract || suffix == QLatin1String("ico") || suffix == QLatin1String("bmp")) {
        const QString destination = Paths::uniqueIconPath(QStringLiteral("png"));
        if (!convertToPng(iconSource, destination, error)) {
            QFile::remove(destination);
            return {};
        }
        return destination;
    }

    // Some 7z extractions are already usable images.
    QImageReader reader(iconSource);
    if (!reader.canRead()) {
        if (error)
            *error = QStringLiteral("The extracted icon format is not supported");
        return {};
    }

    const QString extension = suffix.isEmpty() ? QStringLiteral("png") : suffix;
    const QString destination = Paths::uniqueIconPath(extension);
    if (!QFile::copy(iconSource, destination)) {
        if (error)
            *error = QStringLiteral("Could not save the extracted icon");
        return {};
    }
    return destination;
}

QString IconExtractor::copyImage(const QString &imagePath, QString *error)
{
    QImageReader reader(imagePath);
    if (!reader.canRead()) {
        if (error)
            *error = QStringLiteral("Unsupported image: %1").arg(imagePath);
        return {};
    }

    QString extension = QFileInfo(imagePath).suffix().toLower();
    if (extension.isEmpty())
        extension = QStringLiteral("png");
    const QString destination = Paths::uniqueIconPath(extension);
    if (!QFile::copy(imagePath, destination)) {
        if (error)
            *error = QStringLiteral("Could not copy the image to %1").arg(destination);
        return {};
    }
    return destination;
}
