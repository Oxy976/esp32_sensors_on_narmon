#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <Arduino.h>
#include <time.h>

extern long upTime_d;
extern long upTime_h;
extern long upTime_m;
extern long upTime_sec;

void updateSystemUptime();
String getSystemTimeStr();
String getSystemTimeShortStr(); // Для логов [HH:MM:SS]
String getUptimeStr();          // Красивая строка аптайма
bool isTimeValid();             // Проверка синхронизации NTP

#endif // SYS_TIME_H
