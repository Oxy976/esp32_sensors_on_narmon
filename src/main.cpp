/*  план.
 * на потом:
 *
 *   - вынести сервер  http в отдельный файл.
 *   - разобраться с логами и вывести в http
 *   - закрыть мьютексами экран
 *   - доработать вывод на экран, чтоб данные помещались все
 * -----
 * сделано:
 *  + watchdog на сеть - если нет роутера, то переподключиться
 *  - закрыть мьютексами   получение данных
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
//#include <deque> // Удобный стандартный контейнер C++ для очередей
#include "wifi_server.h" // Подключаем созданный модуль сервера http
#include "strct.h"
stSens vSensVal[SensUnit];

// Хэндл мьютекса для защиты массива vSensVal
SemaphoreHandle_t xSensorsMutex = NULL;

#include "sensors.h"

#include <WiFi.h>
//#include <WebServer.h>
// WiFiServer wserv(80);  //к удалению (устарело)
//WebServer wserv(80);
TaskHandle_t httpTaskHandle = NULL; // Хэндл управления таской сервера

#include <WiFiClient.h>
#include <ESPmDNS.h>

boolean bConnWiFi = false;
struct tm timeinfo;

#include <PubSubClient.h>
WiFiClient wifiClient;
PubSubClient mqttClient(CONF_MQTT_SERVER, 1883, wifiClient);

#include "esp_sntp.h" // Нужен для контроля статуса синхронизации NTP

#include "OutToScr.h"

// Set alarm to call onTimer function every  second ( 80 000 000Gz / 8000 * 10000 ).
// 100000 - 10s, 600000 - 1m(60s)  6000000 - 10m  36000000 - 1h(60m)
#define TIMER_PERIOD 6000000

// 4web server
unsigned long currentTime = millis();
// Переменная для сохранения времени подключения пользователя
unsigned long previousTime = 0;
// Определяем задержку в миллисекундах
const long timeoutTime = 2000;

unsigned long startTime = millis();
unsigned long isrPirTime = millis();
unsigned long isrBtnTime = millis();

#define ESP_INTR_FLAG_DEFAULT 0

void showSensVal() //  for TEST!
{
  for (int i = 0; i < SensUnit; i++)
  {
    Serial.print(i);
    Serial.print(" ");
    Serial.print(vSensVal[i].name);
    Serial.print(" ");
    Serial.print(vSensVal[i].value);
    Serial.print(" ");
    Serial.print(vSensVal[i].unit);
    Serial.print(" ");
    Serial.println(vSensVal[i].actual);
  }
}

// print time to serial
/*void printLocalTime()
{
  struct tm timeinfo;
  static const char *TAG = "LocalTime";
  char tbuffer[80];
  if (getLocalTime(&timeinfo))
  {
    strftime(tbuffer, 80, " %d %b %Y   %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, " %s", tbuffer);
  }
  else
  {
    ESP_LOGD(TAG, "** Failed to obtain time **");
    // return;
  }
}
*/

void printLocalTime()
{
  struct tm timeinfo;
  static const char *TAG = "LocalTime";
  char tbuffer[80];

  // Пытаемся считать локальное время
  if (getLocalTime(&timeinfo))
  {
    // Если год в системе больше 120 (считается от 1900 года, то есть 1900 + 120 = 2020 год)
    if (timeinfo.tm_year > 120)
    {
      strftime(tbuffer, 80, "%d %b %Y %H:%M:%S", &timeinfo);
      ESP_LOGI(TAG, "Валидное время: %s", tbuffer);
    }
    else
    {
      ESP_LOGD(TAG, "[Waiting NTP] Время в системе дефолтное (1970 год), ждем синхронизации...");
    }
  }
  else
  {
    ESP_LOGE(TAG, "Не удалось считать структуру времени!");
  }
}

