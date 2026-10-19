#pragma once

#include "Config.h"
#include "Installer.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QVector>

class ProtonManager;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTextEdit;

class AddGameWizard : public QDialog
{
    Q_OBJECT
public:
    AddGameWizard(Config *store, ProtonManager *protons, const QString &initialPath,
                  QWidget *parent = nullptr);

    const Game &addedGame() const { return m_addedGame; }
    QStringList resultMessages() const { return m_resultMessages; }

signals:
    void gameAdded(const QString &gameName);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void chooseSourceFolder();
    void scanSource(const QString &preselect = QString());
    void selectCandidate(int index);
    void populateDetails();
    void updateDerivedPaths();
    void updateNavigation();
    void startAdd();
    void showProgress(const QString &message);
    void finishAdd();
    void chooseExecutable();
    void chooseInstallPath();
    void choosePrefixPath();
    void chooseInstaller();

    Config *m_store;
    Installer m_installer;
    QString m_initialPath;

    QStackedWidget *m_pages = nullptr;
    QFormLayout *m_form = nullptr;

    // Page 1
    QLineEdit *m_sourcePath = nullptr;
    QListWidget *m_candidates = nullptr;
    QLabel *m_scanHint = nullptr;
    QPushButton *m_nextButton = nullptr;

    // Page 2
    QLineEdit *m_name = nullptr;
    QLineEdit *m_executable = nullptr;
    QPushButton *m_executableBrowse = nullptr;
    QComboBox *m_archiveExecutable = nullptr;
    QLabel *m_archiveExecutableLabel = nullptr;
    QLineEdit *m_installPath = nullptr;
    QPushButton *m_installBrowse = nullptr;
    QLabel *m_installLabel = nullptr;
    QLabel *m_installHint = nullptr;
    QLineEdit *m_prefixPath = nullptr;
    QPushButton *m_prefixBrowse = nullptr;
    QWidget *m_installerRow = nullptr;
    QLineEdit *m_installerPath = nullptr;
    QCheckBox *m_cleanAfter = nullptr;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_addButton = nullptr;

    // Page 3
    QLabel *m_progressStatus = nullptr;
    QTextEdit *m_progressLog = nullptr;

    QVector<CandidateFile> m_candidateFiles;
    CandidateFile m_candidate;
    QStringList m_archiveFiles;
    QVector<CandidateFile> m_archiveExecutables;
    QString m_archivePassword;

    bool m_nameEdited = false;
    bool m_installEdited = false;
    bool m_prefixEdited = false;
    bool m_working = false;

    QFutureWatcher<AddGameResult> *m_watcher = nullptr;
    Game m_addedGame;
    QStringList m_resultMessages;
};
