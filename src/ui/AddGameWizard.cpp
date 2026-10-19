#include "AddGameWizard.h"

#include "Icons.h"
#include "Paths.h"
#include "ProtonManager.h"
#include "RarExtractor.h"
#include "Theme.h"
#include "Utils.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

QString folderName(const QString &path)
{
    return QFileInfo(path).fileName();
}

QWidget *candidateWidget(const CandidateFile &candidate)
{
    auto *widget = new QWidget;
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(12);

    auto *icon = new QLabel;
    icon->setFixedSize(26, 26);
    icon->setPixmap(Icons::pixmap(candidate.archive ? Icons::Name::Folder : Icons::Name::Game,
                                  Theme::TextMuted, 20));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    auto *labels = new QVBoxLayout;
    labels->setSpacing(1);
    auto *name = new QLabel(QFileInfo(candidate.absolutePath).fileName());
    name->setStyleSheet(QStringLiteral("font-weight: 600;"));
    auto *path = new QLabel(candidate.relativePath);
    path->setObjectName(QStringLiteral("small"));
    labels->addWidget(name);
    labels->addWidget(path);
    layout->addLayout(labels, 1);

    if (candidate.archive || candidate.installable) {
        auto *badge = new QLabel(candidate.archive ? QStringLiteral("Archive")
                                                   : QStringLiteral("Installer"));
        badge->setObjectName(QStringLiteral("badge"));
        layout->addWidget(badge, 0, Qt::AlignVCenter);
    }
    return widget;
}

} // namespace

