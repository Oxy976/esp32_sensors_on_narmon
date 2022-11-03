/*
 работа с датчиками - инициализация, получение данных
*/

#include "sensors.h"

static int numChSens; // number of activ sensor channels

// ***Geiger
// RadSens
CG_RadSens sRadSens(RS_DEFAULT_I2C_ADDRESS); // Constructor of the class ClimateGuard_RadSens1v2,

boolean bRAD = false; // b - датчик найден и инициализирован корректно
float vMSVh = 0.0;    // v значене с датчика
float vMRh = 0.0;

// ***BME280
#define BME_ext_ADDR 0x77
#define BME_int_ADDR 0x76

// Create the BME280 object
BME280_I2C sBME_e(BME_ext_ADDR); // I2C using address
BME280_I2C sBME_i(BME_int_ADDR); // I2C using address

boolean bBME_i = false;
boolean bBME_e = false;
float vBME_e_pres = 0.0;
float vBME_e_temp = 0.0;
float vBME_e_humi = 0.0;
float vBME_i_pres = 0.0;
float vBME_i_temp = 0.0;
float vBME_i_humi = 0.0;

// ***HTU21D/SHT21
// https://github.com/enjoyneering/HTU2xD_SHT2x_Si70xx
//Адрес на шине I2C для SHT21/HDC1080/HDC2080/HTU21D/Si7021 совпадает
/*
 sensor type:
  - HTU2xD_SENSOR, SHT2x_SENSOR  //interchangeable with each other, since they have same address & features
  - SI700x_SENSOR, SI702x_SENSOR //interchangeable with each other, since they have same address & features
  - SI701x_SENSOR                //not interchangeable with SI700x_SENSOR/SI702x_SENSOR, different address

 resolution:
  - HUMD_08BIT_TEMP_12BIT        //RH 8-bit  / T 12-bit
  - HUMD_10BIT_TEMP_13BIT        //RH 10-bit / T 13-bit
  - HUMD_11BIT_TEMP_11BIT        //RH 11-bit / T 11-bit
  - HUMD_12BIT_TEMP_14BIT        //RH 12-bit / T 14-bit
*/

HTU2xD_SHT2x_SI70xx sHTU_e(HTU2xD_SENSOR, HUMD_12BIT_TEMP_14BIT); // sensor type, resolution
// HTU2xD_SHT2x_SI70xx htu_ext(SHT2x_SENSOR, HUMD_12BIT_TEMP_14BIT); //sensor type, resolution

boolean bHTU_e = false;
float vHTU_e_temp = 0.0;
float vHTU_e_humi = 0.0;

//*** SHT31 ****
#define SHT31_ADDRESS 0x44
SHT31 sSHT_e;
boolean bSHT_e = false;
float vSHT_e_temp = 0.0;
float vSHT_e_humi = 0.0;

// **** Temp DS18B20
// Номер пина Arduino с подключенным датчиком
#define PIN_DS18B20 5

// Создаем объект OneWire
OneWire oneWire(PIN_DS18B20);
// Создаем объект DallasTemperature для работы с сенсорами, передавая ему ссылку на объект для работы с 1-Wire.
DallasTemperature sDS(&oneWire);
// Специальный объект для хранения адреса устройства
DeviceAddress sensorAddress;

boolean bDS = false;
float vDS = 0.0;
float vDS_fix = -1.0; // fix not correct data from sensor (°C)

