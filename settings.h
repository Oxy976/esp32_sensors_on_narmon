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
byte interruptPin = 12;

//DHT22
#define DHT_PIN D5  //##############!!Fix this!!
#define DHT_VERSION DHT22

//temp

//MH-Z19
#define MH_Z19_RX D7 //##############!!Fix this!!
#define MH_Z19_TX D6 //##############!!Fix this!!



// Application settings
