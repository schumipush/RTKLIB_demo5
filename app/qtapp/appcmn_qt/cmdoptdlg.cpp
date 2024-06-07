//---------------------------------------------------------------------------
// ported to Qt by Jens Reimann

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>

#include "cmdoptdlg.h"
#include "ui_cmdoptdlg.h"

//---------------------------------------------------------------------------
CmdOptDialog::CmdOptDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::CmdOptDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &CmdOptDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &CmdOptDialog::accept);
    connect(ui->btnLoad, &QPushButton::clicked, this, &CmdOptDialog::load);
    connect(ui->btnSave, &QPushButton::clicked, this, &CmdOptDialog::save);
    connect(ui->cBCloseCommands, &QPushButton::clicked, this, &CmdOptDialog::updateEnable);
    connect(ui->cBOpenCommands, &QPushButton::clicked, this, &CmdOptDialog::updateEnable);
    connect(ui->cBPeriodicCommands, &QPushButton::clicked, this, &CmdOptDialog::updateEnable);

    // set default
    setCommandsEnabled(0, true);
    setCommandsEnabled(1, true);
}

//---------------------------------------------------------------------------
void CmdOptDialog::load()
{
    QString fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this));
    QPlainTextEdit *cmd[] = {ui->tEOpenCommands, ui->tECloseCommands, ui->tEPeriodicCommands};
    QByteArray buff;
    int n = 0;

    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, tr("Error"), tr("Could not open %1").arg(fileName));
        return;
    }

    cmd[0]->clear();
    cmd[1]->clear();
    cmd[2]->clear();

    while (!f.atEnd() && n < 3) {
        buff = f.readLine(0);
        if (buff.at(0) == '@') {
            n++;
            continue;
        }
        cmd[n]->appendPlainText(buff);
    }
}
//---------------------------------------------------------------------------
void CmdOptDialog::save()
{
    QString fileName = QDir::toNativeSeparators(QFileDialog::getSaveFileName(this));
    QFile fp(fileName);

    if (!fp.open(QIODevice::WriteOnly)) return;

    fp.write(ui->tEOpenCommands->toPlainText().toLatin1());
    fp.write("\n@\n");
    fp.write(ui->tECloseCommands->toPlainText().toLatin1());
    fp.write("\n@\n");
    fp.write(ui->tEPeriodicCommands->toPlainText().toLatin1());
}

//---------------------------------------------------------------------------
void CmdOptDialog::updateEnable()
{
    ui->tEOpenCommands->setEnabled(ui->cBOpenCommands->isChecked());
    ui->tECloseCommands->setEnabled(ui->cBCloseCommands->isChecked());
    ui->tEPeriodicCommands->setEnabled(ui->cBPeriodicCommands->isChecked());
}
//---------------------------------------------------------------------------
void CmdOptDialog::setCommands(int i, const QString & command)
{
    QPlainTextEdit * edit[] = {ui->tEOpenCommands, ui->tECloseCommands, ui->tEPeriodicCommands};
    edit[i]->appendPlainText(command);
}
//---------------------------------------------------------------------------
QString CmdOptDialog::getCommands(int i)
{
    QPlainTextEdit * edit[] = {ui->tEOpenCommands, ui->tECloseCommands, ui->tEPeriodicCommands};
    return edit[i]->toPlainText();
}
//---------------------------------------------------------------------------
void CmdOptDialog::setCommandsEnabled(int i, bool enabled)
{
    QCheckBox * checkBox[] = {ui->cBOpenCommands, ui->cBCloseCommands, ui->cBPeriodicCommands};
    checkBox[i]->setChecked(enabled);
    updateEnable();
}
//---------------------------------------------------------------------------
bool CmdOptDialog::getCommandsEnabled(int i)
{
    QCheckBox * checkBox[] = {ui->cBOpenCommands, ui->cBCloseCommands, ui->cBPeriodicCommands};
    return checkBox[i]->isChecked();
}
