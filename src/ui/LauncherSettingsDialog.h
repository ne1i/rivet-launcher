#pragma once

#include "Config.h"
#include "ProtonManager.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressDialog;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

struct ProtonFetchResult {
    QVector<ProtonRelease> releases;
    QString error;
};

class LauncherSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    LauncherSettingsDialog(Config *store, ProtonManager *protons, QWidget *parent = nullptr);

    void showProtonPage();
    void reject() override;

signals:
    void settingsSaved();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    QWidget *buildPathsPage();
    QWidget *buildDownloadsPage();
    QWidget *buildProtonPage();
    QWidget *buildLauncherPage();
    QWidget *buildAboutPage();

    void browsePath(QLineEdit *edit, const QString &title, const QString &start);
    void reloadProtonPage();
    void fetchReleases();
    void downloadProton(const ProtonRelease &release);
    void removeProton(const QString &name);
    void refillDefaultProton();
    void save();

    Config *m_store;
    ProtonManager *m_protons;

    QListWidget *m_navigation = nullptr;
    QStackedWidget *m_pages = nullptr;

    QLineEdit *m_downloads = nullptr;
    QLineEdit *m_installs = nullptr;
    QLineEdit *m_prefixes = nullptr;
    QLineEdit *m_protonsPath = nullptr;
    QLineEdit *m_downloadSource = nullptr;

    QComboBox *m_defaultProton = nullptr;
    QWidget *m_protonListHost = nullptr;
    QVBoxLayout *m_protonList = nullptr;
    QLabel *m_protonStatus = nullptr;
    QPushButton *m_refreshProtons = nullptr;

    QCheckBox *m_fullscreen = nullptr;
    QCheckBox *m_noSteam = nullptr;
    QCheckBox *m_waylandDefault = nullptr;
    QCheckBox *m_wined3dDefault = nullptr;

    QFutureWatcher<ProtonFetchResult> *m_fetchWatcher = nullptr;
    QFutureWatcher<QString> *m_downloadWatcher = nullptr;
    QProgressDialog *m_downloadDialog = nullptr;
    QVector<ProtonRelease> m_releases;
    bool m_fetching = false;
    bool m_downloading = false;
    QString m_downloadingName;
};
