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

SetupWizard::SetupWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowFlags(windowFlags()
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setSizeGripEnabled(true);

    // --- Model Config ----
    filesModel_m = new QStandardItemModel(this);
    fileManager_m = new FileManager(this);
    fileManager_m->setModel(filesModel_m);

    filesModel_m->setColumnCount(4);
    filesModel_m->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Status"), tr("Group")});

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
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);

    // Center the wizard on the screen
    move(screenGeometry.center() - rect().center());

    //  Stil-Optionen setzen
    // 'IndependentPages' ensures that pages are more clearly separated.
    setOption(QWizard::IndependentPages, true);

    // Explicitly focus on a modern style
    setWizardStyle(QWizard::ModernStyle);

    welcomePage_m = new WelcomePage(this);
    filterPage_m = new FilterPage(this);
    reviewPage_m = new ReviewPage(this);
    mappingPage_m = new MappingPage(this);

    addPage(welcomePage_m);
    addPage(filterPage_m);
    addPage(reviewPage_m);
    addPage(mappingPage_m);
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
