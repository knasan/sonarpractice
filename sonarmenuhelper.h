#ifndef SONARMENUHELPER_H
#define SONARMENUHELPER_H

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QCoreApplication>

class SonarMenuHelper {
    Q_DECLARE_TR_FUNCTIONS(SonarMenuHelper)

public:
    static void setupMainWindowMenu(QMainWindow* window) {
        if (!window) return;

        QMenuBar* menuBar = window->menuBar();
        menuBar->clear();

        // --- File Menu ---
        QMenu* fileMenu = menuBar->addMenu(QObject::tr("&File"));

        QMenu* importMenu = fileMenu->addMenu(QObject::tr("&Import"));
        importMenu->addAction(QObject::tr("&File"))->setObjectName("actionImportFile");
        importMenu->addAction(QObject::tr("&Directory"))->setObjectName("actionImportDirectory");

        fileMenu->addSeparator();
        QAction* exitAction = fileMenu->addAction(QObject::tr("&Quit"));
        exitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
        QObject::connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

        // --- Help Menu ---
        QMenu* helpMenu = menuBar->addMenu(QObject::tr("&Help"));

        // NEU: Info Action
        QAction* aboutAction = helpMenu->addAction(QObject::tr("&About"));
        QObject::connect(aboutAction, &QAction::triggered, [window]() {
            showAboutDialog(window);
        });

        helpMenu->addSeparator();

        QAction* aboutQtAction = helpMenu->addAction(QObject::tr("About &Qt"));
        QObject::connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    }

private:
    static void showAboutDialog(QMainWindow* parent) {
#ifndef APP_VERSION
#define APP_VERSION "Unknown"
#endif
#ifndef COMPILER_INFO
#define COMPILER_INFO "Unknown Compiler"
#endif

        QMessageBox aboutBox(parent);
        aboutBox.setWindowTitle(QObject::tr("About SonarPractice"));

        QPixmap logo(":/icon");
        if (!logo.isNull()) {
            aboutBox.setIconPixmap(logo.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        const QString txtVersion = tr("Version");
        const QString txtBuildEnv = tr("Build env");
        const QString txtQtVersion = tr("Qt version");
        const QString txtBuildTime = tr("Build time");
        const QString txtOS        = tr("OS");
        const QString txtMsg = tr("I hope SonarPractice helps you with your daily music exercises!");
        const QString txtBmac = tr("Buy me a coffee");

        QString linkColor = "#00bfff";

        QString infoTemplate = QString(
            "<h3>SonarPractice</h3>"
            "<i>%1</i>"
            "<p><b>%2:</b> %3</p>" // Version
            "<p><b>%4:</b> %5</p>" // Build
            "<p><b>%6:</b> %7</p>" // Qt Version
            "<p><b>Developer:</b> Sandy Marko Knauer</p>"
            "<p><b>License:</b> GPLv3</p>"
            "<p><b>GitHub:</b> <a href='https://github.com/knasan/sonarpractice' style='color: %8;'>GitHub Repository</a></p>"
            "<hr>"
            "<p style='color: #aaaaaa; font-size: small;'>"
            "%9: %10</p>"
            "<p style='color: #aaaaaa; font-size: small;'>%11: %12, %13</p>"
            "<hr>"
            "<p><a href='https://buymeacoffee.com/sonarpractice' style='color: %15;' alt='Support the development'> %14</a></p>"
            );

        QString infoText = infoTemplate.arg(
            txtMsg,                       // %1
            txtVersion,   APP_VERSION,    // %2, %3
            txtBuildEnv,  COMPILER_INFO,  // %4, %5
            txtQtVersion, QT_VERSION_STR, // %6, %7
            linkColor,                    // %8
            txtOS, QSysInfo::prettyProductName(), // %9, %10
            txtBuildTime, __DATE__,       // %11, %12
            __TIME__,                     // %13
            txtBmac, linkColor            // %14, %15
            );

        aboutBox.setText(infoText);
        aboutBox.exec();

    }
};

#endif // SONARMENUHELPER_H
