#pragma once

#include "comparator.h"
#include <QWidget>

namespace Ui {
class StubWidget;
}

class StubWidget : /*public QWidget,*/ public Comparator
{
public:
    explicit StubWidget(QWidget *parent = 0);
    ~StubWidget();

    bool lessThan(Comparator*) const override;

private:
    Q_OBJECT
    Ui::StubWidget *ui;
};
