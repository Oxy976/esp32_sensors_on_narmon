#include "wifi_server.h"
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include "strct.h"
#include "settings.h"
#include <ESPmDNS.h>
#include "sys_time.h"
#include "sensors.h"

#include <NimBLEDevice.h>

// Структура для плотной упаковки данных датчиков в BLE пакет (макс 20 байт)
struct BLEPayload
{
    float fBleD1;      // 4 байта (Возьмем, например, vSensVal[0].value)
    float fBleD2;      // 4 байта (Возьмем, например, vSensVal[1].value)
    float fBleD3;      // 4 байта (Возьмем, например, vSensVal[2].value или давление)
    float fBleD4;      // 4 байта
    uint16_t packetId; // 2 байта (Счетчик пакетов для отслеживания обновлений на e-ink)
} __attribute__((packed));

static uint16_t blePacketCounter = 0;
// ---

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

// Обработчик веб-сервера с защитой вызова калибровки мьютексом
void handleSCD30CalibRequest()
{
    static const char *TAG = "Cl_SCD30";
    logToWeb("[" + String(TAG) + "] Получен запрос на принудительную калибровку SCD30...");

    // БЕЗОПАСНОСТЬ: Закрываем вызов функции мьютексом шины I2C на 200 мс
    if (xSensorsMutex != NULL && xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(200)) == pdTRUE)
    {
        SCD30Calibration();            // Безопасный вызов в защищенной секции
        xSemaphoreGive(xSensorsMutex); // Обязательно освобождаем шину!

        logToWeb("[" + String(TAG) + "] Команда калибровки (415 ppm) успешно отправлена.");
        wserv.send(200, "text/plain", "OK");
    }
    else
    {
        logToWeb("[" + String(TAG) + "] Ошибка: Не удалось получить доступ к шине I2C!");
        wserv.send(503, "text/plain", "I2C Bus Busy");
    }
}

// Главная страница
void handleRoot()
{
    String current_time = getSystemTimeStr();
    String uptimeStr = getUptimeStr();

    // Формируем HTML страницу

    String html;
    html.reserve(6144); // Выделяем память заранее для избежания дефрагментации кучи

    html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<link rel=\"icon\" href=\"data:,\">";
    html += "<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; background-color: #f4f6f9; margin: 8px; padding: 0; font-size: 14px; }";
    html += "h1 { font-size: 18px; margin: 8px 0 2px 0; color: #333; }";
    html += "p { margin: 2px 0 10px 0; font-size: 12px; color: #666; }";
    html += "table { border-collapse: collapse; width: 95%; max-width: 440px; margin: 0 auto; box-shadow: 0 2px 4px rgba(0,0,0,0.05); font-size: 13px; }";
    html += "th { padding: 6px 8px; background-color: #0043af; color: white; font-size: 13px; }";
    html += "tr { border: 1px solid #C0C0C0; } tr:hover { background-color: #e8e8e8; } td { padding: 5px 8px; }";
    html += ".actual { color: black; font-weight: bold; background-color: #ffffff; }";
    html += ".not_actual { color: #A0A0A0; font-weight: normal; background-color: #fafafa; }";
    // Стили кнопок управления
    html += ".btn { display: inline-block; padding: 6px 14px; margin: 12px 4px 0 4px; background-color: #333; color: white; text-decoration: none; border-radius: 4px; font-weight: bold; font-size: 12px; border: none; cursor: pointer; }";
    html += ".btn-calib { background-color: #b3392b; }</style>";

    // JavaScript скрипт асинхронной отправки
    html += "<script>function runSCD30Calib() {";
    html += "if (confirm('Вы уверены, что хотите запустить калибровку SCD30 на 415 ppm?')) {";
    html += "fetch('/calib_scd30').then(r => r.ok ? alert('Команда отправлена!') : alert('Ошибка шины.'));";
    html += "}}</script></head><body>";

    html += "<h1>ESP32 Метеостанция</h1>";
    html += "<p>Время: " + current_time + " | Uptime: " + uptimeStr + "</p>";
    html += "<table><tr><th>#</th><th>Параметр</th><th>Значение</th><th>Ед.</th></tr>";

    if (xSensorsMutex != NULL && xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(200)) == pdTRUE)
    {
        for (int i = 0; i < SensUnit; i++)
        {
            html += vSensVal[i].actual ? "<tr class=\"actual\">" : "<tr class=\"not_actual\">";
            html += "<td>" + String(i) + "</td><td style=\"text-align: left;\">" + vSensVal[i].name + "</td>";
            html += "<td style=\"font-family: monospace; font-size: 14px;\">" + String(vSensVal[i].value, 2) + "</td>";
            html += "<td>" + vSensVal[i].unit + "</td></tr>";
        }
        xSemaphoreGive(xSensorsMutex);
    }
    else
    {
        html += "<tr><td colspan='4' style='color:red; padding: 10px;'>Данные обновляются мьютексом...</td></tr>";
    }

    html += "</table>";

    // Блок кнопок
    html += "<div style=\"text-align: center;\">";
    html += "<a href=\"/logs\" class=\"btn\">Системный лог</a>";
    html += "<a href=\"/update\" class=\"btn\" style=\"background-color:#6c757d;\">Обновление ПО</a>";
    html += "<button onclick=\"runSCD30Calib()\" class=\"btn btn-calib\">FRC_SCD30</button></div></body></html>";

    wserv.send(200, "text/html", html);
}

