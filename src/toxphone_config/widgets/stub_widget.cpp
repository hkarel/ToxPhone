#include "stub_widget.h"
#include "ui_stub_widget.h"

StubWidget::StubWidget(QWidget *parent) :
    Comparator(parent),
    ui(new Ui::StubWidget)
{
    ui->setupUi(this);
}

StubWidget::~StubWidget()
{
    delete ui;
}

bool StubWidget::lessThan(Comparator*) const
{
    return false;
}
