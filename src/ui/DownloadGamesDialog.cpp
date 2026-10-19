#include "DownloadGamesDialog.h"

#include "AddGameWizard.h"
#include "Config.h"
#include "Http.h"
#include "Icons.h"
#include "ProtonManager.h"
#include "Theme.h"
#include "Utils.h"

#include <QCloseEvent>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

QString sanitizeFolderName(QString name)
{
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    if (name.isEmpty())
        name = QStringLiteral("download");
    return name.left(120);
}

} // namespace

DownloadGamesDialog::DownloadGamesDialog(Config *store, ProtonManager *protons, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_protons(protons)
{
    setWindowTitle(QStringLiteral("Download games"));
    setMinimumSize(880, 640);
    resize(920, 680);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Download games"));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *subtitle = new QLabel(QStringLiteral(
        "Downloads come from a Hydra/OnlineFix JSON source. Files are downloaded with aria2c, "
        "then you can add them to the library."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *sourceRow = new QHBoxLayout;
    m_source = new QLineEdit(m_store->downloadsSource());
    m_source->setPlaceholderText(QStringLiteral("https://…/onlinefix.json"));
    sourceRow->addWidget(m_source, 1);
    m_loadButton = new QPushButton(QStringLiteral("Load"));
    m_loadButton->setIcon(Icons::icon(Icons::Name::Download, Theme::TextMuted, 16));
    connect(m_loadButton, &QPushButton::clicked, this, &DownloadGamesDialog::loadSource);
    sourceRow->addWidget(m_loadButton);
    layout->addLayout(sourceRow);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Search downloads…"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Name::Search, Theme::TextMuted, 16),
                        QLineEdit::LeadingPosition);
    connect(m_search, &QLineEdit::textChanged, this, &DownloadGamesDialog::updateFilter);
    layout->addWidget(m_search);

    m_list = new QListWidget;
    m_list->setSpacing(4);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_list, 1);

    m_empty = new QLabel(QStringLiteral(
        "Paste a Hydra/OnlineFix source URL above and press Load."));
    m_empty->setObjectName(QStringLiteral("muted"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setMinimumHeight(120);
    layout->addWidget(m_empty);

    auto *bottom = new QHBoxLayout;
    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("small"));
    bottom->addWidget(m_status, 1);
    auto *close = new QPushButton(QStringLiteral("Close"));
    close->setObjectName(QStringLiteral("primary"));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(close);
    m_closeButton = close;
    layout->addLayout(bottom);

    m_fetchWatcher = new QFutureWatcher<DownloadFetchResult>(this);
    connect(m_fetchWatcher, &QFutureWatcher<DownloadFetchResult>::finished, this, [this] {
        m_loadButton->setEnabled(true);
        m_source->setEnabled(true);
        const DownloadFetchResult result = m_fetchWatcher->result();
        if (!result.error.isEmpty()) {
            m_status->setText(result.error);
            m_status->setObjectName(QStringLiteral("error"));
            m_status->style()->unpolish(m_status);
            m_status->style()->polish(m_status);
            m_empty->setText(QStringLiteral("Could not load this source. Check the URL and your connection."));
            m_empty->show();
            return;
        }

        m_items = result.items;
        const QString source = result.source;
        if (!source.isEmpty() && source != m_store->downloadsSource()) {
            m_store->setDownloadsSource(source);
            m_store->save();
        }
        m_status->setObjectName(QStringLiteral("small"));
        m_status->setText(QStringLiteral("%1 games available").arg(m_items.size()));
        populate();
    });

    if (!m_store->downloadsSource().isEmpty())
        QTimer::singleShot(0, this, &DownloadGamesDialog::loadSource);
}

