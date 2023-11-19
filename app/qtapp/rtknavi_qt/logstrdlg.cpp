//---------------------------------------------------------------------------
#include <QDialog>
#include <QShowEvent>
#include <QFileDialog>
#include <QCompleter>
#include <QFileSystemModel>

#include "rtklib.h"
#include "serioptdlg.h"
#include "cmdoptdlg.h"
#include "fileoptdlg.h"
#include "tcpoptdlg.h"
#include "logstrdlg.h"
#include "keydlg.h"

//---------------------------------------------------------------------------
LogStrDialog::LogStrDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    keyDialog = new KeyDialog(this);
    serialOptDialog = new SerialOptDialog(this);
    tcpOptDialog = new TcpOptDialog(this);

    QCompleter *fileCompleter = new QCompleter(this);
    QFileSystemModel *fileModel = new QFileSystemModel(fileCompleter);
    fileModel->setRootPath("");
    fileCompleter->setModel(fileModel);
    lEFilePath1->setCompleter(fileCompleter);
    lEFilePath2->setCompleter(fileCompleter);
    lEFilePath3->setCompleter(fileCompleter);

    connect(btnCancel, SIGNAL(clicked(bool)), this, SLOT(reject()));
    connect(btnOk, SIGNAL(clicked(bool)), this, SLOT(btnOkClicked()));
    connect(btnFile1, SIGNAL(clicked(bool)), this, SLOT(btnFile1Clicked()));
    connect(btnFile2, SIGNAL(clicked(bool)), this, SLOT(btnFile2Clicked()));
    connect(btnFile3, SIGNAL(clicked(bool)), this, SLOT(btnFile3Clicked()));
    connect(btnKey, SIGNAL(clicked(bool)), this, SLOT(btnKeyClicked()));
    connect(btnStream1, SIGNAL(clicked(bool)), this, SLOT(btnStream1Click()));
    connect(btnStream2, SIGNAL(clicked(bool)), this, SLOT(btnStream2Click()));
    connect(btnStream3, SIGNAL(clicked(bool)), this, SLOT(btnStream3Clicked()));
    connect(cBStream1, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEnable()));
    connect(cBStream2, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEnable()));
    connect(cBStream3, SIGNAL(currentIndexChanged(int)), this, SLOT(updateEnable()));
    connect(cBStream1C, SIGNAL(clicked(bool)), this, SLOT(updateEnable()));
    connect(cBStream2C, SIGNAL(clicked(bool)), this, SLOT(updateEnable()));
    connect(cBStream3C, SIGNAL(clicked(bool)), this, SLOT(updateEnable()));

    cBSwapInterval->setValidator(new QDoubleValidator(this));
}
//---------------------------------------------------------------------------
void LogStrDialog::showEvent(QShowEvent *event)
{
    if (event->spontaneous()) return;

    cBStream1C->setChecked(streamC[0]);
    cBStream2C->setChecked(streamC[1]);
    cBStream3C->setChecked(streamC[2]);
    cBStream1->setCurrentIndex(stream[0]);
    cBStream2->setCurrentIndex(stream[1]);
    cBStream3->setCurrentIndex(stream[2]);
    lEFilePath1->setText(getFilePath(paths[0][2]));
    lEFilePath2->setText(getFilePath(paths[1][2]));
    lEFilePath3->setText(getFilePath(paths[2][2]));
    cBSwapInterval->insertItem(0, swapInterval); cBSwapInterval->setCurrentIndex(0);
    cBTimeTag->setChecked(logTimeTag);

	updateEnable();
}
//---------------------------------------------------------------------------
void LogStrDialog::btnOkClicked()
{
    streamC [0] = cBStream1C->isChecked();
    streamC [1] = cBStream2C->isChecked();
    streamC [2] = cBStream3C->isChecked();
    stream  [0] = cBStream1->currentIndex();
    stream  [1] = cBStream2->currentIndex();
    stream  [2] = cBStream3->currentIndex();
    paths[0][2] = setFilePath(lEFilePath1->text());
    paths[1][2] = setFilePath(lEFilePath2->text());
    paths[2][2] = setFilePath(lEFilePath3->text());
    swapInterval = cBSwapInterval->currentText();
    logTimeTag = cBTimeTag->isChecked();

    accept();
}
//---------------------------------------------------------------------------
void LogStrDialog::btnFile1Clicked()
{
    lEFilePath1->setText(QDir::toNativeSeparators(QFileDialog::getSaveFileName(this, tr("Open..."), lEFilePath1->text())));
}
//---------------------------------------------------------------------------
void LogStrDialog::btnFile2Clicked()
{
    lEFilePath2->setText(QDir::toNativeSeparators(QFileDialog::getSaveFileName(this, tr("Open..."), lEFilePath2->text())));
}
//---------------------------------------------------------------------------
void LogStrDialog::btnFile3Clicked()
{
    lEFilePath3->setText(QDir::toNativeSeparators(QFileDialog::getSaveFileName(this, tr("Open..."), lEFilePath3->text())));
}
//---------------------------------------------------------------------------
void LogStrDialog::btnKeyClicked()
{
    keyDialog->exec();
}
//---------------------------------------------------------------------------
void LogStrDialog::btnStream1Click()
{
    switch (cBStream1->currentIndex()) {
    case 0: serialOptions(0, 0); break;
    case 1: tcpOptions(0, 1); break;
    case 2: tcpOptions(0, 0); break;
    case 3: tcpOptions(0, 2); break;
    case 4: tcpOptions(0, 4); break;
    }
}
//---------------------------------------------------------------------------
void LogStrDialog::btnStream2Click()
{
    switch (cBStream2->currentIndex()) {
    case 0: serialOptions(1, 0); break;
    case 1: tcpOptions(1, 1); break;
    case 2: tcpOptions(1, 0); break;
    case 3: tcpOptions(1, 2); break;
    case 4: tcpOptions(0, 4); break;
	}
}
//---------------------------------------------------------------------------
void LogStrDialog::btnStream3Clicked()
{
    switch (cBStream3->currentIndex()) {
    case 0: serialOptions(2, 0); break;
    case 1: tcpOptions(2, 1); break;
    case 2: tcpOptions(2, 0); break;
    case 3: tcpOptions(2, 2); break;
    case 4: tcpOptions(0, 4); break;
    }
}
//---------------------------------------------------------------------------
QString LogStrDialog::getFilePath(const QString &path)
{
    QString file;

    file = path.mid(0, path.indexOf("::"));

    return file;
}
//---------------------------------------------------------------------------
QString LogStrDialog::setFilePath(const QString &p)
{
    QString path = p;
    QString str;
    bool okay;

    if (cBTimeTag->isChecked()) path += "::T";
    str = cBSwapInterval->currentText();
    str.toDouble(&okay);
    if (okay) path += "::S=" + str;
	return path;
}
//---------------------------------------------------------------------------
void LogStrDialog::serialOptions(int index, int opt)
{
    serialOptDialog->path = paths[index][0];
    serialOptDialog->options = opt;

    serialOptDialog->exec();
    if (serialOptDialog->result() != QDialog::Accepted) return;

    paths[index][0] = serialOptDialog->path;
}
//---------------------------------------------------------------------------
void LogStrDialog::tcpOptions(int index, int opt)
{
    tcpOptDialog->path = paths[index][1];
    tcpOptDialog->showOptions = opt;
    for (int i = 0; i < 10; i++) {
        tcpOptDialog->history[i] = history[i];
	}
    tcpOptDialog->exec();
    if (tcpOptDialog->result() != QDialog::Accepted) return;

    paths[index][1] = tcpOptDialog->path;
    for (int i = 0; i < 10; i++) {
        history[i] = tcpOptDialog->history[i];
	}
}
//---------------------------------------------------------------------------
void LogStrDialog::updateEnable(void)
{
    int ena = (cBStream1C->isChecked() && cBStream1->currentIndex() == 5) ||
          (cBStream2C->isChecked() && cBStream2->currentIndex() == 5) ||
          (cBStream3C->isChecked() && cBStream3->currentIndex() == 5);

    cBStream1->setEnabled(cBStream1C->isChecked());
    cBStream2->setEnabled(cBStream2C->isChecked());
    cBStream3->setEnabled(cBStream3C->isChecked());
    btnStream1->setEnabled(cBStream1C->isChecked() && cBStream1->currentIndex() <= 4);
    btnStream2->setEnabled(cBStream2C->isChecked() && cBStream2->currentIndex() <= 4);
    btnStream3->setEnabled(cBStream3C->isChecked() && cBStream3->currentIndex() <= 4);
    lEFilePath1->setEnabled(cBStream1C->isChecked() && cBStream1->currentIndex() == 5);
    lEFilePath2->setEnabled(cBStream2C->isChecked() && cBStream2->currentIndex() == 5);
    lEFilePath3->setEnabled(cBStream3C->isChecked() && cBStream3->currentIndex() == 5);
    btnFile1->setEnabled(cBStream1C->isChecked() && cBStream1->currentIndex() == 5);
    btnFile2->setEnabled(cBStream2C->isChecked() && cBStream2->currentIndex() == 5);
    btnFile3->setEnabled(cBStream3C->isChecked() && cBStream3->currentIndex() == 5);
    lblSwapInterval->setEnabled(ena);
    lblH->setEnabled(ena);
    lblF1->setEnabled(ena);
    cBTimeTag->setEnabled(ena);
    cBSwapInterval->setEnabled(ena);
    btnKey->setEnabled(ena);
    cBOutputEvent->setEnabled(lEFilePath1->isEnabled());
}
//---------------------------------------------------------------------------