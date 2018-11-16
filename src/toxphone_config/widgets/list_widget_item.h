#pragma once
#include <QListWidgetItem>

template<typename T>
struct ListWidgetItem : public QListWidgetItem
{
    ListWidgetItem(T* widget) :
        QListWidgetItem(0, int(QListWidgetItem::Type)),
        widget(widget)
    {}
    bool operator< (const QListWidgetItem &other) const override
    {
        const ListWidgetItem<T>& lwi = dynamic_cast<const ListWidgetItem<T>&>(other);
        return widget->lessThan(lwi.widget);
    }
    T* const widget;
};
