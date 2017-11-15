/*
transmiting data from wemos r32 (esp32) with sensors to narodmon.ru across mqtt
*/

#include "settings.h"

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
BME280_I2C bme;              // I2C using default 0x77
// or BME280_I2C bme(0x76);  // I2C using address 0x76
float pressure = 0.0;
float temp = 0.0;
float humidity = 0.0;

//Geiger
#define LOG_PERIOD 600000  //Logging period in milliseconds, recommended value 15000-60000.
#define MAX_PERIOD 600000  //Maximum logging period without modifying this sketch 60000s=1m

unsigned long counts;     //variable for GM Tube events
float cpm;        //variable for CPM
unsigned long multiplier;  //variable for calculation CPM in this sketch
unsigned long previousMillis;  //variable for time measurement
float MSVh;
float MR;

//DHT22
// - Adafruit Unified Sensor Library: https://github.com/adafruit/Adafruit_Sensor
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
DHT dht(DHT_PIN, DHT_VERSION); //define temperature and humidity sensor

//temp
#include "ds18b20.h" //https://github.com/feelfreelinux/ds18b20


//MH-Z19
SoftwareSerial co2Serial(MH_Z19_RX, MH_Z19_TX); // define MH-Z19

/*
//sleep
#define GPIO_DEEP_SLEEP_DURATION     600  // sleep ... seconds and then wake up
RTC_DATA_ATTR static time_t last;        // remember last boot in RTC Memory
*/


WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);