AddGameWizard::AddGameWizard(Config *store, ProtonManager *protons, const QString &initialPath,
                             QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_installer(store, protons)
    , m_initialPath(initialPath)
{
    setWindowTitle(QStringLiteral("Add a game"));
    setModal(true);
    setMinimumSize(900, 640);
    resize(940, 680);
    buildUi();

    m_watcher = new QFutureWatcher<AddGameResult>(this);
    connect(m_watcher, &QFutureWatcher<AddGameResult>::finished, this, &AddGameWizard::finishAdd);

    if (!m_initialPath.isEmpty())
        QTimer::singleShot(0, this, [this] { scanSource(m_initialPath); });
}

void AddGameWizard::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 22);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("Add a game"));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *subtitle = new QLabel(QStringLiteral(
        "Select the folder that contains the game, a RAR archive, or an installer. "
        "The launcher will detect the OnlineFix/FreeTP files, the cover and the icon."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    m_pages = new QStackedWidget;
    layout->addWidget(m_pages, 1);

    // --- Source page -----------------------------------------------------
    auto *sourcePage = new QWidget;
    auto *sourceLayout = new QVBoxLayout(sourcePage);
    sourceLayout->setContentsMargins(0, 4, 0, 0);
    sourceLayout->setSpacing(10);

    auto *pathRow = new QHBoxLayout;
    m_sourcePath = new QLineEdit;
    m_sourcePath->setPlaceholderText(QStringLiteral("Folder containing the game…"));
    pathRow->addWidget(m_sourcePath, 1);
    auto *browse = new QPushButton(QStringLiteral("Choose folder…"));
    browse->setIcon(Icons::icon(Icons::Name::Folder, Theme::TextMuted, 17));
    connect(browse, &QPushButton::clicked, this, &AddGameWizard::chooseSourceFolder);
    pathRow->addWidget(browse);
    auto *scan = new QPushButton(QStringLiteral("Scan"));
    scan->setIcon(Icons::icon(Icons::Name::Search, Theme::TextMuted, 17));
    connect(scan, &QPushButton::clicked, this, [this] { scanSource(); });
    pathRow->addWidget(scan);
    sourceLayout->addLayout(pathRow);

    m_candidates = new QListWidget;
    m_candidates->setSelectionMode(QAbstractItemView::SingleSelection);
    m_candidates->setSpacing(3);
    connect(m_candidates, &QListWidget::currentRowChanged, this, [this](int) {
        updateNavigation();
    });
    connect(m_candidates, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        selectCandidate(m_candidates->row(item));
    });
    sourceLayout->addWidget(m_candidates, 1);

    m_scanHint = new QLabel;
    m_scanHint->setObjectName(QStringLiteral("small"));
    m_scanHint->setWordWrap(true);
    sourceLayout->addWidget(m_scanHint);

    auto *sourceButtons = new QHBoxLayout;
    sourceButtons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("Cancel"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    sourceButtons->addWidget(cancel);
    m_nextButton = new QPushButton(QStringLiteral("Continue"));
    m_nextButton->setObjectName(QStringLiteral("primary"));
    connect(m_nextButton, &QPushButton::clicked, this, [this] {
        const int row = m_candidates->currentRow();
        if (row < 0)
            return;
        selectCandidate(row);
    });
    sourceButtons->addWidget(m_nextButton);
    sourceLayout->addLayout(sourceButtons);

    m_pages->addWidget(sourcePage);

    // --- Details page ----------------------------------------------------
    auto *detailsPage = new QWidget;
    auto *detailsOuter = new QVBoxLayout(detailsPage);
    detailsOuter->setContentsMargins(0, 4, 0, 0);
    detailsOuter->setSpacing(10);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *formHost = new QWidget;
    auto *form = new QFormLayout(formHost);
    m_form = form;    form->setContentsMargins(4, 4, 12, 4);
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_name = new QLineEdit;
    m_name->setPlaceholderText(QStringLiteral("Game name in the launcher"));
    connect(m_name, &QLineEdit::textEdited, this, [this] {
        m_nameEdited = true;
        updateDerivedPaths();
    });
    form->addRow(QStringLiteral("Game name"), m_name);

    auto *executableRow = new QWidget;
    auto *executableLayout = new QHBoxLayout(executableRow);
    executableLayout->setContentsMargins(0, 0, 0, 0);
    executableLayout->setSpacing(8);
    m_executable = new QLineEdit;
    m_executable->setPlaceholderText(QStringLiteral("Game executable"));
    executableLayout->addWidget(m_executable, 1);
    m_executableBrowse = new QPushButton(QStringLiteral("Browse…"));
    connect(m_executableBrowse, &QPushButton::clicked, this, &AddGameWizard::chooseExecutable);
    executableLayout->addWidget(m_executableBrowse);
    form->addRow(QStringLiteral("Main executable"), executableRow);

    m_archiveExecutable = new QComboBox;
    m_archiveExecutableLabel = new QLabel(QStringLiteral("Executable in archive"));
    form->addRow(m_archiveExecutableLabel, m_archiveExecutable);
    m_archiveExecutableLabel->hide();
    m_archiveExecutable->hide();
    connect(m_archiveExecutable, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < m_archiveExecutables.size())
            m_candidate.archiveMember = m_archiveExecutables[index].relativePath;
    });

    auto *installRow = new QWidget;
    auto *installLayout = new QHBoxLayout(installRow);
    installLayout->setContentsMargins(0, 0, 0, 0);
    installLayout->setSpacing(8);
    m_installPath = new QLineEdit;
    installLayout->addWidget(m_installPath, 1);
    m_installBrowse = new QPushButton(QStringLiteral("Browse…"));
    connect(m_installBrowse, &QPushButton::clicked, this, &AddGameWizard::chooseInstallPath);
    installLayout->addWidget(m_installBrowse);
    m_installLabel = new QLabel(QStringLiteral("Game folder"));
    form->addRow(m_installLabel, installRow);

    auto *installHint = new QLabel(QStringLiteral(
        "Used for archives and installers. The game is unpacked here. "
        "When empty in the launcher settings, the archive's folder is used."));
    installHint->setObjectName(QStringLiteral("small"));
    installHint->setWordWrap(true);
    form->addRow(QString(), installHint);
    m_installHint = installHint;

    auto *prefixRow = new QWidget;
    auto *prefixLayout = new QHBoxLayout(prefixRow);
    prefixLayout->setContentsMargins(0, 0, 0, 0);
    prefixLayout->setSpacing(8);
    m_prefixPath = new QLineEdit;
    prefixLayout->addWidget(m_prefixPath, 1);
    m_prefixBrowse = new QPushButton(QStringLiteral("Browse…"));
    connect(m_prefixBrowse, &QPushButton::clicked, this, &AddGameWizard::choosePrefixPath);
    prefixLayout->addWidget(m_prefixBrowse);
    form->addRow(QStringLiteral("Prefix folder"), prefixRow);

    m_installerRow = new QWidget;
    auto *installerLayout = new QHBoxLayout(m_installerRow);
    installerLayout->setContentsMargins(0, 0, 0, 0);
    installerLayout->setSpacing(8);
    m_installerPath = new QLineEdit;
    m_installerPath->setPlaceholderText(QStringLiteral("Optional FreeTP/fix installer (.exe)"));
    installerLayout->addWidget(m_installerPath, 1);
    auto *installerBrowse = new QPushButton(QStringLiteral("Browse…"));
    connect(installerBrowse, &QPushButton::clicked, this, &AddGameWizard::chooseInstaller);
    installerLayout->addWidget(installerBrowse);
    form->addRow(QStringLiteral("Fix installer"), m_installerRow);

    m_cleanAfter = new QCheckBox(QStringLiteral("Remove the installer/archive files after adding"));
    form->addRow(QString(), m_cleanAfter);

    m_errorLabel = new QLabel;
    m_errorLabel->setObjectName(QStringLiteral("error"));
    m_errorLabel->setWordWrap(true);
    form->addRow(QString(), m_errorLabel);

    scroll->setWidget(formHost);
    detailsOuter->addWidget(scroll, 1);

    auto *detailsButtons = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("Back"));
    back->setIcon(Icons::icon(Icons::Name::Back, Theme::TextMuted, 16));
    connect(back, &QPushButton::clicked, this, [this] {
        m_errorLabel->clear();
        m_pages->setCurrentIndex(0);
    });
    detailsButtons->addWidget(back);
    detailsButtons->addStretch();
    auto *cancelDetails = new QPushButton(QStringLiteral("Cancel"));
    connect(cancelDetails, &QPushButton::clicked, this, &QDialog::reject);
    detailsButtons->addWidget(cancelDetails);
    m_addButton = new QPushButton(QStringLiteral("Add game"));
    m_addButton->setObjectName(QStringLiteral("primary"));
    m_addButton->setIcon(Icons::icon(Icons::Name::Plus, QColor(0x1a, 0x12, 0x06), 17));
    connect(m_addButton, &QPushButton::clicked, this, &AddGameWizard::startAdd);
    detailsButtons->addWidget(m_addButton);
    detailsOuter->addLayout(detailsButtons);

    m_pages->addWidget(detailsPage);

    // --- Progress page ---------------------------------------------------
    auto *progressPage = new QWidget;
    auto *progressLayout = new QVBoxLayout(progressPage);
    progressLayout->setContentsMargins(0, 24, 0, 0);
    progressLayout->setSpacing(14);
    progressLayout->addStretch();
    auto *progressTitle = new QLabel(QStringLiteral("Adding the game"));
    progressTitle->setObjectName(QStringLiteral("pageTitle"));
    progressTitle->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(progressTitle);
    m_progressStatus = new QLabel(QStringLiteral("Preparing…"));
    m_progressStatus->setAlignment(Qt::AlignCenter);
    m_progressStatus->setObjectName(QStringLiteral("muted"));
    progressLayout->addWidget(m_progressStatus);
    auto *progressHint = new QLabel(QStringLiteral("This can take a moment. The window will close automatically."));
    progressHint->setObjectName(QStringLiteral("small"));
    progressHint->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(progressHint);
    m_progressLog = new QTextEdit;
    m_progressLog->setReadOnly(true);
    m_progressLog->setMaximumHeight(160);
    m_progressLog->setPlaceholderText(QStringLiteral("Details will appear here…"));
    progressLayout->addWidget(m_progressLog, 0);
    progressLayout->addStretch();
    m_pages->addWidget(progressPage);

    if (!m_initialPath.isEmpty()) {
        m_sourcePath->setText(m_initialPath);
        if (QFileInfo(m_initialPath).isFile())
            m_sourcePath->setText(QFileInfo(m_initialPath).absolutePath());
    }
}

