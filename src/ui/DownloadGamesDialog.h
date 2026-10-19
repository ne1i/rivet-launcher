#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QVector>

class AddGameWizard;
class Config;
class ProtonManager;
class QLabel;
class QLineEdit;
class QListWidget;
class QProcess;
class QProgressBar;
class QPushButton;

struct DownloadItem {
    QString title;
    QString fileSize;
    QString url;
};

struct DownloadFetchResult {
    QVector<DownloadItem> items;
    QString error;
    QString source;
};

class DownloadGamesDialog : public QDialog
{
    Q_OBJECT
public:
    DownloadGamesDialog(Config *store, ProtonManager *protons, QWidget *parent = nullptr);
    void accept() override;
    void reject() override;

signals:
    void gameAdded(const QString &gameName);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void loadSource();
    void populate();
    void updateFilter();
    void startDownload(const DownloadItem &item);
    void cancelDownload();
    void processDownloadOutput();
    void finishDownload(int exitCode, bool crashed);
    void openDownloadedFile();
    void setBusy(bool busy);

    Config *m_store;
    ProtonManager *m_protons;

    QLineEdit *m_source = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_empty = nullptr;
    QPushButton *m_loadButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    QFutureWatcher<DownloadFetchResult> *m_fetchWatcher = nullptr;
    QVector<DownloadItem> m_items;
    QVector<QPushButton *> m_downloadButtons;

    QProcess *m_process = nullptr;
    QDialog *m_progressDialog = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_progressLabel = nullptr;
    bool m_downloading = false;
    bool m_cancelled = false;
    QString m_downloadDirectory;
    QString m_downloadTitle;
    QByteArray m_outputBuffer;
};
