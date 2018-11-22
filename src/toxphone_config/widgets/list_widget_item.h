#pragma once

#include "comparator.h"
#include <QListWidgetItem>

struct ListWidgetItem : public QListWidgetItem
{
    ListWidgetItem(Comparator* widget) :
        QListWidgetItem(0, int(QListWidgetItem::Type)),
        widget(widget)
    {}
    bool operator< (const QListWidgetItem &other) const override
    {
        const ListWidgetItem& lwi = dynamic_cast<const ListWidgetItem&>(other);
        return widget->lessThan(lwi.widget);
    }
    Comparator* const widget;
};
