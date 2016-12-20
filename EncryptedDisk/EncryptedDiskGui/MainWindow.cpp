#include "MainWindow.h"
#include "CreateNewDiskDialog.h"
#include "MountDiskDialog.h"
#include "VirtualDiskController.h"
#include <QMouseEvent>

namespace
{
    size_t s_bytesInMb = 1024 * 1024;
    const QString s_diskExtension = ".evd";

    enum class TableColumn
    {
        Letter = 0,
        Size,
        Path
    };

    void ShowDisk(const Disk& newDisk, QTableWidget* tableWidget)
    {
        const int newRowIndex = tableWidget->model()->rowCount();
        tableWidget->insertRow(newRowIndex);
        tableWidget->setItem(newRowIndex, static_cast<int>(TableColumn::Letter), new QTableWidgetItem(QChar(newDisk.letter)));
        tableWidget->setItem(newRowIndex, static_cast<int>(TableColumn::Size), new QTableWidgetItem(QString::number(newDisk.size / s_bytesInMb) + "MB"));
        tableWidget->setItem(newRowIndex, static_cast<int>(TableColumn::Path), new QTableWidgetItem(QString::fromStdWString(newDisk.fullDiskPath)));
    }
}

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent)
, m_dlgCreateDisk(new CreateNewDiskDialog(this, s_diskExtension))
, m_dlgMountDisk(new MountDiskDialog(this, s_diskExtension))
, m_diskController(new VirtualDiskController)
, m_canceled(false)
{
    m_ui.setupUi(this);

    connect(this, &MainWindow::ProgressChange, m_dlgCreateDisk, &CreateNewDiskDialog::OnProgressChange, Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::ProgressFinished, m_dlgCreateDisk, &CreateNewDiskDialog::OnProgressFinished, Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::ProgressError, m_dlgCreateDisk, &CreateNewDiskDialog::OnProgressError);
    connect(m_dlgCreateDisk, &CreateNewDiskDialog::CreateDisk, this, &MainWindow::OnCreateDisk);
    connect(m_dlgCreateDisk, &CreateNewDiskDialog::CancelCreation, this, &MainWindow::OnCancelCreation);
    connect(m_dlgMountDisk, &MountDiskDialog::MountDisk, this, &MainWindow::OnMountDisk);

    QHeaderView* header = m_ui.tableWidget->horizontalHeader();
    header->setSectionResizeMode(static_cast<int>(TableColumn::Letter), QHeaderView::ResizeToContents);
    header->setSectionResizeMode(static_cast<int>(TableColumn::Size), QHeaderView::ResizeToContents);

    try
    {
        for (const Disk& disk : m_diskController->getDisks())
        {
            ShowDisk(disk, m_ui.tableWidget);
        }
    }
    catch (const std::exception& ex)
    {
        QMessageBox::warning(0, "Warning", "Cannot communicate with device driver\n" + QString::fromStdString(ex.what()));
    }
}

MainWindow::~MainWindow()
{}

void MainWindow::virtualDiskControllerDidEncryptDisk(uint64_t encryptedSize, uint64_t totalSize)
{
    emit ProgressChange(encryptedSize, totalSize);
}

void MainWindow::virtualDiskControllerDidFinishCreation(bool canceled)
{
    emit ProgressFinished(canceled);
}

void MainWindow::virtualDiskControllerDidFailDiskCreation(const std::string& error)
{
    emit ProgressError(QString::fromStdString(error));
}

void MainWindow::OnCreateDisk(const QString& path, const QString& password, uint64_t size)
{
    m_canceled = false;
    std::thread t(&VirtualDiskController::createNewDisk, m_diskController.get(), path.toStdWString(), size, password.toStdString(), this, std::ref(m_canceled));
    t.detach();
}

void MainWindow::OnCancelCreation()
{
    m_canceled = true;
}

