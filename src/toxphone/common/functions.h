#pragma once
#include <QtCore>

// Возвращает TRUE когда подключен конфигуратор
bool configConnected(bool* = 0);

// Строит путь до различных ресурсов
QString getFilePath(const QString& fileName);
