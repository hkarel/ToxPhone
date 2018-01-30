#include "friend.h"
#include "ui_friend.h"

FriendWidget::FriendWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FriendWidget)
{
    ui->setupUi(this);
}

FriendWidget::~FriendWidget()
{
    delete ui;
}

QString FriendWidget::name() const
{
    return ui->labelFriendName->text();
}

void FriendWidget::setName(const QString& val)
{
    QString ss = ui->labelFriendName->text();

    ui->labelFriendName->setText(val);
}

void FriendWidget::setStatusMessage(const QString& val)
{
    ui->labelFriendStatus->setText(val);
}

QByteArray FriendWidget::publicKey() const
{
    return _publicKey;
}

void FriendWidget::setPublicKey(const QByteArray& val)
{
    _publicKey = val;

}

void FriendWidget::setConnectStatus(bool val)
{
    _isConnected = val;
    if (_isConnected)
        ui->labelOnlineStatus->setText("O");
    else
        ui->labelOnlineStatus->setText("X");

}
