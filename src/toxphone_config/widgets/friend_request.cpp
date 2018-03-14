#include "friend_request.h"
#include "ui_friend_request.h"

#include <QFontMetrics>

FriendRequestWidget::FriendRequestWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FriendRequestWidget)
{
    ui->setupUi(this);

//    QFontMetrics fm {ui->txtPublicKey->font()};
//    int iii = fm.height();
//    ui->textPublicKey->setMinimumHeight(fm.height() * 1.1);
//    ui->textPublicKey->setMaximumHeight(fm.height() * 1.1);
}

FriendRequestWidget::~FriendRequestWidget()
{
    delete ui;
}

QByteArray FriendRequestWidget::publicKey() const
{
    return ui->labelPublicKey->text().toLatin1();
}

void FriendRequestWidget::setPublicKey(const QByteArray& val)
{
    ui->labelPublicKey->setText(val);
}

void FriendRequestWidget::setMessage(const QString& val)
{
    ui->labelMessage->setText(val);
}
