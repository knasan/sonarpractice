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

void UIHelper::clearLayout(QLayout *layout) {
    if (!layout) return;

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        } else if (QLayout *childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        delete item;
    }
}
