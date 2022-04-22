#include "connection.h"
#include "ui_connection.h"

ConnectionWidget::ConnectionWidget(QWidget *parent) :
    Comparator(parent),
    ui(new Ui::ConnectionWidget)
{
    ui->setupUi(this);
}

ConnectionWidget::~ConnectionWidget()
{
    delete ui;
}

QString ConnectionWidget::info() const
{
    return ui->labelInfo->text();
}

void ConnectionWidget::setInfo(const QString& info)
{
    if (info != ui->labelInfo->text())
        ui->labelInfo->setText(info);
}

void ConnectionWidget::setHostPoint(const pproto::HostPoint& val)
{
    _hostPoint = val;
    QString s = "%1 : %2";
    ui->labelHostPoint->setText(s.arg(_hostPoint.address().toString()).arg(_hostPoint.port()));
}

void ConnectionWidget::setConfigConnectCount(int val)
{
    _configConnectCount = val;
    if (_configConnectCount != 0)
    {
        QPixmap pixmap = QPixmap("://resources/connection_lock.png");
        int size = 20;
        if (pixmap.height() > size || pixmap.width() > size)
            pixmap = pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->labelConnectCount->setPixmap(pixmap);
    }
    else
        ui->labelConnectCount->clear();
}

bool ConnectionWidget::lifeTimeExpired() const
{
    return (_lifeTimer.elapsed<std::chrono::seconds>() > _lifeTimeInterval);
}

bool ConnectionWidget::lessThan(Comparator* c) const
{
    if (ConnectionWidget* cw = dynamic_cast<ConnectionWidget*>(c))
        return (QString::compare(info(), cw->info(), Qt::CaseInsensitive) < 0);
    else
        return true;
}
