#include "password_window.h"
#include "ui_password_window.h"

#include "shared/config/appl_conf.h"
#include <QMessageBox>

PasswordWindow::PasswordWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PasswordWindow)
{
    ui->setupUi(this);
    setWindowTitle(qApp->applicationName());
    //setFixedSize(size());
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

PasswordWindow::~PasswordWindow()
{
    delete ui;
}

void PasswordWindow::saveGeometry()
{
    QPoint p = pos();
    QVector<int> v {p.x(), p.y(), width(), height()};
    config::state().setValue("windows.password_window.geometry", v);
}

void PasswordWindow::loadGeometry()
{
    QVector<int> v {0, 0, 265, 110};
    config::state().getValue("windows.password_window.geometry", v);

    //move(v[0], v[1]);
    resize(v[2], v[3]);
}

QString PasswordWindow::password() const
{
    return ui->linePassword->text();
}

void PasswordWindow::on_btnOk_clicked(bool)
{
    if (ui->linePassword->text() != ui->lineConfirm->text())
    {
        QString msg = tr("The fields 'New password' and 'Confirm password' do not match.");
        QMessageBox::warning(this, qApp->applicationName(), msg);
        return;
    }
    if (ui->linePassword->text().length() > 500)
    {
        QString msg = tr("Password length is limited to 500 characters.");
        QMessageBox::warning(this, qApp->applicationName(), msg);
        return;
    }
    accept();
}
