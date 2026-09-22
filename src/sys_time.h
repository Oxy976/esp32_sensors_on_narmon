#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <Arduino.h>

// Объявляем глобальные переменные для совместимости со старым кодом
extern long upTime_d;
extern long upTime_h;
extern long upTime_m;
extern long upTime_sec;

// Чистые системные функции
void updateSystemUptime();
String getSystemTimeStr();

#endif // SYS_TIME_H
