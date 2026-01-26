#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QStackedWidget;
class DatabaseManager;
class LibraryPage;
class SonarLessonPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    QStackedWidget *stackedWidget_m;
    DatabaseManager *dbManager_m;

    LibraryPage *libraryPage_m;
    SonarLessonPage *lessonPage_m;
};

#endif // MAINWINDOW_H