// void SENSORS::startSens() // init sensors
void startSens() // init sensors
{
        static const char *TAG = "sensors_init";
        ESP_LOGD(TAG, "Start init sensors");

        // ESP_LOGD(TAG, "Wire begin");  //i2c init. а надо-ли, в м5 есть...
        // Wire.begin();

        // ***RadSens
        if (!sRadSens.init())
        {
                ESP_LOGD(TAG, "Could not find a RadSens!");
                // Serial.println("*------> Could not find a RadSens!  ***!! ");
                bRAD = false;
        }
        else
        {
                ESP_LOGI(TAG, "RadSens  sensor finded&activated");
                // Serial.println("+++> RadSens  sensor finded&activated");
                bRAD = true;
                uint8_t sensorChipId = sRadSens.getChipId(); // *Returns chip id, default value: 0x7D.
                ESP_LOGI(TAG, "Chip id:  %d", sensorChipId);
                // Serial.print("Chip id: 0x");
                // Serial.println(sensorChipId, HEX);

                uint8_t firmWareVer = sRadSens.getFirmwareVersion(); // *Returns firmware version.
                ESP_LOGI(TAG, "Firmware version:  %d", firmWareVer);
                // Serial.print("Firmware version: ");
                // Serial.println(firmWareVer);

                sRadSens.setSensitivity(105);
                sRadSens.setHVGeneratorState(true);
        }

        // ***BME
        if (!sBME_e.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid BME280 ext sensor");
                // Serial.println("*------> Could not find a valid BME280 ext sensor, check wiring! ***!! ");
                bBME_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "BME280 ext sensor finded&activated");
                // Serial.println("+++> BME280 ext sensor finded&activated");
                bBME_e = true;
                sBME_e.setTempCal(0); // correcting data, need calibrate this!!!   *************
        }

        if (!sBME_i.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid BME280 int sensor");
                // Serial.println("*------> Could not find a valid BME280 int sensor, check wiring!  ***!! ");
                bBME_i = false;
        }
        else
        {
                ESP_LOGI(TAG, "BME280 int sensor finded&activated");
                Serial.println("+++> BME280 int sensor finded&activated");
                bBME_i = true;
                sBME_i.setTempCal(0); // correcting data, need calibrate this!!!   *************
        }

        // ***HTU21D/SHT21
        if (!sHTU_e.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid HTU21D ext sensor");
                // Serial.println("*------> Could not find a valid HTU21D ext sensor, check wiring! ***!! ");
                bHTU_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "HTU21D ext sensor finded&activated");
                // Serial.println("+++> HTU21D ext sensor finded&activated");
                bHTU_e = true;
        }

        // ***SHT31
        if (!sSHT_e.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid SHT31 ext sensor");
                // Serial.println("*------> Could not find a valid SHT31 ext sensor, check wiring! ***!! ");
                bSHT_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "SHT31 ext sensor finded&activated");
                // Serial.println("+++> SHT31 ext sensor finded&activated");
                bSHT_e = true;
        }

        // ***dallas DS18B20
        sDS.begin();

        // Поиск устройства:
        // Ищем адрес устройства по порядку (индекс задается вторым параметром функции)
        if (!sDS.getAddress(sensorAddress, 0))
        {
                ESP_LOGD(TAG, "Could not find Dallas DS18B20 sensor");
                // Serial.println("*------> Could not find Dallas sensor ***");
                bDS = false;
        }
        else
        {
                ESP_LOGI(TAG, "Dallas DS18B20 sensor finded");
                // Serial.println("+++> Dallas sensor finded");
                bDS = true;

                // Устанавливаем разрешение датчика в 12 бит (max) (при уменьшении точности скорость получения данных увеличится)
                sDS.setResolution(sensorAddress, 12);

                // Serial.print("Разрешение датчика: ");
                // Serial.print(sDS.getResolution(sensorAddress), DEC);
                // Serial.println();
                ESP_LOGI(TAG, "Разрешение датчика DS18B20: %d", sDS.getResolution(sensorAddress));
        }
        // ----
}

