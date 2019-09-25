#pragma once
#include <QWidget>

namespace Ui {
class FriendRequestWidget;
}

class FriendRequestWidget : public QWidget
{
public:
    explicit FriendRequestWidget(QWidget *parent = 0);
    ~FriendRequestWidget();

    QByteArray publicKey() const;
    void setPublicKey(const QByteArray&);

    void setMessage(const QString&);

private:
    Q_OBJECT
    Ui::FriendRequestWidget *ui;
};
