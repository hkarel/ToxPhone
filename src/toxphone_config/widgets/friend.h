#pragma once
#include <QWidget>

namespace Ui {
class FriendWidget;
}

class FriendWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FriendWidget(QWidget *parent = 0);
    ~FriendWidget();

    QString name() const;
    void setName(const QString&);
    void setStatusMessage(const QString&);

    QByteArray publicKey() const;
    void setPublicKey(const QByteArray&);

    bool isConnected() const {return _isConnected;}
    void setConnectStatus(bool);

private:
    Ui::FriendWidget *ui;
    QByteArray _publicKey;
    bool _isConnected = {false};
};
