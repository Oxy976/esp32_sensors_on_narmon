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
#include "cactus_io_BME280_I2C.h"
// Create the BME280 object
BME280_I2C bme_ext(BME_ext_ADDR);  // I2C using address
BME280_I2C bme_int(BME_int_ADDR);  // I2C using address
boolean bBME_ext=true;
boolean bBME_int=true;

float bme_ext_pres = 0.0;
float bme_ext_temp = 0.0;
float bme_ext_humi = 0.0;
float bme_int_pres = 0.0;
float bme_int_temp = 0.0;
float bme_int_humi = 0.0;

//***Geiger
unsigned long counts;     //variable for GM Tube events
float cpm = 0.0;        //variable for CPM
unsigned long previousMillis;  //variable for time measurement
float MSVh = 0.0;
float MRh = 0.0;

//***DHT
//#include <Adafruit_Sensor.h>
#include <DHT.h>
//#include <DHT_U.h>

//DHT_Unified dht(DHTPIN, DHTTYPE);
DHT dht(DHTPIN, DHTTYPE);

//***Distance (length)
boolean bDstLvl = false;
int iDst = 0;
unsigned long lDstOffTime; //Off,Update screens


// ===INTERRUPTS==
//hw interrupt
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
// on timer interrupt
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// HW INTERRUPT service
void IRAM_ATTR tube_impulse() {      //subprocedure for capturing events from Geiger Kit
  portENTER_CRITICAL_ISR(&mux);
  counts++;
  portEXIT_CRITICAL_ISR(&mux);
  Serial.print("'");                //4TEST!
}

// timer interrupt service
void IRAM_ATTR onTimer() {
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  //  Serial.print("###");                //4TEST!
  printLocalTime();

}



//==**== End Config ==**==


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








//============
void setup()
{

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

  //--interrupt
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
    Serial.println("!!*** Could not find a valid BME280 ext sensor, check wiring! ***!! ");
    bBME_ext=false;
  }
  Serial.println("BME280 ext sensor activated");
  bme_ext.setTempCal(0);  //correcting data, need calibrate this!!!   *************

    if (!bme_int.begin()) {
    Serial.println("!!*** Could not find a valid BME280 int sensor, check wiring!  ***!! ");
    bBME_int=false;
  }
  Serial.println("BME280 int sensor activated");
  bme_int.setTempCal(0);  //correcting data, need calibrate this!!!   *************

  
  //***Geiger
  pinMode(interruptPin, INPUT);
  Serial.println("init geiger");
  counts = 0;
  cpm = 0.0;
  //  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  attachInterrupt(digitalPinToInterrupt(interruptPin), tube_impulse, FALLING); //define external interrupts


  //distance
  //  pinMode(distPin, INPUT);
  // ln swnsor
  pinMode(lnPin, INPUT);
  lDstOffTime = micros();

  // show on start
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
   if bBME_ext {
    bme_ext.readSensor();      //получили данные с датчика ext
    delay(10);
     bme_ext_temp = bme_ext.getTemperature_C();
      Serial.printf("Temp BME280=%0.1f\n", bme_ext_temp, " *C");
     bmeGotHumidity();
      bme_ext_humi = bme_ext.getHumidity();
      Serial.printf("Humidity BME280=%0.1f\n", bme_ext_humi, "%");
    bmeGotPressure();
      bme_ext_pres = (bme_ext.getPressure_MB() * 0.7500638);
      Serial.printf("Pressure BME280 =%0.1f\n", bme_ext_pres, " mmHg");
    }
    
    if bBME_int {
    bme_int.readSensor();      //получили данные с датчика int
    delay(10);
      bme_int_temp = bme_int.getTemperature_C();
      Serial.printf("Temp BME280=%0.1f\n", bme_int_temp, " *C");
    bmeGotHumidity();
      bme_int_humi = bme_int.getHumidity();
      Serial.printf("Humidity BME280=%0.1f\n", bme_int_humi, "%");
    bmeGotPressure();
      bme_int_pres = (bme_int.getPressure_MB() * 0.7500638);
      Serial.printf("Pressure BME280 =%0.1f\n", bme_int_pres, " mmHg");
    }  
    OutToScr( bbme_ext_temp, bme_ext_humi, bme_ext_pres, bme_int_temp, bme_int_humi , 0.0 );
    OffScrTime = micros() + 2000000;
}


