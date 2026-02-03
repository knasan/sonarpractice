#ifndef SONARMENUHELPER_H
#define SONARMENUHELPER_H

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QMainWindow>

class SonarMenuHelper {
public:
    static void setupMainWindowMenu(QMainWindow* window) {
        if (!window) return;

        QMenuBar* menuBar = window->menuBar();
        menuBar->clear();

        // --- File ---
        QMenu* fileMenu = menuBar->addMenu(QObject::tr("&File"));

        // App-specific actions (import/export etc.) will be added here later.

        fileMenu->addSeparator();

        QAction* exitAction = fileMenu->addAction(QObject::tr("&Quit"));
        exitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
        QObject::connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

        // ----------------------------------------------------------------------------------------------------
        QMenu* importMenu = fileMenu->addMenu(QObject::tr("&Import"));
        QAction* importFileAction = importMenu->addAction(QObject::tr("&File"));
        importFileAction->setObjectName("actionImportFile"); // A unique name for easy retrieval.

        QAction* importDirectoryAction = importMenu->addAction(QObject::tr("&Directory"));
        importDirectoryAction->setObjectName("actionImportDirectory");

        // Connect direct in mainwindow class

        // QAction* importFileAction = importMenu->addAction(QObject::tr("File..."));
        // QAction* importDirAction = importMenu->addAction(QObject::tr("Directory..."));

        // Connect zu einer Methode in deinem MainWindow, die den Dialog steuert
        // QObject::connect(importDirAction, &QAction::triggered, window, &MainWindow::handleDirectoryImport);
        // ----------------------------------------------------------------------------------------------------

        // --- Help ---
        QMenu* helpMenu = menuBar->addMenu(QObject::tr("&Help"));

        // Placeholder for dynamic help per page (see comment in your draft)
        // QAction* dynamicHelpAction = helpMenu->addAction(QObject::tr("Contextual help"));
        // dynamicHelpAction->setObjectName("dynamicHelpAction");
        // dynamicHelpAction->setEnabled(false);

        helpMenu->addSeparator();

        QAction* aboutQtAction = helpMenu->addAction(QObject::tr("About &Qt"));
        QObject::connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    }
};

#endif // SONARMENUHELPER_H
