#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <Arduino.h>

// Объявляем внешние зависимости, чтобы другие файлы знали об их существовании
extern TaskHandle_t httpTaskHandle;
extern SemaphoreHandle_t xSensorsMutex; // Для блокировки задач на время OTA

// УДОБНАЯ И НАГЛЯДНАЯ СТРУКТУРА БЕЗ КРАТНОСТИ БИТАМ
struct BLEPayload
{
    // Показания физических датчиков
    float ext_temperature;  // Уличная температура
    float ext_humidity;     // Уличная влажность
    float ext_pressure;     // Атмосферное давление
    float ext_radiation;    // Радиационный фон
    float ext_dust;         // Запыленность улицы (PM2.5)
    float int_co2;          // Уровень CO2 внутри дома (SCD30)

    // Системные параметры
    uint32_t uptime;        // Время непрерывной работы M5 в сек.
    uint16_t packetId;      // Порядковый номер пакета

    // Индивидуальные флаги актуальности (прямые bool)
    bool ext_temp_actual;
    bool ext_humi_actual;
    bool ext_press_actual;
    bool ext_rad_actual;
    bool ext_dust_actual;
    bool int_co2_actual;
} __attribute__((packed));


// Объявляем функцию таски, которую запустим в main.cpp
void vHttpServerTask(void *pvParameters);
extern void vBleAdvertisingTask(void *pvParameters);

#endif // WIFI_SERVER_H

