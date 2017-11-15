// Network settings
// --
const char* ssid = "xxx";
const char* password = "xxx";
// --

// Time settings
const char* ntpServerName = "pool.ntp.org";
const int TIMEZONE=3;

// narmon

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

//Geiger
/* pin that is attached to interrupt 12 = ESP32 GIO12 */
byte interruptPin = 25; //#GPIO25

//DHT22
#define DHT_PIN 26  //#GPIO26
#define DHT_VERSION DHT22

//temp
// *** GPIO pin number for DS18B20
const int DS_PIN = 27; //#GPIO27

//MH-Z19
#define MH_Z19_RX 16 //#GPIO16
#define MH_Z19_TX 17 //#GPIO17



// Application settings
