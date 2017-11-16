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
// what digital pin we're connected to
#define DHTPIN 26     //#GPIO26
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

//temp
// *** GPIO pin number for DS18B20
const int DS_PIN = 27; //#GPIO27

//MH-Z19
#define MH_Z19_RX 16 //#GPIO16
#define MH_Z19_TX 17 //#GPIO17



// Application settings
