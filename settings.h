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

// ==sensors 
// LED
const int LED_BUILTIN=2;  //Onboard LED Pin

//BME280
const int BME_ADDR=0x77; //I2C using  0x77 (default) or 0x76

//Geiger
/* pin that is attached to interrupt 12 = ESP32 GIO12 */
byte interruptPin = 12; 


// Application settings

