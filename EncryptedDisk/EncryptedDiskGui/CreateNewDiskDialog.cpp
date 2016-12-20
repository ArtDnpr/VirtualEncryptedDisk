#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>

#include "CreateNewDiskDialog.h"

CreateNewDiskDialog::CreateNewDiskDialog(QWidget *parent, const QString& diskExtension)
: QDialog(parent)
, m_inProgress(false)
, m_diskExtension(diskExtension)
{
    m_ui.setupUi(this);

    m_ui.lineSize->setValidator(new QIntValidator(this));

    RefreshCreateButtonState();
    ProgressStarted(false);

    connect(m_ui.linePath, &QLineEdit::textChanged, this, &CreateNewDiskDialog::RefreshCreateButtonState);
    connect(m_ui.lineSize, &QLineEdit::textChanged, this, &CreateNewDiskDialog::RefreshCreateButtonState);
    connect(m_ui.linePassword, &QLineEdit::textChanged, this, &CreateNewDiskDialog::RefreshCreateButtonState);
    connect(m_ui.lineName, &QLineEdit::textChanged, this, &CreateNewDiskDialog::RefreshCreateButtonState);
}

void CreateNewDiskDialog::Cleanup()
{
    m_ui.linePath->clear();
    m_ui.lineSize->clear();
    m_ui.linePassword->clear();
    m_ui.lineName->clear();
    m_ui.progressBar->setValue(0);
    m_ui.checkShowPwd->setChecked(false);

    ProgressStarted(false);
    RefreshCreateButtonState();
}

void CreateNewDiskDialog::OnProgressChange(uint64_t encryptedSize, uint64_t totalSize)
{
    m_ui.progressBar->setValue(encryptedSize * 100 / totalSize);
}

void CreateNewDiskDialog::OnProgressFinished(bool canceled)
{
    QString message;
    if (canceled)
    {
        message = "Disk creation has been canceled";
    }
    else
    {
        message = "Disk creation has been finished successfully";
    }

    QMessageBox::information(this, "New disk creation", message);
    m_inProgress = false;
    close();
}

void CreateNewDiskDialog::OnProgressError(const QString& error)
{
    QMessageBox::critical(this, "Error", error);
    ProgressStarted(false);
}

void CreateNewDiskDialog::showEvent(QShowEvent*)
{
    setFixedSize(size());
}

void CreateNewDiskDialog::closeEvent(QCloseEvent* event)
{
    if (!m_inProgress)
    {
        event->accept();
        return;
    }

    if (QMessageBox::Yes == QMessageBox::question(this,
                                                  "Cancel operation",
                                                  "Are you sure you want to cancel disk creation?",
                                                  QMessageBox::Yes | QMessageBox::No))
    {
        emit CancelCreation();
        event->accept();
        return;
    }

    event->ignore();
}

void CreateNewDiskDialog::on_btnCancel_clicked()
{
    if (!m_inProgress)
    {
        close();
        return;
    }

    CancelWithQuestion();
}

void CreateNewDiskDialog::on_btnCreate_clicked()
{
    emit CreateDisk(GetFullPath(), GetPassword(), GetSize());
    ProgressStarted(true);
}

void CreateNewDiskDialog::on_btnBrowse_clicked()
{
    const QString& choosenPath = QFileDialog::getExistingDirectory(this, "Select folder to save disk file");
    if (!choosenPath.isEmpty())
    {
        m_ui.linePath->setText(choosenPath);
    }
}

void CreateNewDiskDialog::on_checkShowPwd_clicked()
{
    const bool show = m_ui.checkShowPwd->isChecked();
    m_ui.linePassword->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

void CreateNewDiskDialog::RefreshCreateButtonState()
{
    bool enabled = !GetFullPath().isEmpty() && !GetPassword().isEmpty();
    enabled &= GetSize() > 0;
    m_ui.btnCreate->setEnabled(enabled);
}

void CreateNewDiskDialog::ProgressStarted(bool started)
{
    m_inProgress = started;

    m_ui.linePath->setDisabled(started);
    m_ui.lineSize->setDisabled(started);
    m_ui.linePassword->setDisabled(started);
    m_ui.lineName->setDisabled(started);

    m_ui.btnCreate->setDisabled(started);
    m_ui.btnBrowse->setDisabled(started);
    m_ui.checkShowPwd->setDisabled(started);

    m_ui.progressBar->setEnabled(started);
}

void CreateNewDiskDialog::CancelWithQuestion()
{
    if (QMessageBox::Yes == QMessageBox::question(this,
                                                  "Cancel operation",
                                                  "Are you sure you want to cancel disk creation?",
                                                  QMessageBox::Yes | QMessageBox::No))
    {
        emit CancelCreation();
    }
}

uint64_t CreateNewDiskDialog::GetSize()
{
    const QString& sizeStr = m_ui.lineSize->text();

    return sizeStr.isEmpty() ? 0 : sizeStr.toULongLong();
}

QString CreateNewDiskDialog::GetFullPath()
{
    const QString& fullPath = m_ui.linePath->text() + QDir::separator() + m_ui.lineName->text() + m_diskExtension;

    return QDir::toNativeSeparators(fullPath);
}

QString CreateNewDiskDialog::GetPassword()
{
    return m_ui.linePassword->text();
}
