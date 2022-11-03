// ==Network settings
// --
const char* hostname = "esp";
const char* ssid = "xxx";
const char* password = "xxx";
// --

//M5
// LED
const int LED_BUILTIN = 15; //Onboard LED Pin (M5 fire)


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