void MainWindow::OnMountDisk(const QString& path, char diskLetter, const QString& password)
{
    for (int i = 0; i < m_ui.tableWidget->rowCount(); i++)
    {
        if(m_ui.tableWidget->item(i, static_cast<int>(TableColumn::Path))->text() == path)
        {
            QMessageBox::information(this, "Information", "The disk is already mount");
            return;
         }
    }

    try
    {
        const Disk& newDisk = m_diskController->mountDisk(path.toStdWString(), diskLetter, password.toStdString());
        ShowDisk(newDisk, m_ui.tableWidget);
        m_dlgMountDisk->close();
    }
    catch (const std::exception& ex)
    {
        QMessageBox::critical(m_dlgMountDisk, "Error", ex.what());
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (QMessageBox::Yes == QMessageBox::question(this,
                                                 "Quit",
                                                 "Are you sure you want to quit?\nAll disks will be unmounted",
                                                 QMessageBox::Yes | QMessageBox::No))
    {
        try
        {
            m_diskController->unmountAllDisks();
        }
        catch (const std::exception& ex)
        {
            QMessageBox::warning(this, "Warning", "Cannot communicate with device driver\n" + QString::fromStdString(ex.what()));
            return;
        }
        event->accept();
        return;
    }

    event->ignore();
}

void MainWindow::on_btnCreateNewDisk_clicked()
{
    m_dlgCreateDisk->Cleanup();
    m_dlgCreateDisk->show();
}

void MainWindow::on_btnUnmount_clicked()
{
    const int selectedRow = m_ui.tableWidget->currentRow();
    if (-1 == selectedRow)
    {
        return;
    }

    const char diskLetter = m_ui.tableWidget->item(selectedRow, static_cast<int>(TableColumn::Letter))->text().at(0).toUpper().toLatin1();

    try
    {
        m_diskController->unmountDisk(diskLetter);
    }
    catch (const std::exception& ex)
    {
        QMessageBox::warning(this, "Warning", "Cannot communicate with device driver\n" + QString::fromStdString(ex.what()));
        return;
    }

    m_ui.tableWidget->removeRow(selectedRow);
}

void MainWindow::on_btnMountDisk_clicked()
{
    m_dlgMountDisk->Cleanup();
    m_dlgMountDisk->show();
}

void MainWindow::on_btnUnmountAll_clicked()
{
    if (QMessageBox::No == QMessageBox::question(this,
                                                 "Question",
                                                 "Are you sure you want to unmount all disks?",
                                                 QMessageBox::Yes | QMessageBox::No))
    {
        return;
    }

    try
    {
        m_diskController->unmountAllDisks();
    }
    catch (const std::exception& ex)
    {
        QMessageBox::warning(this, "Warning", "Cannot communicate with device driver\n" + QString::fromStdString(ex.what()));
        return;
    }

    int rawsCount = m_ui.tableWidget->rowCount();
    if(!rawsCount)
    {
        return;
    }

    for (int i = 0; i < rawsCount; i++)
    {
        m_ui.tableWidget->removeRow(0);
    }
}

void MainWindow::on_actionCreate_triggered()
{
    on_btnCreateNewDisk_clicked();
}

void MainWindow::on_actionMount_triggered()
{
    on_btnMountDisk_clicked();
}


void MainWindow::on_actionUnmountAll_triggered()
{
    on_btnUnmountAll_clicked();
}

void MainWindow::on_actionQuit_triggered()
{
    qApp->closeAllWindows();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::information(this, "About", "Created by Artem and Sergey");
}

void MainWindow::on_actionHow_to_use_triggered()
{
    const QString str = "After user presses button \"Create\" and set pass and Path to this one, this programm, first of all encryptes this file. Then user can mount device on this file.\n\n"
            "After user presses button \"Mount\" and choose file(with right pass) the programm checkes is this file accessable or not, and create device. If file is not accessable - user will see "
            "appropriate message. Then user need to formate disk.Next step - user can use created device.\n\n"
            "After user presses button \"Unmount\" and choose appropriate device the programm checkes is this file accessable or not, if no - user "
            "will see appropriate message. The other way file would be closed and device would be deleted.\n\n"
            "Press \"Unmount all\" and the programm will unmount all accessible devices. After shut down the programm all mounted devices will be unmounted.\n\n"
            "The driver supported by the unload-routine(it can be unloaded).\n\n"
            "The driver works under next scheme: during start-up of driver, this one created Null_Device(Device Manager), and a symbolic link to it. It is Null_device who leads others "
            "devices and this one get general input controls(mount, status of all devices etc.)."
            "When the manager creates a new device, in user mode appears symbolic link to it. All mounted devices work in multithreading mode!";

    QMessageBox::information(this, "How to use", str);
}
