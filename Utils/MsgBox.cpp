#include "MsgBox.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QIcon>

MsgBox::MsgBox(QWidget *parent, bool isSucess, const QString& text)
    :QDialog(parent)
{
    setWindowModality(Qt::ApplicationModal); //模态
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint); //关闭按钮, 展示于上方
    setWindowTitle("提示");
    setFixedSize(320, 120);
    setStyleSheet(".MsgBox{background-color:rgb(45,45,55)}");

    QVBoxLayout *boxVLayout = new QVBoxLayout(this);
    boxVLayout->setContentsMargins(0, 0, 0, 0);
    boxVLayout->setSpacing(0); //边距都为0, 紧凑

    QWidget *line = new QWidget(this);
    line->setStyleSheet(".QWidget{background-color:rgb(230, 230, 230)}");
    line->setFixedHeight(1);

    QWidget *boxHWidget = new QWidget(this);
    QHBoxLayout *boxHLayout = new QHBoxLayout(boxHWidget);
    boxHLayout->setContentsMargins(20, 0, 20, 0); //左右边距20px

    QString filename;
    if(isSucess){
        filename = ":/res/images/madline3.jpg";
    }else{
        filename = ":/res/images/madline4.jpg";
    }

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(filename).pixmap(50, 50));

    QLabel *textLabel = new QLabel(this);
    textLabel->setWordWrap(true); //换行
    QString showText = QString("<p style = 'font-family:Cascadia Code; font-size:15px; margin:10px; line-height:20px;'>%1</p>").arg(text);
    textLabel->setText(tr(showText.toUtf8()));

    boxHLayout->addWidget(iconLabel);
    boxHLayout->addSpacing(10); //固定间距
    boxHLayout->addWidget(textLabel);
    boxHLayout->addStretch(5); //等比间距

    boxVLayout->addWidget(line);
    boxVLayout->addWidget(boxHWidget);
}

void MsgBox::error(QWidget *parent, const QString &text)
{
    MsgBox mb(parent, false, text);
    mb.exec();
}

void MsgBox::success(QWidget *parent, const QString &text)
{
    MsgBox mb(parent, true, text);
    mb.exec();
}
