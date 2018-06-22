#include "password_window.h"
#include "ui_password_window.h"
#include <QMessageBox>

PasswordWindow::PasswordWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PasswordWindow)
{
    ui->setupUi(this);
    setWindowTitle(qApp->applicationName());
    setFixedSize(size());
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

PasswordWindow::~PasswordWindow()
{
    delete ui;
}

QString PasswordWindow::password() const
{
    return ui->linePassword->text();
}

void PasswordWindow::on_btnOk_clicked(bool)
{
    if (ui->linePassword->text() != ui->lineConfirm->text())
    {
        QString msg = tr("The fields 'New password' and 'Confirm password'\ndo not match.");
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
