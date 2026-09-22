#include "wifi_server.h"
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "strct.h"    // Подключаем вашу структуру датчиков
#include "settings.h" // Подключаем настройки (SensUnit)
#include <ESPmDNS.h>
#include "sys_time.h"

#undef min
#undef max
#include <deque>

// Указываем компилятору, что логи и мьютекс физически живут в main.cpp
extern std::deque<String> webLogs;
extern SemaphoreHandle_t xLogMutex;
extern void logToWeb(String text);

// Указываем компилятору, что эти объекты созданы в main.cpp
extern stSens vSensVal[SensUnit];
extern SemaphoreHandle_t xSensorsMutex;

// Создаем объект сервера внутри этого файла
WebServer wserv(80);

// Хэндл таски, эта переменная берётся из main.cpp
extern TaskHandle_t httpTaskHandle;

// http server -
void handleRoot()
{
   // 1. Обновляем переменные и получаем текущее время
    updateSystemUptime(); 
    String current_time = getSystemTimeStr();

    // 2. Собираем строку аптайма по месту
    String uptimeStr =  String(upTime_d) + "д "+String(upTime_h) + "ч " + String(upTime_m) + "м " + String(upTime_sec) + "с";
    // 3. Формируем HTML страницу
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<link rel=\"icon\" href=\"data:,\">";

    html += "<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; background-color: #f4f6f9; margin: 8px; padding: 0; font-size: 14px; }";
    html += "h1 { font-size: 18px; margin: 8px 0 2px 0; color: #333; }";                                                                                   // Уменьшили h1
    html += "p { margin: 2px 0 10px 0; font-size: 12px; color: #666; }";                                                                                   // Компактное время
    html += "table { border-collapse: collapse; width: 95%; max-width: 440px; margin: 0 auto; box-shadow: 0 2px 4px rgba(0,0,0,0.05); font-size: 13px; }"; // Ограничили ширину
    html += "th { padding: 6px 8px; background-color: #0043af; color: white; font-size: 13px; }";                                                          // Плотный padding заголовков
    html += "tr { border: 1px solid #C0C0C0; }";
    html += "tr:hover { background-color: #e8e8e8; }";
    html += "td { padding: 5px 8px; }"; // Минимальный плотный padding ячеек
    html += ".actual { color: black; font-weight: bold; background-color: #ffffff; }";
    html += ".not_actual { color: #A0A0A0; font-weight: normal; background-color: #fafafa; }";
    html += ".btn { display: inline-block; padding: 6px 14px; margin-top: 12px; background-color: #333; color: white; text-decoration: none; border-radius: 4px; font-weight: bold; font-size: 12px; font-family: Arial, sans-serif; }"; // Миниатюрная кнопка
    html += ".btn:hover { background-color: #555; }</style></head><body>";

    html += "<h1>ESP32 Метеостанция</h1>";
    html += "<p>Время: " + current_time + " | Uptime: " + uptimeStr + "</p>";
    html += "<table><tr><th>#</th><th>Параметр</th><th>Значение</th><th>Ед.</th></tr>";

    if (xSensorsMutex != NULL && xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        for (int i = 0; i < SensUnit; i++)
        {
            if (vSensVal[i].actual)
            {
                html += "<tr class=\"actual\"><td>";
            }
            else
            {
                html += "<tr class=\"not_actual\"><td>";
            }
            html += String(i) + "</td><td style=\"text-align: left;\">";                               // Выравнивание названия по левому краю для читаемости
            html += vSensVal[i].name + "</td><td style=\"font-family: monospace; font-size: 14px;\">"; // Моноширинный шрифт для ровных цифр
            html += String(vSensVal[i].value, 2) + "</td><td>";
            html += vSensVal[i].unit + "</td></tr>";
        }
        xSemaphoreGive(xSensorsMutex);
    }
    else
    {
        html += "<tr><td colspan='4' style='color:red; padding: 10px;'>Данные обновляются...</td></tr>";
    }

    html += "</table>";
    html += "<a href=\"/logs\" class=\"btn\">Открыть системный лог</a>";
    html += "</body></html>";

    wserv.send(200, "text/html", html);
}