void AddGameWizard::chooseSourceFolder()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select the game folder or the folder containing the installer"),
        m_sourcePath->text().isEmpty() ? QDir::homePath() : m_sourcePath->text());
    if (path.isEmpty())
        return;
    m_sourcePath->setText(path);
    scanSource();
}

void AddGameWizard::scanSource(const QString &preselect)
{
    QString root = m_sourcePath->text().trimmed();
    if (root.isEmpty())
        root = m_initialPath;
    if (QFileInfo(root).isFile())
        root = QFileInfo(root).absolutePath();
    if (!QFileInfo(root).isDir()) {
        m_scanHint->setText(QStringLiteral("Choose a folder first."));
        return;
    }

    m_sourcePath->setText(root);
    m_candidateFiles.clear();
    m_candidates->clear();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_candidateFiles = AddGameScanner::scan(root);
    QApplication::restoreOverrideCursor();

    if (m_candidateFiles.isEmpty()) {
        m_scanHint->setText(QStringLiteral(
            "No game executable, RAR archive or installer was found in this folder. "
            "Try the folder that contains the downloaded game."));
        updateNavigation();
        return;
    }

    int autoIndex = -1;
    for (int i = 0; i < m_candidateFiles.size(); ++i) {
        auto *item = new QListWidgetItem;
        item->setSizeHint(QSize(0, 52));
        m_candidates->addItem(item);
        m_candidates->setItemWidget(item, candidateWidget(m_candidateFiles[i]));
        if (AddGameScanner::isAutoSelectable(m_candidateFiles[i].absolutePath) && autoIndex < 0)
            autoIndex = i;
    }

    int preselectIndex = -1;
    if (!preselect.isEmpty()) {
        const QString absolute = QFileInfo(preselect).absoluteFilePath();
        for (int i = 0; i < m_candidateFiles.size(); ++i) {
            if (m_candidateFiles[i].absolutePath == absolute) {
                preselectIndex = i;
                break;
            }
        }
    }

    int target = preselectIndex;
    if (target < 0 && autoIndex >= 0)
        target = autoIndex;
    if (target < 0 && m_candidateFiles.size() == 1)
        target = 0;
    if (target < 0)
        target = 0;

    m_candidates->setCurrentRow(target);
    updateNavigation();

    if (m_candidateFiles.size() == 1 || preselectIndex >= 0 || autoIndex >= 0)
        QTimer::singleShot(0, this, [this, target] { selectCandidate(target); });
    else
        m_scanHint->setText(QStringLiteral("%1 candidates found. Select the game executable or archive.")
                                .arg(m_candidateFiles.size()));
}

