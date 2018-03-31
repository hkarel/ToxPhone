#include "connection.h"
#include "ui_connection.h"

ConnectionWidget::ConnectionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConnectionWidget)
{
    ui->setupUi(this);
}

ConnectionWidget::~ConnectionWidget()
{
    delete ui;
}

void ConnectionWidget::setInfo(const QString& info)
{
    ui->labelInfo->setText(info);
}

void ConnectionWidget::setHostPoint(const communication::HostPoint& val)
{
    _hostPoint = val;
    QString s = "%1 : %2";
    ui->labelHostPoint->setText(s.arg(_hostPoint.address().toString()).arg(_hostPoint.port()));
}

void ConnectionWidget::setConfigConnectCount(int val)
{
    _configConnectCount = val;
    if (_configConnectCount != 0)
        ui->labelConnectCount->setText("X");
    else
        ui->labelConnectCount->clear();
}

bool ConnectionWidget::lifeTimeExpired() const
{
    return (_lifeTimer.elapsed<std::chrono::seconds>() > _lifeTimeInterval);
}
