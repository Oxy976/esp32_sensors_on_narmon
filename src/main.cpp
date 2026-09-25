/*  план.
 * на потом:
 *
 *   - доработать вывод на экран, чтоб данные помещались все /но места нет/
 *   - время в коде причесать, почистить
 *   - добавить пересчет поправочных коэффициентов относительно доверенного термометра (как вводить данные?)
 * -----
 * сделано:
 *  + watchdog на сеть - если нет роутера, то переподключиться
 *  - закрыть мьютексами   получение данных, экран
 *   - вынести сервер  http в отдельный файл.
 *   - разобраться с логами и вывести в http
 *   - если данные с датчиков читаются по таймеру, то надо разделить локальный и таймер для отправки 
 *   - добавить поправки для температуры и (может) прочих
 * таски:
 *  - получить данные с датчиков /функцией. возможно потом будет таском, но надо будет прописать мьютекс/
 *  - вывести на экран данные - vfnvShowData
 *  - вывести время - vfnShowTime
 *  - отправить на сервер mqtt /функцией, т.к. вызывается из таска 1 раз/
 *  - обработка кнопок - vfnButtonTask
 *  - обработка датчика движения vfnPirTask
 *прерывания
 *  - нажатия на кнопки - vfnButtonISR
 *  - с датчика движения - vfnPirISR
 *  - по времени для отправки на сервер - onTimerISR
 *
 *семафоры
 *  - прерывание по таймеру - pxTimerSemaphore
 *
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <driver/gpio.h>
#include <M5Stack.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_log.h>
#include <Arduino.h>
#include "sdkconfig.h"

#include "settings.h"
#include "wifi_server.h" // Подключаем созданный модуль сервера http
#include "strct.h"

stSens vSensVal[SensUnit];

// --webLogs
// === ИСПРАВЛЕНИЕ КОНФЛИКТА ARDUINO И C++ deque ===
#undef min
#undef max
#include <deque> // Удобный стандартный контейнер C++ для очередей weblog

// Настройки веб-журнала (выделяем память здесь, в главном файле)
const size_t MAX_LOG_LINES = 50;
std::deque<String> webLogs;
SemaphoreHandle_t xLogMutex = NULL; // Мьютекс создадим в setup()

// Хэндл мьютекса для защиты массива vSensVal
SemaphoreHandle_t xSensorsMutex = NULL;

#include "sensors.h"

#include <WiFi.h>
TaskHandle_t httpTaskHandle = NULL; // Хэндл управления таской сервера

#include <WiFiClient.h>
#include <ESPmDNS.h>

boolean bConnWiFi = false;
struct tm timeinfo;

WiFiClient wifiClient;

/*
#include <PubSubClient.h>
PubSubClient mqttClient(CONF_MQTT_SERVER, 1883, wifiClient);
*/

#include "esp_sntp.h" // Нужен для контроля статуса синхронизации NTP

#include "OutToScr.h"

// Set alarm to call onTimer function every  second ( 80 000 000Gz / 8000 * 10000 ).
// 100000 - 10s, 600000 - 1m(60s)  6000000 - 10m  36000000 - 1h(60m)
#define TIMER_PERIOD 6000000 // old, 4del

// таймер обновления данных датчиков
#define TIMER_PERIOD_DATA 600000
// таймер отправки данных на народмонитор
#define TIMER_PERIOD_SEND 6000000

unsigned long startTime = millis();
unsigned long isrPirTime = millis();
unsigned long isrBtnTime = millis();

#define ESP_INTR_FLAG_DEFAULT 0

// метка загрузки, для вывода лога
bool bBooting = true;

