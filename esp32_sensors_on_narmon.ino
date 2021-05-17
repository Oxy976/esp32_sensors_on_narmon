/*
  transmiting data from M5stack (esp32) with sensors to narodmon.ru across mqtt
*/


#include "settings.h"

#include "esp_system.h"
#include "soc/rtc.h"
#include <M5Stack.h>
#include "OutToScr.h"
unsigned long OffScrTime; //Off,Update screens
boolean bScrOn = true;


#include <SPI.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "time.h"
struct tm timeinfo;

#include "otg_web.h"
WebServer wserver(80);

#include <PubSubClient.h>
WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);



//** sensors**

#include <Wire.h>

//***Geiger
//RadSens
#include "radSens1v2.h"
ClimateGuard_RadSens1v2 radSens(RS_DEFAULT_I2C_ADDRESS); /*Constructor of the class ClimateGuard_RadSens1v2,
                                                           sets the address parameter of I2C sensor.
                                                           Default address: 0x66.*/

//***BME280
#include "cactus_io_BME280_I2C.h"
// http://static.cactus.io/downloads/library/bme280/cactus_io_BME280_I2C.zip
// Create the BME280 object
BME280_I2C bme_ext(BME_ext_ADDR);  // I2C using address
BME280_I2C bme_int(BME_int_ADDR);  // I2C using address
boolean bBME_ext = true;
boolean bBME_int = true;

float bme_ext_pres = 0.0;
float bme_ext_temp = 0.0;
float bme_ext_humi = 0.0;
float bme_int_pres = 0.0;
float bme_int_temp = 0.0;
float bme_int_humi = 0.0;

//***HTU21D
//Create the HTU21D object
//  https://github.com/enjoyneering/HTU21D
#include <HTU21D.h>
/*
HTU21D resolution:
HTU21D_RES_RH12_TEMP14 - RH: 12Bit, Temperature: 14Bit, by default
HTU21D_RES_RH8_TEMP12  - RH: 8Bit,  Temperature: 12Bit
HTU21D_RES_RH10_TEMP13 - RH: 10Bit, Temperature: 13Bit
HTU21D_RES_RH11_TEMP11 - RH: 11Bit, Temperature: 11Bit
*/
HTU21D htu_ext(HTU21D_RES_RH12_TEMP14);
boolean bHTU_ext = true;
float htu_ext_temp = 0.0;
float htu_ext_humi = 0.0;


//***Geiger +++OLD _4DEL_ ++++
unsigned long counts;     //variable for GM Tube events
float cpm = 0.0;        //variable for CPM
unsigned long previousMillis;  //variable for time measurement
float MSVh = 0.0;
float MRh = 0.0;

//***DHT
//https://github.com/adafruit/DHT-sensor-library
#include <DHT.h>
//#include <DHT_U.h>

//***Distance (length)
boolean bDstLvl = false;
int iDst = 0;
unsigned long lDstOffTime; //Off,Update screens

// Temp DS18B20
//https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <OneWire.h>
#include <DallasTemperature.h>
// Создаем объект OneWire
OneWire oneWire(PIN_DS18B20);
// Создаем объект DallasTemperature для работы с сенсорами, передавая ему ссылку на объект для работы с 1-Wire.
DallasTemperature dallasSensors(&oneWire);
// Специальный объект для хранения адреса устройства
DeviceAddress sensorAddress;
boolean bDS = false;
float ds_temp = 0.0;
float ds_fix = -1.0;  //fix not correct data from sensor (C)


//for out to screen
float scr_ext_temp = 0.0;
float scr_ext_humi = 0.0;
float scr_pres = 0.0;
float scr_int_temp = 0.0;
float scr_int_humi = 0.0;
float scr_mrh = 0.0;



// ===INTERRUPTS==
//setup hw interrupt\timer
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
// on timer interrupt
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