// Функция-колбэк: вызывается автоматически при успешной синхронизации времени
void timeSyncCallback(struct timeval *tv)
{
  Serial.println("[NTP] Время успешно синхронизировано с сервером интернета!");
  printLocalTime(); // Выводим время в консоль для проверки
}

bool NarodmonTcpPublish()
{
  static const char *TAG = "NmonTcp";
  WiFiClient client;

  String buf;
  String mac = "30:AE:A4:69:B9:04";
  //  String mac = "30AEA469B904";
  buf = "#" + mac + "\n"; // заголовок
  for (int i = 0; i < SensUnit; i++)
  {
    if (vSensVal[i].actual & (vSensVal[i].mqttId.length() > 1))
    {
      buf = buf + "#" + vSensVal[i].mqttId + "#" + String(vSensVal[i].value, 2) + "\n"; // #mac1#value1 (#H1#93)
    }
  }
  buf = buf + "##\n"; // закрываем пакет

  // Serial.println(buf);      //TEST
  // ESP_LOGD(TAG, "string to site: %s", buf);  //спец.символы не ест

  if (!client.connect("narodmon.ru", 8283)) // попытка подключения
  // if (!client.connect("127.0.0.1", 8283)) // попытка подключения  test
  {
    ESP_LOGD(TAG, "Connecting failed");
    client.stop();
    return false; // не удалось;
  }
  else
  {
    ESP_LOGI(TAG, "Connected to narodmon, sending data string");
    client.print(buf); // и отправляем данные
    while (client.available())
    {
      String line = client.readStringUntil('\r'); // если что-то в ответ будет
      ESP_LOGD(TAG, "string from site: %s", line);
    }
    client.stop();
    return true; // ушло
  }
}

