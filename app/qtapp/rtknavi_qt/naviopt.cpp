//---------------------------------------------------------------------------
#include <QFileDialog>
#include <QLineEdit>
#include <QPoint>
#include <QFontDialog>
#include <QFont>
#include <QShowEvent>
#include <QFileSystemModel>
#include <QCompleter>

#include "rtklib.h"
#include "naviopt.h"
#include "viewer.h"
#include "refdlg.h"
#include "navimain.h"
#include "maskoptdlg.h"
#include "freqdlg.h"

//---------------------------------------------------------------------------
#define MAXSTR      1024                /* max length of a string */

// receiver options table ---------------------------------------------------
static int strtype[] = {                  /* stream types */
    STR_NONE, STR_NONE, STR_NONE, STR_NONE, STR_NONE, STR_NONE, STR_NONE, STR_NONE
};
static char strpath[8][MAXSTR] = { "" };        /* stream paths */
static int strfmt[] = {                         /* stream formats */
    STRFMT_RTCM3, STRFMT_RTCM3, STRFMT_SP3, SOLF_LLH, SOLF_NMEA, 0, 0, 0
};
static int svrcycle = 10;               /* server cycle (ms) */
static int timeout = 10000;             /* timeout time (ms) */
static int reconnect = 10000;           /* reconnect interval (ms) */
static int nmeacycle = 5000;            /* nmea request cycle (ms) */
static int fswapmargin = 30;            /* file swap marign (s) */
static int buffsize = 32768;            /* input buffer size (bytes) */
static int navmsgsel = 0;               /* navigation mesaage select */
static int nmeareq = 0;                 /* nmea request type (0:off,1:lat/lon,2:single) */
static double nmeapos[] = { 0, 0 };     /* nmea position (lat/lon) (deg) */
static char proxyaddr[MAXSTR] = "";     /* proxy address */

#define TIMOPT  "0:gpst,1:utc,2:jst,3:tow"
#define CONOPT  "0:dms,1:deg,2:xyz,3:enu,4:pyl"
#define FLGOPT  "0:off,1:std+2:age/ratio/ns"
#define ISTOPT  "0:off,1:serial,2:file,3:tcpsvr,4:tcpcli,7:ntripcli,8:ftp,9:http"
#define OSTOPT  "0:off,1:serial,2:file,3:tcpsvr,4:tcpcli,6:ntripsvr"
#define FMTOPT  "0:rtcm2,1:rtcm3,2:oem4,3:oem3,4:ubx,5:ss2,6:hemis,7:skytraq,8:gw10,9:javad,10:nvs,11:binex,12:rt17,13:sbf,14:cmr,17:sp3"
#define NMEOPT  "0:off,1:latlon,2:single"
#define SOLOPT  "0:llh,1:xyz,2:enu,3:nmea"
#define MSGOPT  "0:all,1:rover,2:base,3:corr"

