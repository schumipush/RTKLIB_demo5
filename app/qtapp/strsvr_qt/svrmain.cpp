//---------------------------------------------------------------------------
// strsvr : stream server
//
//          Copyright (C) 2007-2012 by T.TAKASU, All rights reserved.
//          ported to Qt by Jens Reimann
//
// options : strsvr [-t title][-i file][-auto][-tray]
//
//           -t title   window title
//           -i file    ini file path
//           -auto      auto start
//           -tray      start as task tray icon
//
// version : $Revision:$ $Date:$
// history : 2008/04/03  1.1 rtklib 2.3.1
//           2010/07/18  1.2 rtklib 2.4.0
//           2011/06/10  1.3 rtklib 2.4.1
//           2012/12/15  1.4 rtklib 2.4.2
//                       add stream conversion function
//                       add option -auto and -tray
//---------------------------------------------------------------------------
#include <clocale>

#include <QTimer>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QSettings>
#include <QMenu>
#include <QStringList>

#include "rtklib.h"
#include "svroptdlg.h"
#include "serioptdlg.h"
#include "fileoptdlg.h"
#include "tcpoptdlg.h"
#include "ftpoptdlg.h"
#include "cmdoptdlg.h"
#include "convdlg.h"
#include "aboutdlg.h"
#include "mondlg.h"
#include "svrmain.h"
#include "mondlg.h"

//---------------------------------------------------------------------------

#define PRGNAME     "STRSVR-Qt"        // program name
#define TRACEFILE   "strsvr.trace"  // debug trace file
#define CLORANGE    QColor(0x00,0xAA,0xFF)

#define MIN(x,y)    ((x)<(y)?(x):(y))
#define MAX(x,y)    ((x)>(y)?(x):(y))

strsvr_t strsvr;

extern "C" {
extern int showmsg(const char *, ...)  {return 0;}
extern void settime(gtime_t) {}
extern void settspan(gtime_t, gtime_t) {}
}

QString color2String(const QColor &c){
    return QString("rgb(%1,%2,%3)").arg(c.red()).arg(c.green()).arg(c.blue());
}

