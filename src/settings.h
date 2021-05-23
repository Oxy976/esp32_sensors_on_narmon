// Network settings
// --
const char* hostname = "M5weather";
const char* ssid = "private";
const char* password = "nhbyflwfnmcbv";
// --

//M5


// LED
const int LED_BUILTIN = 15; //Onboard LED Pin (M5 fire)

//wemos esp32
// LED
//const int LED_BUILTIN = 1; //Onboard LED Pin  ***! change this for M5!


// Time settings
const char* ntpServerName = "192.168.111.1";
const int   TIMEZONE = 3;
const int   daylightOffset_sec = 0;

// narmon

//#define SRV   "narodmon.ru"
#define SRV   "192.168.111.10"
// 4del:   #define MAC  "240AC40C5C60" //esp
#define MAC  "30AEA469B904"  //m5
#define PASS  "15334"
#define USERNAME  "ghost-weather"
// 4del:   #define TOPIC  "login/wemos-r32/"
#define TOPIC  "login/m5-esp32/"


//--

//mqtt4narmon
char server[] = SRV ;
char authMethod[] = USERNAME;
char token[] = PASS;
char clientId[] = MAC;
char conntopic[] = TOPIC "status";


//***BME280 - I2C using  0x77 (default) or 0x76
const int BME_ext_ADDR=0x77; 
const int BME_int_ADDR=0x76; 

//***HTU21D - I2C using  0x40 -  external sensor


//***Geiger
/* pin that is attached to interrupt 12 = ESP32 GIO12 */
byte interruptPin = 2;
const int LOG_PERIOD = 6000000; //Logging period   100000 - 10s, 600000 - 1m(60s)  6000000 - 10m  36000000 - 1h(60m)
//const int LOG_PERIOD=600000;  // 4test
//J305ß
const float CF = 0.0051; //different for different tubes. The conversion factor (CF) for the official tube J305ß is 0.008120 or CF=0.0058 ?


//***distance sensor FC-51
//byte distPin = 5;
//2Y0A21 length (distance) sensor
byte lnPin = 35;

// ***DS18B20 
// Номер пина Arduino с подключенным датчиком
#define PIN_DS18B20 5


// Application settings