void DownloadGamesDialog::loadSource()
{
    const QString source = m_source->text().trimmed();
    if (source.isEmpty()) {
        m_status->setText(QStringLiteral("Enter a source URL first."));
        return;
    }
    if (!source.startsWith(QLatin1String("http://"))
        && !source.startsWith(QLatin1String("https://"))) {
        m_status->setText(QStringLiteral("The source must be an http:// or https:// URL."));
        return;
    }

    m_loadButton->setEnabled(false);
    m_source->setEnabled(false);
    m_status->setText(QStringLiteral("Loading the source…"));
    m_empty->hide();
    m_items.clear();
    m_list->clear();
    m_downloadButtons.clear();

    m_fetchWatcher->setFuture(QtConcurrent::run([source] {
        DownloadFetchResult result;
        result.source = source;
        QString body;
        QString error;
        if (!Http::getString(source, &body, &error, 20000)) {
            result.error = error;
            return result;
        }

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            result.error = QStringLiteral("The source is not valid JSON: %1")
                               .arg(parseError.errorString());
            return result;
        }

        const QJsonObject root = document.object();
        const QString name = root.value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && name != QLatin1String("onlinefix")) {
            result.error = QStringLiteral("This does not look like an OnlineFix source.");
            return result;
        }

        const QJsonArray downloads = root.value(QStringLiteral("downloads")).toArray();
        if (downloads.isEmpty()) {
            result.error = QStringLiteral("The source contains no downloads.");
            return result;
        }

        for (const QJsonValue &value : downloads) {
            const QJsonObject game = value.toObject();
            DownloadItem item;
            item.title = game.value(QStringLiteral("title")).toString().trimmed();
            if (item.title.isEmpty())
                item.title = game.value(QStringLiteral("name")).toString().trimmed();
            const QJsonValue sizeValue = game.value(QStringLiteral("fileSize"));
            if (sizeValue.isString())
                item.fileSize = sizeValue.toString().trimmed();
            else if (sizeValue.isDouble())
                item.fileSize = Utils::humanBytes(qint64(sizeValue.toDouble()));
            const QJsonArray uris = game.value(QStringLiteral("uris")).toArray();
            for (const QJsonValue &uri : uris) {
                if (uri.isString()) {
                    item.url = uri.toString();
                    break;
                }
            }
            if (item.url.isEmpty())
                item.url = game.value(QStringLiteral("url")).toString();
            if (item.title.isEmpty() || item.url.isEmpty())
                continue;
            result.items.append(item);
        }

        if (result.items.isEmpty())
            result.error = QStringLiteral("The source has no usable downloads.");
        return result;
    }));
}

void DownloadGamesDialog::populate()
{
    m_list->clear();
    m_downloadButtons.clear();

    for (const DownloadItem &item : m_items) {
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        labels->setSpacing(2);
        auto *title = new QLabel(item.title);
        title->setStyleSheet(QStringLiteral("font-weight: 600;"));
        auto *meta = new QLabel(item.fileSize.isEmpty() ? item.url : item.fileSize);
        meta->setObjectName(QStringLiteral("small"));
        meta->setToolTip(item.url);
        labels->addWidget(title);
        labels->addWidget(meta);
        layout->addLayout(labels, 1);

        auto *download = new QPushButton(QStringLiteral("Download"));
        download->setIcon(Icons::icon(Icons::Name::Download, Theme::TextMuted, 16));
        download->setEnabled(!m_downloading);
        connect(download, &QPushButton::clicked, this,
                [this, item] { startDownload(item); });
        layout->addWidget(download);
        m_downloadButtons.append(download);

        auto *listItem = new QListWidgetItem;
        listItem->setSizeHint(QSize(0, 58));
        listItem->setData(Qt::UserRole, item.title);
        m_list->addItem(listItem);
        m_list->setItemWidget(listItem, row);
    }

    m_empty->setVisible(m_items.isEmpty());
    if (m_items.isEmpty())
        m_empty->setText(QStringLiteral("No downloads in this source."));
    updateFilter();
}