//***********************************
// void SENSORS::getSensData() // read data from sensors
void getSensData(stSens *vSensVal) // read data from sensors
{
        // stSens st;
        static const char *TAG = "sensors_init";
        if (bRAD)
        {
                vMSVh = sRadSens.getRadIntensyStatic(); // vMRh ?
                ESP_LOGD(TAG, "Rad - %d", vMRh);

                // vSensVal[0].value = vMSVh;
                vSensVal[0].value = 20.0; //****TEST
                //контроль корректности данных. ДОПОЛНИТЬ!
                vSensVal[0].actual = true;
        }

        if (bBME_i)
        {
                sBME_i.readSensor(); // get data
                // delay(10);
                vTaskDelay(10);
                vBME_i_temp = sBME_i.getTemperature_C(); // read data
                //  Serial.printf("Temp BME280=%0.1f\n", bme_int_temp, " *C");
                vBME_i_humi = sBME_i.getHumidity(); // read data
                //    Serial.printf("Humidity BME280=%0.1f\n", bme_int_humi, "%");
                vBME_i_pres = (sBME_i.getPressure_MB() * 0.7500638); // read data
                // Serial.printf("Pressure BME280 =%0.1f\n", bme_int_pres, " mmHg");
                ESP_LOGD(TAG, "BME_int Temp=%d, Humi=%d, Pres=%d", vBME_i_temp, vBME_i_humi, vBME_i_pres);

                // vSensVal[1].value = vBME_i_temp;
                // vSensVal[2].value = vBME_i_humi;
                // vSensVal[3].value = vBME_i_pres;
                vSensVal[1].value = 21.1; //****TEST
                vSensVal[2].value = 22.2; //****TEST
                vSensVal[3].value = 23.3; //****TEST
                //контроль корректности данных. ДОПОЛНИТЬ!
                vSensVal[1].actual = true;
                vSensVal[2].actual = true;
                vSensVal[3].actual = true;
        }
        if (bBME_e)
        {
                sBME_e.readSensor(); // get data
                // delay(10);
                vTaskDelay(10);
                vBME_e_temp = sBME_e.getTemperature_C(); // read data
                //  Serial.printf("Temp BME280=%0.1f\n", bme_int_temp, " *C");
                vBME_e_humi = sBME_e.getHumidity(); // read data
                //    Serial.printf("Humidity BME280=%0.1f\n", bme_int_humi, "%");
                vBME_e_pres = (sBME_e.getPressure_MB() * 0.7500638); // read data
                // Serial.printf("Pressure BME280 =%0.1f\n", bme_int_pres, " mmHg");
                ESP_LOGD(TAG, "BME_ext Temp=%d, Humi=%d, Pres=%d", vBME_e_temp, vBME_e_humi, vBME_e_pres);

                // vSensVal[4].value = vBME_e_temp;
                // vSensVal[5].value = vBME_e_humi;
                // vSensVal[6].value = vBME_e_pres;
                vSensVal[4].value = 24.4; //****TEST
                vSensVal[5].value = 25.5; //****TEST
                vSensVal[6].value = 26.6; //****TEST
                //контроль корректности данных. ДОПОЛНИТЬ!
                vSensVal[4].actual = true;
                vSensVal[5].actual = true;
                vSensVal[6].actual = true;
        }

        if (bHTU_e)
        {
                vHTU_e_temp = sHTU_e.readTemperature(); // read data
                // Serial.printf("Temp HTU21D=%0.1f *C\n", htu_ext_temp);
                vHTU_e_humi = sHTU_e.readHumidity();
                // Serial.printf("Humidity HTU21D=%0.1f %%\n", htu_ext_humi);
                ESP_LOGD(TAG, "HTU_ext Temp=%d, Humi=%d", vHTU_e_temp, vHTU_e_humi);

                // vSensVal[7].value = vHTU_e_temp;
                // vSensVal[8].value = vHTU_e_humi;
                vSensVal[7].value = 27.7; //****TEST
                vSensVal[8].value = 28.8; //****TEST
                //контроль корректности данных. ДОПОЛНИТЬ!
                vSensVal[7].actual = true;
                vSensVal[8].actual = true;
        }

        if (bSHT_e)
        {
                vSHT_e_temp = sSHT_e.getTemperature(); // read data
                // Serial.printf("Temp HTU21D=%0.1f *C\n", htu_ext_temp);
                vSHT_e_humi = sSHT_e.getHumidity();
                // Serial.printf("Humidity HTU21D=%0.1f %%\n", htu_ext_humi);
                ESP_LOGD(TAG, "SHT_ext Temp=%d, Humi=%d", vSHT_e_temp, vSHT_e_humi);

                // vSensVal[9].value = vSHT_e_temp;
                // vSensVal[10].value = vSHT_e_humi;
                vSensVal[9].value = 27.7;  //****TEST
                vSensVal[10].value = 28.8; //****TEST
                //контроль корректности данных. ДОПОЛНИТЬ!
                vSensVal[9].actual = true;
                vSensVal[10].actual = true;
        }

        if (bDS)
        {
                sDS.requestTemperatures();         // get data
                vDS = sDS.getTempC(sensorAddress); // read data
                vDS += vDS_fix;                    // fix
                //  Serial.printf("Temp DS=%0.1f\n", ds_temp, " *C");
                ESP_LOGD(TAG, "DS  Temp=%d", vDS);

                // vSensVal[11].value = vDS;
                vSensVal[11].value = 29.9; //****TEST
                //контроль корректности данных. ДОПОЛНИТЬ!
                vSensVal[11].actual = true;
        }
        //****TEST ***
        vSensVal[0].value = 30.0;  //****TEST
        vSensVal[1].value = 31.0;  //****TEST
        vSensVal[2].value = 32.0;  //****TEST
        vSensVal[3].value = 33.0;  //****TEST
        vSensVal[4].value = 34.0;  //****TEST
        vSensVal[5].value = 35.0;  //****TEST
        vSensVal[6].value = 36.0;  //****TEST
        vSensVal[7].value = 37.0;  //****TEST
        vSensVal[8].value = 38.0;  //****TEST
        vSensVal[9].value = 39.0;  //****TEST
        vSensVal[10].value = 30.1; //****TEST
        vSensVal[11].value = 31.1; //****TEST

        vSensVal[0].actual = true;
        vSensVal[1].actual = true;
        vSensVal[2].actual = true;
        vSensVal[3].actual = true;
        vSensVal[4].actual = true;
        vSensVal[5].actual = true;
        vSensVal[6].actual = true;
        vSensVal[7].actual = true;
        vSensVal[8].actual = true;
        vSensVal[9].actual = true;
        vSensVal[10].actual = true;
        vSensVal[11].actual = true;
        //*******************
}
