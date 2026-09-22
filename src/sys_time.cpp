#include "sys_time.h"
#include <esp_timer.h> // Нативный компонент ESP-IDF для микросекундных таймеров
#include <time.h>

// Выделяем память под глобальные переменные аптайма
long upTime_d = 0;
long upTime_h = 0;
long upTime_m = 0;
long upTime_sec = 0;

// Расчет на базе нативного счетчика ESP-IDF
void updateSystemUptime() {
    // Получаем микросекунды и переводим в секунды (int64_t защищает от переполнения за 292 000 лет)
    int64_t total_seconds = esp_timer_get_time() / 1000000;

    upTime_sec = total_seconds % 60;
    long total_minutes = total_seconds / 60;
    upTime_m = total_minutes % 60;
    long total_hours = total_minutes / 60;
    upTime_h = total_hours % 24;
    upTime_d = total_hours / 24;
}

// Получение текущего времени станции в национальном формате строкой
String getSystemTimeStr() {
    struct tm timeinfo;
    char timeBuffer[80];
    // Записываем дефолтный текст на случай, если NTP еще не сработал
    snprintf(timeBuffer, sizeof(timeBuffer), "НЕТ СИНХРОНИЗАЦИИ");
    
    if (getLocalTime(&timeinfo) && timeinfo.tm_year > 120) {
        strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y %H:%M:%S", &timeinfo);
    }
    return String(timeBuffer);
}