// 2. ОТДЕЛЬНАЯ СТРАНИЦА ЛОГОВ (/logs)
void handleLogs()
{
    // 1. Обновляем переменные и получаем текущее время
    updateSystemUptime(); 
    String current_time = getSystemTimeStr();

    // 2. Собираем строку аптайма по месту
    String uptimeStr =  String(upTime_d) + "д "+String(upTime_h) + "ч " + String(upTime_m) + "м " + String(upTime_sec) + "с";

    // 3. Формируем HTML страницу
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>Системный журнал</title>";

    html += "<style>body { background-color: #121212; color: #00ff00; font-family: 'Courier New', monospace; padding: 15px; margin: 0; }";
    html += ".console { background-color: #000000; border: 1px solid #333; padding: 15px; border-radius: 5px; height: 75vh; overflow-y: auto; text-align: left; white-space: pre-wrap; line-height: 1.4; font-size: 13px; }";
    html += ".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; color: #fff; font-family: Arial, sans-serif; border-bottom: 1px solid #222; padding-bottom: 8px; }";
    html += ".header-title { text-align: left; }";
    html += ".header h2 { margin: 0 0 4px 0; font-size: 18px; color: #ffffff; }";
    html += ".meta-info { margin: 0; font-size: 12px; color: #e0e0e0; font-family: 'Courier New', monospace; }";
    html += ".btn { padding: 5px 10px; font-size: 12px; background-color: #0043af; color: white; text-decoration: none; border-radius: 4px; font-weight: bold; font-family: Arial, sans-serif; }";
    html += ".btn:hover { background-color: #005be3; }</style></head><body>";

    html += "<div class=\"header\">";
    html += "<div class=\"header-title\">";
    html += "<h2>System Live Log</h2>";
    // Выводим данные
    html += "<p class=\"meta-info\">Время: " + current_time + " | Uptime: " + uptimeStr + "</p>";
    html += "</div>";

    html += "<div>";
    html += "<a href=\"/\" class=\"btn\" style=\"margin-right:8px; background-color:#333;\">На главную</a>";
    html += "<a href=\"/logs\" class=\"btn\">Обновить</a>";
    html += "</div></div>";

    html += "<div class=\"console\" id=\"consoleBlock\">";

    // Считываем буфер логов под защитой мьютекса
    if (xLogMutex != NULL && xSemaphoreTake(xLogMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        if (webLogs.empty())
        {
            html += "Журнал пуст. Ждем событий...<br>";
        }
        else
        {
            for (const auto &logLine : webLogs)
            {
                html += logLine + "\n";
            }
        }
        xSemaphoreGive(xLogMutex);
    }
    else
    {
        html += "Ошибка доступа к буферу журнала...<br>";
    }

    html += "</div>";

    // Небольшой JavaScript-скрипт, чтобы консоль при загрузке автоматически прокручивалась вниз к свежим логам
    html += "<script>var c=document.getElementById('consoleBlock');c.scrollTop=c.scrollHeight;</script>";
    html += "</body></html>";

    wserv.send(200, "text/html", html);
}

// Обработчик для  Metrics
/*
void handleMetrics() {
    String metrics = "";
    if (xSensorsMutex != NULL && xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < SensUnit; i++) {
            if (vSensVal[i].actual && vSensVal[i].mqttId.length() > 1) {
                metrics += "# HELP " + vSensVal[i].mqttId + " " + vSensVal[i].name + "\n";
                metrics += "# TYPE " + vSensVal[i].mqttId + " gauge\n";
                metrics += vSensVal[i].mqttId + " " + String(vSensVal[i].value, 4) + "\n";
            }
        }
        xSemaphoreGive(xSensorsMutex);
    }
    wserv.send(200, "text/plain; version=0.0.4", metrics);
}
*/
void handleNotFound()
{
    wserv.send(404, "text/plain", "404 Not Found");
}

void vHttpServerTask(void *pvParameters)
{
    // Serial.println("[RTOS WebServer] Таска HTTP-сервера стартует");
    logToWeb("[RTOS WebServer] Таска HTTP-сервера стартует");
    static const char *TAG = "http_server";

    // 1. Инициализируем mDNS-респондер
    // Переменная hostname должна быть доступна (через extern или из settings.h)
    if (!MDNS.begin(CONF_HOSTNAME))
    {
        ESP_LOGE(TAG, "Error setting up MDNS responder!");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "mDNS responder started");
    // Serial.printf("[RTOS WebServer] mDNS responder started with name ", CONF_HOSTNAME);
    logToWeb("[RTOS WebServer] mDNS responder started with name " + String(CONF_HOSTNAME));

    // Инициализация путей...
    wserv.on("/", handleRoot);
    wserv.on("/logs", handleLogs); // Регистрация эндпоинта новой страницы логов
                                   // wserv.on("/metrics", handleMetrics);
    wserv.onNotFound(handleNotFound);

    //  объявляем в сеть, что у нас крутится HTTP-сервер
    MDNS.addService("http", "tcp", 80);

    // старт сервера
    wserv.begin();
    ESP_LOGI(TAG, "HTTP server started");
    // Serial.println("[RTOS WebServer] WebServer успешно слушает порт 80");
    logToWeb("[RTOS WebServer] WebServer initialized, online.");

    //  Регистрируем ТЕКУЩУЮ таску в системе Watchdog
    esp_task_wdt_add(NULL);

    for (;;)
    {
        // "Кормим" ватчдог в начале каждого цикла
        esp_task_wdt_reset();
        // Если Wi-Fi подключен, обрабатываем клиентов
        if (WiFi.status() == WL_CONNECTED)
        {
            wserv.handleClient();
        }

        // Спим 5 мс, чтобы дать планировщику FreeRTOS обрабатывать Wi-Fi стек
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
