#include "EnvDialog.h"

#include "Icons.h"
#include "Theme.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

EnvDialog::EnvDialog(const QMap<QString, QString> &environment, bool steamOverlayEnabled, QWidget *parent)
    : QDialog(parent)
    , m_steamOverlayEnabled(steamOverlayEnabled)
{
    setWindowTitle(QStringLiteral("Environment variables"));
    resize(620, 460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Environment variables"));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral(
        "Extra variables passed to Proton. WINEDLLOVERRIDES, PROTON_USE_WINED3D and "
        "PROTON_ENABLE_WAYLAND are managed by the game settings instead."));
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Variable"), QStringLiteral("Value")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(false);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editCurrent(); });
    layout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("Add"));
    add->setIcon(Icons::icon(Icons::Name::Plus, Theme::TextMuted, 16));
    connect(add, &QPushButton::clicked, this, &EnvDialog::addVariable);
    auto *edit = new QPushButton(QStringLiteral("Edit"));
    edit->setIcon(Icons::icon(Icons::Name::Edit, Theme::TextMuted, 16));
    connect(edit, &QPushButton::clicked, this, &EnvDialog::editCurrent);
    auto *remove = new QPushButton(QStringLiteral("Remove"));
    remove->setIcon(Icons::icon(Icons::Name::Trash, Theme::TextMuted, 16));
    connect(remove, &QPushButton::clicked, this, &EnvDialog::removeCurrent);
    actions->addWidget(add);
    actions->addWidget(edit);
    actions->addWidget(remove);
    actions->addStretch();
    layout->addLayout(actions);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Save"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("primary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    for (auto it = environment.constBegin(); it != environment.constEnd(); ++it) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_table->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
}

QMap<QString, QString> EnvDialog::environment() const
{
    QMap<QString, QString> values;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString key = m_table->item(row, 0)->text().trimmed();
        const QString value = m_table->item(row, 1) ? m_table->item(row, 1)->text() : QString();
        if (!key.isEmpty())
            values.insert(key, value);
    }
    return values;
}

bool EnvDialog::validate(const QString &key, const QString &value, QString *error) const
{
    if (key.isEmpty() || key.contains(QLatin1Char('=')) || key.contains(QLatin1Char(' '))) {
        if (error)
            *error = QStringLiteral("The variable name must be a single word without '='.");
        return false;
    }

    static const QStringList managed = {
        QStringLiteral("WINEDLLOVERRIDES"),
        QStringLiteral("PROTON_USE_WINED3D"),
        QStringLiteral("PROTON_ENABLE_WAYLAND"),
        QStringLiteral("STEAM_COMPAT_DATA_PATH"),
        QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"),
    };
    if (managed.contains(key, Qt::CaseInsensitive)) {
        if (error)
            *error = QStringLiteral("%1 is managed by the launcher. Change the matching game setting instead.")
                         .arg(key);
        return false;
    }
    if (m_steamOverlayEnabled && key.compare(QLatin1String("LD_PRELOAD"), Qt::CaseInsensitive) == 0) {
        if (error)
            *error = QStringLiteral("LD_PRELOAD is used by the Steam overlay. Disable the overlay first.");
        return false;
    }
    Q_UNUSED(value);
    return true;
}

void EnvDialog::addVariable()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Add variable"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *keyEdit = new QLineEdit;
    keyEdit->setPlaceholderText(QStringLiteral("VARIABLE"));
    auto *valueEdit = new QLineEdit;
    valueEdit->setPlaceholderText(QStringLiteral("value"));
    form->addRow(QStringLiteral("Variable"), keyEdit);
    form->addRow(QStringLiteral("Value"), valueEdit);
    layout->addLayout(form);
    auto *error = new QLabel;
    error->setObjectName(QStringLiteral("error"));
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Add"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QString message;
        if (!validate(keyEdit->text().trimmed(), valueEdit->text(), &message)) {
            error->setText(message);
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(keyEdit->text().trimmed()));
    m_table->setItem(row, 1, new QTableWidgetItem(valueEdit->text()));
}

void EnvDialog::editCurrent()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Edit variable"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *keyEdit = new QLineEdit(m_table->item(row, 0)->text());
    auto *valueEdit = new QLineEdit(m_table->item(row, 1)->text());
    form->addRow(QStringLiteral("Variable"), keyEdit);
    form->addRow(QStringLiteral("Value"), valueEdit);
    layout->addLayout(form);
    auto *error = new QLabel;
    error->setObjectName(QStringLiteral("error"));
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Save"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QString message;
        if (!validate(keyEdit->text().trimmed(), valueEdit->text(), &message)) {
            error->setText(message);
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_table->item(row, 0)->setText(keyEdit->text().trimmed());
    m_table->item(row, 1)->setText(valueEdit->text());
}

void EnvDialog::removeCurrent()
{
    const int row = m_table->currentRow();
    if (row >= 0)
        m_table->removeRow(row);
}