// Глобальная функция логирования logToWeb
void logToWeb(String text)
{
  struct tm timeinfo; // ************** заменить на вызов из sys_time ********************
  char timeBuf[32] = "";
  String timeStr = "";

  if (getLocalTime(&timeinfo) && timeinfo.tm_year > 120)
  {
    strftime(timeBuf, sizeof(timeBuf), "[%H:%M:%S] ", &timeinfo);
    timeStr = String(timeBuf);
  }
  else
  {
    timeStr = "[" + String(millis() / 1000) + "s] ";
  }

  if (xLogMutex != NULL && xSemaphoreTake(xLogMutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    webLogs.push_back(timeStr + text);
    if (webLogs.size() > MAX_LOG_LINES)
    {
      webLogs.pop_front();
    }
    xSemaphoreGive(xLogMutex);
  }

  // 2. --- ВЫВОД СТАРТОВОГО ЛОГА НА ЭКРАН С ПОДСВЕТКОЙ ОШИБОК ---
  if (bBooting)
  {
    // Переводим текст в нижний регистр для надежного поиска маркеров
    String lowerText = text;
    lowerText.toLowerCase();

    // Проверяем на критические ошибки (красный цвет)
    if (lowerText.indexOf("not found") != -1 ||
        lowerText.indexOf("failed") != -1 ||
        lowerText.indexOf("error") != -1 ||
        lowerText.indexOf("---") != -1)
    {
      M5.Lcd.setTextColor(RED, BLACK);
    }
    // Проверяем на предупреждения или важные системные шаги (желтый цвет)
    else if (lowerText.indexOf("connecting") != -1 ||
             lowerText.indexOf("wait") != -1 ||
             lowerText.indexOf("already started") != -1)
    {
      M5.Lcd.setTextColor(YELLOW, BLACK);
    }
    // Успешные события (оставляем зеленый или белый терминальный цвет)
    else if (lowerText.indexOf("finded") != -1 ||
             lowerText.indexOf("connected") != -1 ||
             lowerText.indexOf("+++") != -1)
    {
      M5.Lcd.setTextColor(GREEN, BLACK);
    }
    else
    {
      M5.Lcd.setTextColor(WHITE, BLACK); // Обычный информационный текст
    }

    // Выводим строку на экран M5Stack
    M5.Lcd.println(text);
  }

  // Дублируем в аппаратный Serial
  Serial.println(timeStr + text);
}

void showSensVal() //  for TEST!
{
  for (int i = 0; i < SensUnit; i++)
  {
    logToWeb(String(i) + " " + vSensVal[i].name + " " + String(vSensVal[i].value) + " " + vSensVal[i].unit + " " + String(vSensVal[i].actual));
  }
}

// Контроль локального времени в консоли
void printLocalTime()
{
  struct tm t_info; // ************** заменить на вызов из sys_time ?? ********************
  static const char *TAG = "LocalTime";
  char tbuffer[64];

  // Пытаемся считать локальное время
  if (getLocalTime(&t_info))
  {
    // Если год в системе больше 120 (считается от 1900 года, то есть 1900 + 120 = 2020 год)
    if (t_info.tm_year > 120)
    {
      strftime(tbuffer, 80, "%d %b %Y %H:%M:%S", &t_info);
      ESP_LOGI(TAG, "Валидное время: %s", tbuffer);
    }
    else
    {
      ESP_LOGD(TAG, "[NTP] Время в системе дефолтное (1970 год), ждем синхронизации...");
      logToWeb("[NTP] Время в системе дефолтное (1970 год), ждем синхронизации...");
    }
  }
  else
  {
    ESP_LOGE(TAG, "Ошибка NTP получения времени!");
    logToWeb("Ошибка NTP получения времени!");
  }
}

// // Колбэк успешной синхронизации времени по NTP
void timeSyncCallback(struct timeval *tv)
{
  logToWeb("[NTP] Время успешно синхронизировано с сервером интернета!");
  printLocalTime(); // Выводим время в консоль для проверки
}

// Отправка данных на Народный Мониторинг
bool NarodmonTcpPublish()
{
  static const char *TAG = "NmonTcp";
  // WiFiClient client;

  String mac = "30:AE:A4:69:B9:04";
  //  String mac = "30AEA469B904";
  String buf;
  buf.reserve(512);       // Выделяем память один раз с запасом под все датчики
  buf = "#" + mac + "\n"; // заголовок
  for (int i = 0; i < SensUnit; i++)
  {
    if (vSensVal[i].actual & (vSensVal[i].mqttId.length() > 1))
    {
      buf = buf + "#" + vSensVal[i].mqttId + "#" + String(vSensVal[i].value, 2) + "\n"; // #mac1#value1 (#H1#93)
    }
  }
  buf = buf + "##\n"; // закрываем пакет

  // Установка таймаута перед подключением, чтобы не заблокировать таску FreeRTOS
  wifiClient.setTimeout(500);

  if (!wifiClient.connect("narodmon.ru", 8283)) // попытка подключения
  // if (!client.connect("127.0.0.1", 8283)) // попытка подключения  test
  {
    ESP_LOGD(TAG, "Connecting failed");
    logToWeb("[NMon] Connecting failed");
    wifiClient.stop();
    return false; // не удалось;
  }
  else
  {
    ESP_LOGI(TAG, "Connected to narodmon, sending data string");
    logToWeb("[NMon] Connected to narodmon, sending data string");
    wifiClient.print(buf); // и отправляем данные

    // Быстро вычитываем ответ, если он есть  |  стоит убрать что-б не тормозило. Все равно не отдает. **************
    while (wifiClient.available())
    {
      String line = wifiClient.readStringUntil('\r'); // если что-то в ответ будет | readStringUntil ждет до 1с стоит убрать что-б не тормозило. Все равно не отдает.
      ESP_LOGD(TAG, "string from site: %s", line);
      logToWeb("[NMon] string from site: " + line);
    }
    wifiClient.stop();
    return true; // ушло
  }
}

// ==MQTT ==публикация
/*  narodmon mqtt больше бесплатно не понимает,  не используется | можно убрать. вместе с объявлением PubSubClient mqttClient
void MqttPublish()  //narodmon mqtt
{
  static const char *TAG = "mqtt";

  int count_reconnect = 0;
  // если не подключен, то подключаемся.
  if (!!!mqttClient.connected())
  {
    ESP_LOGI(TAG, "Reconnecting client to %s", CONF_MQTT_SERVER);
    logToWeb("[MQTT] Reconnecting client");
    while (!!!mqttClient.connect(CONF_CLIENT_ID, CONF_AUTH_METHOD, CONF_TOKEN, CONF_CONN_TOPIC, 0, 0, "online"))
    {
      vTaskDelay(500);
      count_reconnect++;
      // больше 10 попыток - что-то не так...
      if (count_reconnect > 10)
      {
        ESP_LOGI(TAG, "problem with connecting to server !! **");
        logToWeb("[MQTT] problem with connecting to server !! **");
        // ESP.restart();
      }
    }
    ESP_LOGI(TAG, "Connecting to %s with: id %s, auth %s, token %s", CONF_MQTT_SERVER, CONF_CLIENT_ID, CONF_AUTH_METHOD, CONF_TOKEN);
  }

  for (int i = 0; i < SensUnit; i++)
  {
    if (vSensVal[i].actual & (vSensVal[i].mqttId.length() > 1))
    {
      String topic = TOPIC;
      String payload = String(vSensVal[i].value, 1);                  // значение строкой
      topic.concat(vSensVal[i].mqttId);                               // topic+id
      if (mqttClient.publish(topic.c_str(), (char *)payload.c_str())) // если опубликовано
      {
        ESP_LOGD(TAG, "Publishing Ok on: %s payload:  %s", topic.c_str(), payload);
      }
      else
      {
        ESP_LOGD(TAG, "** Publish FAILED on: %s payload:  %s", topic.c_str(), payload);
      }
    }
  }
}

*/

volatile SemaphoreHandle_t pxShowDataSemaphore;
void vfnvShowData(void *vpArg)
{
  static const char *TAG = "taskShowData";
  while (1)
  {
    xSemaphoreTake(pxShowDataSemaphore, portMAX_DELAY); // Программа тут свалится в WAIT до тех пор пока не появится семафор
    ESP_LOGD(TAG, "Task show data");
    // --- ЗАЩИТА МЬЮТЕКСОМ ---
    // Пытаемся взять мьютекс, ждем максимум 100 мс
    if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {

      //  getSensData(vSensVal); // Безопасно получаем-записываем новые данные |  удалено - данные получает vfnTimerTask, не надо ему мешать
      OutToScr(vSensVal); // Безопасно выводим их на дисплей M5Stack

      xSemaphoreGive(xSensorsMutex); // Обязательно освобождаем!
    }
    else
    {
      ESP_LOGW(TAG, "Не удалось обновить экран: данные заняты другой таской");
    }

    // getSensData(vSensVal); // получить данные 4del
    // OutToScr(vSensVal);    // показать данные 4del
  }
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL);
}

