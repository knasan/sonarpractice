#include "mainwindow.h"
#include "databasemanager.h"
#include "librarypage.h"
#include "sonarlessonpage.h"

#include <QMainWindow>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QWidget>
#include <QScreen>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{

    // Display Setup
    // 1. primary screen
    QScreen *screen = QGuiApplication::primaryScreen();
    // Available geometry (without taskbar!)
    QRect screenGeometry = screen->availableGeometry();
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();
    // Calculate a dynamic size (e.g., 80% width, 70% height)
    int wizardWidth = screenWidth * 0.8;
    int wizardHeight = screenHeight * 0.7;
    // Safety check: Not less than your minimum.
    wizardWidth = qMax(wizardWidth, 1000);
    wizardHeight = qMax(wizardHeight, 700);
    // Set window size
    resize(wizardWidth, wizardHeight);
    setMinimumSize(950, 650);
    // Center the wizard on the screen
    move(screenGeometry.center() - rect().center());

    // Database
    dbManager_m = new DatabaseManager(this);

    // Prepare page
    lessonPage_m = new SonarLessonPage(this, dbManager_m);
    libraryPage_m = new LibraryPage();

    // Create a central widget and layout
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // StackedWidget for page switching (Home, Library, etc.)
    stackedWidget_m = new QStackedWidget(this);

    // --- NAVIGATION ---
    QHBoxLayout *navLayout = new QHBoxLayout();

    QPushButton *btnHome = new QPushButton(tr("Exercise (Home)"), this);
    QPushButton *btnLibrary = new QPushButton(tr("Library"), this);

    btnHome->setMinimumHeight(40);
    btnLibrary->setMinimumHeight(40);

    navLayout->addWidget(btnHome);
    navLayout->addWidget(btnLibrary);
    navLayout->addStretch();

    // Add the navigation layout to the main layout (above the QStackedWidget)
    layout->addLayout(navLayout);

    // --- Linking Logic (Signals and Slots) ---
    connect(btnHome, &QPushButton::clicked, this, [this]() {
        stackedWidget_m->setCurrentIndex(0); // page home
    });

    connect(btnLibrary, &QPushButton::clicked, this, [this]() {
        stackedWidget_m->setCurrentIndex(1); // page library
    });


    stackedWidget_m->addWidget(lessonPage_m);   // Index 0
    stackedWidget_m->addWidget(libraryPage_m);  // Index 1

    layout->addWidget(stackedWidget_m);

    // SThe default setting is to show the Lesson Page (your main page).
    stackedWidget_m->setCurrentWidget(lessonPage_m);
}

MainWindow::~MainWindow() {}
