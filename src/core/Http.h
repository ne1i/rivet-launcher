#pragma once

#include <QString>
#include <functional>

namespace Http {

using ProgressCallback = std::function<void(qint64 received, qint64 total)>;

bool download(const QString &url, const QString &destination, QString *error,
              ProgressCallback progress = {}, int idleTimeoutMs = 60000);

bool getString(const QString &url, QString *body, QString *error, int timeoutMs = 30000);

} // namespace Http
