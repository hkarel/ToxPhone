#pragma once
#include <QtCore>

// Возвращает TRUE когда подключен конфигуратор
bool configConnected(bool* = 0);

// Возвращает TRUE когда дивертер активирован
bool diverterIsActive(bool* = 0);

// Строит путь до различных ресурсов
QString getFilePath(const QString& fileName);