void AddGameWizard::updateNavigation()
{
    if (m_nextButton)
        m_nextButton->setEnabled(m_candidates->currentRow() >= 0 && !m_candidateFiles.isEmpty());
}

void AddGameWizard::selectCandidate(int index)
{
    if (index < 0 || index >= m_candidateFiles.size())
        return;

    m_errorLabel->clear();
    m_candidate = m_candidateFiles[index];
    m_archiveExecutables.clear();
    m_archiveFiles.clear();
    m_archivePassword.clear();
    m_nameEdited = m_installEdited = m_prefixEdited = false;

    const QFileInfo candidateInfo(m_candidate.absolutePath);

    if (m_candidate.archive) {
        QStringList files;
        QString error;
        QString password;
        bool ok = false;

        for (int attempt = 0; attempt < 4; ++attempt) {
            bool needsPassword = false;
            QApplication::setOverrideCursor(Qt::WaitCursor);
            ok = RarExtractor::list(m_candidate.absolutePath, &files, &error, password,
                                    &needsPassword);
            QApplication::restoreOverrideCursor();
            if (ok)
                break;

            if (!needsPassword) {
                QMessageBox::critical(this, QStringLiteral("Could not read the archive"), error);
                return;
            }

            bool accepted = false;
            password = QInputDialog::getText(
                this, QStringLiteral("Archive password"),
                QStringLiteral("This archive is password protected. Enter the password:"),
                QLineEdit::Password, QString(), &accepted);
            if (!accepted || password.isEmpty())
                return;
            m_archivePassword = password;
        }

        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("Could not read the archive"), error);
            return;
        }

        m_archiveFiles = files;
        static const QRegularExpression exeRegex(QStringLiteral("\\.(exe|vbs|bat)$"),
                                                  QRegularExpression::CaseInsensitiveOption);
        for (const QString &entry : files) {
            const QString name = QFileInfo(entry).fileName();
            if (!exeRegex.match(name).hasMatch() || AddGameScanner::isJunk(name))
                continue;
            CandidateFile file;
            file.relativePath = entry;
            file.absolutePath = entry;
            m_archiveExecutables.append(file);
        }

        if (m_archiveExecutables.isEmpty()) {
            QMessageBox::critical(this, QStringLiteral("No executable"),
                                  QStringLiteral("No game executable was found inside the archive."));
            return;
        }

        int autoChoice = 0;
        for (int i = 0; i < m_archiveExecutables.size(); ++i) {
            if (AddGameScanner::isAutoSelectable(m_archiveExecutables[i].relativePath)) {
                autoChoice = i;
                break;
            }
        }
        m_candidate.archiveMember = m_archiveExecutables[autoChoice].relativePath;
    }


    populateDetails();
    m_pages->setCurrentIndex(1);
}

