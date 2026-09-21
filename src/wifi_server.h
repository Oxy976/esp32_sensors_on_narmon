#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <Arduino.h>

// Объявляем внешние зависимости, чтобы другие файлы знали об их существовании
extern TaskHandle_t httpTaskHandle;

// Объявляем функцию таски, которую запустим в main.cpp
void vHttpServerTask(void *pvParameters);

#endif // WIFI_SERVER_H