volatile SemaphoreHandle_t pxShowTimeSemaphore;
void vfnShowTime(void *vpArg)
/* может конфликтовать за вывод на экран, в теории. Закрывается мьютексом */
{
  static const char *TAG = "taskShowTime";
  while (1)
  {
    xSemaphoreTake(pxShowTimeSemaphore, portMAX_DELAY); // Программа тут свалится в WAIT до тех пор пока не появится семафор
    ESP_LOGD(TAG, "Task show time");
    // Защищаем экран мьютексом от наползания данных датчиков на время | но вотнадо-ли? Но не мешает.
    if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      ShowTime();
      xSemaphoreGive(xSensorsMutex);
    }
  }
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL);
}

#pragma region timer
/*
// interrupt on timer
// Объявление указателя на структуру аппаратного таймера ESP32
static hw_timer_t *timer = NULL;
//  семафор для синхронизации прерывания таймера и управляющей задачи
volatile SemaphoreHandle_t pxTimerSemaphore;
// Инициализация структуры для создания критических секций (блокировка прерываний на время работы с общими ресурсами)
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED; // to frame  critical portions
// Функция прерывания аппаратного таймера, выполняемая в IRAM (быстрой памяти)
static void IRAM_ATTR onTimerISR()
{
  // Разблокирует семафор из прерывания, сообщая задаче vfnTimerTask, что пора просыпаться
  xSemaphoreGiveFromISR(pxTimerSemaphore, NULL);
}


// Основная задача FreeRTOS, которая обрабатывает события таймера
static void vfnTimerTask(void *vpArg)
{
  static const char *TAG = "timer";
  while (1)
  {
    // Задача засыпает и ждет семафор от onTimerISR бесконечно долго (portMAX_DELAY), не загружая процессор
    xSemaphoreTake(pxTimerSemaphore, portMAX_DELAY);
    // Сброс сторожевого таймера (WDT) для текущего ядра, чтобы ESP32 не ушел в перезагрузку
    esp_task_wdt_reset();
    // Отправка отладочного лога уровня DEBUG в консоль
    ESP_LOGD(TAG, "Timer interrupt now");
    // сюда попадаем только если есть семафор

    // printLocalTime(); // а надо????

    // --- ЗАЩИТА ЗАПИСИ ---
    // Ждем освобождения мьютекса максимум 100 мс (не блокируем таску намертво)
    if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      // Если мьютекс успешно захвачен, безопасно считываем данные с физических датчиков в массив vSensVal
      getSensData(vSensVal);
      // Возвращаем мьютекс обратно, открывая доступ другим задачам
      xSemaphoreGive(xSensorsMutex);
    }
    else
    {
      ESP_LOGW(TAG, "Не удалось получить доступ для записи данных (занято сервером)");
    }

    //  отправить данные на narodmon (семафор для таска?) если wifi подключен
    if (bConnWiFi)
    {
      esp_task_wdt_reset(); // Кормим ПЕРЕД отправкой т.к. может затянуться
      // MqttPublish();   // 4test
      NarodmonTcpPublish();
      esp_task_wdt_reset(); // Кормим СРАЗУ ПОСЛЕ отправки
    }
    // запусить прогрев датчиков (семафор для таска?)
    //  Если влажность (HTU_e_humi или SHT_e_humi) >70% - прогреть
    if (vSensVal[8].value > 70.0 || vSensVal[10].value > 70.0)
    {
      heatSens();
    }
  }
  // Сюда код дойдет только при сбое цикла (в реальности — никогда из-за while(1))
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL); // remove the task whene done
}
  */