void AddGameWizard::populateDetails()
{
    const QFileInfo candidateInfo(m_candidate.absolutePath);

    m_archiveExecutable->blockSignals(true);
    m_archiveExecutable->clear();
    for (const CandidateFile &entry : m_archiveExecutables)
        m_archiveExecutable->addItem(entry.relativePath);
    if (!m_candidate.archiveMember.isEmpty()) {
        const int index = m_archiveExecutable->findText(m_candidate.archiveMember);
        if (index >= 0)
            m_archiveExecutable->setCurrentIndex(index);
    }
    m_archiveExecutable->blockSignals(false);

    const bool isArchive = m_candidate.archive;
    const bool isInstaller = !isArchive && m_candidate.installable;

    m_archiveExecutable->setVisible(isArchive);
    m_archiveExecutableLabel->setVisible(isArchive);
    m_executable->setVisible(!isArchive);
    m_executableBrowse->setVisible(!isArchive);
    m_installerRow->setVisible(isInstaller);
    if (auto *label = qobject_cast<QLabel *>(m_form->labelForField(m_installerRow)))
        label->setVisible(isInstaller);
    m_cleanAfter->setVisible(isArchive || isInstaller);
    m_cleanAfter->setChecked(isArchive || isInstaller);

    if (isArchive) {
        m_name->setText(Utils::cleanArchiveBaseName(candidateInfo.fileName()));
        m_executable->clear();
    } else if (isInstaller) {
        m_name->setText(folderName(candidateInfo.absolutePath()));
        m_executable->setText(candidateInfo.absoluteFilePath());
        m_installerPath->clear();
    } else {
        const QString stem = candidateInfo.completeBaseName();
        const QString lower = candidateInfo.fileName().toLower();
        if (lower == QLatin1String("launcher.exe") || lower == QLatin1String("eosauthlauncher.exe"))
            m_name->setText(folderName(candidateInfo.absolutePath()));
        else
            m_name->setText(stem);
        m_executable->setText(candidateInfo.absoluteFilePath());
    }

    m_installLabel->setText(isArchive || isInstaller ? QStringLiteral("Install folder")
                                                     : QStringLiteral("Game folder"));
    m_installHint->setVisible(isArchive || isInstaller);
    m_installPath->setReadOnly(!isArchive && !isInstaller);
    m_installBrowse->setEnabled(isArchive || isInstaller);

    m_nameEdited = m_installEdited = m_prefixEdited = false;
    updateDerivedPaths();
    m_executable->setCursorPosition(0);
    m_installPath->setCursorPosition(0);
    m_prefixPath->setCursorPosition(0);
}

void AddGameWizard::updateDerivedPaths()
{
    const QString name = m_name->text().trimmed();
    const QString installBase = m_store->installsPath().isEmpty()
        ? m_sourcePath->text().trimmed()
        : m_store->installsPath();

    if (!m_installEdited) {
        if (m_candidate.archive || (!m_candidate.archive && m_candidate.installable))
            m_installPath->setText(name.isEmpty() ? installBase
                                                  : installBase + QLatin1Char('/') + name);
        else
            m_installPath->setText(m_sourcePath->text().trimmed());
    }

    if (!m_prefixEdited) {
        const QString base = m_store->prefixesPath();
        m_prefixPath->setText(name.isEmpty() ? base : base + QLatin1Char('/') + name);
    }
    m_installPath->setCursorPosition(0);
    m_prefixPath->setCursorPosition(0);
}

void AddGameWizard::chooseExecutable()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select the game executable"), m_sourcePath->text(),
        QStringLiteral("Windows executables (*.exe *.bat *.vbs);;All files (*)"));
    if (path.isEmpty())
        return;
    m_executable->setText(path);
    if (!m_nameEdited)
        m_name->setText(QFileInfo(path).completeBaseName());
}

void AddGameWizard::chooseInstallPath()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select the install folder"),
        m_installPath->text().isEmpty() ? Paths::installsDir() : m_installPath->text());
    if (path.isEmpty())
        return;
    m_installEdited = true;
    m_installPath->setText(path);
}