// number to comma-separated number -----------------------------------------
static void num2cnum(int num, char *str)
{
    char buff[256], *p=buff, *q=str;
    int i, n;
    n = sprintf(buff, "%u", (uint32_t)num);
    for (i = 0; i < n; i++) {
        *q++ = *p++;
        if ((n-i-1) % 3 == 0 && i < n-1) *q++=',';
    }
    *q = '\0';
}
// constructor --------------------------------------------------------------
MainForm::MainForm(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    QCoreApplication::setApplicationName(PRGNAME);
    QCoreApplication::setApplicationVersion(QString(VER_RTKLIB) + " " + PATCH_LEVEL);

    setWindowIcon(QIcon(":/icons/rtk6"));
    setWindowTitle(QStringLiteral("%1 ver.%2 %3").arg(PRGNAME).arg(VER_RTKLIB).arg(PATCH_LEVEL));

    setlocale(LC_NUMERIC, "C"); // use point as decimal separator in formatted output

    QString file = QApplication::applicationFilePath();
    QFileInfo fi(file);
    iniFile = fi.absolutePath() + "/" + fi.baseName() + ".ini";

    trayIcon = new QSystemTrayIcon(QIcon(":/icons/strsvr_Icon"));

    QMenu *trayMenu = new QMenu(this);
    trayMenu->addAction(acMenuExpand);
    trayMenu->addSeparator();
    trayMenu->addAction(acMenuStart);
    trayMenu->addAction(acMenuStop);
    trayMenu->addSeparator();
    trayMenu->addAction(acMenuExit);

    trayIcon->setContextMenu(trayMenu);

    svrOptDialog = new SvrOptDialog(this);
    tcpOptDialog = new TcpOptDialog(this);
    serialOptDialog = new SerialOptDialog(this);
    fileOptDialog = new FileOptDialog(this);
    ftpOptDialog = new FtpOptDialog(this);
    strMonDialog = new StrMonDialog(this);

    btnStop->setVisible(false);

    startTime.sec = startTime.time = endTime.sec = endTime.time = 0;

    connect(btnExit, &QPushButton::clicked, this, &MainForm::close);
    connect(btnInput, &QPushButton::clicked, this, &MainForm::btnInputClicked);
    connect(btnStart, &QPushButton::clicked, this, &MainForm::serverStart);
    connect(btnStop, &QPushButton::clicked, this, &MainForm::serverStop);
    connect(btnOpt, &QPushButton::clicked, this, &MainForm::btnOptionsClicked);
    connect(btnCmd, &QPushButton::clicked, this, &MainForm::btnCommandClicked);
    connect(btnCmd1, &QPushButton::clicked, this, &MainForm::btnCommandClicked);
    connect(btnCmd2, &QPushButton::clicked, this, &MainForm::btnCommandClicked);
    connect(btnCmd3, &QPushButton::clicked, this, &MainForm::btnCommandClicked);
    connect(btnCmd4, &QPushButton::clicked, this, &MainForm::btnCommandClicked);
    connect(btnCmd5, &QPushButton::clicked, this, &MainForm::btnCommandClicked);
    connect(btnCmd6, &QPushButton::clicked, this, &MainForm::btnCommandClicked);
    connect(btnAbout, &QPushButton::clicked, this, &MainForm::btnAboutClicked);
    connect(btnStreamMonitor, &QPushButton::clicked, this, &MainForm::btnStreamMonitorClicked);
    connect(acMenuStart, &QAction::triggered, this, &MainForm::serverStart);
    connect(acMenuStop, &QAction::triggered, this, &MainForm::serverStop);
    connect(acMenuExit, &QAction::triggered, this, &MainForm::close);
    connect(acMenuExpand, &QAction::triggered, this, &MainForm::menuExpandClicked);
    connect(btnOutput1, &QPushButton::clicked, this, &MainForm::btnOutputClicked);
    connect(btnOutput2, &QPushButton::clicked, this, &MainForm::btnOutputClicked);
    connect(btnOutput3, &QPushButton::clicked, this, &MainForm::btnOutputClicked);
    connect(btnOutput4, &QPushButton::clicked, this, &MainForm::btnOutputClicked);
    connect(btnOutput5, &QPushButton::clicked, this, &MainForm::btnOutputClicked);
    connect(btnOutput6, &QPushButton::clicked, this, &MainForm::btnOutputClicked);
    connect(btnTaskIcon, &QPushButton::clicked, this, &MainForm::btnTaskIconClicked);
    connect(cBInput, &QComboBox::currentIndexChanged, this, &MainForm::updateEnable);
    connect(cBOutput1, &QComboBox::currentIndexChanged, this, &MainForm::updateEnable);
    connect(cBOutput2, &QComboBox::currentIndexChanged, this, &MainForm::updateEnable);
    connect(cBOutput3, &QComboBox::currentIndexChanged, this, &MainForm::updateEnable);
    connect(cBOutput4, &QComboBox::currentIndexChanged, this, &MainForm::updateEnable);
    connect(cBOutput5, &QComboBox::currentIndexChanged, this, &MainForm::updateEnable);
    connect(cBOutput6, &QComboBox::currentIndexChanged, this, &MainForm::updateEnable);
    connect(btnConv1, &QPushButton::clicked, this, &MainForm::btnConvertClicked);
    connect(btnConv2, &QPushButton::clicked, this, &MainForm::btnConvertClicked);
    connect(btnConv3, &QPushButton::clicked, this, &MainForm::btnConvertClicked);
    connect(btnConv4, &QPushButton::clicked, this, &MainForm::btnConvertClicked);
    connect(btnConv5, &QPushButton::clicked, this, &MainForm::btnConvertClicked);
    connect(btnConv6, &QPushButton::clicked, this, &MainForm::btnConvertClicked);
    connect(btnLog1, &QPushButton::clicked, this, &MainForm::btnLogClicked);
    connect(btnLog2, &QPushButton::clicked, this, &MainForm::btnLogClicked);
    connect(btnLog3, &QPushButton::clicked, this, &MainForm::btnLogClicked);
    connect(btnLog4, &QPushButton::clicked, this, &MainForm::btnLogClicked);
    connect(btnLog5, &QPushButton::clicked, this, &MainForm::btnLogClicked);
    connect(btnLog6, &QPushButton::clicked, this, &MainForm::btnLogClicked);
    connect(&serverStatusTimer, &QTimer::timeout, this, &MainForm::serverStatusTimerTimeout);
    connect(&streamMonitorTimer, &QTimer::timeout, this, &MainForm::streamMonitorTimerTimeout);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainForm::trayIconActivated);

    serverStatusTimer.setInterval(50);
    streamMonitorTimer.setInterval(100);
}
// callback on form create --------------------------------------------------
void MainForm::showEvent(QShowEvent *event)
{
    if (event->spontaneous()) return;

    int autorun = 0, tasktray = 0;

    strsvrinit(&strsvr, MAXSTR-1);    

    QCommandLineParser parser;
    parser.setApplicationDescription(tr("stream server qt"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("source", QCoreApplication::translate("main", "Source file to copy."));
    parser.addPositionalArgument("destination", QCoreApplication::translate("main", "Destination directory."));

    // A boolean option with a single name (-p)
    QCommandLineOption showProgressOption("p", QCoreApplication::translate("main", "Show progress during copy"));
    parser.addOption(showProgressOption);

    QCommandLineOption trayOption(QStringList() << "tray",
                                  QCoreApplication::translate("main", "start as task tray icon."));
    parser.addOption(trayOption);

    QCommandLineOption autoOption(QStringList() << "auto",
                                  QCoreApplication::translate("main", "auto start."));
    parser.addOption(autoOption);

    QCommandLineOption windowTitleOption(QStringList() << "t",
                                         QCoreApplication::translate("main", "window title."),
                                         QCoreApplication::translate("main", "title"));
    parser.addOption(windowTitleOption);

    QCommandLineOption iniFileOption(QStringList() << "i",
                                     QCoreApplication::translate("main", "ini file path."),
                                     QCoreApplication::translate("main", "file"));
    parser.addOption(iniFileOption);

    parser.process(*QApplication::instance());

    if (parser.isSet(iniFileOption))
        iniFile = parser.value(iniFileOption);

    loadOptions();

    if (parser.isSet(windowTitleOption))
        setWindowTitle(parser.value(windowTitleOption));
    if (parser.isSet(autoOption)) autorun = 1;
    if (parser.isSet(trayOption)) tasktray = 1;

    setTrayIcon(0);

    if (tasktray) {
        setVisible(false);
        trayIcon->setVisible(true);
    }
    if (autorun)
        serverStart();

    serverStatusTimer.start();
    streamMonitorTimer.start();
}
// callback on form close ---------------------------------------------------
void MainForm::closeEvent(QCloseEvent *)
{
    saveOptions();
}
// callback on button-options -----------------------------------------------
void MainForm::btnOptionsClicked()
{
    for (int i = 0; i < 6; i++) svrOptDialog->serverOptions[i] = serverOptions[i];
    for (int i = 0; i < 3; i++) svrOptDialog->antennaPos[i] = antennaPosition[i];
    for (int i = 0; i < 3; i++) svrOptDialog->antennaOffset[i] = antennaOffsets[i];

    svrOptDialog->traceLevel = traceLevel;
    svrOptDialog->nmeaRequest = nmeaRequest;
    svrOptDialog->fileSwapMargin = fileSwapMargin;
    svrOptDialog->relayBack = relayBack;
    svrOptDialog->progressBarRange = progressBarRange;
    svrOptDialog->stationPositionFile = stationPositionFile;
    svrOptDialog->exeDirectory = exeDirectory;
    svrOptDialog->localDirectory = localDirectory;
    svrOptDialog->proxyAddress = proxyAddress;
    svrOptDialog->stationId = stationId;
    svrOptDialog->stationSelect = stationSelect;
    svrOptDialog->antennaType = antennaType;
    svrOptDialog->receiverType = receiverType;
    svrOptDialog->logFile = logFile;

    svrOptDialog->exec();
    if (svrOptDialog->result() != QDialog::Accepted) return;

    for (int i = 0; i < 6; i++) serverOptions[i] = svrOptDialog->serverOptions[i];
    for (int i = 0; i < 3; i++) antennaPosition[i] = svrOptDialog->antennaPos[i];
    for (int i = 0; i < 3; i++) antennaOffsets[i] = svrOptDialog->antennaOffset[i];

    traceLevel = svrOptDialog->traceLevel;
    nmeaRequest = svrOptDialog->nmeaRequest;
    fileSwapMargin = svrOptDialog->fileSwapMargin;
    relayBack = svrOptDialog->relayBack;
    progressBarRange = svrOptDialog->progressBarRange;
    stationPositionFile = svrOptDialog->stationPositionFile;
    exeDirectory = svrOptDialog->exeDirectory;
    localDirectory = svrOptDialog->localDirectory;
    proxyAddress = svrOptDialog->proxyAddress;
    stationId = svrOptDialog->stationId;
    stationSelect = svrOptDialog->stationSelect;
    antennaType = svrOptDialog->antennaType;
    receiverType = svrOptDialog->receiverType;
    logFile = svrOptDialog->logFile;
}
// callback on button-input-opt ---------------------------------------------
void MainForm::btnInputClicked()
{
    switch (cBInput->currentIndex()) {
        case 0: serialOptions(0, 0); break;
        case 1: tcpClientOptions(0, 1); break; // TCP Client
        case 2: tcpServerOptions(0, 2); break; // TCP Server
        case 3: ntripClientOptions(0, 3); break; // Ntrip Client
        case 4: udpServerOptions(0, 6); break;  // UDP Server
        case 5: fileOptions(0, 0); break;
    }
}
// callback on button-input-cmd ---------------------------------------------
void MainForm::btnCommandClicked()
{
    CmdOptDialog *cmdOptDialog = new CmdOptDialog(this);

    QPushButton *btn[] = { btnCmd, btnCmd1, btnCmd2, btnCmd3, btnCmd4, btnCmd5, btnCmd6 };
    QComboBox *type[] = { cBInput, cBOutput1, cBOutput2, cBOutput3, cBOutput4, cBOutput5, cBOutput6 };
    int j;
    int stream = -1;

    for (int i = 0; i < MAXSTR; i++)
        if (btn[i] == qobject_cast<QPushButton *>(sender()))
        {
            stream = i;
            break;
        }
    if (stream == -1) return;

    for (j = 0; j < 3; j++) {
        if (type[stream]->currentText() == tr("Serial")) {
            cmdOptDialog->commands[j] = commands[stream][j];
            cmdOptDialog->commandsEnabled[j] = commandsEnabled[stream][j];
        }
        else {
            cmdOptDialog->commands[j] = commandsTcp[stream][j];
            cmdOptDialog->commandsEnabled[j] = commandsEnabledTcp[stream][j];
        }
    }
    if (stream == 0) cmdOptDialog->setWindowTitle("Input Serial/TCP Commands");
    else cmdOptDialog->setWindowTitle(QString("Output%1 Serial/TCP Commands").arg(stream));

    cmdOptDialog->exec();
    if (cmdOptDialog->result() != QDialog::Accepted) return;

    for (j = 0; j < 3; j++) {
        if (type[stream]->currentText() == tr("Serial")) {
            commands[stream][j] = cmdOptDialog->commands[j];
            commandsEnabled[stream][j] = cmdOptDialog->commandsEnabled[j];
        }
        else {
            commandsTcp[stream][j] = cmdOptDialog->commands[j];
            commandsEnabledTcp[stream][j] = cmdOptDialog->commandsEnabled[j];
        }
    }

    delete cmdOptDialog;
}
// callback on button-output1-opt -------------------------------------------
void MainForm::btnOutputClicked()
{
    QPushButton *btn[] = {btnOutput1, btnOutput2, btnOutput3, btnOutput4, btnOutput5, btnOutput6};
    QComboBox *type[] = {cBOutput1, cBOutput2, cBOutput3, cBOutput4, cBOutput5, cBOutput6};
    int i;
    int stream = -1;

    for (i = 0; i < MAXSTR-1; i++) {
        if ((QPushButton *)sender() == btn[i])
        {
            stream = i;
            break;
        }
    }
    if (stream == -1) return;

    switch (type[stream]->currentIndex()) {
        case 1: serialOptions(stream+1, 0); break;
        case 2: tcpClientOptions(stream+1, 1); break;
        case 3: tcpServerOptions(stream+1, 2); break;
        case 4: ntripServerOptions(stream+1, 3); break;
        case 5: ntripCasterOptions(stream+1, 4); break;
        case 6: udpClientOptions(stream+1, 5); break;
        case 7: fileOptions(stream+1, 6); break;
    }
}
// callback on button-output-conv ------------------------------------------
void MainForm::btnConvertClicked()
{
    QPushButton *btn[] = {btnConv1, btnConv2, btnConv3, btnConv4, btnConv5, btnConv6};
    int i;
        int stream = -1;

    for (i = 0; i < MAXSTR-1; i++) {
        if ((QPushButton *)sender() == btn[i])
        {
            stream = i;
            break;
        }
    }
    if (stream == -1) return;

    ConvDialog *convDialog = new ConvDialog(this);

    convDialog->conversionEnabled = conversionEnabled[stream];
    convDialog->conversionInputFormat = conversionInputFormat[stream];
    convDialog->conversionOutputFormat = conversionOutputFormat[stream];
    convDialog->conversionMessage = conversionMessage[stream];
    convDialog->conversionOptions = conversionOptions[stream];
    convDialog->setWindowTitle(QString("Output%1 Conversion Options").arg(stream+1));

    convDialog->exec();
    if (convDialog->result() != QDialog::Accepted) return;

    conversionEnabled[stream] = convDialog->conversionEnabled;
    conversionInputFormat[stream] = convDialog->conversionInputFormat;
    conversionOutputFormat[stream] = convDialog->conversionOutputFormat;
    conversionMessage[stream] = convDialog->conversionMessage;
    conversionOptions[stream] = convDialog->conversionOptions;

    delete convDialog;
}
// callback on buttn-about --------------------------------------------------
void MainForm::btnAboutClicked()
{
    AboutDialog *aboutDialog = new AboutDialog(this);

    aboutDialog->aboutString = PRGNAME;
    aboutDialog->iconIndex = 6;
    aboutDialog->exec();

    delete aboutDialog;
}
// callback on task-icon ----------------------------------------------------
void MainForm::btnTaskIconClicked()
{
    setVisible(false);
    trayIcon->setVisible(true);
}
// callback on task-icon double-click ---------------------------------------
void MainForm::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason != QSystemTrayIcon::DoubleClick) return;

    setVisible(true);
    trayIcon->setVisible(false);
}
// callback on menu-expand --------------------------------------------------
void MainForm::menuExpandClicked()
{
    setVisible(true);
    trayIcon->setVisible(false);
}
// callback on stream-monitor -----------------------------------------------
void MainForm::btnStreamMonitorClicked()
{
    strMonDialog->show();
}
// callback on interval timer -----------------------------------------------
void MainForm::serverStatusTimerTimeout()
{
    QColor color[] = {Qt::red, Qt::white, CLORANGE, Qt::green, QColor(0x00, 0xff, 0x00), QColor(0xff, 0xff, 0x00)};
    QLabel *e0[] = {indInput, indOutput1, indOutput2, indOutput3, indOutput4, indOutput5, indOutput6};
    QLabel *e1[] = {lblInputByte, lblOutput1Byte, lblOutput2Byte, lblOutput3Byte, lblOutput4Byte, lblOutput5Byte, lblOutput6Byte};
    QLabel *e2[] = {lblInputBps, lblOutput1Bps, lblOutput2Bps, lblOutput3Bps, lblOutput4Bps, lblOutput5Bps, lblOutput6Bps};
    QLabel *e3[] = {indLog, indLog1, indLog2, indLog3, indLog4, indLog5, indLog6};
    QString statusStr[] = {tr("Error"), tr("Closed"), tr("Waiting..."), tr("Connected"), tr("Active")};
    gtime_t time = utc2gpst(timeget());
    int stat[MAXSTR] = { 0 }, byte[MAXSTR] = { 0 }, bps[MAXSTR] = { 0 }, log_stat[MAXSTR]={0};
    char msg[MAXSTRMSG * MAXSTR] = "", s1[256], s2[256];
    double ctime, t[4], pos;

    strsvrstat(&strsvr, stat, log_stat, byte, bps, msg);
    for (int i = 0; i < MAXSTR; i++) {
        num2cnum(byte[i], s1);
        num2cnum(bps[i], s2);
        e0[i]->setStyleSheet(QString("background-color: %1").arg(color2String(color[stat[i] + 1])));
        e0[i]->setToolTip(statusStr[stat[i]+1]);
        e1[i]->setText(s1);
        e2[i]->setText(s2);
        e3[i]->setStyleSheet(QString("background-color :%1").arg(color2String(color[log_stat[i]+1])));
        e3[i]->setToolTip(statusStr[log_stat[i]+1]);
    }
    pos = fmod(byte[0] / 1e3 / MAX(progressBarRange, 1), 1.0) * 110.0;
    pBProgress->setValue(!stat[0] ? 0 : MIN((int)pos, 100));

    time2str(time, s1, 0);
    lblTime->setText(QString(tr("%1 GPST")).arg(s1));

    if (Panel1->isEnabled())
        ctime = timediff(endTime, startTime);
    else
        ctime = timediff(time, startTime);
    ctime = floor(ctime);
    t[0] = floor(ctime / 86400.0); ctime -= t[0] * 86400.0;
    t[1] = floor(ctime / 3600.0); ctime -= t[1] * 3600.0;
    t[2] = floor(ctime / 60.0); ctime -= t[2] * 60.0;
    t[3] = ctime;
    lblConnectionTime->setText(QString("%1d %2:%3:%4").arg(t[0], 0, 'f', 0).arg(t[1], 2, 'f', 0, QChar('0')).arg(t[2], 2, 'f', 0, QChar('0')).arg(t[3], 2, 'f', 2, QChar('0')));

    num2cnum(byte[0], s1); num2cnum(bps[0], s2);
    trayIcon->setToolTip(QString(tr("%1 bytes %2 bps")).arg(s1).arg(s2));
    setTrayIcon(stat[0] <= 0 ? 0 : (stat[0] == 3 ? 2 : 1));

    lblMessage->setText(msg);
    lblMessage->setToolTip(msg);
}
// start stream server ------------------------------------------------------
void MainForm::serverStart(void)
{
    QComboBox *type[] = {cBInput, cBOutput1, cBOutput2, cBOutput3, cBOutput4, cBOutput5, cBOutput6};
    strconv_t *conv[MAXSTR-1] = {0};
    static char str1[MAXSTR][1024], str2[MAXSTR][1024];
    int itype[] = {
        STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPCLI, STR_UDPSVR,
        STR_FILE, STR_FTP, STR_HTTP
    };
    int otype[] = {
        STR_NONE, STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPSVR, STR_NTRIPCAS,
        STR_UDPCLI, STR_FILE
    };
    int streamTypes[MAXSTR] = {0}, opt[8] = {0};
    char *pths[MAXSTR], *logs[MAXSTR], *cmds[MAXSTR] = {0}, *cmds_periodic[MAXSTR] = {0};
    char filepath[1024];
    char *p;

    if (traceLevel > 0) {
        traceopen(logFile.isEmpty()?TRACEFILE:qPrintable(logFile));
        tracelevel(traceLevel);
    }
    for (int i = 0; i < MAXSTR; i++) {
        pths[i] = str1[i];
        logs[i] = str2[i];
    }

    streamTypes[0] = itype[type[0]->currentIndex()];
    strncpy(pths[0], qPrintable(paths[0][type[0]->currentIndex()]), 1024);
    strncpy(logs[0], type[0]->currentIndex()>5 || !pathEnabled[0]?"":qPrintable(pathLog[0]), 1024);

    for (int i = 1; i < MAXSTR; i++) {
        streamTypes[i] = otype[type[i]->currentIndex()];
        strncpy(pths[i], !type[i]->currentIndex() ? "" : qPrintable(paths[i][type[i]->currentIndex() - 1]), 1024);
        strncpy(logs[i], !pathEnabled[i] ? "" : qPrintable(pathLog[i]), 1024);
    }

    // get start and period commands
    for (int i = 0; i < MAXSTR; i++) {
        cmds[i] = cmds_periodic[i] = NULL;
        if (streamTypes[i] == STR_SERIAL) {
            cmds[i] = new char[1024];
            cmds_periodic[i] = new char[1024];
            if (commandsEnabled[i][0]) strncpy(cmds[i], qPrintable(commands[i][0]), 1024);
            if (commandsEnabled[i][2]) strncpy(cmds_periodic[i], qPrintable(commands[i][2]), 1024);
        }
        else if (streamTypes[i] == STR_TCPCLI || streamTypes[i] == STR_NTRIPCLI) {
            cmds[i] = new char[1024];
            cmds_periodic[i] = new char[1024];
            if (commandsEnabledTcp[i][0]) strncpy(cmds[i], qPrintable(commandsTcp[i][0]), 1024);
            if (commandsEnabledTcp[i][2]) strncpy(cmds_periodic[i], qPrintable(commandsTcp[i][2]), 1024);
        }
    }

    // fill server options
    for (int i = 0; i < 5; i++) {
        opt[i] = serverOptions[i];
    }
    opt[5] = nmeaRequest ? serverOptions[5] : 0; // set NMEA cycle time
    opt[6] = fileSwapMargin;
    opt[7] = relayBack;

    // check for file overwrite
    for (int i = 1; i < MAXSTR; i++) { // for each out stream
        if (streamTypes[i] != STR_FILE) continue;
        strncpy(filepath, pths[i], 1024);
        if (strstr(filepath, "::A")) continue;
        if ((p = strstr(filepath, "::"))) *p = '\0';
        if (!QFile::exists(filepath)) continue;
        if (QMessageBox::question(this, tr("Overwrite"), tr("File %1 exists.\nDo you want to overwrite?").arg(filepath)) != QMessageBox::Yes) return;
    }
    for (int i = 0; i < MAXSTR; i++) { // for each log stream
        if (!*logs[i]) continue;
        strncpy(filepath, logs[i], 1024);
        if (strstr(filepath, "::A")) continue;
        if ((p  =strstr(filepath, "::"))) *p = '\0';
        if (!QFile::exists(filepath)) continue;
        if (QMessageBox::question(this, tr("Overwrite"), tr("File %1 exists.\nDo you want to overwrite?").arg(filepath)) != QMessageBox::Yes) return;
    }

    strsetdir(qPrintable(localDirectory));
    strsetproxy(qPrintable(proxyAddress));

    // set up conversion if neccessary
    for (int i = 0; i < MAXSTR - 1; i++) { // for each out stream
        if (cBInput->currentIndex() == 2 || cBInput->currentIndex() == 4) continue;  // TCP/UDP server
        if (!conversionEnabled[i]) continue;
        if (!(conv[i] = strconvnew(conversionInputFormat[i], conversionOutputFormat[i], qPrintable(conversionMessage[i]),
                                   stationId, stationSelect, qPrintable(conversionOptions[i])))) continue;
        QStringList tokens = antennaType.split(',');
        if (tokens.size() >= 3)
        {
            strncpy(conv[i]->out.sta.antdes, qPrintable(tokens.at(0)), 64);
            strncpy(conv[i]->out.sta.antsno, qPrintable(tokens.at(1)), 64);
            conv[i]->out.sta.antsetup = atoi(qPrintable(tokens.at(2)));
        }
        tokens = receiverType.split(',');
        if (tokens.size() >= 3)
        {
            strncpy(conv[i]->out.sta.rectype, qPrintable(tokens.at(0)), 64);
            strncpy(conv[i]->out.sta.recver, qPrintable(tokens.at(1)), 64);
            strncpy(conv[i]->out.sta.recsno, qPrintable(tokens.at(2)), 64);
        }
        matcpy(conv[i]->out.sta.pos, antennaPosition, 3, 1);
        matcpy(conv[i]->out.sta.del, antennaOffsets, 3, 1);
    }
    // stream server start
    if (!strsvrstart(&strsvr, opt, streamTypes, pths, logs, conv, cmds, cmds_periodic, antennaPosition)) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (cmds[i]) delete[] cmds[i];
        if (cmds_periodic[i]) delete[] cmds_periodic[i];
    };

    startTime = utc2gpst(timeget());
    Panel1->setEnabled(false);
    btnStart->setVisible(false);
    btnStop->setVisible(true);
    btnOpt->setEnabled(false);
    btnExit->setEnabled(false);
    acMenuStart->setEnabled(false);
    acMenuStop->setEnabled(true);
    acMenuExit->setEnabled(false);
    setTrayIcon(1);

}
// stop stream server -------------------------------------------------------
void MainForm::serverStop(void)
{
    char *cmds[MAXSTR];
    QComboBox *type[]={cBInput, cBOutput1, cBOutput2, cBOutput3, cBOutput4, cBOutput5, cBOutput6};
    const int itype[] = {STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPCLI, STR_UDPSVR, STR_FILE,
                         STR_FTP, STR_HTTP};
    const int otype[] = {
        STR_NONE, STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPSVR, STR_NTRIPCAS,
        STR_FILE
    };
    int strs[MAXSTR];

    strs[0] = itype[cBInput->currentIndex()];
    for (int i = 1; i < MAXSTR; i++) {
        strs[1] = otype[type[i]->currentIndex()];
    }

    // get stop commands
    for (int i = 0; i < MAXSTR; i++) {
        cmds[i] = NULL;
        if (strs[i] == STR_SERIAL) {
            cmds[i] = new char[1024];
            if (commandsEnabled[i][1]) strncpy(cmds[i], qPrintable(commands[i][1]), 1024);
        } else if (strs[i] == STR_TCPCLI || strs[i] == STR_NTRIPCLI) {
            cmds[i] = new char[1024];
            if (commandsEnabledTcp[i][1]) strncpy(cmds[i], qPrintable(commandsTcp[i][1]), 1024);
        }
    }
    strsvrstop(&strsvr, cmds);

    endTime = utc2gpst(timeget());
    Panel1->setEnabled(true);
    btnStart->setVisible(true);
    btnStop->setVisible(false);
    btnOpt->setEnabled(true);
    btnExit->setEnabled(true);
    acMenuStart->setEnabled(true);
    acMenuStop->setEnabled(false);
    acMenuExit->setEnabled(true);

    setTrayIcon(0);

    for (int i = 0; i < MAXSTR - 1; i++) {
        if (cmds[i]) delete[] cmds[i];
        if (conversionEnabled[i]) strconvfree(strsvr.conv[i]);
    }
    if (traceLevel > 0) traceclose();
}
// callback on interval timer for stream monitor ----------------------------
void MainForm::streamMonitorTimerTimeout()
{
    const QString types[]={
        tr("None"), tr("Serial"), tr("File"), tr("TCP Server"), tr("TCP Client"), tr("UDP"), tr("Ntrip Sever"),
        tr("Ntrip Client"), tr("FTP"), tr("HTTP"), tr("Ntrip Cast"), tr("UDP Server"),
        tr("UDP Client")
    };
    const QString modes[]={tr("-"), tr("R"), tr("W"), tr("R/W")};
    const QString states[]={tr("ERR"), tr("-"), tr("WAIT"), tr("CONN")};
    unsigned char *msg = 0;
    char *p;
    int i, len, inb, inr, outb, outr;

    if (strMonDialog->streamFormat) {
        lock(&strsvr.lock);
        len = strsvr.npb;
        if (len > 0 && (msg = (unsigned char *)malloc(len))) {
            memcpy(msg, strsvr.pbuf, len);
            strsvr.npb = 0;
        }
        unlock(&strsvr.lock);
        if (len <= 0 || !msg) return;
        strMonDialog->addMessage(msg, len);
        free(msg);
    }
    else {
        if (!(msg = (unsigned char *)malloc(16000))) return;

        for (i = 0, p = (char*)msg; i < MAXSTR; i++) {
            p += sprintf(p, "[STREAM %d]\n", i);
            strsum(strsvr.stream + i, &inb, &inr, &outb, &outr);
            strstatx(strsvr.stream + i, p);
            p += strlen(p);
            if (inb > 0) {
                p += sprintf(p,"  inb     = %d\n", inb);
                p += sprintf(p,"  inr     = %d\n", inr);
            }
            if (outb > 0) {
                p += sprintf(p,"  outb    = %d\n", outb);
                p += sprintf(p,"  outr    = %d\n", outr);
            }
        }
        strMonDialog->addMessage(msg, strlen((char*)msg));

        free(msg);
    }
}
// set serial options -------------------------------------------------------
void MainForm::serialOptions(int index, int path)
{
    serialOptDialog->path = paths[index][path];
    serialOptDialog->options = (index == 0) ? 0 : 1;

    serialOptDialog->exec();
    if (serialOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = serialOptDialog->path;
}
// set tcp server options -------------------------------------------------------
void MainForm::tcpServerOptions(int index, int path)
{
    tcpOptDialog->path = paths[index][path];
    tcpOptDialog->showOptions = 0;

    tcpOptDialog->exec();
    if (tcpOptDialog->result()!=QDialog::Accepted) return;

    paths[index][path] = tcpOptDialog->path;
}
// set tcp client options ---------------------------------------------------
void MainForm::tcpClientOptions(int index, int path)
{
    tcpOptDialog->path = paths[index][path];
    tcpOptDialog->showOptions = 1;
    for (int i = 0; i < MAXHIST; i++) tcpOptDialog->history[i] = tcpHistory[i];

    tcpOptDialog->exec();
    if (tcpOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = tcpOptDialog->path;
    for (int i = 0; i < MAXHIST; i++) tcpHistory[i] = tcpOptDialog->history[i];
}
// set ntrip server options ---------------------------------------------------------
void MainForm::ntripServerOptions(int index, int path)
{
    tcpOptDialog->path = paths[index][path];
    tcpOptDialog->showOptions = 2;
    for (int i = 0; i < MAXHIST; i++) tcpOptDialog->history[i] = tcpHistory[i];

    tcpOptDialog->exec();
    if (tcpOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = tcpOptDialog->path;
    for (int i = 0; i < MAXHIST; i++) tcpHistory[i] = tcpOptDialog->history[i];
}
// set ntrip client options ---------------------------------------------------------
void MainForm::ntripClientOptions(int index, int path)
{
    tcpOptDialog->path = paths[index][path];
    tcpOptDialog->showOptions = 3;
    for (int i = 0; i < MAXHIST; i++) tcpOptDialog->history[i] = tcpHistory[i];

    tcpOptDialog->exec();
    if (tcpOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = tcpOptDialog->path;
    for (int i = 0; i < MAXHIST; i++) tcpHistory[i] = tcpOptDialog->history[i];
}
// set ntrip caster options ---------------------------------------------------------
void MainForm::ntripCasterOptions(int index, int path)
{
    tcpOptDialog->path = paths[index][path];
    tcpOptDialog->showOptions = 4;

    tcpOptDialog->exec();
    if (tcpOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = tcpOptDialog->path;
}
// set udp server options ---------------------------------------------------------
void MainForm::udpServerOptions(int index, int path)
{
    tcpOptDialog->path = paths[index][path];
    tcpOptDialog->showOptions = 6;

    tcpOptDialog->exec();
    if (tcpOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = tcpOptDialog->path;
}
// set udp client options ---------------------------------------------------------
void MainForm::udpClientOptions(int index, int path)
{
    tcpOptDialog->path = paths[index][path];
    tcpOptDialog->showOptions = 7;

    tcpOptDialog->exec();
    if (tcpOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = tcpOptDialog->path;
}
// set file options ---------------------------------------------------------
void MainForm::fileOptions(int index, int path)
{
    fileOptDialog->path = paths[index][path];
    fileOptDialog->setWindowTitle("File Options");
    fileOptDialog->options = (index == 0) ? 0 : 1;

    fileOptDialog->exec();
    if (fileOptDialog->result() != QDialog::Accepted) return;

    paths[index][path] = fileOptDialog->path;
}
// undate enable of widgets -------------------------------------------------
void MainForm::updateEnable(void)
{
    QComboBox *type[]={cBOutput1, cBOutput2, cBOutput3, cBOutput4, cBOutput5, cBOutput6};
    QLabel *label1[]={lblOutput1, lblOutput2, lblOutput3, lblOutput4, lblOutput5, lblOutput6};
    QLabel *label2[]={lblOutput1Byte, lblOutput2Byte, lblOutput3Byte, lblOutput4Byte, lblOutput5Byte, lblOutput6Byte};
    QLabel *label3[]={lblOutput1Bps, lblOutput2Bps, lblOutput3Bps, lblOutput4Bps, lblOutput5Bps, lblOutput6Bps};
    QPushButton *btn1[]={btnOutput1, btnOutput2, btnOutput3, btnOutput4, btnOutput5, btnOutput6};
    QPushButton *btn2[]={btnCmd1, btnCmd2, btnCmd3, btnCmd4, btnCmd5, btnCmd6};
    QPushButton *btn3[]={btnConv1, btnConv2, btnConv3, btnConv4, btnConv5, btnConv6};
    QPushButton *btn4[]={btnLog1, btnLog2, btnLog3, btnLog4, btnLog5, btnLog6};

    btnCmd->setEnabled(cBInput->currentIndex() < 2 || cBInput->currentIndex() == 3);
    for (int i = 0; i < MAXSTR - 1; i++) {
        label1[i]->setStyleSheet(QString("color :%1").arg(color2String(type[i]->currentIndex() > 0 ? Qt::black : Qt::gray)));
        label2[i]->setStyleSheet(QString("color :%1").arg(color2String(type[i]->currentIndex() > 0 ? Qt::black : Qt::gray)));
        label3[i]->setStyleSheet(QString("color :%1").arg(color2String(type[i]->currentIndex() > 0 ? Qt::black : Qt::gray)));
        btn1[i]->setEnabled(type[i]->currentIndex() > 0);
        btn2[i]->setEnabled(btn1[i]->isEnabled() && (type[i]->currentIndex() == 1 || type[i]->currentIndex() == 2));
        btn3[i]->setEnabled(btn1[i]->isEnabled() && cBInput->currentIndex() != 2 && cBInput->currentIndex() != 4);
        btn4[i]->setEnabled(btn1[i]->isEnabled() && (type[i]->currentIndex() == 1 || type[i]->currentIndex() == 2));
    }
}
// set task-tray icon -------------------------------------------------------
void MainForm::setTrayIcon(int index)
{
    QString icon[] = { ":/icons/tray0", ":/icons/tray1", ":/icons/tray2" };

    trayIcon->setIcon(QIcon(icon[index]));
}
// load options -------------------------------------------------------------
void MainForm::loadOptions(void)
{
    QSettings settings(iniFile, QSettings::IniFormat);
    QComboBox *type[]={cBOutput1, cBOutput2, cBOutput3, cBOutput4, cBOutput5, cBOutput6};
    int optdef[] = {10000, 10000, 1000, 32768, 10, 0};

    cBInput->setCurrentIndex(settings.value("set/input", 0).toInt());
    for (int i = 0; i < MAXSTR - 1; i++) {
        type[i]->setCurrentIndex(settings.value(QString("set/output%1").arg(i), 0).toInt());
    }
    traceLevel = settings.value("set/tracelevel", 0).toInt();
    nmeaRequest = settings.value("set/nmeareq", 0).toInt();
    fileSwapMargin = settings.value("set/fswapmargin", 30).toInt();
    relayBack = settings.value("set/relayback", 30).toInt();
    progressBarRange = settings.value("set/progbarrange", 30).toInt();
    stationId = settings.value("set/staid", 0).toInt();
    stationSelect = settings.value("set/stasel", 0).toInt();
    antennaType = settings.value("set/anttype", "").toString();
    receiverType = settings.value("set/rcvtype", "").toString();

    for (int i = 0; i < 6; i++)
        serverOptions[i] = settings.value(QString("set/svropt_%1").arg(i), optdef[i]).toInt();

    for (int i = 0; i < 3; i++) {
        antennaPosition[i] = settings.value(QString("set/antpos_%1").arg(i), 0.0).toDouble();
        antennaOffsets[i] = settings.value(QString("set/antoff_%1").arg(i), 0.0).toDouble();
    }
    for (int i = 0; i < MAXSTR - 1; i++) {
        conversionEnabled[i] = settings.value(QString("conv/ena_%1").arg(i), 0).toInt();
        conversionInputFormat[i] = settings.value(QString("conv/inp_%1").arg(i), 0).toInt();
        conversionOutputFormat[i] = settings.value(QString("conv/out_%1").arg(i), 0).toInt();
        conversionMessage[i] = settings.value(QString("conv/msg_%1").arg(i), "").toString();
        conversionOptions[i] = settings.value(QString("conv/opt_%1").arg(i), "").toString();
    }
    for (int i = 0; i < MAXSTR; i++)
        for (int j = 0; j < 2; j++) {
            commandsEnabled[i][j] = settings.value(QString("serial/cmdena_%1_%2").arg(i).arg(j), 1).toInt();
            commandsEnabledTcp[i][j] = settings.value(QString("tcpip/cmdena_%1_%2").arg(i).arg(j), 1).toInt();
        }
    for (int i = 0; i < MAXSTR; i++) for (int j = 0; j < 4; j++)
            paths[i][j] = settings.value(QString("path/path_%1_%2").arg(i).arg(j), "").toString();

    for (int i=0;i<MAXSTR;i++) {
        pathLog[i] = settings.value(QString("path/path_log_%1").arg(i), "").toString();
        pathEnabled[i] = settings.value(QString("path/path_ena_%1").arg(i), 0).toInt();
    }
    for (int i = 0; i < MAXSTR; i++)
        for (int j = 0; j < 2; j++) {
            commands[i][j] = settings.value(QString("serial/cmd_%1_%2").arg(i).arg(j), "").toString();
            commands[i][j] = commands[i][j].replace("@@", "\n");
        }
    for (int i = 0; i < MAXSTR; i++)
        for (int j = 0; j < 2; j++) {
            commandsTcp[i][j] = settings.value(QString("tcpip/cmd_%1_%2").arg(i).arg(j), "").toString();
            commandsTcp[i][j] = commandsTcp[i][j].replace("@@", "\n");
        }
    for (int i = 0; i < MAXHIST; i++)
        tcpHistory[i] = settings.value(QString("tcpopt/history%1").arg(i), "").toString();
    for (int i = 0; i < MAXHIST; i++)
        tcpMountpointHistory[i] = settings.value(QString("tcpopt/mntphist%1").arg(i), "").toString();
    stationPositionFile = settings.value("stapos/staposfile", "").toString();
    exeDirectory = settings.value("dirs/exedirectory", "").toString();
    localDirectory = settings.value("dirs/localdirectory", "").toString();
    proxyAddress = settings.value("dirs/proxyaddress", "").toString();
    logFile = settings.value("file/logfile", "").toString();

    updateEnable();
}
// save options--------------------------------------------------------------
void MainForm::saveOptions(void)
{
    QSettings settings(iniFile, QSettings::IniFormat);
    QComboBox *type[]={cBOutput1, cBOutput2, cBOutput3, cBOutput4, cBOutput5, cBOutput6};

    settings.setValue("set/input", cBInput->currentIndex());
    for (int i = 0; i < MAXSTR - 1; i++) {
        settings.setValue(QString("set/output%1").arg(i), type[i]->currentIndex());
    }
    settings.setValue("set/tracelevel", traceLevel);
    settings.setValue("set/nmeareq", nmeaRequest);
    settings.setValue("set/fswapmargin", fileSwapMargin);
    settings.setValue("set/relayback", relayBack);
    settings.setValue("set/progbarrange", progressBarRange);
    settings.setValue("set/staid", stationId);
    settings.setValue("set/stasel", stationSelect);
    settings.setValue("set/anttype", antennaType);
    settings.setValue("set/rcvtype", receiverType);

    for (int i = 0; i < 6; i++)
        settings.setValue(QString("set/svropt_%1").arg(i), serverOptions[i]);

    for (int i = 0; i < 3; i++) {
        settings.setValue(QString("set/antpos_%1").arg(i), antennaPosition[i]);
        settings.setValue(QString("set/antoff_%1").arg(i), antennaOffsets[i]);
    }
    for (int i = 0; i < MAXSTR - 1; i++) {
        settings.setValue(QString("conv/ena_%1").arg(i), conversionEnabled[i]);
        settings.setValue(QString("conv/inp_%1").arg(i), conversionInputFormat[i]);
        settings.setValue(QString("conv/out_%1").arg(i), conversionOutputFormat[i]);
        settings.setValue(QString("conv/msg_%1").arg(i), conversionMessage[i]);
        settings.setValue(QString("conv/opt_%1").arg(i), conversionOptions[i]);
    }
    for (int i = 0; i < MAXSTR; i++)
        for (int j = 0; j < 2; j++) {
            settings.setValue(QString("serial/cmdena_%1_%2").arg(i).arg(j), commandsEnabled[i][j]);
            settings.setValue(QString("tcpip/cmdena_%1_%2").arg(i).arg(j), commandsEnabledTcp[i][j]);
        }
    for (int i = 0; i < MAXSTR; i++) for (int j = 0; j < 4; j++)
            settings.setValue(QString("path/path_%1_%2").arg(i).arg(j), paths[i][j]);

    for (int i=0;i<MAXSTR;i++) {
        settings.setValue(QString("path/path_log_%1").arg(i), pathLog[i]);
        settings.setValue(QString("path/path_ena_%1").arg(i), pathEnabled[i]);
    }

    for (int i = 0; i < MAXSTR; i++)
        for (int j = 0; j < 2; j++) {
            commands[j][i] = commands[j][i].replace("\n", "@@");
            settings.setValue(QString("serial/cmd_%1_%2").arg(i).arg(j), commands[i][j]);
        }
    for (int i = 0; i < MAXSTR; i++)
        for (int j = 0; j < 2; j++) {
            commandsTcp[j][i] = commandsTcp[j][i].replace("\n", "@@");
            settings.setValue(QString("tcpip/cmd_%1_%2").arg(i).arg(j), commandsTcp[i][j]);
        }
    for (int i = 0; i < MAXHIST; i++)
        settings.setValue(QString("tcpopt/history%1").arg(i), tcpOptDialog->history[i]);

    settings.setValue("stapos/staposfile", stationPositionFile);
    settings.setValue("dirs/exedirectory", exeDirectory);
    settings.setValue("dirs/localdirectory", localDirectory);
    settings.setValue("dirs/proxyaddress", proxyAddress);
    settings.setValue("file/logfile",logFile);
}
//---------------------------------------------------------------------------
void MainForm::btnLogClicked()
{
    QPushButton *btn[] = {btnLog, btnLog1, btnLog2, btnLog3, btnLog4, btnLog5, btnLog6};
    int i, stream = -1;

    for (i = 0; i < MAXSTR; i++) {
        if ((QPushButton *)sender()==btn[i])
        {
            stream = i;
            break;
        }
    }
    if (stream == -1) return;

    fileOptDialog->path = pathLog[stream];
    fileOptDialog->pathEnabled = pathEnabled[stream];
    fileOptDialog->setWindowTitle((i == 0) ? tr("Input Log Options") : tr("Return Log Options"));
    fileOptDialog->options = 2;

    fileOptDialog->exec();
    if (fileOptDialog->result() != QDialog::Accepted) return;

    pathLog[stream] = fileOptDialog->path;
    pathEnabled[stream] = fileOptDialog->pathEnabled;
}
//---------------------------------------------------------------------------