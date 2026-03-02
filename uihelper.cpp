#include "uihelper.h"
#include "fileutils.h"

#include "QMessageBox"

#include <QLayout>
#include <QLayoutItem>

void UIHelper::openFileWithFeedback(QWidget *parent, const QString &fullPath) {
    if (!FileUtils::openLocalFile(fullPath)) {
        QMessageBox::critical(parent,
                              QObject::tr("Error opening"),
                              QObject::tr("The file could not be opened.\nPath: %1").arg(fullPath));
    }
}