// --- ЗАДАЧА 1: Опрос датчиков каждую 1 минуту ---
static void vfnSensorUpdateTask(void *vpArg)
{
  static const char *TAG = "sensor_task";

  // Инициализируем счетчик времени для точного выдерживания интервалов
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xReadDataTicks = pdMS_TO_TICKS(TIMER_PERIOD_DATA); // 60 000 мс = 1 минута

  while (1)
  {
    // Задача засыпает ровно на 1 минуту с учетом времени, потраченного на выполнение кода
    vTaskDelayUntil(&xLastWakeTime, xReadDataTicks);

    esp_task_wdt_reset(); // Кормим ватчдог ядра
    ESP_LOGD(TAG, "start reading data from sensors");
    logToWeb("[" + String(TAG) + "] start reading data from sensors");

    // Защищаем массив мьютексом на время записи данных с физических шин
    if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      getSensData(vSensVal);         // Считываем физические показатели
      xSemaphoreGive(xSensorsMutex); // Освобождаем мьютекс
    }
    else
    {
      ESP_LOGW(TAG, "Датчики заняты сервером, пропуск минутного замера");
    }
  }
  vTaskDelete(NULL);
}

// --- ЗАДАЧА 2: Опрос и отправка на Народный Мониторинг каждые 10 минут ---
static void vfnNetworkSendTask(void *vpArg)
{
  static const char *TAG = "network_task";

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xSendToNetTicks = pdMS_TO_TICKS(TIMER_PERIOD_SEND);

  while (1)
  {
    // Задача засыпает ровно на 10 минут
    vTaskDelayUntil(&xLastWakeTime, xSendToNetTicks);

    esp_task_wdt_reset();
    ESP_LOGI(TAG, "start sendind data from sensors to net");
    logToWeb("[" + String(TAG) + "] start sendind data from sensors to net");

    // Перед отправкой делаем свежий замер данных
    if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(200)) == pdTRUE)
    {
      getSensData(vSensVal);
      xSemaphoreGive(xSensorsMutex);
    }

    // Если Wi-Fi подключен — отправляем пакет на сервер
    if (bConnWiFi)
    {
      esp_task_wdt_reset(); // Кормим ПЕРЕД тяжелой сетевой сессией
      NarodmonTcpPublish();
      esp_task_wdt_reset(); // Кормим СРАЗУ ПОСЛЕ отправки
    }
    else
    {
      ESP_LOGI(TAG, "NO network connection, data not sent");
      logToWeb("[" + String(TAG) + "] NO network connection, data not sent");
    }

    // Проверка необходимости прогрева датчиков во время отправки пакета
    // Если влажность (датчик 8 или 10) > 90% - включаем подогрев
    if (vSensVal[8].value > 90.0 || vSensVal[10].value > 90.0)
    {

      if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(200)) == pdTRUE)
      {
        heatSens();
        xSemaphoreGive(xSensorsMutex);
      }
    }
  }
  vTaskDelete(NULL);
}

#pragma endregion

#pragma region buttons
static const char *TAG = "gpio_button"; // tag for logging
// Хендл для управления задачей обработки кнопок (используется для отправки ей уведомлений)
static TaskHandle_t xButtonHandle = NULL; // task handle for interrupt callback
// Хендл для управления задачей обработки PIR датчика
static TaskHandle_t xPirHandle = NULL; // task handle for interrupt callback

