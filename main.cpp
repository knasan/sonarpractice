#include "mainwindow.h"
#include "setupwizard.h"
#include "databasemanager.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QDialog>
#include <QDir>
#include <QStandardPaths>

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // Path in AppData/Local/SonarPractice/sonar_log.txt
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logPath);

    QFile outFile(logPath + "/sonar_log.txt");
    // Append mode: New logs are appended at the bottom.
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&outFile);
        QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        switch (type) {
        case QtDebugMsg:    ts << time << " [DEBUG] "; break;
        case QtInfoMsg:     ts << time << " [INFO ] "; break;
        case QtWarningMsg:  ts << time << " [WARN ] "; break;
        case QtCriticalMsg: ts << time << " [CRIT ] "; break;
        case QtFatalMsg:    ts << time << " [FATAL] "; break;
        default:            ts << time << " [MSG  ] "; break;
        }

        ts << msg << Qt::endl;
    }
}

bool isSetupNeeded(const QString &dbPath) {
    return !QFile::exists(dbPath);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("SonarPractice");

    qInstallMessageHandler(myMessageHandler);

    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    #ifdef QT_DEBUG
        QString dbPath = appDataDir + "/sonar_practice_debug.db";
    #else
        QString dbPath = appDataDir + "/sonar_practice.db";
    #endif

    a.setStyleSheet(
        "QWidget { background-color: #353535; color: #ffffff; }"
        "QCheckBox { color: #ffffff; spacing: 5px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #777777; background: #252525; }"
        "QCheckBox::indicator:checked { background-color: #2a82da; }"

        "QGroupBox { border: 1px solid #777777; margin-top: 1.5ex; color: #ffffff; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 3px; }"

        "QLineEdit, QListWidget, QTreeWidget { background-color: #252525; border: 1px solid #777777; color: #ffffff; }"

        // --- STANDARDBUTTONS (Wenn bereit / Aktiv) ---
        "QPushButton { background-color: #555555; color: white; border: 1px solid #777777; padding: 5px 15px; border-radius: 2px; }"
        "QPushButton:hover { background-color: #666666; }"
        // "QPushButton:pressed { background-color: #2a82da; }" // Blau beim Drücken für Feedback

        // --- DEAKTIVIERTER STATUS (Dein Wunsch: Dunkelgrau) ---
        "QPushButton:disabled { "
        "  background-color: #222222; " // Sehr dunkles Grau (fast schwarz)
        "  color: #555555; "            // Dunkelgrauer Text (kaum lesbar)
        "  border: 1px solid #333333; " // Kaum sichtbarer Rahmen
        "}"
        );

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "SonarPractice_" + QLocale(locale).name();

        // IMPORTANT: The path must match the RESOURCE_PREFIX from CMake
        // We'll try using the full path in the resource
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            qInfo() << "Translation successfully loaded:" << baseName;
            break;
        } else {
            qDebug() << "Could not load:" << ":/i18n/" + baseName;
        }
    }

    if (isSetupNeeded(dbPath)) {
        qInfo() << "SonarPractice setup wizard started";
        SetupWizard wizard;

        QFileInfo dbInfo(dbPath);
        if (!dbInfo.dir().exists()) {
            QDir().mkpath(dbInfo.absolutePath());
        }

        if (!dbInfo.dir().isReadable()) {
            qDebug() << "Database read error";
        }

        if (wizard.exec() == QDialog::Rejected) {
            return 0;
        }
    } else {
        qInfo()<< "------------------------------------------";
        qInfo() << "SonarPractice launched.";
    }

    // Database connection
    if (!DatabaseManager::instance().initDatabase(dbPath)) [[unlikely]] {
        qDebug() << "Critical error: Database could not be opened!";
        return -1;
    }


    MainWindow w;
    w.show();
    return a.exec();
}