static opt_t rcvopts[] = {
    { "inpstr1-type",     3, (void *)&strtype[0],  ISTOPT  },
    { "inpstr2-type",     3, (void *)&strtype[1],  ISTOPT  },
    { "inpstr3-type",     3, (void *)&strtype[2],  ISTOPT  },
    { "inpstr1-path",     2, (void *)strpath [0],  ""      },
    { "inpstr2-path",     2, (void *)strpath [1],  ""      },
    { "inpstr3-path",     2, (void *)strpath [2],  ""      },
    { "inpstr1-format",   3, (void *)&strfmt [0],  FMTOPT  },
    { "inpstr2-format",   3, (void *)&strfmt [1],  FMTOPT  },
    { "inpstr3-format",   3, (void *)&strfmt [2],  FMTOPT  },
    { "inpstr2-nmeareq",  3, (void *)&nmeareq,     NMEOPT  },
    { "inpstr2-nmealat",  1, (void *)&nmeapos[0],  "deg"   },
    { "inpstr2-nmealon",  1, (void *)&nmeapos[1],  "deg"   },
    { "outstr1-type",     3, (void *)&strtype[3],  OSTOPT  },
    { "outstr2-type",     3, (void *)&strtype[4],  OSTOPT  },
    { "outstr1-path",     2, (void *)strpath [3],  ""      },
    { "outstr2-path",     2, (void *)strpath [4],  ""      },
    { "outstr1-format",   3, (void *)&strfmt [3],  SOLOPT  },
    { "outstr2-format",   3, (void *)&strfmt [4],  SOLOPT  },
    { "logstr1-type",     3, (void *)&strtype[5],  OSTOPT  },
    { "logstr2-type",     3, (void *)&strtype[6],  OSTOPT  },
    { "logstr3-type",     3, (void *)&strtype[7],  OSTOPT  },
    { "logstr1-path",     2, (void *)strpath [5],  ""      },
    { "logstr2-path",     2, (void *)strpath [6],  ""      },
    { "logstr3-path",     2, (void *)strpath [7],  ""      },

    { "misc-svrcycle",    0, (void *)&svrcycle,    "ms"    },
    { "misc-timeout",     0, (void *)&timeout,     "ms"    },
    { "misc-reconnect",   0, (void *)&reconnect,   "ms"    },
    { "misc-nmeacycle",   0, (void *)&nmeacycle,   "ms"    },
    { "misc-buffsize",    0, (void *)&buffsize,    "bytes" },
    { "misc-navmsgsel",   3, (void *)&navmsgsel,   MSGOPT  },
    { "misc-proxyaddr",   2, (void *)proxyaddr,    ""      },
    { "misc-fswapmargin", 0, (void *)&fswapmargin, "s"     },

    { "",		      0, NULL,		       ""      }
};
//---------------------------------------------------------------------------
OptDialog::OptDialog(QWidget *parent)
    : QDialog(parent)
{
    QString label;
    int nglo = MAXPRNGLO, ngal = MAXPRNGAL, nqzs = MAXPRNQZS;
    int ncmp = MAXPRNCMP, nirn = MAXPRNIRN;

    setupUi(this);

    processOptions = prcopt_default;
    solutionOption = solopt_default;

    textViewer = new TextViewer(this);
    freqDialog = new FreqDialog(this);

    updateEnable();

    cBFrequency->clear();
    for (int i = 0; i < NFREQ; i++) {
        label="L1";
                for (int j=1;j<=i;j++) {
                    label+=QString("+%1").arg(j+1);
                }
        cBFrequency->addItem(label);
	}
    if (nglo <= 0) cBNavSys2->setEnabled(false);
    if (ngal <= 0) cBNavSys3->setEnabled(false);
    if (nqzs <= 0) cBNavSys4->setEnabled(false);
    if (ncmp <= 0) cBNavSys6->setEnabled(false);
    if (nirn <= 0) cBNavSys7->setEnabled(false);

    QCompleter *fileCompleter = new QCompleter(this);
    QFileSystemModel *fileModel = new QFileSystemModel(fileCompleter);
    fileModel->setRootPath("");
    fileCompleter->setModel(fileModel);
    lEStationPositionFile->setCompleter(fileCompleter);
    lEAntennaPcvFile->setCompleter(fileCompleter);
    lESatellitePcvFile->setCompleter(fileCompleter);
    lEDCBFile->setCompleter(fileCompleter);
    lEGeoidDataFile->setCompleter(fileCompleter);
    lEEOPFile->setCompleter(fileCompleter);
    lEOLFile->setCompleter(fileCompleter);

    QCompleter *dirCompleter = new QCompleter(this);
    QFileSystemModel *dirModel = new QFileSystemModel(dirCompleter);
    dirModel->setRootPath("");
    dirModel->setFilter(QDir::AllDirs | QDir::Drives | QDir::NoDotAndDotDot);
    dirCompleter->setModel(dirModel);
    lELocalDirectory->setCompleter(dirCompleter);

	updateEnable();

    connect(btnAntennaPcvFile, SIGNAL(clicked(bool)), this, SLOT(btnAntennaPcvFileClicked()));
    connect(btnAntennaPcvView, SIGNAL(clicked(bool)), this, SLOT(btnAntennaPcvViewClicked()));
    connect(btnCancel, SIGNAL(clicked(bool)), this, SLOT(reject()));
    connect(btnDCBFile, SIGNAL(clicked(bool)), this, SLOT(btnDCBFileClicked()));
    connect(btnEOPFile, SIGNAL(clicked(bool)), this, SLOT(btnEOPFileClicked()));
    connect(btnEOPView, SIGNAL(clicked(bool)), this, SLOT(btnEOPViewClicked()));
    connect(btnFont1, SIGNAL(clicked(bool)), this, SLOT(btnFont1Clicked()));
    connect(btnFont2, SIGNAL(clicked(bool)), this, SLOT(btnFont2Clicked()));
    connect(btnGeoidDataFile, SIGNAL(clicked(bool)), this, SLOT(btnGeoidDataFileClicked()));
    connect(btnLoad, SIGNAL(clicked(bool)), this, SLOT(btnLoadClicked()));
    connect(btnLocalDirectory, SIGNAL(clicked(bool)), this, SLOT(btnLocalDirectoryClicked()));
    connect(btnOk, SIGNAL(clicked(bool)), this, SLOT(btnOkClicked()));
//    connect(BtnOLFile,SIGNAL(clicked(bool)),this,SLOT(Btn));//Ocean Load
    connect(btnReferencePosition, SIGNAL(clicked(bool)), this, SLOT(btnReferencePositionClicked()));
    connect(btnRoverPosition, SIGNAL(clicked(bool)), this, SLOT(btnRoverPositionClicked()));
    connect(btnSatellitePcvFile, SIGNAL(clicked(bool)), this, SLOT(btnSatellitePcvFileClicked()));
    connect(btnSatellitePcvView, SIGNAL(clicked(bool)), this, SLOT(btnSatellitePcvViewClicked()));
    connect(btnSave, SIGNAL(clicked(bool)), this, SLOT(btnSaveClicked()));
    connect(btnSnrMask, SIGNAL(clicked(bool)), this, SLOT(btnSnrMaskClicked()));
    connect(btnStationPositionFile, SIGNAL(clicked(bool)), this, SLOT(btnStationPositionFileClicked()));
    connect(btnStationPositionView, SIGNAL(clicked(bool)), this, SLOT(btnStationPositionViewClicked()));
    connect(cBPositionMode, SIGNAL(currentIndexChanged(int)), this, SLOT(positionModeChanged(int)));
    connect(cBSolutionFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(solutionFormatChanged(int)));
    connect(cBReferencePositionTypeP, SIGNAL(currentIndexChanged(int)), this, SLOT(referencePositionTypePChanged(int)));
    connect(cBRoverPositionTypeP, SIGNAL(currentIndexChanged(int)), this, SLOT(roverPositionTypePChanged(int)));
    connect(cBAmbiguityResolutionGPS, SIGNAL(currentIndexChanged(int)), this, SLOT(ambiguityResolutionChanged(int)));
    connect(cBRoverAntennaPcv, SIGNAL(clicked(bool)), this, SLOT(roverAntennaPcvClicked()));
    connect(cBReferenceAntennaPcv, SIGNAL(clicked(bool)), this, SLOT(roverAntennaPcvClicked()));
    connect(cBOutputHeight, SIGNAL(currentIndexChanged(int)), this, SLOT(outputHeightClicked()));
    connect(cBNavSys2, SIGNAL(clicked(bool)), this, SLOT(navSys2Clicked()));
    connect(cBNavSys6, SIGNAL(clicked(bool)), this, SLOT(navSys6Clicked()));
    connect(cBBaselineConstrain, SIGNAL(clicked(bool)), this, SLOT(baselineConstrainClicked()));
    connect(btnFrequency, SIGNAL(clicked(bool)), this, SLOT(btnFrequencyClicked()));
    connect(cBReferenceAntenna, SIGNAL(currentIndexChanged(int)), this, SLOT(referenceAntennaClicked()));
    connect(cBRoverAntenna, SIGNAL(currentIndexChanged(int)), this, SLOT(roverAntennaClicked()));

}
//---------------------------------------------------------------------------
void OptDialog::showEvent(QShowEvent *event)
{
    if (event->spontaneous()) return;

	getOption();
}
//---------------------------------------------------------------------------
void OptDialog::btnOkClicked()
{
	setOption();

    accept();
}
//---------------------------------------------------------------------------
void OptDialog::btnLoadClicked()
{
    QString fileName;

    fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Load Options..."), QString(), tr("Options File (*.conf);;All (*.*)")));

    loadOption(fileName);
}
//---------------------------------------------------------------------------
void OptDialog::btnSaveClicked()
{
    QString file;

    file = QDir::toNativeSeparators(QFileDialog::getSaveFileName(this, tr("Save Options..."), QString(), tr("Options File (*.conf);;All (*.*)")));

    if (!file.contains('.')) file += ".conf";

    saveOption(file);
}
//---------------------------------------------------------------------------
void OptDialog::btnStationPositionViewClicked()
{
    if (lEStationPositionFile->text() == "") return;

    TextViewer *viewer = new TextViewer(this);
    viewer->show();

    viewer->read(lEStationPositionFile->text());
}
//---------------------------------------------------------------------------
void OptDialog::btnStationPositionFileClicked()
{
    QString fileName;

    fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Station Position File"), QString(), tr("Position File (*.pos);;All (*.*)")));

    lEStationPositionFile->setText(fileName);
}
//---------------------------------------------------------------------------
void OptDialog::btnSnrMaskClicked()
{
    MaskOptDialog maskOptDialog(this);
    
    maskOptDialog.mask = processOptions.snrmask;

    maskOptDialog.exec();
    if (maskOptDialog.result() != QDialog::Accepted) return;
    processOptions.snrmask = maskOptDialog.mask;
}
//---------------------------------------------------------------------------
void OptDialog::roverPositionTypePChanged(int)
{
    QLineEdit *edit[] = { lERoverPosition1, lERoverPosition2, lERoverPosition3 };
	double pos[3];

    getPosition(roverPositionTypeF, edit, pos);
    setPosition(cBRoverPositionTypeP->currentIndex(), edit, pos);
    roverPositionTypeF = cBRoverPositionTypeP->currentIndex();
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::referencePositionTypePChanged(int)
{
    QLineEdit *edit[] = { lEReferencePosition1, lEReferencePosition2, lEReferencePosition3 };
	double pos[3];

    getPosition(referencePositionTypeF, edit, pos);
    setPosition(cBReferencePositionTypeP->currentIndex(), edit, pos);
    referencePositionTypeF = cBReferencePositionTypeP->currentIndex();

	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::btnRoverPositionClicked()
{
    RefDialog refDialog(this);
    QLineEdit *edit[] = { lERoverPosition1, lERoverPosition2, lERoverPosition3 };
    double p[3], posi[3];

    getPosition(cBRoverPositionTypeP->currentIndex(), edit, p);
    ecef2pos(p, posi);
    
    refDialog.roverPosition[0] = posi[0] * R2D;
    refDialog.roverPosition[1] = posi[1] * R2D;
    refDialog.position[2] = posi[2];
    refDialog.stationPositionFile = lEStationPositionFile->text();
    refDialog.move(pos().x() + size().width() / 2 - refDialog.size().width() / 2,
               pos().y() + size().height() / 2 - refDialog.size().height() / 2);

    refDialog.exec();
    if (refDialog.result() != QDialog::Accepted) return;

    posi[0] = refDialog.position[0] * D2R;
    posi[1] = refDialog.position[1] * D2R;
    posi[2] = refDialog.position[2];

    pos2ecef(posi, p);
    setPosition(cBRoverPositionTypeP->currentIndex(), edit, p);
}
//---------------------------------------------------------------------------
void OptDialog::btnReferencePositionClicked()
{
    RefDialog refDialog(this);
    QLineEdit *edit[] = { lEReferencePosition1, lEReferencePosition2, lEReferencePosition3 };
    double p[3], posi[3];

    getPosition(cBReferencePositionTypeP->currentIndex(), edit, p);
    ecef2pos(p, posi);
    refDialog.roverPosition[0] = posi[0] * R2D;
    refDialog.roverPosition[1] = posi[1] * R2D;
    refDialog.roverPosition[2] = posi[2];
    refDialog.stationPositionFile = lEStationPositionFile->text();
    refDialog.move(pos().x() + size().width() / 2 - refDialog.size().width() / 2,
               pos().y() + size().height() / 2 - refDialog.size().height() / 2);

    refDialog.exec();
    if (refDialog.result() != QDialog::Accepted) return;

    posi[0] = refDialog.position[0] * D2R;
    posi[1] = refDialog.position[1] * D2R;
    posi[2] = refDialog.position[2];

    pos2ecef(posi, p);
    setPosition(cBReferencePositionTypeP->currentIndex(), edit, p);
}
//---------------------------------------------------------------------------
void OptDialog::btnSatellitePcvViewClicked()
{
    if (lESatellitePcvFile->text() == "") return;

    textViewer->show();

    textViewer->read(lESatellitePcvFile->text());
}
//---------------------------------------------------------------------------
void OptDialog::btnSatellitePcvFileClicked()
{
    lESatellitePcvFile->setText(QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Satellite Antenna PCV File"), QString(), tr("PCV File (*.pcv *.atx);Position File (*.pcv *.snx);All (*.*)"))));
}
//---------------------------------------------------------------------------
void OptDialog::btnAntennaPcvViewClicked()
{
    if (lEAntennaPcvFile->text() == "") return;

    textViewer->show();

    textViewer->read(lEAntennaPcvFile->text());
}
//---------------------------------------------------------------------------
void OptDialog::btnAntennaPcvFileClicked()
{
    lEAntennaPcvFile->setText(QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Receiver Antenna PCV File"), QString(), tr("PCV File (*.pcv *.atx);Position File (*.pcv *.snx);All (*.*)"))));
    readAntennaList();
}
//---------------------------------------------------------------------------
void OptDialog::btnGeoidDataFileClicked()
{
    QString fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Geoid Data File"), QString(), tr("All (*.*)")));

    lEGeoidDataFile->setText(fileName);
}
//---------------------------------------------------------------------------
void OptDialog::btnDCBFileClicked()
{
    QString fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("DCB Data File"), QString(), tr("DCB (*.dcb)")));

    lEDCBFile->setText(fileName);
}
//---------------------------------------------------------------------------
void OptDialog::btnEOPFileClicked()
{
    QString fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("EOP Data File"), QString(), tr("EOP (*.erp)")));

    lEEOPFile->setText(fileName);
}
//---------------------------------------------------------------------------
void OptDialog::btnEOPViewClicked()
{
    if (lEEOPFile->text() == "") return;

    textViewer->show();

    textViewer->read(lEEOPFile->text());
}
//---------------------------------------------------------------------------
void OptDialog::btnLocalDirectoryClicked()
{
    QString dir = lELocalDirectory->text();

    dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("FTP/HTTP Local Directory"), dir));
    lELocalDirectory->setText(dir);
}
//---------------------------------------------------------------------------
void OptDialog::btnFont1Clicked()
{
    QFontDialog dialog(this);

    dialog.setCurrentFont(fontLabel1->font());
    dialog.exec();

    fontLabel1->setFont(dialog.selectedFont());
    fontLabel1->setText(fontLabel1->font().family() + QString::number(fontLabel1->font().pointSize()) + " pt");
}
//---------------------------------------------------------------------------
void OptDialog::btnFont2Clicked()
{
    QFontDialog dialog(this);

    dialog.setCurrentFont(fontLabel2->font());
    dialog.exec();

    fontLabel2->setFont(dialog.selectedFont());
    fontLabel2->setText(fontLabel2->font().family() + QString::number(fontLabel2->font().pointSize()) + " pt");
}

