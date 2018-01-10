#include "connection.h"
#include "ui_connection.h"

ConnectionWidget::ConnectionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConnectionWidget)
{
    ui->setupUi(this);
    ui->labelConnectStatus->clear();
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

void ConnectionWidget::setConnectStatus(bool val)
{
    _connectStatus = val;
    if (_connectStatus)
        ui->labelConnectStatus->setText("C");
    else
        ui->labelConnectStatus->clear();
}

bool ConnectionWidget::lifeTimeExpired() const
{
    return (_lifeTimer.elapsed<std::chrono::seconds>() > _lifeTimeInterval);
}
