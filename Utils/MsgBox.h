#ifndef MSGBOX_H
#define MSGBOX_H

#include <QDialog>

class MsgBox : public QDialog
{
    Q_OBJECT
private:
    explicit MsgBox(QWidget *parent, bool isSucess, const QString& text);
public:
    static void error(QWidget *parent, const QString& text);
    static void success(QWidget *parent, const QString& text);
};


#endif // MSGBOX_H
