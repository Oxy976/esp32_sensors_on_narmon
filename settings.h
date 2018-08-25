// ==Network settings
// --
const char* ssid = "xxx";
const char* password = "xxx";
// --

// Time settings
const char* ntpServerName = "pool.ntp.org";
const int TIMEZONE=3;

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

// LED
const int LED_BUILTIN=2;  //Onboard LED Pin

//***BME280
const int BME_ADDR=0x77; //I2C using  0x77 (default) or 0x76

//***Geiger
/* pin that is attached to interrupt 12 = ESP32 GIO12 */
byte interruptPin = 12; 
const int LOG_PERIOD=600000;  //Logging period in milliseconds, 600000=10m 900000=15m
//const int LOG_PERIOD=60000;  //Logging period in milliseconds, 600000=10m
  //J305?
const float CF=0.0058; //different for different tubes. The conversion factor (CF) for the official tube J305? is 0.008120


//***DHT
#define DHTPIN 25  //#GPIO25

// Uncomment the type of sensor in use:
//#define DHTTYPE           DHT11     // DHT 11 
#define DHTTYPE           DHT22     // DHT 22 (AM2302)
//#define DHTTYPE           DHT21     // DHT 21 (AM2301)


// Application settings

