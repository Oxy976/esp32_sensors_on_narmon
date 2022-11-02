/*
таски:
  - получить данные с датчиков (?? а надо-ли таском??)
  - вывести на экран
  - отправить на сервер
  - обработка кнопок - vfnButtonTask
  - обработка датчика движения vfnPirTask
прерывания
  - нажатия на кнопки - vfnButtonISR
  - с датчика движения - vfnPirISR
  - по времени для отправки на сервер - onTimerISR

семафоры
  - прерывание по таймеру - pxTimerSemaphore



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
stSens vSensVal[9]; // 10 параметров снимается с датчиков

#include "sensors.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>

#include "OutToScr.h"

#define ESP_INTR_FLAG_DEFAULT 0

#pragma region timer
// interrupt on timer
static hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t pxTimerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED; // to frame  critical portions
static void IRAM_ATTR onTimerISR()
{
  // portENTER_CRITICAL_ISR(&timerMux);
  //  Give a semaphore that we can check
  xSemaphoreGiveFromISR(pxTimerSemaphore, NULL);
  // portEXIT_CRITICAL_ISR(&timerMux);
}

static void vfnTimerTask(void *vpArg)
{
  static const char *TAG = "timer";

  while (1)
  {
    xSemaphoreTake(pxTimerSemaphore, portMAX_DELAY); //ожидаем семафор бесконечно долго (portMAX_DELAY)
    ESP_LOGD(TAG, "Timer interrupt now");            //сюда попадаем только если есть семафор
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
  uint32_t ulGPIONumber = (uint32_t)vpArg; // get the triggering GPIO

  if (gpio_get_level((gpio_num_t)ulGPIONumber) == 0)
  {                                       // only when pressed
    xTaskNotifyFromISR(xButtonHandle,     // task to notify
                       ulGPIONumber - 37, // 32 bit integer for passing a value
                       eSetBits,          // notify action (pass value in this case)
                       NULL);             // wake a higher prio task (default behavior)
  }
  portYIELD_FROM_ISR(); // if notified task has higher prio then current interrupted task, set it to the head of the task queue
}

static void IRAM_ATTR vfnPirISR(void *vpArg)
{
  /***
  Interrupt routine for handling GPIO interrupts.
  vpArg is a pointer to the GPIO number.
  ***/
  uint32_t ulGPIONumber = (uint32_t)vpArg; // get the triggering GPIO

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
  /***
  Task for handling interrupts generated by button presses on the M5Stack
  vpArg is a pointer to the passed arguments. Not used in this task.
  ***/
  uint32_t ulNotifiedValue = 0;
  BaseType_t xResult;

  while (1)
  {
    // m5.Lcd.setCursor(0, 0);
    // m5.Lcd.printf("running on core %d", xPortGetCoreID());
    // m5.Lcd.setCursor(0, 40);
    xResult = xTaskNotifyWait(pdFALSE,          // don't clear bits on entry
                              0xFFFFFFFF,       // clear all bits on exit
                              &ulNotifiedValue, // stores the notified value
                              portMAX_DELAY);   // wait forever
    if (xResult == pdPASS)
      ESP_LOGD(TAG, "HW button interrupt now");
    { // if a notification is received
      // m5.Lcd.printf("button %d", ulNotifiedValue);
      switch (ulNotifiedValue)
      { // to demonstrate the use of the switch statement
      case 0:
        ESP_LOGD(TAG, "==button 0==");

        break;
      case 1:
        ESP_LOGD(TAG, "==button 1==");

        getSensData(vSensVal); // считать данные

        ScreenOn();
        ESP_LOGD(TAG, "show data");
        OutToScr(8.80, 55.5, 766.6, 22.2, 77.7, 0.23); //показать данные
        ScreenOff();

        // resetDataFlag();   //убрать бит актуальности данных
        break;
      case 2:
        ESP_LOGD(TAG, "==button 2==");

        ScreenOn();
        struct tm timeinf;
        timeinf.tm_year = 120;
        timeinf.tm_mon = 7;
        timeinf.tm_mday = 12;
        timeinf.tm_hour = 7;
        timeinf.tm_min = 20;
        timeinf.tm_sec = 30;
        timeinf.tm_wday = 6;

        ESP_LOGD(TAG, "show time");
        ShowTime(timeinf);
        ScreenOff();

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
  /***
  Task for handling interrupts generated by button presses on the M5Stack
  vpArg is a pointer to the passed arguments. Not used in this task.
  ***/
  uint32_t ulNotifiedValue = 0; //получаем, но не используем
  BaseType_t xResult;

  while (1)
  {
    xResult = xTaskNotifyWait(pdFALSE, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);
    if (xResult == pdPASS)
    {
      ESP_LOGD(TAG, "HW PIR interrupt now (pin 36)");

      ScreenOn();
      ESP_LOGD(TAG, "show data");
      OutToScr(8.80, 55.5, 766.6, 22.2, 77.7, 0.23);
      ScreenOff();
    }
  }
  ESP_LOGD(TAG, "Crash!");
  vTaskDelete(NULL); // remove the task whene done
}

#pragma endregion

// ==WIFI ================
void setup_wifi()
{
  WiFi.disconnect();
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // если за 30*500 не подключился - перегрузить
  int wifiCounter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("#");
    if (++wifiCounter > 30)
      ESP.restart();
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // use mdns for host name resolution  http://hostname.local
  if (!MDNS.begin(hostname))
  {
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
}

// ==== NTP===
// print time to serial
void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo);
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setup()
{
  // m5.begin();
  M5.begin(true, false, true, true);
  Serial.begin(115200);
  M5.Speaker.mute();
  dacWrite(25, 0); // Speaker OFF
  M5.Lcd.setBrightness(0);
  // m5.Lcd.setTextSize(3);

  ESP_LOGI(TAG, "===Starting...====");

  
  // setup_wifi();
  // printLocalTime();

#pragma region hw interupt cfg
  ESP_LOGD(TAG, "set pin36-39");
  // config pins for interrupt

  gpio_config_t xButtonConfig;
  xButtonConfig.pin_bit_mask = GPIO_SEL_37 | GPIO_SEL_38 | GPIO_SEL_39;
  xButtonConfig.mode = GPIO_MODE_INPUT;
  xButtonConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  xButtonConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
  xButtonConfig.intr_type = GPIO_INTR_ANYEDGE; // both rising and falling edge
  gpio_config(&xButtonConfig);

  // Buttons
  ESP_LOGD(TAG, "cfg hw interrupt 4 buttons");
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
  ESP_LOGD(TAG, "cfg hw interrupt 4 sensor");
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
      timer,  // the pointer to the Timer created previously
      600000, // the frequency of triggering of the alarm in ticks
      true);  // autoreload, Repeat the alarm, true to reset the alarm automatically after each trigger.
  // timerAlarmWrite(timer, LOG_PERIOD, true);
  // delayMicroseconds(0);
  // delay(1);
  vTaskDelay(2);
  // Start an alarm
  ESP_LOGD(TAG, "timerAlarmEnable");
  timerAlarmEnable(timer);
  vTaskDelay(2);
  // delay(1);

  TaskHandle_t task2Handle = NULL;
  xTaskCreate(
      vfnTimerTask,  //* Function that implements the task.
      "Timer task",  //* Text name for the task.
      2048,          //* Stack size in words, not bytes.
      (void *)NULL,  //* Parameter passed into the task.
      10,            //* Priority at which the task is created.
      &task2Handle); //* Used to pass out the created task's handle.
#pragma endregion

  // start sensors init
  ESP_LOGI(TAG, "start sensors init");
  startSens();
}

void loop(void) {}