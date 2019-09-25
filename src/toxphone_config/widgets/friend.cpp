#include "friend.h"
#include "ui_friend.h"

#include <QBitmap>
#include <QPainter>

FriendWidget::FriendWidget(QWidget *parent) :
    Comparator(parent),
    ui(new Ui::FriendWidget)
{
    ui->setupUi(this);

    QFontMetrics fontMetrics {ui->labelPhoneNumber->font()};
    int w = fontMetrics.width("*99#");
    QSize sz {w + 3, 0};
    ui->labelPhoneNumber->setMinimumSize(sz);
}

FriendWidget::~FriendWidget()
{
    delete ui;
}

static QString friendName(const data::FriendItem& properties)
{
    if (!properties.nameAlias.isEmpty())
        return QString("%1 (%2)").arg(properties.nameAlias)
                                 .arg(properties.name);
    return properties.name;
}

void FriendWidget::setProperties(const data::FriendItem& val)
{
    _properties = val;
    ui->labelFriendName->setText(friendName(_properties));

    QString msg = _properties.statusMessage.trimmed();
    ui->labelFriendStatus->setText(msg);
    ui->labelFriendStatus->setVisible(!msg.isEmpty());

    if (_properties.phoneNumber != 0)
        ui->labelPhoneNumber->setText(QString("*%1#").arg(_properties.phoneNumber));
    else
        ui->labelPhoneNumber->clear();

//    if (_properties.isConnecnted)
//        ui->labelOnlineStatus->setText("O");
//    else
//        ui->labelOnlineStatus->setText("X");

    QPixmap onlineStatus;
    if (_properties.isConnecnted)
        onlineStatus = QPixmap("://resources/online.png");
    else
        onlineStatus = QPixmap("://resources/offline.png");

//    int onlineStatusSize = 15;
//    if (onlineStatus.height() > onlineStatusSize || onlineStatus.width() > onlineStatusSize)
//        onlineStatus = onlineStatus.scaled(onlineStatusSize, onlineStatusSize,
//                                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->labelOnlineStatus->setPixmap(onlineStatus);

    updateAvatar();
}

bool FriendWidget::lessThan(Comparator* c) const
{
    FriendWidget* fw = dynamic_cast<FriendWidget*>(c);
    if (_properties.isConnecnted == fw->_properties.isConnecnted)
    {
        QString name1 = friendName(_properties);
        QString name2 = friendName(fw->_properties);
        if (QString::compare(name1, name2, Qt::CaseInsensitive) < 0)
            return true;
    }
    return (_properties.isConnecnted > fw->_properties.isConnecnted);
}

void FriendWidget::updateAvatar()
{
    ui->labelAvatar->clear();

    QPixmap avatar;
    bool roundCorner = true;
    int avatarSize = ui->labelAvatar->height() * 0.8;
    ui->labelAvatar->setMinimumWidth(ui->labelAvatar->height());

    if (_properties.avatar.isEmpty())
    {
        roundCorner = false;
        avatarSize = avatarSize * 0.9;
        avatar = QPixmap("://resources/avatar_default.svg");
    }
    else
        avatar.loadFromData(_properties.avatar);

    //QSize sz = ui->labelAvatar->sizeHint();

    if ((avatar.height() > avatarSize) || (avatar.width() > avatarSize))
        avatar = avatar.scaled(avatarSize, avatarSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (!roundCorner)
    {
        ui->labelAvatar->setPixmap(avatar);
        return;
    }

    QPixmap mask {"://resources/avatar_mask.svg"};
    mask = mask.scaled(avatar.width() + 2, avatar.height() + 2,
                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap renderTarget {avatar.width() + 2, avatar.height() + 2};
    renderTarget.fill(Qt::transparent);
    {
        QPainter p(&renderTarget);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.drawPixmap(1, 1, avatar);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawPixmap(0, 0, mask);
        p.end();
    }
    ui->labelAvatar->setPixmap(renderTarget);
}

void FriendWidget::showEvent(QShowEvent*)
{
    QMetaObject::invokeMethod(this, "updateAvatar", Qt::QueuedConnection);
}