/* 4del
// HW INTERRUPT service
void IRAM_ATTR tube_impulse() {      //subprocedure for capturing events from Geiger Kit
  portENTER_CRITICAL_ISR(&mux);
  counts++;
  portEXIT_CRITICAL_ISR(&mux);
 // Serial.print("'");                //4TEST!
}
*/ 
// timer interrupt service
void IRAM_ATTR onTimer() {
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  //  Serial.print("###");                //4TEST!
  printLocalTime();

}



//==**== End Config ==**==

// !!! добавить отключение прерываний при загрузке обновления !!
void setup_OTGwserver() {
  /*return index page which is stored in serverIndex */
  wserver.on("/", HTTP_GET, []() {
    wserver.sendHeader("Connection", "close");
    wserver.send(200, "text/html", loginIndex);
  });
  wserver.on("/serverIndex", HTTP_GET, []() {
    wserver.sendHeader("Connection", "close");
    wserver.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  wserver.on("/update", HTTP_POST, []() {
    wserver.sendHeader("Connection", "close");
    wserver.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = wserver.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  wserver.begin();
}

// ==== NTP===
//print time to serial
void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo);
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// ==WIFI ================
void setup_wifi() {

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
    if (++wifiCounter > 30) ESP.restart();
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution  http://hostname.local  */
  if (!MDNS.begin(hostname)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
}



// ==MQTT ==публикация
void doPublish(String id, String value) {
  int count_reconnect = 0;
  // если не подключен, то подключаемся. Висит пока не подключится!!
  if (!!!client.connected()) {
    Serial.print("Reconnecting client to "); Serial.println(server);
    while (!!!client.connect(clientId, authMethod, token, conntopic, 0, 0, "online")) {
      Serial.print("#");
      delay(500);
      count_reconnect++;
      // больше 10 попыток - что-то не так...
      if (count_reconnect > 10)  {
        Serial.println("problem with connecting to server, restarting");
        ESP.restart();
      }
    }
    Serial.print("connected with: "); Serial.print(clientId); Serial.print(authMethod); Serial.print(token);
    Serial.println();
  }

  String topic = TOPIC;
  String payload = value ;
  // String topic += id;
  topic.concat(id);
  Serial.print("Publishing on: "); Serial.println(topic);
  Serial.print("Publishing payload: "); Serial.println(payload);
  if (client.publish(topic.c_str(), (char*) payload.c_str())) {
    Serial.println("Publish ok");
  } else {
    Serial.println("Publish failed");
  }
}

void GetDataFromSensors() {
  if (bBME_int) {
    bme_int.readSensor();      //get data
    delay(10);
    bme_int_temp = bme_int.getTemperature_C(); //read data
    Serial.printf("Temp BME280=%0.1f\n", bme_int_temp, " *C");
    bme_int_humi = bme_int.getHumidity(); //read data
    Serial.printf("Humidity BME280=%0.1f\n", bme_int_humi, "%");
    bme_int_pres = (bme_int.getPressure_MB() * 0.7500638); //read data
    Serial.printf("Pressure BME280 =%0.1f\n", bme_int_pres, " mmHg");
    //--  to screen
    scr_pres = bme_int_pres;
    scr_int_temp = bme_int_temp;
    scr_int_humi = bme_int_humi;
  }

  if (bBME_ext) {
    bme_ext.readSensor();      //get data
    delay(10);
    bme_ext_temp = bme_ext.getTemperature_C(); //read data
    Serial.printf("Temp BME280=%0.1f\n", bme_ext_temp, " *C");
    bme_ext_humi = bme_ext.getHumidity(); //read data
    Serial.printf("Humidity BME280=%0.1f\n", bme_ext_humi, "%");
    bme_ext_pres = (bme_ext.getPressure_MB() * 0.7500638); //read data
    Serial.printf("Pressure BME280 =%0.1f\n", bme_ext_pres, " mmHg");
    //--  to screen
    scr_ext_temp = bme_ext_temp;
    scr_ext_humi = bme_ext_humi;
  }

  if (bHTU_ext) {
    htu_ext_temp = htu_ext.readTemperature(); //read data
    Serial.printf("Temp HTU21D=%0.1f\n", htu_ext_temp, " *C");
    htu_ext_humi = htu_ext.readCompensatedHumidity(); //read data
    Serial.printf("Humidity HTU21D=%0.1f\n", htu_ext_humi, "%");
    //--  to screen
    scr_ext_temp = htu_ext_temp;
    scr_ext_humi = htu_ext_humi;
  }

  if (bDS) {
    dallasSensors.requestTemperatures(); // get data
    ds_temp = dallasSensors.getTempC(sensorAddress);  //read data
    ds_temp+=ds_fix; //fix 
    Serial.printf("Temp DS=%0.1f\n", ds_temp, " *C");
    //--  to screen
    scr_ext_temp = ds_temp;
  }
}






//============
void setup()  {

  M5.begin();
  m5.Speaker.mute();
  dacWrite(25, 0); // Speaker OFF
  M5.Lcd.setBrightness(0);

  // sound amplifer off
  // ledcDetachPin (SPEAKER_PIN);
  // pinMode (SPEAKER_PIN, INPUT);

  // Start the ethernet client, open up serial port for debugging
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  setup_wifi();
  setup_OTGwserver();

  //--init timer
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();
  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 8000, true);  //divider over 8000 is not worked correctly
  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);
  // Set alarm to call onTimer function every  second ( 80 000 000Gz / 8000 * 10000 ).
  // Repeat the alarm (third parameter)
  // timerAlarmWrite(timer, 10000, true);
  //100000 - 10s, 600000 - 1m(60s)  6000000 - 10m  36000000 - 1h(60m)

  timerAlarmWrite(timer, LOG_PERIOD, true);
  // Start an alarm
  timerAlarmEnable(timer);
  //--

  // NTP
  //init and get the time
  long gmtOffset_sec = 0;
  gmtOffset_sec = TIMEZONE * 3600;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServerName);
  printLocalTime();


  // init sensors
  //***BME
  if (!bme_ext.begin()) {
    Serial.println("*------> Could not find a valid BME280 ext sensor, check wiring! ***!! ");
    bBME_ext = false;
  } else {
    Serial.println("+++> BME280 ext sensor finded&activated");
    bBME_ext = true;
  }
  bme_ext.setTempCal(0);  //correcting data, need calibrate this!!!   *************

  if (!bme_int.begin()) {
    Serial.println("*------> Could not find a valid BME280 int sensor, check wiring!  ***!! ");
    bBME_int = false;
  } else {
    Serial.println("+++> BME280 int sensor finded&activated");
    bBME_int = true;
  }
  bme_int.setTempCal(0);  //correcting data, need calibrate this!!!   *************

  //***HTU21D
   if (!htu_ext.begin()) {
    Serial.println("*------> Could not find a valid HTU21D ext sensor, check wiring! ***!! ");
    bHTU_ext = false;
  } else {
    Serial.println("+++> HTU21D ext sensor finded&activated");
    bHTU_ext = true;
  
  //***RadSens
    radSens.radSens_init(); /*Initialization function and sensor connection. 
                            Returns false if the sensor is not connected to the I2C bus.*/
  
  uint8_t sensorChipId = radSens.getChipId(); /*Returns chip id, default value: 0x7D.*/

  Serial.print("Chip id: 0x");
  Serial.println(sensorChipId, HEX);

  uint8_t firmWareVer = radSens.getFirmwareVersion(); /*Returns firmware version.*/

  Serial.print("Firmware version: ");
  Serial.println(firmWareVer);

  Serial.println("-------------------------------------");
  Serial.println("Set Sensitivity example:\n");

  uint8_t sensitivity = radSens.getSensitivity(); /*Rerutns the value coefficient used for calculating
                                                    the radiation intensity or 0 if sensor isn't connected.*/

  Serial.print("\t getSensitivity(): "); Serial.println(sensitivity);
  Serial.println("\t setSensitivity(55)... ");

  radSens.setSensitivity(55); /*Sets the value coefficient used for calculating
                                the radiation intensity*/

  sensitivity = radSens.getSensitivity();
  Serial.print("\t getSensitivity(): "); Serial.println(sensitivity);
  Serial.println("\t setSensitivity(105)... ");

  radSens.setSensitivity(105);

  Serial.print("\t getSensitivity(): "); Serial.println(radSens.getSensitivity());

  bool hvGeneratorState = radSens.getHVGeneratorState(); /*Returns state of high-voltage voltage Converter.
                                                           If return true -> on
                                                           If return false -> off or sensor isn't conneted*/

  Serial.print("\n\t HV generator state: "); Serial.println(hvGeneratorState);
  Serial.println("\t setHVGeneratorState(false)... ");

  radSens.setHVGeneratorState(false); /*Set state of high-voltage voltage Converter.
                                        if setHVGeneratorState(true) -> turn on HV generator
                                        if setHVGeneratorState(false) -> turn off HV generator*/
  
  hvGeneratorState = radSens.getHVGeneratorState();
  Serial.print("\t HV generator state: "); Serial.println(hvGeneratorState);
  Serial.println("\t setHVGeneratorState(true)... ");

  radSens.setHVGeneratorState(true);

  hvGeneratorState = radSens.getHVGeneratorState();
  Serial.print("\t HV generator state: "); Serial.print(hvGeneratorState);
  Serial.println("\n-------------------------------------");
  
  
  
  
  }
 

/* 4del
  //***Geiger
  pinMode(interruptPin, INPUT);
  Serial.println("init geiger");
  counts = 0;
  cpm = 0.0;
  //  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  attachInterrupt(digitalPinToInterrupt(interruptPin), tube_impulse, FALLING); //define external interrupts
  //---
*/

  // ***dallas
  dallasSensors.begin();

  // Поиск устройства:
  // Ищем адрес устройства по порядку (индекс задается вторым параметром функции)
  if (!dallasSensors.getAddress(sensorAddress, 0)) {
    Serial.println("*------> Could not find Dallas sensor ***");
    bDS = false;
  } else {
    Serial.println("+++> Dallas sensor finded");
    bDS = true;
  }

  // Устанавливаем разрешение датчика в 12 бит (мы могли бы установить другие значения, точность уменьшится, но скорость получения данных увеличится
  dallasSensors.setResolution(sensorAddress, 12);

  Serial.print("Разрешение датчика: ");
  Serial.print(dallasSensors.getResolution(sensorAddress), DEC);
  Serial.println();

  // ----



  //distance
  //  pinMode(distPin, INPUT);
  // ln swnsor
  pinMode(lnPin, INPUT);
  lDstOffTime = micros();

  // *** show on start
  ScreenOn();
  bScrOn = true;

  if (getLocalTime(&timeinfo)) {
    ShowTime(timeinfo);
  }
  else
  {
    Serial.println("Failed to obtain time");
    //    return;
  }
  delay(2000);

  GetDataFromSensors();

  scr_mrh = 88.8;
  OutToScr( scr_ext_temp, scr_ext_humi, scr_pres, scr_int_temp, scr_int_humi , scr_mrh );

  // OutToScr( ds_temp, bme_ext_humi, bme_ext_pres, bme_int_temp, bme_int_humi , 0.0 );
  OffScrTime = micros() + 2000000;
}


void loop()  {
  //web OTG
  wserver.handleClient();

  // If Timer has fired
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {

    Serial.println();
    Serial.println("-----------------------");
    printLocalTime();


    // собираем данные с  датчиков и отправляем...
/* 4del
    // --geiger--
    //calc per minutes
    cpm = counts * (600000.0 / LOG_PERIOD);
    Serial.println();
    Serial.print("counts="); Serial.println(counts);
    Serial.print("CPM="); Serial.println(cpm);
    Serial.print("CPM_calc="); Serial.println(counts * (600000.0 / LOG_PERIOD));

    MSVh = cpm * CF;
    MRh = cpm * CF * 100;  //100 рентген = 1 зиверт

    Serial.print("MSVhCF="); Serial.println(MSVh);
    Serial.print("MRhCF="); Serial.println(MRh);

    counts = 0;
*/

//radSens
  Serial.print("Rad intensy dyanmic: ");

  Serial.println(radSens.getRadIntensyDyanmic()); /*Returns radiation intensity (dynamic period T < 123 sec).*/

  Serial.print("Rad intensy static: ");
  
  Serial.println(radSens.getRadIntensyStatic()); /*Returns radiation intensity (static period T = 500 sec).*/


    // отравляем данные на сервер...
    // пока отправка вкл. led
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.print("Send to server - ");
    printLocalTime();

    // send uRgh from geiger
    doPublish("R0", String(MRh, 1));

    //-----
    GetDataFromSensors();

    if (bBME_int) {
      if (bme_int_temp > -50 and bme_int_temp < 50 )  {
        doPublish("T2", String(bme_int_temp, 1));
      }
      if (bme_int_humi > 5 and bme_int_humi < 101 )  {
        doPublish("H2", String(bme_int_humi, 1));
      }
      if (bme_int_pres > 600 and bme_int_pres < 900 )  {
        doPublish("P2", String(bme_int_pres, 1));
      }
    }

    if (bBME_ext) {
      if (bme_ext_temp > -50 and bme_ext_temp < 50 )  {
        doPublish("T1", String(bme_ext_temp, 1));
      }
      if (bme_ext_humi > 5 and bme_ext_humi < 101 )  {
        doPublish("H1", String(bme_ext_humi, 1));
      }
      if (bme_ext_pres > 600 and bme_ext_pres < 900 )  {
        doPublish("P1", String(bme_ext_pres, 1));
      }
    }

    if (bDS) {
      if (ds_temp > -50 and ds_temp < 50 )  {
        doPublish("T0", String(ds_temp, 1));
      }
    }

    if (bHTU_ext) {
      if (htu_ext_temp > -50 and htu_ext_temp < 50 )  {
        doPublish("T1", String(htu_ext_temp, 1));
      }
      if (htu_ext_humi > 5 and htu_ext_humi < 101 )  {
        doPublish("H1", String(htu_ext_humi, 1));
      }
    }


    // LED OFF
    digitalWrite(LED_BUILTIN, LOW);
  }

// distatce sensor
iDst = analogRead(lnPin);

if ( lDstOffTime > micros()) { //passage through 0
  lDstOffTime = micros();
}

if ((iDst > 3500) and !bDstLvl and (lDstOffTime < micros()) ) {
  bDstLvl = true;
}
// Serial.println(bDstLvl);
//  Serial.println(iDst);


// =Buttons=
if (M5.BtnA.wasPressed()  or bDstLvl )  {
  Serial.print(" _Pressed kb A - ");
  printLocalTime();

  ScreenOn();
  bScrOn = true;

  GetDataFromSensors();

  scr_mrh = MRh;
  OutToScr( scr_ext_temp, scr_ext_humi, scr_pres, scr_int_temp, scr_int_humi , scr_mrh );

  OffScrTime = micros() + 5000000;
  lDstOffTime = OffScrTime + 1000000;
}

if (M5.BtnB.wasPressed())
{
  Serial.print(" _Pressed kb B - ");
  printLocalTime();

  ScreenOn();
  bScrOn = true;

  if (getLocalTime(&timeinfo)) {
    ShowTime(timeinfo);
    OffScrTime = micros() + 5000000;
  }
  else
  {
    Serial.println("Failed to obtain time");
    //    return;
  }
}


//off screens
if  (bScrOn) {
  if (micros() >= OffScrTime) {
    ScreenOff();
    bScrOn = false;
  }
}

bDstLvl = false;

M5.update();

}
// ================
