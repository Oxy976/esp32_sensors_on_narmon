// ==Network settings
// --
const char* hostname = "esp";
const char* ssid = "xxx";
const char* password = "xxx";
// --

//M5

//wemos esp32
// LED
const int LED_BUILTIN = 2; //Onboard LED Pin
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


//***BME280
const int BME_ADDR=0x77; //I2C using  0x77 (default) or 0x76

//***Geiger
/* pin that is attached to interrupt 12 = ESP32 GIO12 */
byte interruptPin = 12; 
const int LOG_PERIOD=6000000;  //Logging period //100000 - 10s, 600000 - 60s,1m  6000000 - 10m  36000000 -60m,1h

//const int LOG_PERIOD=60000;  // 4test
//J305?
const float CF = 0.0052; //different for different tubes. The conversion factor (CF) for the official tube J305? is 0.008120 or CF=0.0058 ?



//***DHT
#define DHTPIN 25  //#GPIO25

// Uncomment the type of sensor in use:
//#define DHTTYPE           DHT11     // DHT 11 
#define DHTTYPE           DHT22     // DHT 22 (AM2302)
//#define DHTTYPE           DHT21     // DHT 21 (AM2301)

//distance 
//sensor FC-51
//byte distPin = 5;
//2Y0A21 length (distance) sensor
byte lnPin = 35;


// Application settings

