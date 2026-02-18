#include "setupwizard.h"
#include "filemanager.h"
#include "welcomepage.h"
#include "filterpage.h"
#include "reviewpage.h"
#include "mappingpage.h"
#include "filefilterproxymodel.h"

#include <QScreen>
#include <QGuiApplication>
#include <QAbstractButton>
#include <QMessageBox>

SetupWizard::SetupWizard(QWidget *parent)
    : QWizard(parent)
{
    setObjectName("setupWizard");
    setWindowFlags(windowFlags()
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setSizeGripEnabled(true);

    setOption(QWizard::HaveHelpButton, true);
    setButtonText(QWizard::HelpButton, tr("About Qt"));

    // Link to the Help dialog
    connect(this, &QWizard::helpRequested, this, [this]() {
        QMessageBox::aboutQt(this, tr("About Qt"));
    });

    // --- Model Config ----
    filesModel_m = new QStandardItemModel(this);
    fileManager_m = new FileManager(this);
    fileManager_m->setModel(filesModel_m);

    filesModel_m->setColumnCount(4);
    setProxyModelHeader();

    // Instanzen
    proxyModel_m = new FileFilterProxyModel(this);

    // The mapping (Source -> Proxy)
    proxyModel_m->setSourceModel(filesModel_m);

    // Proxy configuration
    proxyModel_m->setDynamicSortFilter(true);

    // --- Setup Wizard -----
    setWindowTitle(tr("SonarPractice Setup Assistant"));

    // Primary screen
    QScreen *screen = QGuiApplication::primaryScreen();
    // Available geometry (without taskbar!)
    QRect screenGeometry = screen->availableGeometry();

    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);

    int dynamicHeight = qMin(800, (int)(screenGeometry.height() * 0.7));
    int dynamicWidth = qMin(1000, (int)(screenGeometry.width() * 0.7));

    setMinimumSize(600, 500); // Absolute Untergrenze
    resize(dynamicWidth, dynamicHeight); // Vernünftiger Startwert
    move(screenGeometry.center() - rect().center());

    //  Stil-Optionen setzen
    // 'IndependentPages' ensures that pages are more clearly separated.
    setOption(QWizard::IndependentPages, true);

    // Explicitly focus on a modern style
    setWizardStyle(QWizard::ModernStyle);

    connect(this, &QWizard::currentIdChanged, this, [this](int id) {
        qDebug() << "SetupWizard currentIdChanged: " << id;

        if (id == 1) {
            qDebug() << "initialize for Filterpage";
            FilterPage *page = qobject_cast<FilterPage*>(currentPage());
            if (page) {
                page->initializePage();
            }
        }

        if (id == 2) {
            qDebug() << "initialize for ReviewPage";
            ReviewPage *page = qobject_cast<ReviewPage*>(currentPage());
            if (page) {
                page->initializePage();
            }
        }

        // id 3 ?
    });

    welcomePage_m = new WelcomePage(this);
    filterPage_m = new FilterPage(this);
    reviewPage_m = new ReviewPage(this);
    mappingPage_m = new MappingPage(this);

    addPage(welcomePage_m); // Id 0
    addPage(filterPage_m);  // Id 1
    addPage(reviewPage_m);  // Id 2
    addPage(mappingPage_m); // Id 3
}

SetupWizard::~SetupWizard() {}

void SetupWizard::setButtonsState(bool backEnabled, bool nextEnabled, bool cancelEnabled) {
    if (auto *btn = button(QWizard::BackButton))   btn->setEnabled(backEnabled);
    if (auto *btn = button(QWizard::NextButton))   btn->setEnabled(nextEnabled);
    if (auto *btn = button(QWizard::CancelButton)) btn->setEnabled(cancelEnabled);

    QCoreApplication::processEvents();
}

QStringList SetupWizard::getSelectedPaths() const {
    return sourcePaths_m;
}

QStringList SetupWizard::getFileFilters() const {
    return activeFilters_m;
}

void SetupWizard::setProxyModelHeader() {
    filesModel_m->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Status"), tr("Group")});
}