// ==MQTT ==публикация
void MqttPublish() // narodmon mqtt больше бесплатно не понимает,  не используется
{
  static const char *TAG = "mqtt";

  int count_reconnect = 0;
  // если не подключен, то подключаемся.
  if (!!!mqttClient.connected())
  {
    ESP_LOGI(TAG, "Reconnecting client to %s", CONF_MQTT_SERVER);
    while (!!!mqttClient.connect(CONF_CLIENT_ID, CONF_AUTH_METHOD, CONF_TOKEN, CONF_CONN_TOPIC, 0, 0, "online"))
    {
      vTaskDelay(500);
      count_reconnect++;
      // больше 10 попыток - что-то не так...
      if (count_reconnect > 10)
      {
        ESP_LOGI(TAG, "problem with connecting to server !! **");
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

      getSensData(vSensVal); // Безопасно получаем-записываем новые данные
      OutToScr(vSensVal);    // Безопасно выводим их на дисплей M5Stack

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
{
  static const char *TAG = "taskShowTime";
  while (1)
  {
    xSemaphoreTake(pxShowTimeSemaphore, portMAX_DELAY); // Программа тут свалится в WAIT до тех пор пока не появится семафор
    ESP_LOGD(TAG, "Task show time");
    ShowTime();
  }
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL);
}

#pragma region timer
// interrupt on timer
static hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t pxTimerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED; // to frame  critical portions
static void IRAM_ATTR onTimerISR()
{
  xSemaphoreGiveFromISR(pxTimerSemaphore, NULL);
}

static void vfnTimerTask(void *vpArg)
{
  static const char *TAG = "timer";
  while (1)
  {
    xSemaphoreTake(pxTimerSemaphore, portMAX_DELAY); // ожидаем семафор бесконечно долго (portMAX_DELAY)
    esp_task_wdt_reset();                            // кормим собаку - ватчдог ядра
    ESP_LOGD(TAG, "Timer interrupt now");            // сюда попадаем только если есть семафор

    printLocalTime();

    // --- ЗАЩИТА ЗАПИСИ ---
    // Ждем освобождения мьютекса максимум 100 мс (не блокируем таску намертво)
    if (xSemaphoreTake(xSensorsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      getSensData(vSensVal);         // Спокойно пишем данные в массив, никто нам не помешает
      xSemaphoreGive(xSensorsMutex); // Обязательно освобождаем мьютекс!
    }
    else
    {
      ESP_LOGW(TAG, "Не удалось получить доступ для записи данных (занято сервером)");
    }

    // getSensData(vSensVal); // считать данные
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
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL); // remove the task whene done
}
#pragma endregion

#pragma region buttons
static const char *TAG = "gpio_button";   // tag for logging
static TaskHandle_t xButtonHandle = NULL; // task handle for interrupt callback
static TaskHandle_t xPirHandle = NULL;    // task handle for interrupt callback

static void IRAM_ATTR vfnButtonISR(void *vpArg)
{
  /***
  Interrupt routine for handling GPIO interrupts.
  vpArg is a pointer to the GPIO number.
  ***/
  uint32_t ulGPIONumber = (uint32_t)vpArg;           // get the triggering GPIO
                                                     // ESP_LOGD(TAG, "ISR GPIO %d is %d",ulGPIONumber,gpio_get_level((gpio_num_t)ulGPIONumber)); // ****** TEST *** УБРАТЬ!****
  if (gpio_get_level((gpio_num_t)ulGPIONumber) == 0) // по нажатию
  {
    xTaskNotifyFromISR(xButtonHandle,     // task to notify
                       ulGPIONumber - 37, // 32 bit integer for passing a value
                       eSetBits,          // notify action (pass value in this case)
                       NULL);             // wake a higher prio task (default behavior)
  }
  portYIELD_FROM_ISR(); // if notified task has higher prio then current interrupted task, set it to the head of the task queue
}

static void IRAM_ATTR vfnPirISR(void *vpArg)
{
  uint32_t ulGPIONumber = (uint32_t)vpArg; // get the triggering GPIO

  // на пине верхний уровень
  if (gpio_get_level((gpio_num_t)ulGPIONumber) == 1)
  {
    xTaskNotifyFromISR(xPirHandle,        // task to notify
                       ulGPIONumber - 36, // 32 bit integer for passing a value
                       eSetBits,          // notify action (pass value in this case)
                       NULL);             // wake a higher prio task (default behavior)
  }
  portYIELD_FROM_ISR(); // if notified task has higher prio then current interrupted task, set it to the head of the task queue
}

static void vfnButtonTask(void *vpArg)
{
  uint32_t ulNotifiedValue = 0;
  BaseType_t xResult;

  while (1)
  {
    xResult = xTaskNotifyWait(pdFALSE,          // don't clear bits on entry
                              0xFFFFFFFF,       // clear all bits on exit
                              &ulNotifiedValue, // stores the notified value
                              portMAX_DELAY);   // wait forever
    if ((xResult == pdPASS) && (millis() - isrBtnTime > 100ul))
    {
      ESP_LOGD(TAG, "HW button interrupt now");
      isrBtnTime = millis();

      switch (ulNotifiedValue)
      {
      case 0:
        ESP_LOGD(TAG, "==button 0==");
        ESP_LOGD(TAG, "give semaphore time");
        xSemaphoreGive(pxShowTimeSemaphore);
        break;
      case 1:
        ESP_LOGD(TAG, "==button 1==");
        ESP_LOGD(TAG, "give semaphore data");
        xSemaphoreGive(pxShowDataSemaphore);
        break;
      case 2:
        ESP_LOGD(TAG, "==button 2==");
        printLocalTime();
        showSensVal(); // TEST!
        // NarodmonTcpPublish();  // ****************TEST**********
        break;
      default:
        ESP_LOGD(TAG, "This should not happen...");
        break;
      }
    }
  }
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL); // remove the task whene done
}

static void vfnPirTask(void *vpArg)
{
  uint32_t ulNotifiedValue = 0; // получаем, но не используем
  BaseType_t xResult;

  while (1)
  {
    xResult = xTaskNotifyWait(pdFALSE, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);
    if ((xResult == pdPASS) && (millis() - isrPirTime > 5000ul) && (millis() > 3000ul)) // если прерывание и прошло достаточно от прошлого и задержка на активацию датчика
    {
      ESP_LOGD(TAG, "HW PIR interrupt now (pin 36)");
      ESP_LOGD(TAG, "give semaphore data");
      xSemaphoreGive(pxShowDataSemaphore);
      isrPirTime = millis();
    }
  }
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL); // remove the task whene done
}

#pragma endregion

#pragma region Wifi

//log to web
/*
// Настройки лога
const size_t MAX_LOG_LINES = 25; // Храним только последние 25 строк
std::deque<String> webLogs;       // Очередь строк лога
SemaphoreHandle_t xLogMutex = xSemaphoreCreateMutex(); // Мьютекс защиты логов

// Функция добавления новой записи в лог (вызывать вместо или вместе с Serial.println)

void logToWeb(String text) {
    struct tm timeinfo;
    char timeBuf[12];
    String timeStr = "";
    
    // Добавляем штамп времени к логу, если оно синхронизировано
    if (getLocalTime(&timeinfo) && timeinfo.tm_year > 120) {
        strftime(timeBuf, sizeof(timeBuf), "[%H:%M:%S] ", &timeinfo);
        timeStr = String(timeBuf);
    }

    if (xLogMutex != NULL && xSemaphoreTake(xLogMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        webLogs.push_back(timeStr + text); // Добавляем строку в конец
        
        // Если превысили лимит — удаляем самую старую строку из начала
        if (webLogs.size() > MAX_LOG_LINES) {
            webLogs.pop_front();
        }
        xSemaphoreGive(xLogMutex);
    }
}
*/

// ==WIFI ================
void setup_wifi()
{
  static const char *TAG = "wifi";

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  vTaskDelay(10); // delay(10);
  ESP_LOGI(TAG, "Connecting to %s", CONF_SSID);

 // WiFi.begin(ssid, password);
 WiFi.begin(CONF_SSID, CONF_PASSWORD);

  // если за 5*500 не подключился - прекратить
  int wifiCounter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(500); // delay(500);
    Serial.print("#");
    Serial.println();
    if (++wifiCounter > 5)
    {
      // ESP.restart();
      ESP_LOGI(TAG, "** WiFi not connected! **");
      bConnWiFi = false;
      WiFi.disconnect();
      return;
    }
  }

  randomSeed(micros()); //  ????

  bConnWiFi = true;
  ESP_LOGI(TAG, "WiFi connected. IP address: %s", WiFi.localIP().toString().c_str());
}

TaskHandle_t wifiWatchdogTaskHandle = NULL;

void vWifiWatchdogTask(void *pvParameters)
{
  Serial.println("[RTOS] Таска контроля Wi-Fi связи запущена на Ядре 1");

  // Переменная для подсчета неудачных проверок
  int disconnectCount = 0;

  for (;;)
  {
    // Проверяем физический статус подключения к роутеру
    if (WiFi.status() != WL_CONNECTED)
    {
      bConnWiFi = false;
      disconnectCount++;
      Serial.printf("[WIFI WATCHDOG] Связь потеряна! Попытка %d из 3...\n", disconnectCount);

      // Если связь отсутствует уже более  3 проверки по минуте)
      if (disconnectCount >= 3)
      {
        Serial.println("[WIFI WATCHDOG] Долгий обрыв связи. Жесткий перезапуск Wi-Fi...");

        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(60000));
        // Запускаем вашу функцию подключения заново (используем имя вашей функции из проекта)
        // Если у вас авторизация вшита в setup, можно вызвать WiFi.begin(ssid, password);
        WiFi.begin();

        disconnectCount = 0; // Сбрасываем счетчик после попытки сброса
      }
    }
    else
    {
      // Если связь есть — обнуляем счетчик брака
      if (disconnectCount > 0)
      {
        Serial.println("[WIFI WATCHDOG] Связь с роутером успешно восстановлена.");
        bConnWiFi = true;
        disconnectCount = 0;
      }
    }

    // Опрашиваем статус не слишком часто — раз в минуту
    vTaskDelay(pdMS_TO_TICKS(60000));
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
  M5.Lcd.setBrightness(0);
  // m5.Lcd.setTextSize(3);

  // Переопределяем частоту i2c на Fast-mode
  Wire.setClock(400000);
  Serial.println("[I2C] Частота шины переключена на 400 кГц");

  // Создаем мьютекс защиты данных
  xSensorsMutex = xSemaphoreCreateMutex();
  if (xSensorsMutex == NULL)
  {
    Serial.println("[ERROR] Не удалось создать мьютекс датчиков!");
  }

  ESP_LOGI(TAG, "===Starting...====");

  startTime = millis();

  setup_wifi();
  /* if (bConnWiFi)
   {
     // init ntp
     long gmtOffset_sec = 0;
     gmtOffset_sec = TIMEZONE * 3600;
     configTime(gmtOffset_sec, daylightOffset_sec, ntpServerName);
     // print time
     printLocalTime();
   }
   */
  // ИСПРАВЛЕНИЕ NTP: Настраиваем параметры времени асинхронно
  long gmtOffset_sec = TIMEZONE * 3600;
  // 1. Задаем колбэк, который сообщит нам, когда время станет валидным
  sntp_set_time_sync_notification_cb(timeSyncCallback);
  // 2. Инициализируем системную службу времени (она сама начнет стучаться на сервера, как только появится Wi-Fi)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServerName);
  delay(500); // на получение времени по ntp
  printLocalTime();

#pragma region hw interupt cfg
  ESP_LOGD(TAG, "set pin36-39");
  // config pins for interrupt

  // GPIO34-39 can only be set as input mode and do not have software-enabled pullup or pulldown functions.
  gpio_config_t xButtonConfig;
  xButtonConfig.pin_bit_mask = GPIO_SEL_37 | GPIO_SEL_38 | GPIO_SEL_39;
  xButtonConfig.mode = GPIO_MODE_INPUT;
  xButtonConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  xButtonConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  // xButtonConfig.intr_type = GPIO_INTR_ANYEDGE; // both rising and falling edge
  xButtonConfig.intr_type = GPIO_INTR_NEGEDGE; // on low level
  gpio_config(&xButtonConfig);

  // Buttons
  ESP_LOGD(TAG, "cfg hw interrupt for buttons");
  xTaskCreatePinnedToCore(vfnButtonTask,  // function with task's code
                          "Button task",  // name
                          2048,           // stack size
                          (void *)NULL,   // input parameters
                          10,             // priority
                          &xButtonHandle, // task handle (for callback from ISR)
                          1);             // core to run on
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(GPIO_NUM_39,          // GPIO to attach to
                       vfnButtonISR,         // ISR to call
                       (void *)GPIO_NUM_39); // Parameters to pass
  gpio_isr_handler_add(GPIO_NUM_38, vfnButtonISR, (void *)GPIO_NUM_38);
  gpio_isr_handler_add(GPIO_NUM_37, vfnButtonISR, (void *)GPIO_NUM_37);

  //
  gpio_config_t xSensorConfig;
  xSensorConfig.pin_bit_mask = GPIO_SEL_36;
  xSensorConfig.mode = GPIO_MODE_INPUT;
  xSensorConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  xSensorConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  xSensorConfig.intr_type = GPIO_INTR_POSEDGE; // rising edge
  gpio_config(&xSensorConfig);

  // Sensor Pin
  ESP_LOGD(TAG, "cfg hw interrupt for sensor");
  xTaskCreatePinnedToCore(vfnPirTask,        // function with task's code
                          "PIR sensor task", // name
                          2048,              // stack size
                          (void *)NULL,      // input parameters
                          10,                // priority
                          &xPirHandle,       // task handle (for callback from ISR)
                          1);                // core to run on
  gpio_isr_handler_add(GPIO_NUM_36, vfnPirISR, (void *)GPIO_NUM_36);
//
#pragma endregion
#pragma region timer interupt cfg
  //
  ESP_LOGD(TAG, "set interrupt on timer");
  // Create semaphore to inform us when the timer has fired
  pxTimerSemaphore = xSemaphoreCreateBinary();
  ESP_LOGD(TAG, "timerBegin");
  timer = timerBegin(
      1,     // the Timer number from 0 to 3
      8000,  //  the value of the time divider. Timer has a 16-bit Prescaler (from 2 to 65536)
      true); // true to count on the rising edge, false to count on the falling edge
  // Attach onTimer function to our timer.
  ESP_LOGD(TAG, "timerAttachInterrupt");
  timerAttachInterrupt(
      timer,       // is the pointer to the Timer we have just created
      &onTimerISR, // the function that will be executed each time the Timer alarm is triggered
      false);      // true-по фронту (edge) / false-по уровню (level) [ ?? EDGE timer interrupt is not supported!]

  // Set alarm to call onTimer function every  second ( 80 000 000Gz / 8000 * 10000 ).
  // 100000 - 10s, 600000 - 1m(60s)  6000000 - 10m  36000000 - 1h(60m)
  ESP_LOGD(TAG, "timerAlarmWrite");
  timerAlarmWrite(
      timer,        // the pointer to the Timer created previously
      TIMER_PERIOD, // the frequency of triggering of the alarm in ticks
      true);        // autoreload, Repeat the alarm, true to reset the alarm automatically after each trigger.
  vTaskDelay(2);
  // Start an alarm
  ESP_LOGD(TAG, "timerAlarmEnable");
  timerAlarmEnable(timer);
  vTaskDelay(2);

  TaskHandle_t task2Handle = NULL;
  xTaskCreate(
      vfnTimerTask,  //* Function that implements the task.
      "Timer task",  //* Text name for the task.
      2048,          //* Stack size in words, not bytes.
      (void *)NULL,  //* Parameter passed into the task.
      10,            //* Priority at which the task is created.
      &task2Handle); //* Used to pass out the created task's handle.

#pragma endregion

  pxShowDataSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(vfnvShowData, "Show data on screen", 2048, NULL, 10, NULL);
  pxShowTimeSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(vfnShowTime, "Show time on screen", 2048, NULL, 10, NULL);

  // xTaskCreate(vfnWifiSrv, "WiFi Web server", 4096, NULL, 5, NULL);     //к удалению (устарело)
  // xTaskCreatePinnedToCore(vfnWifiSrv, "WiFi Web server", 4096, NULL, 5, NULL, 0);      //к удалению (устарело)
  xTaskCreatePinnedToCore(vHttpServerTask, "WiFi Web server", 4096, NULL, 5, NULL, 0);

  // Core watchDog
  esp_task_wdt_init(15, true);

  // Запускаем таску контроля Wi-Fi
  xTaskCreatePinnedToCore(
      vWifiWatchdogTask, // Функция таски
      "WiFi_Watchdog",   // Имя для отладки
      3072,              // Размер стека (3КБ вполне достаточно для проверки статуса)
      NULL,              // Параметры
      1,                 // Низкий приоритет (фоновая задача)
      &wifiWatchdogTaskHandle,
      1 // Строго на Ядре 1, где живет Wi-Fi стек
  );

  // start sensors init
  ESP_LOGI(TAG, "start sensors init");
  startSens(vSensVal);
  vTaskDelay(10);
  xSemaphoreGive(pxShowTimeSemaphore);
  // vTaskDelay(2000);
  // xSemaphoreGive(pxShowDataSemaphore);
  getSensData(vSensVal);
}

void loop() {}