// Функция прерывания для аппаратных кнопок платы (работает в IRAM)
static void IRAM_ATTR vfnButtonISR(void *vpArg)
{

  // Приведение типа аргумента к номеру GPIO, который вызвал прерывание (передается при настройке прерывания)
  uint32_t ulGPIONumber = (uint32_t)vpArg;
  // ESP_LOGD(TAG, "ISR GPIO %d is %d",ulGPIONumber,gpio_get_level((gpio_num_t)ulGPIONumber)); // ****** TEST *** УБРАТЬ!****
  // Проверяем физический уровень на пине: если он равен 0 (кнопка прижата к земле при нажатии)
  if (gpio_get_level((gpio_num_t)ulGPIONumber) == 0) // по нажатию
  {
    // Отправляем уведомление задаче обработки кнопок
    xTaskNotifyFromISR(xButtonHandle,     // Какую задачу разбудить
                       ulGPIONumber - 37, // Передаем значение: вычитаем 37, чтобы получить индекс 0, 1 или 2 для кнопок 37, 38, 39
                       eSetBits,          // Устанавливаем биты в значении уведомления
                       NULL);             // Флаг необходимости переключения контекста на более приоритетную задачу
  }
  // Запрос смены контекста процессора: если разбуженная задача приоритетнее текущей, управление сразу передается ей
  portYIELD_FROM_ISR();
}

// Функция прерывания для датчика движения PIR (работает в IRAM)
static void IRAM_ATTR vfnPirISR(void *vpArg)
{
  // Получаем номер GPIO датчика PIR
  uint32_t ulGPIONumber = (uint32_t)vpArg;

  // Проверяем уровень: датчик движения выдает логическую 1 (HIGH) при обнаружении объекта
  if (gpio_get_level((gpio_num_t)ulGPIONumber) == 1)
  {
    // Отправляем уведомление задаче обработки PIR
    xTaskNotifyFromISR(xPirHandle,        // Какую задачу разбудить
                       ulGPIONumber - 36, // Передаем значение (36 - 36 = 0)
                       eSetBits,          // notify action (pass value in this case) // Устанавливаем биты
                       NULL);             // wake a higher prio task (default behavior)
  }
  // Переключаем контекст, если задача PIR приоритетнее прерванной задачи
  portYIELD_FROM_ISR(); // if notified task has higher prio then current interrupted task, set it to the head of the task queue
}

// Задача обработки нажатий кнопок
static void vfnButtonTask(void *vpArg)
{
  // Переменная, куда запишется переданное из прерывания значение (номер кнопки)
  uint32_t ulNotifiedValue = 0;
  // Переменная для проверки статуса получения уведомления (pdPASS или pdFAIL)
  BaseType_t xResult;

  while (1)
  {
    // Задача засыпает и ожидает уведомление от прерывания
    xResult = xTaskNotifyWait(pdFALSE,          // Не очищать биты при входе
                              0xFFFFFFFF,       // Очистить все биты при выходе (сбросить маску)
                              &ulNotifiedValue, // Переменная для записи значения
                              portMAX_DELAY);   // Ждать бесконечно долго

    // АНТИДРЕБЕЗГ КНОПОК: Если уведомление получено И с момента прошлого успешного нажатия прошло более 200 мс
    if ((xResult == pdPASS) && (millis() - isrBtnTime > 200ul))
    {
      // Записываем лог
      ESP_LOGD(TAG, "HW button interrupt now");
      logToWeb("[GPIO_BTN]HW button interrupt now");
      // Обновляем глобальный таймер последнего нажатия текущим временем millis()
      isrBtnTime = millis();

      // Анализируем, какая именно кнопка была нажата (значение `ulGPIONumber - 37`)
      switch (ulNotifiedValue)
      {
      case 0: // Кнопка GPIO 37 & PIR
        // Показ даты-времени на экране
        ESP_LOGD(TAG, "==button 0==");
        logToWeb("[" + String(TAG) + "] button 0&PIR");
        ESP_LOGD(TAG, "give semaphore time");
        // Отдаем семафор задаче, которая отвечает за вывод времени на дисплей
        xSemaphoreGive(pxShowTimeSemaphore);
        break;
      case 1: // Кнопка GPIO 38
        // Показ погодных показателей на экране
        ESP_LOGD(TAG, "==button 1==");
        logToWeb("[" + String(TAG) + "] button 1");
        ESP_LOGD(TAG, "give semaphore data");
        // Отдаем семафор задаче, которая выводит  данные датчиков на дисплей
        xSemaphoreGive(pxShowDataSemaphore);
        break;
      case 2: // Кнопка GPIO 39
        // Значения датчиков - в лог
        ESP_LOGD(TAG, "==button 2==");
        logToWeb("[" + String(TAG) + "] button 2");
        printLocalTime();
        // Вызываем функцию вывода значений всех датчиков в локальный лог
        showSensVal(); // TEST!
        // NarodmonTcpPublish();  // ****************TEST**********
        break;
      default:
        // Обработка непредвиденных значений маски уведомления
        ESP_LOGD(TAG, "This should not happen...");
        break;
      }
    }
  }
  // Код отладки на случай критического завершения таски
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL); // remove the task whene done
}

