#include "Http.h"

#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>

namespace {

QNetworkRequest makeRequest(const QString &url)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("rivet/1.0 (Qt; Linux)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    return request;
}

bool statusIsSuccess(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return status >= 200 && status < 300;
}

QString replyError(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status > 0)
        return QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString());
    return reply->errorString();
}

} // namespace

namespace Http {

bool download(const QString &url, const QString &destination, QString *error,
              ProgressCallback progress, int idleTimeoutMs)
{
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(makeRequest(url));

    QFileInfo info(destination);
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Could not open %1: %2").arg(destination, file.errorString());
        reply->abort();
        reply->deleteLater();
        return false;
    }

    QEventLoop loop;
    bool failed = false;
    QString failure;
    qint64 received = 0;

    QTimer idleTimer;
    idleTimer.setSingleShot(true);
    idleTimer.setInterval(idleTimeoutMs);

    QObject::connect(&idleTimer, &QTimer::timeout, &loop, [&] {
        failed = true;
        failure = QStringLiteral("The download timed out");
        reply->abort();
        loop.quit();
    });

    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&] {
        const QByteArray chunk = reply->readAll();
        if (!chunk.isEmpty()) {
            file.write(chunk);
            received += chunk.size();
            if (progress)
                progress(received, reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());
        }
        idleTimer.start();
    });

    QObject::connect(reply, &QNetworkReply::finished, &loop, [&] {
        idleTimer.stop();
        if (reply->error() != QNetworkReply::NoError || !statusIsSuccess(reply)) {
            failed = true;
            failure = replyError(reply);
        } else {
            const QByteArray chunk = reply->readAll();
            if (!chunk.isEmpty()) {
                file.write(chunk);
                received += chunk.size();
                if (progress)
                    progress(received, reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());
            }
        }
        loop.quit();
    });

    idleTimer.start(idleTimeoutMs);
    loop.exec();

    reply->deleteLater();

    if (failed) {
        file.cancelWriting();
        QFile::remove(destination);
        if (error)
            *error = failure;
        return false;
    }

    if (!file.commit()) {
        QFile::remove(destination);
        if (error)
            *error = QStringLiteral("Could not save %1: %2").arg(destination, file.errorString());
        return false;
    }

    if (received == 0) {
        QFile::remove(destination);
        if (error)
            *error = QStringLiteral("The server returned an empty file");
        return false;
    }
    return true;
}

bool getString(const QString &url, QString *body, QString *error, int timeoutMs)
{
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(makeRequest(url));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    bool ok = true;
    QString failure;
    if (timedOut) {
        ok = false;
        failure = QStringLiteral("The request timed out");
    } else if (reply->error() != QNetworkReply::NoError || !statusIsSuccess(reply)) {
        ok = false;
        failure = replyError(reply);
    }

    if (ok && body)
        *body = QString::fromUtf8(reply->readAll());
    if (!ok && error)
        *error = failure;

    reply->deleteLater();
    return ok;
}

} // namespace Http