//---------------------------------------------------------------------------
void OptDialog::frequencyChanged()
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::navSys2Clicked()
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::baselineConstrainClicked()
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::solutionFormatChanged(int)
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::positionModeChanged(int)
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::ambiguityResolutionChanged(int)
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::roverAntennaPcvClicked()
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::outputHeightClicked()
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::getOption(void)
{
    QLineEdit *editu[] = { lERoverPosition1, lERoverPosition2, lERoverPosition3 };
    QLineEdit *editr[] = { lEReferencePosition1, lEReferencePosition2, lEReferencePosition3 };

    cBPositionMode->setCurrentIndex(processOptions.mode);
    cBFrequency->setCurrentIndex(processOptions.nf - 1 > NFREQ - 1 ? NFREQ - 1 : processOptions.nf - 1);
    cBElevationMask->setCurrentIndex(cBElevationMask->findText(QString::number(processOptions.elmin * R2D)));
    cBDynamicModel->setCurrentIndex(processOptions.dynamics);
    cBTideCorrection->setCurrentIndex(processOptions.tidecorr);
    cBIonosphereOption->setCurrentIndex(processOptions.ionoopt);
    cBTroposphereOption->setCurrentIndex(processOptions.tropopt);
    cBSatelliteEphemeris->setCurrentIndex(processOptions.sateph);
    cBAmbiguityResolutionGPS->setCurrentIndex(processOptions.modear);
    cBAmbiguityResolutionGlo->setCurrentIndex(processOptions.glomodear);
    cBAmbiguityResolutionBDS->setCurrentIndex(processOptions.bdsmodear);
    sBValidThresAR->setValue(processOptions.thresar[0]);
    sBOutageCountResetAmbiguity->setValue(processOptions.maxout);
    sBLockCountFixAmbiguity->setValue(processOptions.minlock);
    sBFixCountHoldAmbiguity->setValue(processOptions.minfix);
    sBElevationMaskAR->setValue(processOptions.elmaskar * R2D);
    sBElevationMaskHold->setValue(processOptions.elmaskhold * R2D);
    sBMaxAgeDifference->setValue(processOptions.maxtdiff);
    sBRejectCode->setValue(processOptions.maxinno[0]);
    sBRejectPhase->setValue(processOptions.maxinno[1]);
    sBSlipThres->setValue(processOptions.thresslip);
    sBNumIteration->setValue(processOptions.niter);
    cBSyncSolution->setCurrentIndex(processOptions.syncsol);
    lEExcludedSatellites->setText(excludedSatellites);
    cBNavSys1->setChecked(processOptions.navsys & SYS_GPS);
    cBNavSys2->setChecked(processOptions.navsys & SYS_GLO);
    cBNavSys3->setChecked(processOptions.navsys & SYS_GAL);
    cBNavSys4->setChecked(processOptions.navsys & SYS_QZS);
    cBNavSys5->setChecked(processOptions.navsys & SYS_SBS);
    cBNavSys6->setChecked(processOptions.navsys & SYS_CMP);
    cBNavSys7->setChecked(processOptions.navsys & SYS_IRN);
    cBPositionOption1->setChecked(processOptions.posopt[0]);
    cBPositionOption2->setChecked(processOptions.posopt[1]);
    cBPositionOption3->setChecked(processOptions.posopt[2]);
    cBPositionOption4->setChecked(processOptions.posopt[3]);
    cBPositionOption5->setChecked(processOptions.posopt[4]);

    cBSolutionFormat->setCurrentIndex(solutionOption.posf);
    cBTimeFormat->setCurrentIndex(solutionOption.timef == 0 ? 0 : solutionOption.times + 1);
    sBTimeDecimal->setValue(solutionOption.timeu);
    cBLatLonFormat->setCurrentIndex(solutionOption.degf);
    lEFieldSeperator->setText(solutionOption.sep);
    cBOutputHeader->setCurrentIndex(solutionOption.outhead);
    cBOutputOption->setCurrentIndex(solutionOption.outopt);
    cBOutputVelocity->setCurrentIndex(solutionOption.outvel);
    cBOutputSingle->setCurrentIndex(processOptions.outsingle);
    sBMaxSolutionStd->setValue(solutionOption.maxsolstd);
    cBOutputDatum->setCurrentIndex(solutionOption.datum);
    cBOutputHeight->setCurrentIndex(solutionOption.height);
    cBOutputGeoid->setCurrentIndex(solutionOption.geoid);
    sBNmeaInterval1->setValue(solutionOption.nmeaintv[0]);
    sBNmeaInterval2->setValue(solutionOption.nmeaintv[1]);
    cBDebugStatus->setCurrentIndex(debugStatusF);
    cBDebugTrace->setCurrentIndex(debugTraceF);

    cBBaselineConstrain->setChecked(baselineC);
    sBBaselineLen->setValue(baseline[0]);
    sBBaselineSig->setValue(baseline[1]);

    sBMeasurementErrorR1->setValue(processOptions.eratio[0]);
    sBMeasurementErrorR2->setValue(processOptions.eratio[1]);
    sBMeasurementError2->setValue(processOptions.err[1]);
    sBMeasurementError3->setValue(processOptions.err[2]);
    sBMeasurementError4->setValue(processOptions.err[3]);
    sBMeasurementError5->setValue(processOptions.err[4]);
    lEProcessNoise1->setText(QString::number(processOptions.prn[0]));
    lEProcessNoise2->setText(QString::number(processOptions.prn[1]));
    lEProcessNoise3->setText(QString::number(processOptions.prn[2]));
    lEProcessNoise4->setText(QString::number(processOptions.prn[3]));
    lEProcessNoise5->setText(QString::number(processOptions.prn[4]));
    lESatelliteClockStability->setText(QString::number(processOptions.sclkstab));
    sBMaxAveEp->setValue(processOptions.maxaveep);
    cBInitRestart->setChecked(processOptions.initrst);

    cBRoverPositionTypeP->setCurrentIndex(roverPositionTypeF);
    cBReferencePositionTypeP->setCurrentIndex(referencePositionTypeF);
    cBRoverAntennaPcv->setChecked(roverAntennaPcvF);
    cBReferenceAntennaPcv->setChecked(referenceAntennaPcvF);
    sBRoverAntennaE->setValue(roverAntennaDelta[0]);
    sBRoverAntennaN->setValue(roverAntennaDelta[1]);
    sBRoverAntennaU->setValue(roverAntennaDelta[2]);
    sBReferenceAntennaE->setValue(referenceAntennaDelta[0]);
    sBReferenceAntennaN->setValue(referenceAntennaDelta[1]);
    sBReferenceAntennaU->setValue(referenceAntennaDelta[2]);
    setPosition(cBRoverPositionTypeP->currentIndex(), editu, roverPosition);
    setPosition(cBReferencePositionTypeP->currentIndex(), editr, referencePosition);

    lESatellitePcvFile->setText(satellitePcvFileF);
    lEAntennaPcvFile->setText(antennaPcvFileF);
    lEStationPositionFile->setText(stationPositionFileF);
    lEGeoidDataFile->setText(geoidDataFileF);
    lEDCBFile->setText(dcbFileF);
    lEEOPFile->setText(eopFileF);
    lELocalDirectory->setText(localDirectory);
	readAntennaList();

    cBRoverAntenna->setCurrentIndex(cBRoverAntenna->findText(roverAntennaF));
    cBReferenceAntenna->setCurrentIndex(cBReferenceAntenna->findText(referenceAntennaF));

    sBServerCycle->setValue(serverCycle);
    sBTimeoutTime->setValue(timeoutTime);
    sBReconnectTime->setValue(reconnectTime);
    sBNmeaCycle->setValue(nmeaCycle);
    sBFileSwapMargin->setValue(fileSwapMargin);
    sBServerBufferSize->setValue(serverBufferSize);
    sBSolutionBufferSize->setValue(solutionBufferSize);
    sBSavedSolution->setValue(savedSolution);
    cBNavSelect->setCurrentIndex(navSelect);
    sBSbasSatellite->setValue(processOptions.sbassatsel);
    lEProxyAddress->setText(proxyAddr);
    sBMonitorPort->setValue(monitorPort);
    sBSolutionBufferSize->setValue(solutionBufferSize);
    cBPanelStack->setCurrentIndex(panelStack);

    fontLabel1->setFont(panelFont);
    fontLabel1->setText(fontLabel1->font().family() + QString::number(fontLabel1->font().pointSize()) + "pt");
    fontLabel2->setFont(positionFont);
    fontLabel2->setText(fontLabel2->font().family() + QString::number(fontLabel2->font().pointSize()) + "pt");

	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::setOption(void)
{
    QString FieldSep_Text = lEFieldSeperator->text();
    QLineEdit *editu[] = { lERoverPosition1, lERoverPosition2, lERoverPosition3 };
    QLineEdit *editr[] = { lEReferencePosition1, lEReferencePosition2, lEReferencePosition3 };

    processOptions.mode = cBPositionMode->currentIndex();
    processOptions.nf = cBFrequency->currentIndex() + 1;
    processOptions.elmin = cBElevationMask->currentText().toDouble() * D2R;
    processOptions.dynamics = cBDynamicModel->currentIndex();
    processOptions.tidecorr = cBTideCorrection->currentIndex();
    processOptions.ionoopt = cBIonosphereOption->currentIndex();
    processOptions.tropopt = cBTroposphereOption->currentIndex();
    processOptions.sateph = cBSatelliteEphemeris->currentIndex();
    processOptions.modear = cBAmbiguityResolutionGPS->currentIndex();
    processOptions.glomodear = cBAmbiguityResolutionGlo->currentIndex();
    processOptions.bdsmodear = cBAmbiguityResolutionBDS->currentIndex();
    processOptions.thresar[0] = sBValidThresAR->value();
    processOptions.maxout = sBOutageCountResetAmbiguity->value();
    processOptions.minlock = sBLockCountFixAmbiguity->value();
    processOptions.minfix = sBFixCountHoldAmbiguity->value();
    processOptions.elmaskar = sBElevationMaskAR->value() * D2R;
    processOptions.elmaskhold = sBElevationMaskHold->value() * D2R;
    processOptions.maxtdiff = sBMaxAgeDifference->value();
    processOptions.maxinno[0] = sBRejectPhase->value();
    processOptions.maxinno[1] = sBRejectCode->value();
    processOptions.thresslip = sBSlipThres->value();
    processOptions.niter = sBNumIteration->value();
    processOptions.syncsol = cBSyncSolution->currentIndex();
    excludedSatellites = lEExcludedSatellites->text();
    processOptions.navsys = 0;

    if (cBNavSys1->isChecked()) processOptions.navsys |= SYS_GPS;
    if (cBNavSys2->isChecked()) processOptions.navsys |= SYS_GLO;
    if (cBNavSys3->isChecked()) processOptions.navsys |= SYS_GAL;
    if (cBNavSys4->isChecked()) processOptions.navsys |= SYS_QZS;
    if (cBNavSys5->isChecked()) processOptions.navsys |= SYS_SBS;
    if (cBNavSys6->isChecked()) processOptions.navsys |= SYS_CMP;
    if (cBNavSys7->isChecked()) processOptions.navsys |= SYS_IRN;
    processOptions.posopt[0] = cBPositionOption1->isChecked();
    processOptions.posopt[1] = cBPositionOption2->isChecked();
    processOptions.posopt[2] = cBPositionOption3->isChecked();
    processOptions.posopt[3] = cBPositionOption4->isChecked();
    processOptions.posopt[4] = cBPositionOption5->isChecked();

    solutionOption.posf = cBSolutionFormat->currentIndex();
    solutionOption.timef = cBTimeFormat->currentIndex() == 0 ? 0 : 1;
    solutionOption.times = cBTimeFormat->currentIndex() == 0 ? 0 : cBTimeFormat->currentIndex() - 1;
    solutionOption.timeu = static_cast<int>(sBTimeDecimal->value());
    solutionOption.degf = cBLatLonFormat->currentIndex();
    strcpy(solutionOption.sep, qPrintable(FieldSep_Text));
    solutionOption.outhead = cBOutputHeader->currentIndex();
    solutionOption.outopt = cBOutputOption->currentIndex();
    solutionOption.outvel = cBOutputVelocity->currentIndex();
    processOptions.outsingle = cBOutputSingle->currentIndex();
    solutionOption.maxsolstd = sBMaxSolutionStd->value();
    solutionOption.datum = cBOutputDatum->currentIndex();
    solutionOption.height = cBOutputHeight->currentIndex();
    solutionOption.geoid = cBOutputGeoid->currentIndex();
    solutionOption.nmeaintv[0] = sBNmeaInterval1->value();
    solutionOption.nmeaintv[1] = sBNmeaInterval2->value();
    debugStatusF = cBDebugStatus->currentIndex();
    debugTraceF = cBDebugTrace->currentIndex();

    baselineC = cBBaselineConstrain->isChecked();
    baseline[0] = sBBaselineLen->value();
    baseline[1] = sBBaselineSig->value();

    processOptions.eratio[0] = sBMeasurementErrorR1->value();
    processOptions.eratio[1] = sBMeasurementErrorR2->value();
    processOptions.err[1] = sBMeasurementError2->value();
    processOptions.err[2] = sBMeasurementError3->value();
    processOptions.err[3] = sBMeasurementError4->value();
    processOptions.err[4] = sBMeasurementError5->value();
    processOptions.prn[0] = lEProcessNoise1->text().toDouble();
    processOptions.prn[1] = lEProcessNoise2->text().toDouble();
    processOptions.prn[2] = lEProcessNoise3->text().toDouble();
    processOptions.prn[3] = lEProcessNoise4->text().toDouble();
    processOptions.prn[4] = lEProcessNoise5->text().toDouble();
    processOptions.sclkstab = lESatelliteClockStability->text().toDouble();
    processOptions.maxaveep = sBMaxAveEp->value();
    processOptions.initrst = cBInitRestart->isChecked();

    roverPositionTypeF = cBRoverPositionTypeP->currentIndex();
    referencePositionTypeF = cBReferencePositionTypeP->currentIndex();
    roverAntennaPcvF = cBRoverAntennaPcv->isChecked();
    referenceAntennaPcvF = cBReferenceAntennaPcv->isChecked();
    roverAntennaF = cBRoverAntenna->currentText();
    referenceAntennaF = cBReferenceAntenna->currentText();
    roverAntennaDelta[0] = sBRoverAntennaE->value();
    roverAntennaDelta[1] = sBRoverAntennaN->value();
    roverAntennaDelta[2] = sBRoverAntennaU->value();
    referenceAntennaDelta[0] = sBReferenceAntennaE->value();
    referenceAntennaDelta[1] = sBReferenceAntennaN->value();
    referenceAntennaDelta[2] = sBReferenceAntennaU->value();
    getPosition(cBRoverPositionTypeP->currentIndex(), editu, roverPosition);
    getPosition(cBReferencePositionTypeP->currentIndex(), editr, referencePosition);

    satellitePcvFileF = lESatellitePcvFile->text();
    antennaPcvFileF = lEAntennaPcvFile->text();
    stationPositionFileF = lEStationPositionFile->text();
    geoidDataFileF = lEGeoidDataFile->text();
    dcbFileF = lEDCBFile->text();
    eopFileF = lEEOPFile->text();
    localDirectory = lELocalDirectory->text();

    serverCycle = sBServerCycle->value();
    timeoutTime = sBTimeoutTime->value();
    reconnectTime = sBReconnectTime->value();
    nmeaCycle = sBNmeaCycle->value();
    fileSwapMargin = sBFileSwapMargin->value();
    serverBufferSize = sBServerBufferSize->value();
    solutionBufferSize = sBSolutionBufferSize->value();
    savedSolution = sBSavedSolution->value();
    navSelect = cBNavSelect->currentIndex();
    processOptions.sbassatsel = sBSbasSatellite->value();
    proxyAddr = lEProxyAddress->text();
    monitorPort = sBMonitorPort->value();
    panelStack = cBPanelStack->currentIndex();
    panelFont = fontLabel1->font();
    positionFont = fontLabel2->font();

	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::loadOption(const QString &file)
{
    int itype[] = { STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPCLI, STR_FILE, STR_FTP, STR_HTTP };
    int otype[] = { STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPSVR, STR_NTRIPCAS, STR_FILE };
    QLineEdit *editu[] = { lERoverPosition1, lERoverPosition2, lERoverPosition3 };
    QLineEdit *editr[] = { lEReferencePosition1, lEReferencePosition2, lEReferencePosition3 };
    QString buff;
    char id[32];
	int sat;
    prcopt_t prcopt = prcopt_default;
    solopt_t solopt = solopt_default;
    filopt_t filopt;

    memset(&filopt, 0, sizeof(filopt_t));

	resetsysopts();
    if (!loadopts(qPrintable(file), sysopts) ||
        !loadopts(qPrintable(file), rcvopts)) return;
    getsysopts(&prcopt, &solopt, &filopt);

    for (int i = 0; i < 8; i++) {
        mainForm->StreamC[i] = strtype[i] != STR_NONE;
        mainForm->Stream[i] = STR_NONE;
        for (int j = 0; j < (i < 3 ? 7 : 5); j++) {
            if (strtype[i] != (i < 3 ? itype[j] : otype[j])) continue;
            mainForm->Stream[i] = j;
			break;
		}
        if (i < 5) mainForm->Format[i] = strfmt[i];

        if (strtype[i] == STR_SERIAL)
            mainForm->Paths[i][0] = strpath[i];
        else if (strtype[i] == STR_FILE)
            mainForm->Paths[i][2] = strpath[i];
        else if (strtype[i] <= STR_NTRIPCLI)
            mainForm->Paths[i][1] = strpath[i];
        else if (strtype[i] <= STR_HTTP)
            mainForm->Paths[i][3] = strpath[i];
	}
    mainForm->NmeaReq = nmeareq;
    mainForm->nmeaPosition[0] = nmeapos[0];
    mainForm->nmeaPosition[1] = nmeapos[1];

    sBSbasSatellite->setValue(prcopt.sbassatsel);

    cBPositionMode->setCurrentIndex(prcopt.mode);
    cBFrequency->setCurrentIndex(prcopt.nf > NFREQ - 1 ? NFREQ - 1 : prcopt.nf - 1);
    cBSolution->setCurrentIndex(prcopt.soltype);
    cBElevationMask->setCurrentIndex(cBElevationMask->findText(QString::number(prcopt.elmin * R2D, 'f', 0)));
    cBDynamicModel->setCurrentIndex(prcopt.dynamics);
    cBTideCorrection->setCurrentIndex(prcopt.tidecorr);
    cBIonosphereOption->setCurrentIndex(prcopt.ionoopt);
    cBTroposphereOption->setCurrentIndex(prcopt.tropopt);
    cBSatelliteEphemeris->setCurrentIndex(prcopt.sateph);
    lEExcludedSatellites->setText("");
    for (sat = 1; sat <= MAXSAT; sat++) {
        if (!prcopt.exsats[sat - 1]) continue;
        satno2id(sat, id);
        buff += QString(buff.isEmpty() ? "" : " ") + (prcopt.exsats[sat - 1] == 2 ? "+" : "") + id;
	}
    lEExcludedSatellites->setText(buff);
    cBNavSys1->setChecked(prcopt.navsys & SYS_GPS);
    cBNavSys2->setChecked(prcopt.navsys & SYS_GLO);
    cBNavSys3->setChecked(prcopt.navsys & SYS_GAL);
    cBNavSys4->setChecked(prcopt.navsys & SYS_QZS);
    cBNavSys5->setChecked(prcopt.navsys & SYS_SBS);
    cBNavSys6->setChecked(prcopt.navsys & SYS_CMP);
    cBNavSys7->setChecked(prcopt.navsys & SYS_IRN);
    cBPositionOption1->setChecked(prcopt.posopt[0]);
    cBPositionOption2->setChecked(prcopt.posopt[1]);
    cBPositionOption3->setChecked(prcopt.posopt[2]);
    cBPositionOption4->setChecked(prcopt.posopt[3]);
    cBPositionOption5->setChecked(prcopt.posopt[4]);

    cBAmbiguityResolutionGPS->setCurrentIndex(prcopt.modear);
    cBAmbiguityResolutionGlo->setCurrentIndex(prcopt.glomodear);
    cBAmbiguityResolutionBDS->setCurrentIndex(prcopt.bdsmodear);
    sBValidThresAR->setValue(prcopt.thresar[0]);
    sBOutageCountResetAmbiguity->setValue(prcopt.maxout);
    sBFixCountHoldAmbiguity->setValue(prcopt.minfix);
    sBLockCountFixAmbiguity->setValue(prcopt.minlock);
    sBElevationMaskAR->setValue(prcopt.elmaskar * R2D);
    sBElevationMaskHold->setValue(prcopt.elmaskhold * R2D);
    sBMaxAgeDifference->setValue(prcopt.maxtdiff);
    sBRejectCode->setValue(prcopt.maxinno[0]);
    sBRejectPhase->setValue(prcopt.maxinno[1]);
    sBSlipThres->setValue(prcopt.thresslip);
    sBNumIteration->setValue(prcopt.niter);
    cBSyncSolution->setCurrentIndex(prcopt.syncsol);
    sBBaselineLen->setValue(prcopt.baseline[0]);
    sBBaselineSig->setValue(prcopt.baseline[1]);
    cBBaselineConstrain->setChecked(prcopt.baseline[0] > 0.0);

    cBSolutionFormat->setCurrentIndex(solopt.posf);
    cBTimeFormat->setCurrentIndex(solopt.timef == 0 ? 0 : solopt.times + 1);
    sBTimeDecimal->setValue(solopt.timeu);
    cBLatLonFormat->setCurrentIndex(solopt.degf);
    lEFieldSeperator->setText(solopt.sep);
    cBOutputHeader->setCurrentIndex(solopt.outhead);
    cBOutputOption->setCurrentIndex(solopt.outopt);
    cBOutputVelocity->setCurrentIndex(solopt.outvel);
    cBOutputSingle->setCurrentIndex(prcopt.outsingle);
    sBMaxSolutionStd->setValue(solopt.maxsolstd);
    cBOutputDatum->setCurrentIndex(solopt.datum);
    cBOutputHeight->setCurrentIndex(solopt.height);
    cBOutputGeoid->setCurrentIndex(solopt.geoid);
    sBNmeaInterval1->setValue(solopt.nmeaintv[0]);
    sBNmeaInterval2->setValue(solopt.nmeaintv[1]);
    cBDebugTrace->setCurrentIndex(solopt.trace);
    cBDebugStatus->setCurrentIndex(solopt.sstat);

    sBMeasurementErrorR1->setValue(prcopt.eratio[0]);
    sBMeasurementErrorR2->setValue(prcopt.eratio[1]);
    sBMeasurementError2->setValue(prcopt.err[1]);
    sBMeasurementError3->setValue(prcopt.err[2]);
    sBMeasurementError4->setValue(prcopt.err[3]);
    sBMeasurementError5->setValue(prcopt.err[4]);
    lESatelliteClockStability->setText(QString::number(prcopt.sclkstab));
    lEProcessNoise1->setText(QString::number(prcopt.prn[0]));
    lEProcessNoise2->setText(QString::number(prcopt.prn[1]));
    lEProcessNoise3->setText(QString::number(prcopt.prn[2]));
    lEProcessNoise4->setText(QString::number(prcopt.prn[3]));
    lEProcessNoise5->setText(QString::number(prcopt.prn[4]));

    cBRoverAntennaPcv->setChecked(*prcopt.anttype[0]);
    cBReferenceAntennaPcv->setChecked(*prcopt.anttype[1]);
    cBRoverAntenna->setCurrentIndex(cBRoverAntenna->findText(prcopt.anttype[0]));
    cBReferenceAntenna->setCurrentIndex(cBReferenceAntenna->findText(prcopt.anttype[1]));
    sBRoverAntennaE->setValue(prcopt.antdel[0][0]);
    sBRoverAntennaN->setValue(prcopt.antdel[0][1]);
    sBRoverAntennaU->setValue(prcopt.antdel[0][2]);
    sBReferenceAntennaE->setValue(prcopt.antdel[1][0]);
    sBReferenceAntennaN->setValue(prcopt.antdel[1][1]);
    sBReferenceAntennaU->setValue(prcopt.antdel[1][2]);
    sBMaxAveEp->setValue(prcopt.maxaveep);

    cBRoverPositionTypeP->setCurrentIndex(0);
    cBReferencePositionTypeP->setCurrentIndex(0);
    if      (prcopt.refpos==POSOPT_RTCM  ) cBReferencePositionTypeP->setCurrentIndex(3);
    else if (prcopt.refpos==POSOPT_SINGLE) cBReferencePositionTypeP->setCurrentIndex(4);

    roverPositionTypeF = cBRoverPositionTypeP->currentIndex();
    referencePositionTypeF = cBReferencePositionTypeP->currentIndex();
    setPosition(cBRoverPositionTypeP->currentIndex(), editu, prcopt.ru);
    setPosition(cBReferencePositionTypeP->currentIndex(), editr, prcopt.rb);

    lESatellitePcvFile->setText(filopt.satantp);
    lEAntennaPcvFile->setText(filopt.rcvantp);
    lEStationPositionFile->setText(filopt.stapos);
    lEGeoidDataFile->setText(filopt.geoid);
    lEDCBFile->setText(filopt.dcb);
    lELocalDirectory->setText(filopt.tempdir);

	readAntennaList();
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::saveOption(const QString &file)
{
    QString ProxyAddrE_Text = lEProxyAddress->text();
    QString ExSatsE_Text = lEExcludedSatellites->text();
    QString FieldSep_Text = lEFieldSeperator->text();
    QString RovAnt_Text = cBRoverAntenna->currentText(), RefAnt_Text = cBReferenceAntenna->currentText();
    QString SatPcvFile_Text = lESatellitePcvFile->text();
    QString AntPcvFile_Text = lEAntennaPcvFile->text();
    QString StaPosFile_Text = lEStationPositionFile->text();
    QString GeoidDataFile_Text = lEGeoidDataFile->text();
    QString DCBFile_Text = lEDCBFile->text();
    QString LocalDir_Text = lELocalDirectory->text();
    int itype[] = { STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPCLI, STR_FILE, STR_FTP, STR_HTTP };
    int otype[] = { STR_SERIAL, STR_TCPCLI, STR_TCPSVR, STR_NTRIPSVR, STR_NTRIPCAS, STR_FILE };
    QLineEdit *editu[] = { lERoverPosition1, lERoverPosition2, lERoverPosition3 };
    QLineEdit *editr[] = { lEReferencePosition1, lEReferencePosition2, lEReferencePosition3 };
    char buff[1024], *p, *q, comment[256], s[64];
    int sat, ex;
    prcopt_t prcopt = prcopt_default;
    solopt_t solopt = solopt_default;
    filopt_t filopt;

    memset(&filopt, 0, sizeof(filopt_t));

    for (int i = 0; i < 8; i++) {
        strtype[i] = i < 3 ? itype[mainForm->Stream[i]] : otype[mainForm->Stream[i]];
        strfmt[i] = mainForm->Format[i];

        if (!mainForm->StreamC[i]) {
            strtype[i] = STR_NONE;
            strcpy(strpath[i], "");
        } else if (strtype[i] == STR_SERIAL) {
            strcpy(strpath[i], qPrintable(mainForm->Paths[i][0]));
        } else if (strtype[i] == STR_FILE) {
            strcpy(strpath[i], qPrintable(mainForm->Paths[i][2]));
        } else if (strtype[i] == STR_TCPSVR) {
            strcpy(buff,qPrintable(mainForm->Paths[i][1]));
            if ((p=strchr(buff,'/'))) *p='\0'; // TODO
            if ((p=strrchr(buff,':'))) {
                strcpy(strpath[i],p);
            }
            else {
                strcpy(strpath[i],"");
            }
        }
        else if (strtype[i]==STR_TCPCLI) {
            strcpy(buff,qPrintable(mainForm->Paths[i][1]));
            if ((p=strchr(buff,'/'))) *p='\0';
            if ((p=strrchr(buff,'@'))) {
                strcpy(strpath[i],p+1);
            }
            else {
                strcpy(strpath[i],buff);
            }
        }
        else if (strtype[i]==STR_NTRIPSVR) {
            strcpy(buff,qPrintable(mainForm->Paths[i][1]));
            if ((p=strchr(buff,':'))&&strchr(p+1,'@')) {
                strcpy(strpath[i],p);
            }
            else {
                strcpy(strpath[i],buff);
            }
        }
        else if (strtype[i]==STR_NTRIPCLI) {
            strcpy(buff,qPrintable(mainForm->Paths[i][1]));
            if ((p=strchr(buff,'/'))&&(q=strchr(p+1,':'))) *q='\0';
            strcpy(strpath[i],buff);
        }
        else if (strtype[i]==STR_NTRIPCAS) {
            strcpy(buff,qPrintable(mainForm->Paths[i][1]));
            if ((p=strchr(buff,'/'))&&(q=strchr(p+1,':'))) *q='\0';
            if ((p=strchr(buff,'@'))) {
                *(p+1)='\0';
                strcpy(strpath[i],buff);
            }
            if ((p=strchr(p?p+2:buff,':'))) {
                strcat(strpath[i],p);
            }
        } else if (strtype[i]==STR_FTP||strtype[i]==STR_HTTP)
        {
            strcpy(strpath[i], qPrintable(mainForm->Paths[i][3]));
		}
	}
    nmeareq = mainForm->NmeaReq;
    nmeapos[0] = mainForm->nmeaPosition[0];
    nmeapos[1] = mainForm->nmeaPosition[1];

    svrcycle = sBServerCycle->value();
    timeout = sBTimeoutTime->value();
    reconnect = sBReconnectTime->value();
    nmeacycle = sBNmeaCycle->value();
    buffsize = sBServerBufferSize->value();
    navmsgsel = cBNavSelect->currentIndex();
    strcpy(proxyaddr, qPrintable(ProxyAddrE_Text));
    fswapmargin = sBFileSwapMargin->value();
    prcopt.sbassatsel = sBSbasSatellite->value();

    prcopt.mode = cBPositionMode->currentIndex();
    prcopt.nf = cBFrequency->currentIndex() + 1;
    prcopt.soltype = cBSolution->currentIndex();
    prcopt.elmin = cBElevationMask->currentText().toDouble() * D2R;
    prcopt.dynamics = cBDynamicModel->currentIndex();
    prcopt.tidecorr = cBTideCorrection->currentIndex();
    prcopt.ionoopt = cBIonosphereOption->currentIndex();
    prcopt.tropopt = cBTroposphereOption->currentIndex();
    prcopt.sateph = cBSatelliteEphemeris->currentIndex();
    if (lEExcludedSatellites->text() != "") {
        strcpy(buff, qPrintable(ExSatsE_Text));
        for (p = strtok(buff, " "); p; p = strtok(NULL, " ")) {
            if (*p == '+') {
                ex = 2; p++;
            } else {
                ex = 1;
            }
            if (!(sat = satid2no(p))) continue;
            prcopt.exsats[sat - 1] = (unsigned char)ex;
		}
	}
    prcopt.navsys = (cBNavSys1->isChecked() ? SYS_GPS : 0) |
            (cBNavSys2->isChecked() ? SYS_GLO : 0) |
            (cBNavSys3->isChecked() ? SYS_GAL : 0) |
            (cBNavSys4->isChecked() ? SYS_QZS : 0) |
            (cBNavSys5->isChecked() ? SYS_SBS : 0) |
            (cBNavSys6->isChecked() ? SYS_CMP : 0) |
            (cBNavSys7->isChecked() ? SYS_IRN : 0);
    prcopt.posopt[0] = cBPositionOption1->isChecked();
    prcopt.posopt[1] = cBPositionOption2->isChecked();
    prcopt.posopt[2] = cBPositionOption3->isChecked();
    prcopt.posopt[3] = cBPositionOption4->isChecked();
    prcopt.posopt[4] = cBPositionOption5->isChecked();

    prcopt.modear = cBAmbiguityResolutionGPS->currentIndex();
    prcopt.glomodear = cBAmbiguityResolutionGlo->currentIndex();
    prcopt.bdsmodear = cBAmbiguityResolutionBDS->currentIndex();
    prcopt.thresar[0] = sBValidThresAR->value();
    prcopt.maxout = sBOutageCountResetAmbiguity->value();
    prcopt.minfix = sBFixCountHoldAmbiguity->value();
    prcopt.minlock = sBLockCountFixAmbiguity->value();
    prcopt.elmaskar = sBElevationMaskAR->value() * D2R;
    prcopt.elmaskhold = sBElevationMaskHold->value() * D2R;
    prcopt.maxtdiff = sBMaxAgeDifference->value();
    prcopt.maxinno[0] = sBRejectCode->value();
    prcopt.maxinno[1] = sBRejectPhase->value();
    prcopt.thresslip = sBSlipThres->value();
    prcopt.niter = sBNumIteration->value();
    prcopt.syncsol = cBSyncSolution->currentIndex();
    if (prcopt.mode == PMODE_MOVEB && cBBaselineConstrain->isChecked()) {
        prcopt.baseline[0] = sBBaselineLen->value();
        prcopt.baseline[1] = sBBaselineSig->value();
	}
    solopt.posf = cBSolutionFormat->currentIndex();
    solopt.timef = cBTimeFormat->currentIndex() == 0 ? 0 : 1;
    solopt.times = cBTimeFormat->currentIndex() == 0 ? 0 : cBTimeFormat->currentIndex() - 1;
    solopt.timeu = sBTimeDecimal->value();
    solopt.degf = cBLatLonFormat->currentIndex();
    strcpy(solopt.sep, qPrintable(FieldSep_Text));
    solopt.outhead = cBOutputHeader->currentIndex();
    solopt.outopt = cBOutputOption->currentIndex();
    solopt.outvel = cBOutputVelocity->currentIndex();
    prcopt.outsingle = cBOutputSingle->currentIndex();
    solopt.maxsolstd = sBMaxSolutionStd->value();
    solopt.datum = cBOutputDatum->currentIndex();
    solopt.height = cBOutputHeight->currentIndex();
    solopt.geoid = cBOutputGeoid->currentIndex();
    solopt.nmeaintv[0] = sBNmeaInterval1->value();
    solopt.nmeaintv[1] = sBNmeaInterval2->value();
    solopt.trace = cBDebugTrace->currentIndex();
    solopt.sstat = cBDebugStatus->currentIndex();

    prcopt.eratio[0] = sBMeasurementErrorR1->value();
    prcopt.eratio[1] = sBMeasurementErrorR2->value();
    prcopt.err[1] = sBMeasurementError2->value();
    prcopt.err[2] = sBMeasurementError3->value();
    prcopt.err[3] = sBMeasurementError4->value();
    prcopt.err[4] = sBMeasurementError5->value();
    prcopt.sclkstab = lESatelliteClockStability->text().toDouble();
    prcopt.prn[0] = lEProcessNoise1->text().toDouble();
    prcopt.prn[1] = lEProcessNoise2->text().toDouble();
    prcopt.prn[2] = lEProcessNoise3->text().toDouble();
    prcopt.prn[3] = lEProcessNoise4->text().toDouble();
    prcopt.prn[4] = lEProcessNoise5->text().toDouble();

    if (cBRoverAntennaPcv->isChecked()) strcpy(prcopt.anttype[0], qPrintable(RovAnt_Text));
    if (cBReferenceAntennaPcv->isChecked()) strcpy(prcopt.anttype[1], qPrintable(RefAnt_Text));
    prcopt.antdel[0][0] = sBRoverAntennaE->value();
    prcopt.antdel[0][1] = sBRoverAntennaN->value();
    prcopt.antdel[0][2] = sBRoverAntennaU->value();
    prcopt.antdel[1][0] = sBReferenceAntennaE->value();
    prcopt.antdel[1][1] = sBReferenceAntennaN->value();
    prcopt.antdel[1][2] = sBReferenceAntennaU->value();
    prcopt.maxaveep = sBMaxAveEp->value();
    prcopt.initrst = cBInitRestart->isChecked();

    prcopt.rovpos = 4;
    prcopt.refpos = 4;
    if      (cBReferencePositionTypeP->currentIndex()==3) prcopt.refpos=POSOPT_RTCM;
    else if (cBReferencePositionTypeP->currentIndex()==4) prcopt.refpos=POSOPT_SINGLE;

    if (prcopt.rovpos == POSOPT_POS) getPosition(cBRoverPositionTypeP->currentIndex(), editu, prcopt.ru);
    if (prcopt.refpos == POSOPT_POS) getPosition(cBReferencePositionTypeP->currentIndex(), editr, prcopt.rb);

    strcpy(filopt.satantp, qPrintable(SatPcvFile_Text));
    strcpy(filopt.rcvantp, qPrintable(AntPcvFile_Text));
    strcpy(filopt.stapos, qPrintable(StaPosFile_Text));
    strcpy(filopt.geoid, qPrintable(GeoidDataFile_Text));
    strcpy(filopt.dcb, qPrintable(DCBFile_Text));
    strcpy(filopt.tempdir, qPrintable(LocalDir_Text));

    time2str(utc2gpst(timeget()), s, 0);
    sprintf(comment, qPrintable(tr("RTKNAVI options (%s, v.%s)")), s, VER_RTKLIB);
    setsysopts(&prcopt, &solopt, &filopt);
    if (!saveopts(qPrintable(file), "w", comment, sysopts) ||
        !saveopts(qPrintable(file), "a", "", rcvopts)) return;
}
//---------------------------------------------------------------------------
void OptDialog::updateEnable(void)
{
    bool rel = PMODE_DGPS <= cBPositionMode->currentIndex() && cBPositionMode->currentIndex() <= PMODE_FIXED;
    bool rtk = PMODE_KINEMA <= cBPositionMode->currentIndex() && cBPositionMode->currentIndex() <= PMODE_FIXED;
    bool ppp = cBPositionMode->currentIndex() >= PMODE_PPP_KINEMA;
    bool ar = rtk || ppp;

    cBFrequency->setEnabled(rel);
    cBSolution->setEnabled(false);
    cBDynamicModel->setEnabled(rel);
    cBTideCorrection->setEnabled(rel || ppp);
    cBPositionOption1->setEnabled(ppp);
    cBPositionOption2->setEnabled(ppp);
    cBPositionOption3->setEnabled(ppp);
    cBPositionOption4->setEnabled(ppp);

    cBAmbiguityResolutionGPS->setEnabled(ar);
    cBAmbiguityResolutionGlo->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() > 0 && cBNavSys2->isChecked());
    cBAmbiguityResolutionBDS->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() > 0 && cBNavSys6->isChecked());
    sBValidThresAR->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() >= 1 && cBAmbiguityResolutionGPS->currentIndex() < 4);
    sBThresAR2->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() >= 4);
    sBThresAR3->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() >= 4);
    sBLockCountFixAmbiguity->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() >= 1);
    sBElevationMaskAR->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() >= 1);
    sBOutageCountResetAmbiguity->setEnabled(ar || ppp);
    sBFixCountHoldAmbiguity->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() == 3);
    sBElevationMaskHold->setEnabled(ar && cBAmbiguityResolutionGPS->currentIndex() == 3);
    sBSlipThres->setEnabled(ar || ppp);
    sBMaxAgeDifference->setEnabled(rel);
    sBRejectPhase->setEnabled(rel || ppp);
    sBNumIteration->setEnabled(rel || ppp);
    cBSyncSolution->setEnabled(rel || ppp);
    cBBaselineConstrain->setEnabled(cBPositionMode->currentIndex() == PMODE_MOVEB);
    sBBaselineLen->setEnabled(cBBaselineConstrain->isChecked() && cBPositionMode->currentIndex() == PMODE_MOVEB);
    sBBaselineSig->setEnabled(cBBaselineConstrain->isChecked() && cBPositionMode->currentIndex() == PMODE_MOVEB);

    cBOutputHeader->setEnabled(cBSolutionFormat->currentIndex() != 3);
    cBOutputOption->setEnabled(false);
    cBTimeFormat->setEnabled(cBSolutionFormat->currentIndex() != 3);
    sBTimeDecimal->setEnabled(cBSolutionFormat->currentIndex() != 3);
    cBLatLonFormat->setEnabled(cBSolutionFormat->currentIndex() == 0);
    lEFieldSeperator->setEnabled(cBSolutionFormat->currentIndex() != 3);
    cBOutputSingle->setEnabled(cBPositionMode->currentIndex() != 0);
    cBOutputDatum->setEnabled(cBSolutionFormat->currentIndex() == 0);
    cBOutputHeight->setEnabled(cBSolutionFormat->currentIndex() == 0);
    cBOutputGeoid->setEnabled(cBSolutionFormat->currentIndex() == 0 && cBOutputHeight->currentIndex() == 1);

    cBRoverAntennaPcv->setEnabled(rel || ppp);
    cBRoverAntenna->setEnabled((rel || ppp) && cBRoverAntennaPcv->isChecked());
    sBRoverAntennaE->setEnabled((rel || ppp) && cBRoverAntennaPcv->isChecked()&&cBRoverAntenna->currentText()!="*");
    sBRoverAntennaN->setEnabled((rel || ppp) && cBRoverAntennaPcv->isChecked()&&cBRoverAntenna->currentText()!="*");
    sBRoverAntennaU->setEnabled((rel || ppp) && cBRoverAntennaPcv->isChecked()&&cBRoverAntenna->currentText()!="*");
    lblRoverAntennaD->setEnabled((rel || ppp) && cBRoverAntennaPcv->isChecked()&&cBRoverAntenna->currentText()!="*");
    cBReferenceAntennaPcv->setEnabled(rel);
    cBReferenceAntenna->setEnabled(rel && cBReferenceAntennaPcv->isChecked());
    sBReferenceAntennaE->setEnabled(rel && cBReferenceAntennaPcv->isChecked()&&cBReferenceAntenna->currentText()!="*");
    sBReferenceAntennaN->setEnabled(rel && cBReferenceAntennaPcv->isChecked()&&cBReferenceAntenna->currentText()!="*");
    sBReferenceAntennaU->setEnabled(rel && cBReferenceAntennaPcv->isChecked()&&cBReferenceAntenna->currentText()!="*");
    lblReferenceAntennaD->setEnabled(rel && cBReferenceAntennaPcv->isChecked()&&cBReferenceAntenna->currentText()!="*");

    cBRoverPositionTypeP->setEnabled(cBPositionMode->currentIndex() == PMODE_FIXED || cBPositionMode->currentIndex() == PMODE_PPP_FIXED);
    lERoverPosition1->setEnabled(cBRoverPositionTypeP->isEnabled() && cBRoverPositionTypeP->currentIndex() <= 2);
    lERoverPosition2->setEnabled(cBRoverPositionTypeP->isEnabled() && cBRoverPositionTypeP->currentIndex() <= 2);
    lERoverPosition3->setEnabled(cBRoverPositionTypeP->isEnabled() && cBRoverPositionTypeP->currentIndex() <= 2);
    btnRoverPosition->setEnabled(cBRoverPositionTypeP->isEnabled() && cBRoverPositionTypeP->currentIndex() <= 2);

    cBReferencePositionTypeP->setEnabled(rel && cBPositionMode->currentIndex() != PMODE_MOVEB);
    lEReferencePosition1->setEnabled(cBReferencePositionTypeP->isEnabled() && cBReferencePositionTypeP->currentIndex() <= 2);
    lEReferencePosition2->setEnabled(cBReferencePositionTypeP->isEnabled() && cBReferencePositionTypeP->currentIndex() <= 2);
    lEReferencePosition3->setEnabled(cBReferencePositionTypeP->isEnabled() && cBReferencePositionTypeP->currentIndex() <= 2);
    btnReferencePosition->setEnabled(cBReferencePositionTypeP->isEnabled() && cBReferencePositionTypeP->currentIndex() <= 2);

    lblMaxAveEp->setVisible(cBReferencePositionTypeP->currentIndex() == 4);
    sBMaxAveEp->setVisible(cBReferencePositionTypeP->currentIndex() == 4);
    cBInitRestart->setVisible(cBReferencePositionTypeP->currentIndex() == 4);

    sBSbasSatellite->setEnabled(cBPositionMode->currentIndex() == 0);
}
//---------------------------------------------------------------------------
void OptDialog::getPosition(int type, QLineEdit **edit, double *pos)
{
    double p[3] = { 0 }, dms1[3] = { 0 }, dms2[3] = { 0 };

    if (type == 1) { /* lat/lon/height dms/m */
        QStringList tokens = edit[0]->text().split(" ");
        for (int i = 0; i < 3 || i < tokens.size(); i++)
            dms1[i] = tokens.at(i).toDouble();

        tokens = edit[1]->text().split(" ");
        for (int i = 0; i < 3 || i < tokens.size(); i++)
            dms2[i] = tokens.at(i).toDouble();

        p[0] = (dms1[0] < 0 ? -1 : 1) * (fabs(dms1[0]) + dms1[1] / 60 + dms1[2] / 3600) * D2R;
        p[1] = (dms2[0] < 0 ? -1 : 1) * (fabs(dms2[0]) + dms2[1] / 60 + dms2[2] / 3600) * D2R;
        p[2] = edit[2]->text().toDouble();
        pos2ecef(p, pos);
    } else if (type == 2) { /* x/y/z-ecef */
        pos[0] = edit[0]->text().toDouble();
        pos[1] = edit[1]->text().toDouble();
        pos[2] = edit[2]->text().toDouble();
    } else {
        p[0] = edit[0]->text().toDouble() * D2R;
        p[1] = edit[1]->text().toDouble() * D2R;
        p[2] = edit[2]->text().toDouble();
        pos2ecef(p, pos);
	}
}
//---------------------------------------------------------------------------
void OptDialog::setPosition(int type, QLineEdit **edit, double *pos)
{
    double p[3], dms1[3], dms2[3], s1, s2;

    if (type == 1) { /* lat/lon/height dms/m */
        ecef2pos(pos, p); s1 = p[0] < 0 ? -1 : 1; s2 = p[1] < 0 ? -1 : 1;
        p[0] = fabs(p[0]) * R2D + 1E-12; p[1] = fabs(p[1]) * R2D + 1E-12;
        dms1[0] = floor(p[0]); p[0] = (p[0] - dms1[0]) * 60.0;
        dms1[1] = floor(p[0]); dms1[2] = (p[0] - dms1[1]) * 60.0;
        dms2[0] = floor(p[1]); p[1] = (p[1] - dms2[0]) * 60.0;
        dms2[1] = floor(p[1]); dms2[2] = (p[1] - dms2[1]) * 60.0;
        edit[0]->setText(QString("%1 %2 %3").arg(s1 * dms1[0], 0, 'f', 0).arg(dms1[1], 2, 'f', 0, '0').arg(dms1[2], 9, 'f', 6, '0'));
        edit[1]->setText(QString("%1 %2 %3").arg(s2 * dms2[0], 0, 'f', 0).arg(dms2[1], 2, 'f', 0, '0').arg(dms2[2], 9, 'f', 6, '0'));
        edit[2]->setText(QString::number(p[2], 'f', 4));
    } else if (type == 2) { /* x/y/z-ecef */
        edit[0]->setText(QString::number(pos[0], 'f', 4));
        edit[1]->setText(QString::number(pos[1], 'f', 4));
        edit[2]->setText(QString::number(pos[2], 'f', 4));
    } else {
        ecef2pos(pos, p);
        edit[0]->setText(QString::number(p[0] * R2D, 'f', 9));
        edit[1]->setText(QString::number(p[1] * R2D, 'f', 9));
        edit[2]->setText(QString::number(p[2], 'f', 4));
	}
}
//---------------------------------------------------------------------------
void OptDialog::readAntennaList(void)
{
    QString AntPcvFile_Text = lEAntennaPcvFile->text();
    QStringList list;
    pcvs_t pcvs = { 0, 0, NULL };
	char *p;

    if (!readpcv(qPrintable(AntPcvFile_Text), &pcvs)) return;

    list.append("");
    list.append("*");

    for (int i = 0; i < pcvs.n; i++) {
		if (pcvs.pcv[i].sat) continue;
        if ((p = strchr(pcvs.pcv[i].type, ' '))) *p = '\0';
        if (i > 0 && !strcmp(pcvs.pcv[i].type, pcvs.pcv[i - 1].type)) continue;
        list.append(pcvs.pcv[i].type);
	}
    cBRoverAntenna->clear();
    cBReferenceAntenna->clear();
    cBRoverAntenna->addItems(list);
    cBReferenceAntenna->addItems(list);

	free(pcvs.pcv);
}
//---------------------------------------------------------------------------

void OptDialog::navSys6Clicked()
{
	updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::btnFrequencyClicked()
{
    freqDialog->exec();
}
//---------------------------------------------------------------------------
void OptDialog::referenceAntennaClicked()
{
    updateEnable();
}
//---------------------------------------------------------------------------
void OptDialog::roverAntennaClicked()
{
    updateEnable();
}
//---------------------------------------------------------------------------