void loop()
{
  //web OTG
  wserver.handleClient();

  // If Timer has fired
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {

    Serial.println();
    Serial.println("-----------------------");
    printLocalTime();


    // собираем данные с  датчиков и отправляем...

    // --geiger--
    //calc per minutes
    cpm = counts * (60000.0 / LOG_PERIOD);
    Serial.println();
    Serial.print("counts="); Serial.println(counts);
    Serial.print("CPM="); Serial.println(cpm);
    Serial.print("CPM_calc="); Serial.println(counts * (60000.0 / LOG_PERIOD));

    MSVh = cpm * CF;
    MRh = cpm * CF * 100;  //100 рентген = 1 зиверт

    Serial.print("MSVhCF="); Serial.println(MSVh);
    Serial.print("MRhCF="); Serial.println(MRh);

    counts = 0;

    // отравили данные на сервер...
    // пока отправка вкл. led
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.print("Send to server - ");
    printLocalTime();

    // send uRgh from geiger
    doPublish("R0", String(MRh, 1));
    
    //-----
    // --- BME280
    if bBME_ext {
    bme_ext.readSensor();      //получили данные с датчика ext
    delay(10);
     bme_ext_temp = bme_ext.getTemperature_C();
      Serial.printf("Temp BME280=%0.1f\n", bme_ext_temp, " *C");
      if (bme_ext_temp > -50 and bme_ext_temp < 50 )  {
        doPublish("t0", String(bme_ext_temp, 1));
      }
    bmeGotHumidity();
      bme_ext_humi = bme_ext.getHumidity();
      Serial.printf("Humidity BME280=%0.1f\n", bme_ext_humi, "%");
      if (bme_ext_humi > 5 and bme_ext_humi < 99 )  {
        doPublish("h0", String(bme_ext_humi, 1));
      }
    bmeGotPressure();
      bme_ext_pres = (bme_ext.getPressure_MB() * 0.7500638);
      Serial.printf("Pressure BME280 =%0.1f\n", bme_ext_pres, " mmHg");
      if (bme_ext_pres > 600 and bme_ext_pres < 900 )  {
        doPublish("p0", String(bme_ext_pres, 1));
      }
    }
    
    if bBME_int {
    bme_int.readSensor();      //получили данные с датчика int
    delay(10);
      bme_int_temp = bme_int.getTemperature_C();
      Serial.printf("Temp BME280=%0.1f\n", bme_int_temp, " *C");
      if (bme_int_temp > -50 and bme_int_temp < 50 )  {
        doPublish("t1", String(bme_int_temp, 1));
      }
    bmeGotHumidity();
      bme_int_humi = bme_int.getHumidity();
      Serial.printf("Humidity BME280=%0.1f\n", bme_int_humi, "%");
      if (bme_int_humi > 5 and bme_int_humi < 99 )  {
        doPublish("h1", String(bme_int_humi, 1));
      }
    bmeGotPressure();
      bme_int_pres = (bme_int.getPressure_MB() * 0.7500638);
      Serial.printf("Pressure BME280 =%0.1f\n", bme_int_pres, " mmHg");
      if (bme_int_pres > 600 and bme_int_pres < 900 )  {
        doPublish("p1", String(bme_int_pres, 1));
      }
    }
    
    

    // LED OFF
    digitalWrite(LED_BUILTIN, LOW);
  }

  // distatce sensor
  iDst = analogRead(lnPin);
  //  Serial.println(iDst);

  if ((iDst > 3500) and !bDstLvl and (lDstOffTime < micros()) ) {
    bDstLvl = true;
  }
  // Serial.println(bDstLvl);
  //  Serial.println(iDst);


  // =Buttons=
  if (M5.BtnA.wasPressed()  or bDstLvl )  {
    //if (M5.BtnA.wasPressed()  )  {
    Serial.print(" _Pressed kb A - ");
    printLocalTime();

    ScreenOn();
    bScrOn = true;

   if bBME_ext {
    bme_ext.readSensor();      //получили данные с датчика ext
    delay(10);
     bme_ext_temp = bme_ext.getTemperature_C();
      Serial.printf("Temp BME280=%0.1f\n", bme_ext_temp, " *C");
     bmeGotHumidity();
      bme_ext_humi = bme_ext.getHumidity();
      Serial.printf("Humidity BME280=%0.1f\n", bme_ext_humi, "%");
    bmeGotPressure();
      bme_ext_pres = (bme_ext.getPressure_MB() * 0.7500638);
      Serial.printf("Pressure BME280 =%0.1f\n", bme_ext_pres, " mmHg");
    }
    
    if bBME_int {
    bme_int.readSensor();      //получили данные с датчика int
    delay(10);
      bme_int_temp = bme_int.getTemperature_C();
      Serial.printf("Temp BME280=%0.1f\n", bme_int_temp, " *C");
    bmeGotHumidity();
      bme_int_humi = bme_int.getHumidity();
      Serial.printf("Humidity BME280=%0.1f\n", bme_int_humi, "%");
    bmeGotPressure();
      bme_int_pres = (bme_int.getPressure_MB() * 0.7500638);
      Serial.printf("Pressure BME280 =%0.1f\n", bme_int_pres, " mmHg");
    }  
    OutToScr( bbme_ext_temp, bme_ext_humi, bme_ext_pres, bme_int_temp, bme_int_humi , MRh );
    
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
