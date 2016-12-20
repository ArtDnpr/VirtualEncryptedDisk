#pragma once

#include <QDialog>
#include "ui_MountDiskDialog.h"

class MountDiskDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MountDiskDialog(QWidget *parent, const QString& diskExtension);

    void Cleanup();

signals:
    void MountDisk(const QString &path, char diskLetter, const QString &password);

protected:
    virtual void showEvent(QShowEvent*);

private slots:
    void on_btnMount_clicked();
    void on_btnCancel_clicked();
    void on_btnBrowse_clicked();
    void on_checkShowPwd_clicked();

private:
    void RefreshMountButtonState();
    void RefreshAvailableDrives();

    QString GetFullPath();
    char GetDiskLetter();
    QString GetPassword();

private:
    Ui::MountDiskDialog m_ui;
    QString m_diskExtension;
};