void DownloadGamesDialog::updateFilter()
{
    const QString query = m_search->text().trimmed();
    int visible = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const bool match = query.isEmpty()
            || item->data(Qt::UserRole).toString().contains(query, Qt::CaseInsensitive);
        item->setHidden(!match);
        if (match)
            ++visible;
    }
    if (m_items.isEmpty()) {
        m_list->hide();
        m_empty->setText(QStringLiteral("No downloads in this source."));
        m_empty->show();
    } else if (visible == 0) {
        m_list->hide();
        m_empty->setText(QStringLiteral("No downloads match “%1”.").arg(query));
        m_empty->show();
    } else {
        m_empty->hide();
        m_list->show();
    }
}

void DownloadGamesDialog::startDownload(const DownloadItem &item)
{
    if (m_downloading)
        return;
    if (!Utils::which(QStringLiteral("aria2c"))) {
        QMessageBox::warning(this, QStringLiteral("aria2c"),
                             QStringLiteral("aria2c was not found. Install aria2 with your package manager."));
        return;
    }

    const QString base = m_store->downloadsPath();
    if (!QDir().mkpath(base)) {
        QMessageBox::critical(this, QStringLiteral("Download folder"),
                              QStringLiteral("Could not create %1").arg(base));
        return;
    }

    m_downloadDirectory = base + QLatin1Char('/') + sanitizeFolderName(item.title);
    if (!QDir().mkpath(m_downloadDirectory)) {
        QMessageBox::critical(this, QStringLiteral("Download folder"),
                              QStringLiteral("Could not create %1").arg(m_downloadDirectory));
        return;
    }
    m_downloadTitle = item.title;
    m_cancelled = false;
    m_outputBuffer.clear();

    m_progressDialog = new QDialog(this);
    m_progressDialog->setWindowTitle(QStringLiteral("Downloading %1").arg(item.title));
    m_progressDialog->setModal(true);
    m_progressDialog->setMinimumWidth(480);
    auto *progressLayout = new QVBoxLayout(m_progressDialog);
    progressLayout->setContentsMargins(24, 20, 24, 16);
    progressLayout->setSpacing(12);
    m_progressLabel = new QLabel(QStringLiteral("Starting the download…"));
    m_progressLabel->setWordWrap(true);
    progressLayout->addWidget(m_progressLabel);
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);
    auto *cancel = new QPushButton(QStringLiteral("Cancel"));
    connect(cancel, &QPushButton::clicked, this, &DownloadGamesDialog::cancelDownload);
    progressLayout->addWidget(cancel, 0, Qt::AlignRight);
    m_progressDialog->show();

    m_process = new QProcess(this);
    m_process->setProgram(QStringLiteral("aria2c"));
    m_process->setArguments({
        QStringLiteral("--dir=") + m_downloadDirectory,
        QStringLiteral("--continue=true"),
        QStringLiteral("--auto-file-renaming=false"),
        QStringLiteral("--allow-overwrite=false"),
        QStringLiteral("--max-connection-per-server=8"),
        QStringLiteral("--split=8"),
        QStringLiteral("--min-split-size=1M"),
        QStringLiteral("--file-allocation=none"),
        QStringLiteral("--console-log-level=warn"),
        QStringLiteral("--summary-interval=1"),
        item.url,
    });
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this,
            &DownloadGamesDialog::processDownloadOutput);
    connect(m_process, &QProcess::readyReadStandardError, this,
            &DownloadGamesDialog::processDownloadOutput);
    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                finishDownload(exitCode, status == QProcess::CrashExit);
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finishDownload(-1, true);
    });

    setBusy(true);
    m_status->setText(QStringLiteral("Downloading %1…").arg(item.title));
    m_process->start();
}