// Задача обработки датчика движения PIR
static void vfnPirTask(void *vpArg)
{
  uint32_t ulNotifiedValue = 0; // получаем, но не используем
  BaseType_t xResult;

  while (1)
  {
    // Ждем уведомление от прерывания vfnPirISR бесконечно долго
    xResult = xTaskNotifyWait(pdFALSE, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);
    // АНАЛИЗ ЗАДЕРЖЕК PIR:
    // 1. Проверка на получение уведомления (xResult == pdPASS)
    // 2. Слепой интервал (millis() - isrPirTime > 5000ul): игнорируем датчик в течение 5 сек после прошлого срабатывания
    // 3. Задержка старта (millis() > 60000ul): датчик полностью игнорируется в первые 60 секунд работы ESP32
    if ((xResult == pdPASS) && (millis() - isrPirTime > 5000ul) && (millis() > 60000ul))
    {
      // Вывод в лог факта обнаружения движения на пине 36
      ESP_LOGD(TAG, "HW PIR interrupt now (pin 36)");
      logToWeb("HW PIR interrupt now (pin 36)");
      // ESP_LOGD(TAG, "give semaphore data");
      //  Отдаем семафор задаче экрана, чтобы включить его или показать данные
      xSemaphoreGive(pxShowDataSemaphore);
      // Запоминаем время текущего срабатывания PIR датчика
      isrPirTime = millis();
    }
  }
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL); // remove the task whene done
}

#pragma endregion

#pragma region Wifi

// ==WIFI ================
void setup_wifi()
{
  static const char *TAG = "wifi";

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(10)); // delay(10);
  ESP_LOGI(TAG, "Connecting to %s", CONF_SSID);
  logToWeb("[WiFi] Connecting to " + String(CONF_SSID));

  // WiFi.begin(ssid, password);
  WiFi.begin(CONF_SSID, CONF_PASSWORD);

  // если за 5*500 не подключился - прекратить
  int wifiCounter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(500)); // delay(500);
    logToWeb("[WiFi] trying to connect ");
    // Serial.print("#");
    // Serial.println();
    if (++wifiCounter > 5) //  роутеры часто выдают IP дольше 2 секунд потому рекомендовано >20, но на старте столько тормозить не стоит
    {
      // ESP.restart();
      ESP_LOGI(TAG, "--- WiFi not connected on boot! ");
      logToWeb("[WiFi] --- not connected on boot! **");
      bConnWiFi = false;
      WiFi.disconnect();
      return; // Уходим, фоновый ватчдог подключит нас позже
    }
  }

  bConnWiFi = true;
  ESP_LOGI(TAG, "WiFi connected. IP address: %s", WiFi.localIP().toString().c_str());
  logToWeb("[WiFi] connected. IP address: " + WiFi.localIP().toString());
}

TaskHandle_t wifiWatchdogTaskHandle = NULL;

void vWifiWatchdogTask(void *pvParameters)
{
  // Serial.println("[RTOS] Таска контроля Wi-Fi связи запущена на Ядре 1");
  logToWeb("[RTOS] Таска контроля Wi-Fi связи запущена на Ядре 1");

  // Переменная для подсчета неудачных проверок
  int disconnectCount = 0;

  for (;;)
  {
    // Проверяем физический статус подключения к роутеру
    if (WiFi.status() != WL_CONNECTED)
    {
      bConnWiFi = false;
      disconnectCount++;
      // Serial.printf("[WIFI WATCHDOG] Связь потеряна! Попытка %d из 3...\n", disconnectCount);
      logToWeb("[WIFI WATCHDOG] Связь потеряна! Попытка " + String(disconnectCount) + " из 3");

      // Если связь отсутствует уже более  3 проверки по минуте)
      if (disconnectCount >= 3)
      {
        // Serial.println("[WIFI WATCHDOG] Долгий обрыв связи. Жесткий перезапуск Wi-Fi...");
        logToWeb("[WIFI WATCHDOG] Долгий обрыв связи. Жесткий перезапуск Wi-Fi...");

        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(60000));
        // Запускаем вашу функцию подключения заново (используем имя вашей функции из проекта)
        // Если у вас авторизация вшита в setup, можно вызвать WiFi.begin(ssid, password);
        WiFi.begin(CONF_SSID, CONF_PASSWORD);

        vTaskDelay(pdMS_TO_TICKS(10000)); // Даем 10 секунд на попытку фонового подключения
        disconnectCount = 0;              // Сбрасываем счетчик после попытки сброса
      }
    }
    else
    {
      // если было отключение (!bConnWiFi) и подключились заново
      if (!bConnWiFi)
      {
        logToWeb("[WIFI WATCHDOG] Связь с роутером успешно восстановлена.");
        bConnWiFi = true;
        disconnectCount = 0;
      }
    }

    // Опрашиваем статус не слишком часто — раз в 5 минут
    vTaskDelay(pdMS_TO_TICKS(300000));
  }
}

#pragma endregion

