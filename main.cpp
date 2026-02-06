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
#include <QStyleFactory>
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

static QString loadQss(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("SonarPractice");

    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    #ifdef QT_DEBUG
        qDebug() << "in debug";
        QString dbPath = appDataDir + "/sonar_practice_debug.db";
    #else
        qDebug() << "in release";
        qInstallMessageHandler(myMessageHandler);
        QString dbPath = appDataDir + "/sonar_practice.db";
    #endif

    // Load Style
    a.setStyle(QStyleFactory::create("Fusion"));
    a.setWindowIcon(QIcon(":/icon"));
    a.setStyleSheet(loadQss(":/base.qss"));

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
        // a.setStyleSheet(loadQss(":/wizard.qss"));
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
        // a.setStyleSheet(loadQss(":/app.qss"));
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
