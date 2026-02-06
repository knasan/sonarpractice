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

    [[nodiscard]] QString formatFilter(const QString& description, const QStringList& extensions);

private slots:
    void reloadStyle();

signals:
    void dataChanged();

public slots:
    void onImportFileTriggered();
    void onImportDirectoryTriggered();

};

#endif // MAINWINDOW_H
