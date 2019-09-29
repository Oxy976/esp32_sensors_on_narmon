// ==Network settings
// --
const char* hostname = "esp";
const char* ssid = "xxx";
const char* password = "xxx";
// --

//M5
// LED
const int LED_BUILTIN = 15; //Onboard LED Pin (M5 fire)

//wemos esp32
// LED
const int LED_BUILTIN = 1; //Onboard LED Pin ***! change this for M5!

// Time settings
const char* ntpServerName = "pool.ntp.org";
const int TIMEZONE=3;
const int   daylightOffset_sec = 0;

//==narmon

#define SRV   "narodmon.ru"
#define MAC  "xxx"
#define PASS  "xxx"
#define USERNAME  "xxx"
#define TOPIC  "login/xxx"

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
const int LOG_PERIOD=6000000;  //Logging period //100000 - 10s, 600000 - 60s,1m  6000000 - 10m  36000000 -60m,1h
//J305 tube
const float CF = 0.0051; //different for different tubes. The conversion factor (CF) for the official tube J305? is 0.008120 or CF=0.0058 ?


//***distance sensor FC-51
//byte distPin = 5;
//2Y0A21 length (distance) sensor
byte lnPin = 35;

// ***DS18B20 
#define PIN_DS18B20 5


// Application settings
