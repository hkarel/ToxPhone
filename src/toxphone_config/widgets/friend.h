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

    const data::FriendItem& properties() const {return _properties;}
    void setProperties(const data::FriendItem&);

    bool lessThan(FriendWidget*);

private:
    void setAvatar(QPixmap avatar, bool roundCorner);

private:
    Ui::FriendWidget *ui;
    data::FriendItem _properties;
};
