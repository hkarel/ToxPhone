#pragma once
#include <QWidget>

struct Comparator : public QWidget
{
    Comparator(QWidget *parent) : QWidget(parent) {}
    virtual bool lessThan(Comparator*) const = 0;
};
