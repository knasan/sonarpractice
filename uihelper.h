#ifndef UIHELPER_H
#define UIHELPER_H

class QString;
class QWidget;
class QLayout;

class UIHelper
{
public:
    /**
    * @brief Opens a file and displays a message box if there are errors.
    */
    static void openFileWithFeedback(QWidget *parent, const QString &fullPath);
    static void clearLayout(QLayout *layout);
};

#endif // UIHELPER_H