void setup()
{
  /*
    // ОТКЛЮЧАЕМ ДЕТЕКТОР ПРОСАДОК (для старых версий ядер ESP32)
    // В зависимости от версии вашей библиотеки, регистр называется либо так:
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Либо если выдаст ошибку, раскомментируйте строку ниже, а верхнюю удалите:
    // WRITE_PERI_REG(RTC_CNTL_BROWNOUT_REG, 0);
  */

  M5.begin(true, false, true, true);
  Serial.begin(115200);
  M5.Speaker.mute();
  dacWrite(25, 0); // Speaker OFF
  // M5.Lcd.setBrightness(0);
  //  m5.Lcd.setTextSize(3);

  // --- НАСТРОЙКА КАНАЛА ЗАГРУЗОЧНОГО ЛОГА ---
  M5.Lcd.setBrightness(100);       // Включаем подсветку на 100% для чтения лога
  M5.Lcd.fillScreen(BLACK);        // Очищаем экран в глубокий черный цвет
  M5.Lcd.setTextColor(GREEN);      // Задаем "хакерский" зеленый цвет текста (или WHITE)
  M5.Lcd.setTextSize(2);           // Размер 2 — идеальный баланс между читаемостью и емкостью
  M5.Lcd.setCursor(0, 0);          // Ставим курсор в левый верхний угол
  M5.Lcd.setTextWrap(false, true); //  автоперенос длинных строк отключен, скроллинг включен

  // Переопределяем частоту i2c на Fast-mode
  // Wire.setClock(400000);
  // Serial.println("[I2C] Частота шины i2c переключена на 400 кГц");

  // Создаем мьютекс защиты данных
  xSensorsMutex = xSemaphoreCreateMutex();
  // Создаем мьютекс для логов
  xLogMutex = xSemaphoreCreateMutex();

  ESP_LOGI(TAG, "===Starting...====");
  logToWeb("===Starting...====");

  startTime = millis();

  // start sensors init
  ESP_LOGI(TAG, "start sensors init");
  logToWeb("start sensors init");
  startSens(vSensVal);
  // показать что подключили -
  getSensData(vSensVal);

  // start wifi
  setup_wifi();

  // Настраиваем асинхронное NTP время, только если сеть поднялась успешно
  if (bConnWiFi)
  {
    long gmtOffset_sec = TIMEZONE * 3600;
    sntp_set_time_sync_notification_cb(timeSyncCallback);
    // 2. Инициализируем системную службу времени (она сама начнет стучаться на сервера, как только появится Wi-Fi)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServerName);
    vTaskDelay(pdMS_TO_TICKS(500));
    printLocalTime();
  }

#pragma region hw interupt cfg
  ESP_LOGD(TAG, "set pin36-39");
  logToWeb("Init keys&interrupt");
  // config pins for interrupt

  // GPIO34-39 can only be set as input mode and do not have software-enabled pullup or pulldown functions.
  gpio_config_t xButtonConfig;
  xButtonConfig.pin_bit_mask = GPIO_SEL_37 | GPIO_SEL_38 | GPIO_SEL_39;
  xButtonConfig.mode = GPIO_MODE_INPUT;
  xButtonConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  xButtonConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  // xButtonConfig.intr_type = GPIO_INTR_ANYEDGE; // both rising and falling edge
  xButtonConfig.intr_type = GPIO_INTR_NEGEDGE; // on low level | Срабатывание по спаду (нажатие)
  gpio_config(&xButtonConfig);

  // Buttons
  ESP_LOGD(TAG, "cfg hw interrupt for buttons");
  xTaskCreatePinnedToCore(vfnButtonTask,  // function with task's code
                          "Button task",  // name
                          2048,           // stack size
                          (void *)NULL,   // input parameters
                          5,              // priority
                          &xButtonHandle, // task handle (for callback from ISR)
                          1);             // core to run on
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  //                GPIO to attach to | ISR to call | Parameters to pass
  gpio_isr_handler_add(GPIO_NUM_39, vfnButtonISR, (void *)GPIO_NUM_39);
  gpio_isr_handler_add(GPIO_NUM_38, vfnButtonISR, (void *)GPIO_NUM_38);
  gpio_isr_handler_add(GPIO_NUM_37, vfnButtonISR, (void *)GPIO_NUM_37);

  // Настройка пина PIR датчика (36)
  gpio_config_t xSensorConfig;
  xSensorConfig.pin_bit_mask = GPIO_SEL_36;
  xSensorConfig.mode = GPIO_MODE_INPUT;
  xSensorConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  xSensorConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  xSensorConfig.intr_type = GPIO_INTR_POSEDGE; // rising edge |  По фронту (высокий уровень при движении)
  gpio_config(&xSensorConfig);

  // Sensor Pin
  ESP_LOGD(TAG, "cfg hw interrupt for sensor");
  xTaskCreatePinnedToCore(vfnPirTask,        // function with task's code
                          "PIR sensor task", // name
                          2048,              // stack size
                          (void *)NULL,      // input parameters
                          5,                 // priority
                          &xPirHandle,       // task handle (for callback from ISR)
                          1);                // core to run on
  gpio_isr_handler_add(GPIO_NUM_36, vfnPirISR, (void *)GPIO_NUM_36);
//
#pragma endregion

  /*  4del
  #pragma region timer interupt cfg
    //
    ESP_LOGD(TAG, "set interrupt on timer");
    // Create semaphore to inform us when the timer has fired
    pxTimerSemaphore = xSemaphoreCreateBinary();
    ESP_LOGD(TAG, "timerBegin");

    // Инициализация таймера (Делитель 8000 дает тики по 0.1 мс при частоте 80МГц)
    timer = timerBegin(
        1,     // the Timer number from 0 to 3
        8000,  //  the value of the time divider. Timer has a 16-bit Prescaler (from 2 to 65536)
        true); // true to count on the rising edge, false to count on the falling edge
    // Attach onTimer function to our timer.
    // ESP_LOGD(TAG, "timerAttachInterrupt");   //4test
    timerAttachInterrupt(
        timer,       // is the pointer to the Timer we have just created
        &onTimerISR, // the function that will be executed each time the Timer alarm is triggered
        false);      // true-по фронту (edge) / false-по уровню (level)  | игнорируется, считается устаревшим (deprecated), так как прерывания таймера теперь жестко работают по фронту.

    // Set alarm to call onTimer function every  second ( 80 000 000Gz / 8000 * 10000 ).
    // 100000 - 10s, 600000 - 1m(60s)  6000000 - 10m  36000000 - 1h(60m)
    // ESP_LOGD(TAG, "timerAlarmWrite");  //4test
    timerAlarmWrite(
        timer,        // the pointer to the Timer created previously
        TIMER_PERIOD, // the frequency of triggering of the alarm in ticks
        true);        // autoreload, Repeat the alarm, true to reset the alarm automatically after each trigger.

    vTaskDelay(pdMS_TO_TICKS(2));
    // Start an alarm
    // ESP_LOGD(TAG, "timerAlarmEnable");  //4test
    timerAlarmEnable(timer);
    vTaskDelay(pdMS_TO_TICKS(2));

    TaskHandle_t task2Handle = NULL;
    // таймер подучения данных и отправки на народмон
    xTaskCreate(
        vfnTimerTask,  //* Function that implements the task.
        "Timer task",  //* Text name for the task.
        4096,          //* Stack size in words, not bytes.
        (void *)NULL,  //* Parameter passed into the task.
        4,             //* Priority at which the task is created.
        &task2Handle); //* Used to pass out the created task's handle.

  #pragma endregion
  */

#pragma region timer cfg
  // 1. Запуск минутной задачи сбора данных (Средний приоритет 3, Ядро 1 — где датчики)
  xTaskCreatePinnedToCore(
      vfnSensorUpdateTask,
      "Read Sensors data",
      3072, // 3 КБ стека для датчиков вполне достаточно
      NULL,
      3,
      NULL,
      1 // Выполняется на Ядре 1
  );

  // 2. Запуск 10-минутной задачи отправки данных (Средний приоритет 3, Ядро 0 — где сетевой стек Wi-Fi)
  xTaskCreatePinnedToCore(
      vfnNetworkSendTask,
      "Send data to Network",
      4096, // Стек увеличен до 4 КБ под тяжелые String буферы сети
      NULL,
      3,
      NULL,
      0 // Строго на Ядре 0, чтобы сетевые задержки не фризили Ядро 1
  );
#pragma endregion

  logToWeb("start tasks data&time");
  pxShowDataSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(vfnvShowData, "Show data on screen", 2048, NULL, 3, NULL);
  pxShowTimeSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(vfnShowTime, "Show time on screen", 2048, NULL, 3, NULL);

  // Закрываем консоль длгов загрузки - дальше смотреть в web.
  // Если поставить ниже вебсервера - будет конфликт и свалится в перезапуск
  bBooting = false;
  // Очищаем экран под графику метеостанции
  if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE); // Возвращаем дефолтный цвет
    M5.Lcd.setBrightness(0);
    xSemaphoreGive(xSensorsMutex);
  }

  // http server
  logToWeb("start Web server");
  xTaskCreatePinnedToCore(vHttpServerTask, "WiFi Web server", 4096, NULL, 2, NULL, 0);

  // Core watchDog
  esp_task_wdt_init(20, true);

  // Запускаем таску контроля Wi-Fi
  logToWeb("start task WiFi_Watchdog");
  xTaskCreatePinnedToCore(
      vWifiWatchdogTask, // Функция таски
      "WiFi_Watchdog",   // Имя для отладки
      3072,              // Размер стека (3КБ вполне достаточно для проверки статуса)
      NULL,              // Параметры
      1,                 // Низкий приоритет (фоновая задача)
      &wifiWatchdogTaskHandle,
      1 // Строго на Ядре 1, где живет Wi-Fi стек
  );

  logToWeb("Boot process finished successfully!");

  // Запускаем таски вывода времени и погодных данных
  xSemaphoreGive(pxShowTimeSemaphore);
}

void loop() {}