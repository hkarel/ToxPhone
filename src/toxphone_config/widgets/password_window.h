#pragma once
#include <QDialog>

namespace Ui {
class PasswordWindow;
}

class PasswordWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordWindow(QWidget *parent = 0);
    ~PasswordWindow();

    QString password() const;

private slots:
    void on_btnOk_clicked(bool);

private:
    Ui::PasswordWindow *ui;
};
