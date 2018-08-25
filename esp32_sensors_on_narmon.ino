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
//BME280_I2C bme;              //def
// or BME280_I2C bme(BME_ADDR);  // I2C using address 
BME280_I2C bme(BME_ADDR);  // I2C using address 

float pressure = 0.0;
float temp = 0.0;
float humidity = 0.0;

//***Geiger
unsigned long counts;     //variable for GM Tube events
float cpm = 0.0;        //variable for CPM
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

//***DHT
//#include <Adafruit_Sensor.h>
#include <DHT.h>
//#include <DHT_U.h>

//DHT_Unified dht(DHTPIN, DHTTYPE);
DHT dht(DHTPIN, DHTTYPE); 

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);


//============
void setup()
{

  // Start the ethernet client, open up serial port for debugging
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
   setup_wifi();

// NTP
    GetNTP();    //получили время, записано в ntp_time, в seril отобразилось. можно использовать где-нибудь еще
 // init sensors
 //***BME
   if (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  Serial.println("BME280 sensor activated");
  bme.setTempCal(0);  //correcting data, need calibrate this!!!   *************

//***Geiger
  pinMode(interruptPin, INPUT);
  Serial.println("init geiger"); 
  counts = 0;
  cpm = 0.0;
//  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  attachInterrupt(digitalPinToInterrupt(interruptPin), tube_impulse, FALLING); //define external interrupts

//***DHT
  // Initialize device.
  dht.begin();
  Serial.println("DHTxx Unified Sensor init");

}



void loop()
{

// считаем тики, как вышло время - собираем данные с других датчиков и отправляем...
unsigned long currentMillis = millis();
 if(currentMillis - previousMillis > LOG_PERIOD){
   previousMillis = currentMillis;
   // --geiger--
   //calc per minutes
   cpm = counts*(60000.0/LOG_PERIOD);
   Serial.println();
   Serial.print("counts="); Serial.println(counts);
   Serial.print("CPM="); Serial.println(cpm);
   Serial.print("CPM_calc="); Serial.println(counts*(60000.0/LOG_PERIOD));

   MSVh = cpm * CF;
   MRh = cpm * CF * 100;  //100 рентген = 1 зиверт
   
   Serial.print("MSVhCF="); Serial.println(MSVh);
   Serial.print("MRhCF="); Serial.println(MRh);

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

 //--- DHT22
   dhtGotTemp();
   delay(10);
   dhtGotHumidity();
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
 int count_reconnect = 0;
// если не подключен, то подключаемся. Висит пока не подключится!!
  if (!!!client.connected()) {
     Serial.print("Reconnecting client to "); Serial.println(server);
     while (!!!client.connect(clientId, authMethod, token, conntopic,0,0,"online")) {
        Serial.print("#");
        delay(500);
        count_reconnect++;
        // больше 10 попыток - что-то не так... 
        if (count_reconnect>10)  {
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
    Serial.printf("Temp BME280=%0.1f\n", temp, " *C");
    if (temp>-50 and temp<50 )  {
      doPublish("t0", String(temp, 1));
    }
}

void bmeGotHumidity() {
     //
    float  humidity = bme.getHumidity();
    //Serial.print(humidity); Serial.print("H  \t");
    Serial.printf("Humidity BME280=%0.1f\n", humidity, "%");
    if (humidity>5 and humidity<99 )  {
      doPublish("h0", String(humidity, 1));
    }
}

void bmeGotPressure() {
     //
    float  pressure = (bme.getPressure_MB() * 0.7500638);
    //Serial.print(pressure); Serial.print("p  \t");
    Serial.printf("Pressure BME280 =%0.1f\n", pressure," mmHg");
    if (pressure>600 and pressure<900 )  {
      doPublish("p0", String(pressure, 1));
    }
}

//DHT22
void dhtGotTemp() {
     //
 //    sensors_event_t event;
 //      dht.temperature().getEvent(&event);
//     float t = event.temperature;
  // Read temperature as Celsius (the default)
      float t = dht.readTemperature(); 
  //     if (isnan(event.temperature)) {
     if (isnan(t)) {
       Serial.println("Error reading DHT temperature!");
     }
     else {
       Serial.printf("Temp DHT=%0.1f\n", t, " *C");
//       doPublish("dht-t0", String(event.temperature, 1));
        if (t>-50 and t<50 )  {
           doPublish("t1", String(t, 1));
        }
     }
}

void dhtGotHumidity() {
     //
//     sensors_event_t event;
//     dht.humidity().getEvent(&event);
//    float h = event.relative_humidity;
      float h = dht.readHumidity(); 
  //     if (isnan(event.relative_humidity)) {
    if (isnan(h)) {
       Serial.println("Error reading humidity!");
     }
     else {
       Serial.printf("Humidity DHT=%0.1f\n", h, "%");
//       doPublish("dht-h0", String(event.relative_humidity, 1));
        if (h>5 and h<99 )  {
          doPublish("h1", String(h, 1));
        }
     }
}
//Geiger
//null
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
