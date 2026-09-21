#include "wifi_server.h"
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "strct.h"    // Подключаем вашу структуру датчиков
#include "settings.h" // Подключаем настройки (SensUnit)
#include <ESPmDNS.h> 

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
    html += "</style></head><body>";

    html += "<title>M5Stack Метеостанция</title>";
    html += "<h1>Мониторинг Датчиков</h1>";

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
            html += i;
            html += "</td><td>";
            html += vSensVal[i].name;
            html += "</td><td>";
            html += vSensVal[i].value;
            html += "</td><td>";
            html += vSensVal[i].unit;
            html += "</td></span></tr>";
            vTaskDelay(5);
        }

        xSemaphoreGive(xSensorsMutex); // Прочитали? Сразу отдаем ключ обратно!
    }
    else
    {
        // Если датчики как раз сейчас пишут данные, вежливо просим пользователя обновить страницу
        html += "<p style='color:red;'>Данные обновляются, пожалуйста, обновите страницу через секунду...</p>";
    }
    html += "</div>";

    html += "</body></html>";

    // Отправляем HTTP-ответ 200 OK
    wserv.send(200, "text/html", html);
}
// Обработчик для несуществующих страниц (Ошибка 404)
/* handleMetrics
void handleMetrics() {
    String metrics = "";
    if (xSensorsMutex != NULL && xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < SensUnit; i++) {
            if (vSensVal[i].actual && vSensVal[i].mqttId.length() > 1) {
                String metricName = vSensVal[i].mqttId;
                metrics += "# HELP " + metricName + " " + vSensVal[i].name + "\n";
                metrics += "# TYPE " + metricName + " gauge\n";
                metrics += metricName + " " + String(vSensVal[i].value, 4) + "\n";
            }
        }
        xSemaphoreGive(xSensorsMutex);
    }
    wserv.send(200, "text/plain; version=0.0.4", metrics);
}
*/
void handleNotFound()
{
    String message = "404 Not Found\n\n";
    message += "URI: " + wserv.uri() + "\nMethod: ";
    message += (wserv.method() == HTTP_GET) ? "GET" : "POST";

    wserv.send(404, "text/plain", message);
}

void vHttpServerTask(void *pvParameters)
{
    Serial.println("[RTOS WebServer] Таска HTTP-сервера стартует");
    static const char *TAG = "http_server";

       // 1. Инициализируем mDNS-респондер 
    // Переменная hostname должна быть доступна (через extern или из settings.h)
    if (!MDNS.begin(CONF_HOSTNAME)) {
        ESP_LOGE(TAG, "Error setting up MDNS responder!");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        }
    }
    ESP_LOGI(TAG, "mDNS responder started");
    Serial.printf("[RTOS WebServer] mDNS responder started with name ", CONF_HOSTNAME);

    // Инициализация путей...
    wserv.on("/", handleRoot);
    // wserv.on("/metrics", handleMetrics);
    wserv.onNotFound(handleNotFound);

    //  объявляем в сеть, что у нас крутится HTTP-сервер
    MDNS.addService("http", "tcp", 80);
    // доменное имя
    // MDNS.begin(hostname);

    // старт сервера
    wserv.begin();
    ESP_LOGI(TAG, "HTTP server started");
    Serial.println("[RTOS WebServer] WebServer успешно слушает порт 80");

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
