#include "MountDiskDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfoList>
#include <QDir>

MountDiskDialog::MountDiskDialog(QWidget *parent, const QString& diskExtension)
: QDialog(parent)
, m_diskExtension(diskExtension)
{
    m_ui.setupUi(this);


    connect(m_ui.linePath, &QLineEdit::textChanged, this, &MountDiskDialog::RefreshMountButtonState);
    connect(m_ui.linePassword, &QLineEdit::textChanged, this, &MountDiskDialog::RefreshMountButtonState);

    Cleanup();
}

void MountDiskDialog::Cleanup()
{
    m_ui.linePassword->clear();
    m_ui.linePath->clear();
    m_ui.checkShowPwd->setChecked(false);

    RefreshMountButtonState();
    RefreshAvailableDrives();
}

void MountDiskDialog::showEvent(QShowEvent*)
{
    setFixedSize(size());
}

void MountDiskDialog::on_btnMount_clicked()
{
    emit MountDisk(GetFullPath(), GetDiskLetter(), GetPassword());
}

void MountDiskDialog::on_btnCancel_clicked()
{
    close();
}

void MountDiskDialog::on_btnBrowse_clicked()
{
    QString choosenPath = QFileDialog::getOpenFileName(this, "Select disk file", "", QString("Encrypted disk files (*%1)").arg(m_diskExtension));
    if (!choosenPath.isEmpty())
    {
        choosenPath = ::QDir::toNativeSeparators(choosenPath);
        m_ui.linePath->setText(choosenPath);
    }
}

void MountDiskDialog::on_checkShowPwd_clicked()
{
    const bool show = m_ui.checkShowPwd->isChecked();
    m_ui.linePassword->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

void MountDiskDialog::RefreshMountButtonState()
{
    bool enabled = !GetFullPath().isEmpty() && !GetPassword().isEmpty();
    m_ui.btnMount->setEnabled(enabled);
}

void MountDiskDialog::RefreshAvailableDrives()
{
    const QFileInfoList& drives = QDir::drives();
    QStringList busyLetters;
    for (const QFileInfo& driveInfo : drives)
    {
        busyLetters << driveInfo.absolutePath().at(0);
    }

    QStringList availableLetters;
    for (char c = 'A'; c <= 'Z'; c++)
    {
        QChar letter(c);
        if (!busyLetters.contains(letter))
        {
            availableLetters << letter;
        }
    }

    m_ui.comboDiskLetter->clear();
    m_ui.comboDiskLetter->addItems(availableLetters);
    m_ui.comboDiskLetter->setCurrentIndex(0);
}

QString MountDiskDialog::GetFullPath()
{
    return m_ui.linePath->text();
}

char MountDiskDialog::GetDiskLetter()
{
    return m_ui.comboDiskLetter->currentText().at(0).toUpper().toLatin1();
}

QString MountDiskDialog::GetPassword()
{
    return m_ui.linePassword->text();
}