//============
void setup()
{

  // Start the ethernet client, open up serial port for debugging
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
//   digitalWrite(LED_BUILTIN, HIGH);
   setup_wifi();


 // init sensor
   if (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  Serial.println("BME280 sensor activated");
  bme.setTempCal(-1);  //correct data, need? TEST this!!!

//

/* поспать не удастся  - считаем постоянно тики с рентгена
//=====
    //sleep - wakeup
    struct timeval now;
    Serial.println("wakeup: start ESP32 loop \n");
    gettimeofday(&now, NULL);  //получить приблизительное время от системы. для индикации сколько спали.
*/
     GetNTP();    //получили время, записано в ntp_time, в seril отобразилось. можно использовать где-нибудь еще
/*
     bme.readSensor();      //получили данные с датчика
     delay(1000);
     bme.readSensor();      //получили данные с датчика
     delay(10);
     // отправка на сервер
     gotTemp();
     delay(10);
     gotHumidity();
     delay(10);
     gotPressure();
     delay(10);

    // close mqtt-connection
    client.disconnect();
    // .. and go sleep
//    Serial.println("deep sleep (%lds since last reset, %lds since last boot)\n",now.tv_sec,now.tv_sec-last);
    Serial.print("last=");  Serial.println(last);
    Serial.print("go deep sleep (%lds since last reset, %lds since last boot)\n");     Serial.print(now.tv_sec); Serial.print("; "); Serial.println(now.tv_sec-last);
    last = now.tv_sec;
    Serial.print("last=");  Serial.println(last);
 //спать...
   esp_sleep_enable_timer_wakeup(1000000LL * GPIO_DEEP_SLEEP_DURATION);
   Serial.println("Setup ESP32 to sleep for every " + String(GPIO_DEEP_SLEEP_DURATION) + " Seconds");
   Serial.println("Going to sleep now");
   digitalWrite(LED_BUILTIN, LOW);
   esp_deep_sleep_start();
*/

//Geiger
counts = 0;
cpm = 0;
multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
attachInterrupt(digitalPinToInterrupt(interruptPin), tube_impulse, FALLING); //define external interrupts
//
//temperature
DS_init(DS_PIN);

//DHT22
// Initialize device.
  dht.begin();
  Serial.println("DHTxx Unified Sensor Example");
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.println("Temperature");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");
  Serial.println("------------------------------------");
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.println("Humidity");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");
  Serial.println("------------------------------------");

//MH-Z19

Delay(300); // задержка на разогрев MH-Z19
}



void loop()
{
// считаем тики, как вышло время - собираем данные с других датчиков и отправляем...
unsigned long currentMillis = millis();
 if(currentMillis - previousMillis > LOG_PERIOD){
   previousMillis = currentMillis;
   // --geiger--
   cpm = counts * multiplier/10; //время учета увеличил в 10 раз, тут уменьшил
   MSVh = cpm/151;
   MR = MSVh * 100;

//    Serial.print(cpm);
   Serial.print("CPM="); Serial.println(cpm);
   Serial.print("MSVh="); Serial.println(MSVh);
   Serial.print("MR="); Serial.println(MR);
//    Serial.printf("%s\n", );(" CPM=%d",cpm);
//    Serial.printf("%s\n", );(" MSVh=%d MSVh",MSVh);
//    Serial.printf("%s\n", );(" MR=%d MR",MR);
   counts = 0;
   // отравили данные на сервер...
   doPublish("gg-MR0", String(MR, 1));
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
   //--- DS
   dsGotTemp();
   delay(10);
   //--- DHT22
   dhtGotTemp();
   delay(10);
   dhtGotHumidity();
   //-- MH
   mhGotPpm();
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
        Serial.print(".");
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
//==

//BME280
// получаем данные - gotXxxx и публикуем

void bmeGotTemp() {
     //
    float  temp = bme.getTemperature_C();
    //Serial.print(temp); Serial.print("*C  \t");
    Serial.printf("BME280  temp=%0.1f\n", temp);
    doPublish("bme-t0", String(temp, 1));
}

void bmeGotHumidity() {
     //
    float  humidity = bme.getHumidity();
    //Serial.print(humidity); Serial.print("H  \t");
    Serial.printf("BME280  humidity=%0.1f\n", humidity);
    doPublish("bme-h0", String(humidity, 1));
}

void bmeGotPressure() {
     //
    float  pressure = (bme.getPressure_MB() * 0.7500638);
    //Serial.print(pressure); Serial.print("p  \t");
    Serial.printf("BME280 pressure=%0.1f\n", pressure);
    doPublish("bme-p0", String(pressure, 1));
}

//DHT22
void dhtGotTemp() {
     //
     sensors_event_t event;
     dht.temperature().getEvent(&event);
     if (isnan(event.temperature)) {
       Serial.println("Error reading DHT temperature!");
     }
     else {
       Serial.print("DHT temperature: ");
       Serial.print(event.temperature);
       Serial.println(" *C");
       doPublish("dht-t0", String(event.temperature, 1));
     }
}

void dhtGotHumidity() {
     //
     sensors_event_t event;
     dht.humidity().getEvent(&event);
     if (isnan(event.relative_humidity)) {
       Serial.println("Error reading humidity!");
     }
     else {
       Serial.print("DHT humidity: ");
       Serial.print(event.relative_humidity);
       Serial.println("%");
       doPublish("dht-h0", String(event.relative_humidity, 1));
     }
}


//temp-DS
void dsGotTemp() {
     //
    float  temp = DS_get_temp();
    //Serial.print(temp); Serial.print("*C  \t");
    Serial.printf("DS  temp=%0.1f\n", temp);
    doPublish("ds-t0", String(temp, 1));
}


//MH-Z19
int readCO2()
{

  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  // command to ask for data
  byte response[9]; // for answer

  co2Serial.write(cmd, 9); //request PPM CO2

  // The serial stream can get out of sync. The response starts with 0xff, try to resync.
  while (co2Serial.available() > 0 && (unsigned char)co2Serial.peek() != 0xFF) {
    co2Serial.read();
  }

  memset(response, 0, 9);
  co2Serial.readBytes(response, 9);

  if (response[1] != 0x86)
  {
    Serial.println("Invalid response from co2 sensor!");
    return -1;
  }

  byte crc = 0;
  for (int i = 1; i < 8; i++) {
    crc += response[i];
  }
  crc = 255 - crc + 1;

  if (response[8] == crc) {
    int responseHigh = (int) response[2];
    int responseLow = (int) response[3];
    int ppm = (256 * responseHigh) + responseLow;
    return ppm;
  } else {
    Serial.println("MH-Z19 CRC error!");
    return -1;
  }
}

void mhGotPpm() {
     //
    int  ppm = readCO2();
    if (ppm > 100 || ppm < 6000)
  {
    Serial.printf("MH-Z19 ppm=%d\n", ppm);
    doPublish("mh-ppm0", String(ppm, 1));
  }
  else
  {
    Serial.println("MH-Z19 error ppm");
  }
}


//Geiger
// in loop


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
