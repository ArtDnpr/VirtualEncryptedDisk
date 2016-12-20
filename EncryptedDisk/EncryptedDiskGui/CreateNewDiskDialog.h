#pragma once

#include <QDialog>
#include "ui_CreateNewDiskDialog.h"

class CreateNewDiskDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateNewDiskDialog(QWidget* parent, const QString& diskExtension);

    void Cleanup();

public slots:
    void OnProgressChange(uint64_t encryptedSize, uint64_t totalSize);
    void OnProgressFinished(bool canceled);
    void OnProgressError(const QString& error);

signals:
    void CreateDisk(const QString& path, const QString& password, uint64_t size);
    void CancelCreation();

protected:
    virtual void showEvent(QShowEvent *);
    virtual void closeEvent(QCloseEvent* event);

private slots:
    void on_btnCancel_clicked();
    void on_btnCreate_clicked();
    void on_btnBrowse_clicked();
    void on_checkShowPwd_clicked();

private:
    void RefreshCreateButtonState();
    void ProgressStarted(bool started);
    void CancelWithQuestion();

    uint64_t GetSize();
    QString GetFullPath();
    QString GetPassword();

private:
    Ui::CreateNewDiskDialog m_ui;
    bool m_inProgress;
    QString m_diskExtension;
};