void DownloadGamesDialog::processDownloadOutput()
{
    if (!m_process)
        return;

    QByteArray data = m_process->readAllStandardOutput() + m_process->readAllStandardError();
    m_outputBuffer.append(data);
    if (m_outputBuffer.size() > 64 * 1024)
        m_outputBuffer = m_outputBuffer.right(32 * 1024);

    QString text = QString::fromUtf8(m_outputBuffer);
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression percentRegex(QStringLiteral("(\\d{1,3})%"));
    for (const QString &line : lines) {
        const auto match = percentRegex.match(line);
        if (match.hasMatch()) {
            const int value = qBound(0, match.captured(1).toInt(), 100);
            m_progressBar->setValue(value);
            m_progressLabel->setText(QStringLiteral("Downloading %1 — %2%")
                                         .arg(m_downloadTitle)
                                         .arg(value));
        }
    }
}

void DownloadGamesDialog::finishDownload(int exitCode, bool crashed)
{
    if (!m_process)
        return;

    QProcess *process = m_process;
    m_process = nullptr;
    const bool cancelled = m_cancelled;
    m_cancelled = false;
    process->deleteLater();

    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
        m_progressBar = nullptr;
        m_progressLabel = nullptr;
    }
    setBusy(false);

    if (cancelled) {
        m_status->setText(QStringLiteral("Download cancelled. Partial files are kept for resume."));
        return;
    }

    if (crashed || exitCode != 0) {
        m_status->setText(QStringLiteral("Download failed (aria2c exit code %1).").arg(exitCode));
        QMessageBox::critical(this, QStringLiteral("Download failed"),
                              QStringLiteral("aria2c exited with code %1.\n\n"
                                             "Partial files are kept in:\n%2\n\n"
                                             "You can press Download again to resume.")
                                  .arg(exitCode)
                                  .arg(m_downloadDirectory));
        return;
    }

    if (m_progressBar)
        m_progressBar->setValue(100);
    m_status->setText(QStringLiteral("Downloaded %1.").arg(m_downloadTitle));
    openDownloadedFile();
}

void DownloadGamesDialog::cancelDownload()
{
    if (!m_process)
        return;
    m_cancelled = true;
    if (m_progressLabel)
        m_progressLabel->setText(QStringLiteral("Cancelling…"));
    m_process->kill();
}

void DownloadGamesDialog::openDownloadedFile()
{
    QString file;
    qint64 largest = -1;
    QDirIterator iterator(m_downloadDirectory, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        if (info.suffix() == QLatin1String("aria2"))
            continue;
        if (info.size() > largest) {
            largest = info.size();
            file = path;
        }
    }

    if (file.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Download"),
                             QStringLiteral("The download finished but no file was found in:\n%1")
                                 .arg(m_downloadDirectory));
        return;
    }

    AddGameWizard wizard(m_store, m_protons, file, this);
    if (wizard.exec() == QDialog::Accepted) {
        m_status->setText(QStringLiteral("Added %1.").arg(wizard.addedGame().name));
        emit gameAdded(wizard.addedGame().name);
    } else {
        m_status->setText(QStringLiteral("Downloaded to %1").arg(m_downloadDirectory));
    }
}

void DownloadGamesDialog::setBusy(bool busy)
{
    m_downloading = busy;
    m_loadButton->setEnabled(!busy);
    m_search->setEnabled(!busy);
    m_list->setEnabled(!busy);
    if (m_closeButton)
        m_closeButton->setEnabled(!busy);
    for (QPushButton *button : m_downloadButtons)
        button->setEnabled(!busy);
}

void DownloadGamesDialog::accept()
{
    if (m_downloading) {
        QMessageBox::information(this, QStringLiteral("Download in progress"),
                                 QStringLiteral("Cancel the download before closing this window."));
        return;
    }
    QDialog::accept();
}

void DownloadGamesDialog::reject()
{
    if (m_downloading) {
        QMessageBox::information(this, QStringLiteral("Download in progress"),
                                 QStringLiteral("Cancel the download before closing this window."));
        return;
    }
    QDialog::reject();
}

void DownloadGamesDialog::closeEvent(QCloseEvent *event)
{
    if (m_downloading) {
        QMessageBox::information(this, QStringLiteral("Download in progress"),
                                 QStringLiteral("Cancel the download before closing this window."));
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}
