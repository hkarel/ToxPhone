#pragma once

#include "kernel/communication/commands.h"
#include <QWidget>

namespace Ui {
class FriendWidget;
}

using namespace std;
using namespace communication;

class FriendWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FriendWidget(QWidget *parent = 0);
    ~FriendWidget();

    //QString name() const;
    //void setName(const QString&);
    //void setStatusMessage(const QString&);

    //QByteArray publicKey() const;
    //void setPublicKey(const QByteArray&);

    //bool isConnected() const {return _isConnected;}
    //void setConnectStatus(bool);

    const data::FriendItem& properties() const {return _properties;}
    void setProperties(const data::FriendItem&);

private:
    Ui::FriendWidget *ui;
    data::FriendItem _properties;


    ///QByteArray _publicKey;
    //bool _isConnected = {false};
};
