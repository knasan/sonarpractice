#include "welcomepage.h"

#include <QTextBrowser>
#include <QVBoxLayout>
#include <QTextBrowser>

WelcomePage::WelcomePage(QWidget *parent) : BasePage(parent) {
    static QString title = tr("Welcome to SonarPractice");
    static QString subtitle = tr("by a musician for musicians");

    setTitle(title);
    setSubTitle(subtitle);

    auto *browser = new QTextBrowser(this);
    auto *layout = new QVBoxLayout(this);

    addHeaderLogo(layout, tr("Introduction"));

    static QString headline = tr("The vision behind the project");
    static QString paragraph1 = tr("SonarPractice arose from a very personal need: As a guitarist, I was looking for a way to precisely track my learning progress. Paper-based systems are disorganized, and spreadsheets quickly reached their limits. The solution was a local database – secure, private, and without registration.");

    static QString subheading = tr("No more excuses: Practice, track, improve");
    static QString paragraph2 = tr("During development, I realized that musicians often face another problem: Over decades, thousands of files accumulate – Guitar Pro tabs, PDFs, MP3s, and tutorial videos. Often, these are duplicated or corrupted by technical errors (0-byte files).");

    static QString subheading2 = tr("What SonarPractice can do for you:");

    static QString listItem1a = tr("Tracking & Journaling:");
    static QString listItem1b = tr("Record your practice successes and daily progress on the instrument.");

    static QString listItem2a = tr("Course Management:");
    static QString listItem2b = tr("Organize different file formats (GP, PDF, audio, video) in one central location.");

    static QString listItem3a = tr("System hygiene:");
    static QString listItem3b = tr("Find corrupted files and clean up duplicates with intelligent hash analysis to keep your collection clean.");

    static QString endParagraph = tr("SonarPractice is written by a musician for musicians – for structured practice and a well-organized system.");

    // Text with tr() and HTML formatting
    QString welcomeText =
        QString("<style>p, li { line-height: 140%; }</style><h1>%1</h1>"
        "<style>"
        "  p { margin-bottom: 15px; line-height: 140%; }"
        "  ul { margin-bottom: 15px; }"
        "  li { margin-bottom: 8px; }"
        "</style>"

        "<p><b>%2</b><br>%3</p>"

        "<p><b>%4</b><br>%5</p>"

        "<h3>%6</h3>"
        "<ul>"
        "<li><b>%7</b> %8</li>"
        "<li><b>%9</b> %10</li>"
        "<li><b>%11</b> %12</li>"
        "</ul>"

        "<p><i>%13</i></p>"
        ).arg(title, headline, paragraph1, subheading, paragraph2, subheading2, listItem1a, listItem1b, listItem2a, listItem2b, listItem3a, listItem3b, endParagraph);


    static QString stepHeader = tr("Here's what happens next:");
    static QString stepParagraph = tr("On the next page, we will configure your system together:");
    static QString stepListItem1a = tr("Data management:");
    static QString stepListItem1b = tr("You decide whether SonarPractice should actively manage your files to guarantee order and consistency in your collection.");

    static QString stepListItem2a = tr("One-time search:");
    static QString stepListItem2b = tr("You specify which directories we are allowed to search once for your existing courses, tabs and media in order to initially populate your database.");

    static QString stepEndParagraph = tr("Don't worry: You retain full control over which files are imported at all times.");

    QString nextSteps =
        QString(
        "<h3>%1</h3>"
        "<p>%2</p>"
        "<ul>"
        "  <li><b>%3</b> %4</li>"
        "  <li><b>%5</b> %6</li>"
        "</ul>"
        "<p>%7</p>"
        ).arg(stepHeader, stepParagraph, stepListItem1a, stepListItem1b, stepListItem2a, stepListItem2b, stepEndParagraph);

    browser->setHtml(welcomeText + nextSteps);

    layout->addWidget(browser);
}
