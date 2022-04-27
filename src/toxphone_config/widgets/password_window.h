#pragma once
#include <QDialog>

namespace Ui {
class PasswordWindow;
}

class PasswordWindow : public QDialog
{
public:
    explicit PasswordWindow(QWidget *parent = 0);
    ~PasswordWindow();

    void saveGeometry();
    void loadGeometry();

    QString password() const;

private slots:
    void on_btnOk_clicked(bool);

private:
    Q_OBJECT
    Ui::PasswordWindow *ui;
};
