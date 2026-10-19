#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

class QTableWidget;

class EnvDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EnvDialog(const QMap<QString, QString> &environment, bool steamOverlayEnabled,
                       QWidget *parent = nullptr);

    QMap<QString, QString> environment() const;

private:
    void addVariable();
    void editCurrent();
    void removeCurrent();
    bool validate(const QString &key, const QString &value, QString *error) const;

    QTableWidget *m_table = nullptr;
    bool m_steamOverlayEnabled;
};
