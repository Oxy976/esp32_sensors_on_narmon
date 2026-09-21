#include "wifi_server.h"
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "strct.h"    // Подключаем вашу структуру датчиков
#include "settings.h" // Подключаем настройки (SensUnit)
#include <ESPmDNS.h>

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
    struct tm timeinfo;
    unsigned long upTime_sec = 0;
    int upTime_d = 0;
    int upTime_h = 0;
    int upTime_m = 0;
    int upTime_s = 0;

    // Собираем HTML-страницу, используя текущие данные из вашего массива vSensVal
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<link rel=\"icon\" href=\"data:,\">";
    // Добавляем стили CSS
    html += "<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial;}";
    html += "table { border-collapse: collapse; width:35%; margin-left:auto; margin-right:auto; }";
    html += "th { padding: 10px; background-color: #0043af; color: white; }";
    html += "tr { border: 1px solid #C0C0C0; padding: 10px; }";
    html += "tr:hover { background-color: #bcbcbc; }";
    html += "td { border: none; padding: 10px; }";
    html += ".actual { color: black; font-weight: bold; background-color: #e3e3e3; padding: 1px; }";
    html += ".not_actual { color: #DCDCDC; font-weight: normal; background-color: white; padding: 1px; }";
    html += ".btn { display: inline-block; padding: 10px 20px; margin-top: 20px; background-color: #333; color: white; text-decoration: none; border-radius: 4px; font-weight: bold; }";
    html += ".btn:hover { background-color: #555; }</style></head><body>";

    html += "<title>M5Stack Метеостанция</title>";
    html += "<h1>ESP32 sensors</h1>";

    html += "<div class=\"card\">";
    if (getLocalTime(&timeinfo))
    {
        html += "on time ";

        char timeBuffer[32];
        // %d - день (01-31), %m - месяц (01-12), %Y - год (4 цифры)
        // %H - часы (24ч), %M - минуты, %S - секунды
        strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y %H:%M:%S", &timeinfo);
        html += timeBuffer;
        html += "</br>";
    }
    html += "up time ";
    html += upTime_sec;
    html += "sec (";
    html += upTime_d;
    html += "days  ";
    html += upTime_h;
    html += ":";
    html += upTime_m;
    html += ":";
    html += upTime_s;
    html += ")</br>";

    // --- ЗАЩИТА ЧТЕНИЯ ---
    if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {

        html += "<table><tr><th>#</th><th>Name</th><th>VALUE</th><th>Unit</th></tr>";

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
            html += String(i);
            html += "</td><td>";
            html += vSensVal[i].name;
            html += "</td><td>";
            html += String(vSensVal[i].value, 2);
            html += "</td><td>";
            html += vSensVal[i].unit;
            html += "</td></span></tr>";
            // vTaskDelay(5);
        }
        html += "</table>";

        xSemaphoreGive(xSensorsMutex); // Прочитали? Сразу отдаем ключ обратно!
    }
    else
    {
        // Если датчики как раз сейчас пишут данные, вежливо просим пользователя обновить страницу
        html += "<p style='color:red;'>Данные обновляются, пожалуйста, обновите страницу через секунду...</p>";
    }

    // Добавляем красивую кнопку перевода на страницу логов
    html += "<a href=\"/logs\" class=\"btn\">Открыть системный лог</a>";
    html += "</div></body></html>";

    // Отправляем HTTP-ответ 200 OK
    wserv.send(200, "text/html", html);
}

// 2. ОТДЕЛЬНАЯ СТРАНИЦА ЛОГОВ (/logs)
void handleLogs()
{
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>Системный журнал</title>";
    html += "<style>body { background-color: #121212; color: #00ff00; font-family: 'Courier New', monospace; padding: 20px; margin: 0; }";
    html += ".console { background-color: #000000; border: 1px solid #333; padding: 15px; border-radius: 5px; height: 75vh; overflow-y: auto; text-align: left; white-space: pre-wrap; line-height: 1.4; font-size: 14px; }";
    html += ".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; color: #fff; font-family: Arial, sans-serif; }";
    html += ".btn { padding: 8px 16px; background-color: #0043af; color: white; text-decoration: none; border-radius: 4px; font-weight: bold; }";
    html += ".btn:hover { background-color: #005be3; }</style></head><body>";

    html += "<div class=\"header\">";
    html += "<h2>System Live Log</h2>";
    html += "<div>";
    html += "<a href=\"/\" class=\"btn\" style=\"margin-right:10px; background-color:#333;\">На главную</a>";
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
    Serial.println("[RTOS WebServer] Таска HTTP-сервера стартует");
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
    Serial.printf("[RTOS WebServer] mDNS responder started with name ", CONF_HOSTNAME);

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
    Serial.println("[RTOS WebServer] WebServer успешно слушает порт 80");
    logToWeb("System initialized. Web server online."); // Первая тестовая запись

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
