/*  план.
 * на потом:
 *   - вынести сервер  http в отдельный файл.
 *   - разобраться с логами и вывести в http
 *   - закрыть мьютексами экран и получение данных
 *   - доработать вывод на экран, чтоб данные помещались все
 * -----
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
#include <driver/gpio.h>
#include <M5Stack.h>
#include <esp_log.h>
#include <Arduino.h>
#include "sdkconfig.h"

#include "settings.h"
#include "strct.h"
stSens vSensVal[SensUnit];

#include "sensors.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
WiFiServer wserv(80);
boolean bConnWiFi = false;
struct tm timeinfo;

#include <PubSubClient.h>
WiFiClient wifiClient;
PubSubClient mqttClient(mqttServer, 1883, wifiClient);

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
void printLocalTime()
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

  //Serial.println(buf);      //TEST
  //ESP_LOGD(TAG, "string to site: %s", buf);  //спец.символы не ест

   if (!client.connect("narodmon.ru", 8283)) // попытка подключения
  //if (!client.connect("127.0.0.1", 8283)) // попытка подключения  test
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
    return true; //ушло
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
    ESP_LOGI(TAG, "Reconnecting client to %s", mqttServer);
    while (!!!mqttClient.connect(clientId, authMethod, token, conntopic, 0, 0, "online"))
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
    ESP_LOGI(TAG, "Connecting to %s with: id %s, auth %s, token %s", mqttServer, clientId, authMethod, token);
  }

  for (int i = 0; i < SensUnit; i++)
  {
    if (vSensVal[i].actual & (vSensVal[i].mqttId.length() > 1))
    {
      String topic = TOPIC;
      String payload = String(vSensVal[i].value, 1);                  //значение строкой
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
    getSensData(vSensVal); // получить данные
    OutToScr(vSensVal);    //показать данные
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
    xSemaphoreTake(pxTimerSemaphore, portMAX_DELAY); //ожидаем семафор бесконечно долго (portMAX_DELAY)
    ESP_LOGD(TAG, "Timer interrupt now");            //сюда попадаем только если есть семафор

    printLocalTime();
    getSensData(vSensVal); // считать данные
    //отправить данные на narodmon (семафор для таска?) если wifi подключен
    if (bConnWiFi)             
      // MqttPublish();
      NarodmonTcpPublish();
    //запусить прогрев датчиков (семафор для таска?)
    // Если влажность (HTU_e_humi или SHT_e_humi) >70% - прогреть
    if (vSensVal[8].value > 70.0 || vSensVal[10].value > 70.0)
      heatSens();
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
  if (gpio_get_level((gpio_num_t)ulGPIONumber) == 0) //по нажатию
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
        //NarodmonTcpPublish();  // ****************TEST**********
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
  uint32_t ulNotifiedValue = 0; //получаем, но не используем
  BaseType_t xResult;

  while (1)
  {
    xResult = xTaskNotifyWait(pdFALSE, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);
    if ((xResult == pdPASS) && (millis() - isrPirTime > 5000ul) && (millis() > 3000ul)) //если прерывание и прошло достаточно от прошлого и задержка на активацию датчика
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

// ==WIFI ================
void setup_wifi()
{
  static const char *TAG = "wifi";

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  vTaskDelay(10); // delay(10);
  ESP_LOGI(TAG, "Connecting to %d", ssid);

  WiFi.begin(ssid, password);

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
  ESP_LOGI(TAG, "WiFi connected. IP address: %d", WiFi.localIP());

  // use mDNS for host name resolution  https://espressif.github.io/esp-protocols/mdns/en/index.html
  if (!MDNS.begin(hostname))
  {
    ESP_LOGD(TAG, "Error setting up MDNS responder!");
    while (1)
    {
      vTaskDelay(1000); // delay(1000);
    }
  }
  ESP_LOGI(TAG, "mDNS responder started");

  // Start TCP (HTTP) server
  wserv.begin();
  ESP_LOGI(TAG, "HTTP server started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
}

void vfnWifiSrv(void *vpArg)
{
  unsigned long upTime_sec = 0;
  int upTime_d = 0;
  int upTime_h = 0;
  int upTime_m = 0;
  int upTime_s = 0;

  String header;
  while (1)
  {
    WiFiClient client = wserv.available(); // Ждем подключения пользователя

    if (client)
    { // Если есть подключение
      currentTime = millis();
      if (previousTime < currentTime)
      {
        previousTime = currentTime;
        upTime_sec = (currentTime - startTime) / 1000ul;    // ms->s
        upTime_d = (upTime_sec / 24ul / 3600ul);            //дни
        upTime_h = (upTime_sec / 3600ul - upTime_d * 24ul); // часы
        upTime_m = (upTime_sec % 3600ul) / 60ul;            // минуты
        upTime_s = (upTime_sec % 3600ul) % 60ul;            // секунды
      }
      else
      {
        // ***** обработать переход через 0 !!!
        previousTime = currentTime;
      }
      Serial.println("New Client."); // выводим сообщение в монитор порта
      String currentLine = "";       // создаем строку для хранения входящих данных
      while (client.connected() && currentTime - previousTime <= timeoutTime)
      { // выполняем программу, пока пользователь подключен
        currentTime = millis();
        if (client.available())
        {                         // проверяем, есть ли входящее сообщение
          char c = client.read(); // читаем и
          Serial.write(c);        // выводим в монитор порта
          header += c;
          if (c == '\n')
          { // если входящее сообщение – переход на новую строку (пустая строка)
            // то считаем это концом HTTP запроса и выдаем ответ
            if (currentLine.length() == 0)
            {
              // заголовок всегда начинается с ответа (например, HTTP/1.1 200 OK)
              // добавляем тип файла ответа:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              // Выводим HTML-страницу
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              // Добавляем стили CSS
              client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial;}");
              client.println("table { border-collapse: collapse; width:35%; margin-left:auto; margin-right:auto; }");
              client.println("th { padding: 12px; background-color: #0043af; color: white; }");
              client.println("tr { border: 1px solid #C0C0C0; padding: 12px; }");
              client.println("tr:hover { background-color: #bcbcbc; }");
              client.println("td { border: none; padding: 12px; }");
              //              client.println(".sensor { color: black; font-weight: bold; background-color: #e3e3e3; padding: 1px; }");
              client.println(".actual { color: black; font-weight: bold; background-color: #e3e3e3; padding: 1px; }");
              client.println(".not_actual { color: #DCDCDC; font-weight: normal; background-color: white; padding: 1px; }");

              // Заголовок веб-страницы
              client.println("</style></head><body><h1>ESP32 sensors</h1>");
              if (getLocalTime(&timeinfo))
              {
                client.println("on time ");
                client.println(&timeinfo);
                client.println("</br>");
              }
              client.println("up time ");
              client.println(upTime_sec);
              client.println("sec (");
              client.println(upTime_d);
              client.println("days  ");
              client.println(upTime_h);
              client.println(":");
              client.println(upTime_m);
              client.println(":");
              client.println(upTime_s);
              client.println(")</br>");

              client.println("<table><tr><th>#</th><th>Name</th><th>VALUE</th><th>Unit</th></tr>");
              for (int i = 0; i < SensUnit; i++)
              {
                if (vSensVal[i].actual)
                {
                  client.println("<tr class=\"actual\"><td>");
                }
                else
                {
                  client.println("<tr class=\"not_actual\"><td>");
                }
                client.print(i);
                client.println("</td><td>");
                client.println(vSensVal[i].name);
                client.println("</td><td>");
                client.println(vSensVal[i].value);
                client.println("</td><td>");
                client.println(vSensVal[i].unit);
                client.println("</td></span></tr>");
                vTaskDelay(5);
              }
              client.println("</body></html>");

              // Ответ HTTP также заканчивается пустой строкой
              client.println();
              // Прерываем выполнение программы
              break;
            }
            else
            { // если у нас есть новый запрос, очищаем строку
              currentLine = "";
            }
          }
          else if (c != '\r')
          {                   // но, если отправляемая строка не пустая
            currentLine += c; // добавляем ее в конец строки
          }
        }
      }
      // Очищаем заголовок
      header = "";
      // Сбрасываем соединение
      client.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    }
    vTaskDelay(500);
  }
  ESP_LOGD(TAG, "WiFi server Crash!");
  vTaskDelete(NULL); // remove the task whene done
}

void setup()
{
  M5.begin(true, false, true, true);
  Serial.begin(115200);
  M5.Speaker.mute();
  dacWrite(25, 0); // Speaker OFF
  M5.Lcd.setBrightness(0);
  // m5.Lcd.setTextSize(3);

  ESP_LOGI(TAG, "===Starting...====");

  startTime = millis();

  setup_wifi();
  if (bConnWiFi)
  {
    // init ntp
    long gmtOffset_sec = 0;
    gmtOffset_sec = TIMEZONE * 3600;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServerName);
    // print time
    printLocalTime();
  }

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

  // start task wifi http server
  xTaskCreate(vfnWifiSrv, "WiFi Web server", 2048, NULL, 10, NULL);

  // start sensors init
  ESP_LOGI(TAG, "start sensors init");
  startSens(vSensVal);
  vTaskDelay(10);
  xSemaphoreGive(pxShowTimeSemaphore);
  // vTaskDelay(2000);
  // xSemaphoreGive(pxShowDataSemaphore);
  getSensData(vSensVal);
}

void loop(void) {}