// Страница логов
void handleLogs()
{
    String current_time = getSystemTimeStr();
    String uptimeStr = getUptimeStr();

    //  Формируем HTML страницу
    String html;
    html.reserve(8192); // Логи занимают много места! Пре аллокация обязательна

    html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Системный журнал</title>";
    html += "<style>body { background-color: #121212; color: #00ff00; font-family: monospace; padding: 15px; margin: 0; }";
    html += ".console { background-color: #000; border: 1px solid #333; padding: 15px; height: 70vh; overflow-y: auto; white-space: pre-wrap; font-size: 13px; }";
    html += ".header { display: flex; justify-content: space-between; align-items: center; color: #fff; font-family: Arial; border-bottom: 1px solid #222; padding-bottom: 8px; }";
    html += ".btn { padding: 5px 10px; font-size: 12px; background-color: #0043af; color: white; text-decoration: none; border-radius: 4px; font-weight: bold; }</style></head><body>";

    html += "<div class=\"header\"><div><h2>System Live Log</h2>";
    html += "<p style=\"margin:0; font-size:12px; color:#e0e0e0;\">Время: " + current_time + " | Uptime: " + uptimeStr + "</p></div>";
    html += "<div><a href=\"/\" class=\"btn\" style=\"background-color:#333; margin-right:8px;\">На главную</a><a href=\"/logs\" class=\"btn\">Обновить</a></div></div>";
    html += "<div class=\"console\" id=\"cBlock\">";

    if (xLogMutex != NULL && xSemaphoreTake(xLogMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (webLogs.empty())
        {
            html += "Журнал пуст. Ждем событий...\n";
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
        html += "Ошибка блокировки буфера логов...\n";
    }

    html += "</div>";

    // Небольшой JavaScript-скрипт, чтобы консоль при загрузке автоматически прокручивалась вниз к свежим логам
    html += "<script>var c=document.getElementById('consoleBlock');c.scrollTop=c.scrollHeight;</script>";
    html += "</body></html>";

    wserv.send(200, "text/html", html);
}

// Форма загрузки файла прошивки (.bin)
void handleUpdateForm()
{
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>OTA Update</title>";
    html += "<style>body{background:#f4f6f9;font-family:Arial;text-align:center;padding:50px;}";
    html += ".box{background:#fff;padding:30px;border-radius:8px;display:inline-block;box-shadow:0 2px 10px rgba(0,0,0,0.1);}</style></head><body>";
    html += "<div class='box'><h2>Обновление прошивки ESP32</h2>";
    html += "<form method='POST' action='/update_upload' enctype='multipart/form-data' id='uForm'>";
    html += "<input type='file' name='update' accept='.bin' required><br><br>";
    html += "<input type='submit' value='Загрузить и обновить' style='padding:8px 20px; background:#0043af; color:#fff; border:none; border-radius:4px; cursor:pointer;'>";
    html += "</form><div id='prg' style='margin-top:15px; font-weight:bold; color:#b3392b;'></div></div>";
    html += "<script>document.getElementById('uForm').onsubmit = function(){ document.getElementById('prg').innerHTML = 'Файл отправляется. Пожалуйста, подождите...'; };</script>";
    html += "<br><br><a href='/'>На главную</a></body></html>";
    wserv.send(200, "text/html", html);
}

void handleNotFound()
{
    wserv.send(404, "text/plain", "404 Not Found");
}

// Фоновая задача сервера
void vHttpServerTask(void *pvParameters)
{
    static const char *TAG = "http_server";
    // logToWeb("[" + String(TAG) + "] Таска HTTP-сервера стартует");

    // 1. Инициализируем mDNS-респондер
    // Переменная hostname должна быть доступна (через extern или из settings.h)
    /*  if (!MDNS.begin(CONF_HOSTNAME))
       {
           ESP_LOGE(TAG, "Error setting up MDNS responder!");
           while (1)
           {
               vTaskDelay(pdMS_TO_TICKS(1000));
           }
       }
       */
    if (!MDNS.begin(CONF_HOSTNAME))
    {
        ESP_LOGE(TAG, "Error setting up MDNS responder!");
        vTaskDelete(NULL); // <--- Безопасное уничтожение упавшей задачи
    }
    else
    {
        ESP_LOGI(TAG, "mDNS responder started");
        logToWeb("[" + String(TAG) + "] mDNS responder started with name " + String(CONF_HOSTNAME));
    }

    // Инициализация путей...
    wserv.on("/", handleRoot);
    wserv.on("/logs", handleLogs);
    wserv.on("/calib_scd30", handleSCD30CalibRequest);
    wserv.on("/update", HTTP_GET, handleUpdateForm);

    // Переработчик самого процесса закачки бинарника
    wserv.on("/update_upload", HTTP_POST, []()
             {
        wserv.sendHeader("Connection", "close");
        wserv.send(200, "text/html;charset=UTF-8", Update.hasError() ? "<h3>Ошибка обновления! Проверьте файл.</h3><a href='/update'>Назад</a>" : "<h3>Успешно обновлено! ESP32 перезагружается...</h3><script>setTimeout(function(){window.location.href='/';},5000);</script>");
        delay(1000);
        ESP.restart(); }, []()
             {
        HTTPUpload& upload = wserv.upload();
        if (upload.status == UPLOAD_FILE_START) {
            logToWeb("[OTA] Начало загрузки прошивки: " + upload.filename);
            
            // Захватываем мьютекс датчиков навсегда на время прошивки, чтобы другие таски не дергали I2C/SPI
            if (xSensorsMutex != NULL) {
                xSemaphoreTake(xSensorsMutex, portMAX_DELAY);
            }

            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                logToWeb("[OTA] Успешно прошито: " + String(upload.totalSize) + " байт.");
            } else {
                Update.printError(Serial);
            }
            if (xSensorsMutex != NULL) {
                xSemaphoreGive(xSensorsMutex); // На всякий случай возвращаем
            }
        } });

    wserv.onNotFound(handleNotFound);

    //  объявляем в сеть, что у нас крутится HTTP-сервер
    MDNS.addService("http", "tcp", 80);

    // старт сервера
    wserv.begin();
    ESP_LOGI(TAG, "HTTP server started");
    logToWeb("[" + String(TAG) + "] WebServer initialized with OTA Update.");

    //  Регистрируем ТЕКУЩУЮ таску в системе Watchdog
    esp_task_wdt_add(NULL);

    // for BLE
    unsigned long lastBleUpdate = 0;

    for (;;)
    {
        // "Кормим" ватчдог в начале каждого цикла
        esp_task_wdt_reset();
        // Если Wi-Fi подключен, обрабатываем клиентов
        if (WiFi.status() == WL_CONNECTED)
        {
            wserv.handleClient();
        }

        // --- ОБНОВЛЕНИЕ И ОТПРАВКА ДАННЫХ В BLE ЭФИР ---
        // Обновляем данные в эфире раз в 5 секунд (5000 мс)
        // --- ОБНОВЛЕНИЕ И ОТПРАВКА ДАННЫХ В BLE ЭФИР ---
        if (millis() - lastBleUpdate >= 5000)
        {
            lastBleUpdate = millis();

            if (xSensorsMutex != NULL && xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                BLEPayload payload;

                // Наполняем структуру данными с датчиков
                payload.fBleD1 = vSensVal[0].actual ? vSensVal[0].value : 0.0f;
                payload.fBleD2 = vSensVal[10].actual ? vSensVal[10].value : 0.0f;
                payload.fBleD3 = vSensVal[13].actual ? vSensVal[13].value : 0.0f;
                payload.fBleD4 = vSensVal[2].actual ? vSensVal[2].value : 0.0f;
                payload.packetId = blePacketCounter++;

                xSemaphoreGive(xSensorsMutex); 

                // Упаковываем структуру в сырую байтовую строку
                std::string strData((char *)&payload, sizeof(payload));

                BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
                if (pAdvertising != NULL)
                {
                    // ПАКЕТ А (Основной): Передает исключительно метеоданные (22 байта)
                    BLEAdvertisementData oAdvertisementData;
                    oAdvertisementData.setManufacturerData(strData); 
                    pAdvertising->setAdvertisementData(oAdvertisementData);

                    // ПАКЕТ Б (Scan Response): Сюда выносим имя "M5_DATA"
                    // Оно гарантированно будет считано e-ink станцией через NimBLE
                    BLEAdvertisementData oScanResponseData;
                    oScanResponseData.setName("M5_DATA");
                    pAdvertising->setScanResponseData(oScanResponseData);

                    // Если трансляция по какой-то причине остановилась — перезапускаем
                    if (!pAdvertising->isAdvertising())
                    {
                        pAdvertising->start();  
                        logToWeb("[BLE] Трансляция пакета M5_DATA запущена.");
                    }
                    else
                    {
                        // Обновляем данные в эфире без перезапуска радиомодуля
                        pAdvertising->start();
                    }
                }
            }
            else
            {
                logToWeb("[BLE] Ошибка: Датчики заняты мьютексом, пропуск отправки BLE");
            }
        }

        // Спим 5 мс, чтобы дать планировщику FreeRTOS обрабатывать Wi-Fi стек
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
