#pragma once

#include <QMainWindow>
#include <QMessageBox>

#include <memory>
#include <thread>
#include <atomic>
#include <functional>

#include "ui_MainWindow.h"
#include "IVirtualDiskControllerObserver.h"

class CreateNewDiskDialog;
class VirtualDiskController;
class MountDiskDialog;
struct Disk;

class MainWindow : public QMainWindow, public IVirtualDiskControllerObserver
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    virtual ~MainWindow();

    virtual void virtualDiskControllerDidEncryptDisk(uint64_t encryptedSize, uint64_t totalSize);
    virtual void virtualDiskControllerDidFinishCreation(bool canceled);
    virtual void virtualDiskControllerDidFailDiskCreation(const std::string& error);

public slots:
    void OnCreateDisk(const QString& path, const QString& password, uint64_t size);
    void OnCancelCreation();
    void OnMountDisk (const QString& path, char diskLetter, const QString& password);

signals:
    void ProgressChange(uint64_t encryptedSize, uint64_t totalSize);
    void ProgressFinished(bool canceled);
    void ProgressError(const QString& error);

protected:
    virtual void closeEvent(QCloseEvent *event);

private slots:
    void on_btnUnmount_clicked();
    void on_btnCreateNewDisk_clicked();
    void on_btnMountDisk_clicked();
    void on_btnUnmountAll_clicked();

    void on_actionCreate_triggered();
    void on_actionMount_triggered();
    void on_actionUnmountAll_triggered();
    void on_actionQuit_triggered();
    void on_actionAbout_triggered();

    void on_actionHow_to_use_triggered();

private:
    Ui::MainWindow m_ui;
    CreateNewDiskDialog *m_dlgCreateDisk;
    MountDiskDialog *m_dlgMountDisk;

    std::auto_ptr<VirtualDiskController> m_diskController;
    std::atomic<bool> m_canceled;
};
