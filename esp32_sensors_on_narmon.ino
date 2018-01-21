/*
transmiting data from wemos r32 (esp32) with sensors to narodmon.ru across mqtt
*/

#include "settings.h"
#include "esp_system.h"
#include "soc/rtc.h" 


#include <SPI.h>
#include <WiFi.h>


#include <WiFiUdp.h>
IPAddress timeServerIP;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[ NTP_PACKET_SIZE];
unsigned long ntp_time = 0;
WiFiUDP udp;

#include <PubSubClient.h>

#include <Wire.h>
#include "cactus_io_BME280_I2C.h"
// Create the BME280 object
BME280_I2C bme;              //def
// or BME280_I2C bme(BME_ADDR);  // I2C using address 
float pressure = 0.0;
float temp = 0.0;
float humidity = 0.0;

//Geiger
#define LOG_PERIOD 600000  //Logging period in milliseconds, 600000=10m
//#define MAX_PERIOD 600000  //Maximum logging period without modifying this sketch 60000s=1m 600000=10m

unsigned long counts;     //variable for GM Tube events
float cpm = 0.0;        //variable for CPM
//unsigned long multiplier;  //variable for calculation CPM in this sketch
unsigned long previousMillis;  //variable for time measurement
float MSVh = 0.0;
float MRh = 0.0;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// INTERRUPT 
void IRAM_ATTR tube_impulse(){       //subprocedure for capturing events from Geiger Kit
  portENTER_CRITICAL_ISR(&mux);
  counts++;
  portEXIT_CRITICAL_ISR(&mux);
  Serial.print("'");                //4TEST! 
}

// OTA
//#include <ArduinoOTA.h>

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);


//============
void setup()
{

  // Start the ethernet client, open up serial port for debugging
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
   setup_wifi();


 // init sensor
   if (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  Serial.println("BME280 sensor activated");
  bme.setTempCal(-1);  //correct data, need? TEST this!!!   TEST *************

     GetNTP();    //получили время, записано в ntp_time, в seril отобразилось. можно использовать где-нибудь еще

//Geiger
  pinMode(interruptPin, INPUT);
  Serial.println("init geiger"); 
  counts = 0;
  cpm = 0;
//  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  attachInterrupt(digitalPinToInterrupt(interruptPin), tube_impulse, FALLING); //define external interrupts



//======ArduinoOTA

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

 /* 
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("OTA Ready");
  Serial.print("OTA IP address: ");
  Serial.println(WiFi.localIP());
// ==============================
*/
}



void loop()
{
//ArduinoOTA
//ArduinoOTA.handle();

// считаем тики, как вышло время - собираем данные с других датчиков и отправляем...
unsigned long currentMillis = millis();
 if(currentMillis - previousMillis > LOG_PERIOD){
   previousMillis = currentMillis;
   // --geiger--
   //время учета увеличил в 10 раз, тут уменьшил
   cpm = counts/10;
//   cpm = counts * multiplier/10;
   MSVh = cpm/151;
   MRh = cpm*1,96078;

   Serial.print("CPM="); Serial.println(cpm);
   Serial.print("MSVh="); Serial.println(MSVh);
   Serial.print("MRh="); Serial.println(MRh);

   counts = 0;

   // отравили данные на сервер...
   // пока отправка вкл. led
   digitalWrite(LED_BUILTIN, HIGH);

   // send uRgh from geiger
   doPublish("R0", String(MRh, 1));
   //-----
   // --- BME280
   bme.readSensor();      //получили данные с датчика
   delay(10);
   bmeGotTemp();
   delay(10);
   bmeGotHumidity();
   delay(10);
   bmeGotPressure();
   delay(10);

     digitalWrite(LED_BUILTIN, LOW);

  }
}



// ================
// ==WIFI ================
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 50) {
    delay(500);
    i++;
    Serial.print(".");
  }
  // если до сих пор нет подключения - перегрузить.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Connection failed, restart");
    WiFi.disconnect();
    ESP.restart();
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// ==MQTT ==публикация
void doPublish(String id, String value) {
// если не подключен, то подключаемся. Висит пока не подключится!!
  if (!!!client.connected()) {
     Serial.print("Reconnecting client to "); Serial.println(server);
     while (!!!client.connect(clientId, authMethod, token, conntopic,0,0,"online")) {
        Serial.print("#");
        delay(500);
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

/*
// ############# TEST!!! ### 4DEL!!!! 
void doPublish(String id, String value) {
  Serial.print("ID: "); Serial.println(id);
  Serial.print("Val: "); Serial.println(value);
}
*/
//==

//BME280
// получаем данные - gotXxxx и публикуем

void bmeGotTemp() {
     //
    float  temp = bme.getTemperature_C();
    //Serial.print(temp); Serial.print("*C  \t");
    Serial.printf("BME280  temp=%0.1f\n", temp);
    doPublish("t0", String(temp, 1));
}

void bmeGotHumidity() {
     //
    float  humidity = bme.getHumidity();
    //Serial.print(humidity); Serial.print("H  \t");
    Serial.printf("BME280  humidity=%0.1f\n", humidity);
    doPublish("h0", String(humidity, 1));
}

void bmeGotPressure() {
     //
    float  pressure = (bme.getPressure_MB() * 0.7500638);
    //Serial.print(pressure); Serial.print("p  \t");
    Serial.printf("BME280 pressure=%0.1f\n", pressure);
    doPublish("p0", String(pressure, 1));
}


//Geiger
//---

// ==== NTP===
/**
 * Посылаем и парсим запрос к NTP серверу
 */
bool GetNTP(void) {
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP);
  delay(1000);

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("No packet yet");
    return false;
  }
  else {
    Serial.print("packet received, length=");
    Serial.println(cb);
// Читаем пакет в буфер
    udp.read(packetBuffer, NTP_PACKET_SIZE);
// 4 байта начиная с 40-го сождержат таймстамп времени - число секунд
// от 01.01.1900
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
// Конвертируем два слова в переменную long
    unsigned long secsSince1900 = highWord << 16 | lowWord;
// Конвертируем в UNIX-таймстамп (число секунд от 01.01.1970
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
// Делаем поправку на местную тайм-зону
    ntp_time = epoch + TIMEZONE*3600;
    Serial.print("Unix time = ");
    Serial.println(ntp_time);
// и в привычном виде:
   uint16_t s = ( ntp_time )%60;
   uint16_t m = ( ntp_time/60 )%60;
   uint16_t h = ( ntp_time/3600 )%24;
   Serial.print("Time: ");
   Serial.print(h);
   Serial.print(":");
   Serial.print(m);
   Serial.print(":");
   Serial.println(s);
   //---

  }
  return true;
}

/**
 * Посылаем запрос NTP серверу на заданный адрес
 */
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
// Очистка буфера в 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
// Формируем строку зыпроса NTP сервера
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
// Посылаем запрос на NTP сервер (123 порт)
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