void AddGameWizard::choosePrefixPath()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select the prefix folder"),
        m_prefixPath->text().isEmpty() ? Paths::defaultPrefixesDir() : m_prefixPath->text());
    if (path.isEmpty())
        return;
    m_prefixEdited = true;
    m_prefixPath->setText(path);
}

void AddGameWizard::chooseInstaller()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select the FreeTP/fix installer"), m_sourcePath->text(),
        QStringLiteral("Windows executables (*.exe);;All files (*)"));
    if (!path.isEmpty())
        m_installerPath->setText(path);
}

void AddGameWizard::startAdd()
{
    m_errorLabel->clear();
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("Give the game a name."));
        return;
    }
    if (m_store->hasGame(name)) {
        m_errorLabel->setText(QStringLiteral("A game with this name already exists."));
        return;
    }

    if (m_candidate.archive) {
        if (m_candidate.archiveMember.isEmpty()) {
            m_errorLabel->setText(QStringLiteral("Select the executable inside the archive."));
            return;
        }
    } else {
        if (m_executable->text().trimmed().isEmpty()
            || !QFileInfo(m_executable->text().trimmed()).isFile()) {
            m_errorLabel->setText(QStringLiteral("Select an existing game executable."));
            return;
        }
    }

    const QString installPath = m_installPath->text().trimmed();
    const QString prefixPath = m_prefixPath->text().trimmed();
    if (installPath.isEmpty() || prefixPath.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("The install folder and prefix folder are required."));
        return;
    }

    if (!m_installerPath->text().trimmed().isEmpty()
        && !QFileInfo(m_installerPath->text().trimmed()).isFile()) {
        m_errorLabel->setText(QStringLiteral("The selected fix installer does not exist."));
        return;
    }

    if ((m_candidate.archive || m_candidate.installable) && QFileInfo(installPath).isDir()) {
        const QDir dir(installPath);
        if (!dir.isEmpty()) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("Folder is not empty"),
                QStringLiteral("The install folder is not empty:\n%1\n\n"
                               "Files may be merged or overwritten. Continue?").arg(installPath));
            if (answer != QMessageBox::Yes)
                return;
        }
    }

    if (QFileInfo(prefixPath).isDir() && !QDir(prefixPath).isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Prefix folder is not empty"),
                             QStringLiteral("This prefix folder is already in use:\n%1\n\n"
                                            "Removing the game may delete it.").arg(prefixPath));
    }

    AddGameRequest request;
    request.sourceRoot = m_sourcePath->text().trimmed();
    request.candidate = m_candidate.archive ? m_candidate.absolutePath
                                            : m_executable->text().trimmed();
    request.archiveMember = m_candidate.archiveMember;
    request.archivePassword = m_archivePassword;
    request.name = name;
    request.installPath = installPath;
    request.prefixPath = prefixPath;
    request.installerPath = m_installerPath->text().trimmed();
    request.canInstall = !m_candidate.archive && m_candidate.installable;
    request.cleanAfterAdd = m_cleanAfter->isChecked();

    m_working = true;
    m_pages->setCurrentIndex(2);
    m_progressStatus->setText(QStringLiteral("Preparing…"));
    m_progressLog->clear();

    m_watcher->setFuture(QtConcurrent::run([this, request] {
        return m_installer.addGame(request, [this](const QString &message) {
            QMetaObject::invokeMethod(
                this, [this, message] { showProgress(message); }, Qt::QueuedConnection);
        });
    }));
}

void AddGameWizard::showProgress(const QString &message)
{
    m_progressStatus->setText(message);
    m_progressLog->append(message);
}

void AddGameWizard::finishAdd()
{
    m_working = false;
    const AddGameResult result = m_watcher->result();

    if (!result.success) {
        m_pages->setCurrentIndex(1);
        m_errorLabel->setText(result.error);
        QMessageBox::critical(this, QStringLiteral("Could not add the game"), result.error);
        return;
    }

    QString error;
    if (!m_store->saveGame(result.game, &error)) {
        m_pages->setCurrentIndex(1);
        m_errorLabel->setText(error);
        QMessageBox::critical(this, QStringLiteral("Could not save the game"), error);
        return;
    }

    m_addedGame = result.game;
    m_resultMessages = result.messages;
    emit gameAdded(result.game.name);
    accept();
}

void AddGameWizard::closeEvent(QCloseEvent *event)
{
    if (m_working) {
        QMessageBox::information(this, QStringLiteral("Still working"),
                                 QStringLiteral("The game is still being installed. Please wait."));
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}
