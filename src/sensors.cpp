/*
 работа с датчиками - инициализация, получение данных
*/

#include "sensors.h"

// static int numChSens; // number of activ sensor channels

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
// float vDS = 0.0;
float vDS_fix = -0.8; // fix  data from sensor (°C)

// ***Geiger
// RadSens
// CG_RadSens sRadSens(RS_DEFAULT_I2C_ADDRESS); // Constructor of the class ClimateGuard_RadSens1v2,
ClimateGuard_RadSens1v2 sRadSens(RS_DEFAULT_I2C_ADDRESS);

boolean bRAD = false; // b - датчик найден и инициализирован корректно
int vNumPulse = 0;
// float vRadD = 0.0;
// float vRadS = 0.0; // v значене с датчика

// ***BME280
#define BME_int_ADDR 0x76
#define BME_ext_ADDR 0x77

// Create the BME280 object
BME280_I2C sBME_i(BME_int_ADDR); // I2C using address
BME280_I2C sBME_e(BME_ext_ADDR); // I2C using address

boolean bBME_i = false;
boolean bBME_e = false;
// float vBME_i_pres = 0.0;
// float vBME_i_temp = 0.0;
// float vBME_i_humi = 0.0;
// float vBME_e_pres = 0.0;
// float vBME_e_temp = 0.0;
// float vBME_e_humi = 0.0;

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
// float vHTU_e_temp = 0.0;
// float vHTU_e_humi = 0.0;

//*** SHT31 ****
#define SHT31_ADDRESS 0x44
SHT31 sSHT_e;
boolean bSHT_e = false;
// float vSHT_e_temp = 0.0;
// float vSHT_e_humi = 0.0;

#define HEATTIME 2000 //сколько держать прогрев

//*** SCD30
SCD30 sSCD30_i;
boolean bSCD30_i = false;
// float vSCD30_i_temp = 0.0;
// float vSCD30_i_humi = 0.0;
// float vSCD30_i_co2 = 0.0;

// String GRAD = strcat("\x00B0","C");
String sC = "C";
String GRAD = "\x00B0" + sC;
//-----------------

// void SENSORS::startSens() // init sensors
void startSens(stSens *vSensVal) // init sensors
{
        static const char *TAG = "sensors_init";
        ESP_LOGD(TAG, "Start init sensors");

        // ESP_LOGD(TAG, "I2C begin"); // i2c init. а надо-ли?, в м5 есть...
        // Wire.begin();

        // -- наименование датчика (для сервера http)
        vSensVal[0].name = "extDS_Temp";
        vSensVal[1].name = "Rad_dyn";
        vSensVal[2].name = "Rad_stat";
        vSensVal[3].name = "Rad_pulses";
        vSensVal[4].name = "intBME_Temp";
        vSensVal[5].name = "intBME_Hum";
        vSensVal[6].name = "intBME_Press";
        vSensVal[7].name = "extBME_Temp";
        vSensVal[8].name = "extBME_Hum";
        vSensVal[9].name = "extBME_Press";
        vSensVal[10].name = "extHTU_Temp";
        vSensVal[11].name = "extHTU_Hum";
        vSensVal[12].name = "extSHT_Temp";
        vSensVal[13].name = "extSHT_Hum";
        vSensVal[14].name = "intSCD30_Temp";
        vSensVal[15].name = "intSCD30__Hum";
        vSensVal[16].name = "intSCD30_CO2";

        //-------- ID для публикации на narodmon(Какие заданы - будут отправлены, умолчательные значения "" пропущены)
        vSensVal[0].mqttId = "T0";
        vSensVal[2].mqttId = "R0";
        vSensVal[4].mqttId = "T2";
        vSensVal[5].mqttId = "H2";
        vSensVal[6].mqttId = "P2";
        // vSensVal[9].mqttId = "T1";
        // vSensVal[10].mqttId = "H1";
        vSensVal[12].mqttId = "T1";
        vSensVal[13].mqttId = "H1";

        // ***dallas DS18B20
        sDS.begin();

        // Поиск устройства:
        // Ищем адрес устройства по порядку (индекс задается вторым параметром функции)
        if (!sDS.getAddress(sensorAddress, 0))
        {
                ESP_LOGD(TAG, "Could not find Dallas DS18B20 sensor---");
                bDS = false;
        }
        else
        {
                ESP_LOGI(TAG, "Dallas DS18B20 sensor finded on address 0x%X  ***", sensorAddress);
                bDS = true;

                // Устанавливаем разрешение датчика в 12 бит (max) (при уменьшении точности скорость получения данных увеличится)
                sDS.setResolution(sensorAddress, 12);
                ESP_LOGI(TAG, "Разрешение датчика DS18B20: %d", sDS.getResolution(sensorAddress));
                vSensVal[0].unit = GRAD;
        }

        if (!sRadSens.radSens_init())
        // if (!sRadSens.init())
        {
                ESP_LOGD(TAG, "Could not find a RadSens!---");
                bRAD = false;
        }
        else
        {
                vTaskDelay(100);
                ESP_LOGI(TAG, "RadSens  sensor finded ***");
                bRAD = true;
                ESP_LOGI(TAG, "Chip id:  %X", sRadSens.getChipId());
                ESP_LOGI(TAG, "Firmware version:  %d", sRadSens.getFirmwareVersion());
                vTaskDelay(10);
                ESP_LOGI(TAG, "sensitivity get:  %d", sRadSens.getSensitivity());
                vTaskDelay(10);
                ESP_LOGI(TAG, "HV generator state get:  %d", sRadSens.getHVGeneratorState());
                vTaskDelay(10);
                // sRadSens.setSensitivity(55);
                vTaskDelay(50);
                // ESP_LOGI(TAG, "sensitivity:  %d", sRadSens.getSensitivity());
                ESP_LOGD(TAG, "sensitivity set to 105");
                sRadSens.setSensitivity(105);
                vTaskDelay(50);
                sRadSens.setHVGeneratorState(true);
                vTaskDelay(50);
                ESP_LOGI(TAG, "sensitivity get:  %d", sRadSens.getSensitivity());
                ESP_LOGI(TAG, "HV generator state:  %d", sRadSens.getHVGeneratorState());

                vSensVal[1].unit = "mRg/h";
                vSensVal[2].unit = "mRg/h";
                vSensVal[3].unit = "#";
        }

        // ***BME
        if (!sBME_i.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid BME280 int sensor---");
                bBME_i = false;
        }
        else
        {
                ESP_LOGI(TAG, "BME280 int sensor finded&activated***");
                bBME_i = true;
                sBME_i.setTempCal(0); // correcting data, need calibrate this!!!   *************

                vSensVal[4].unit = GRAD;
                vSensVal[5].unit = "%";
                vSensVal[6].unit = "mmHg";
        }

        if (!sBME_e.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid BME280 ext sensor---");
                bBME_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "BME280 ext sensor finded&activated***");
                bBME_e = true;
                sBME_e.setTempCal(0); // correcting data, need calibrate this!!!   *************

                vSensVal[7].unit = GRAD;
                vSensVal[8].unit = "%";
                vSensVal[9].unit = "mmHg";
        }

        // ***HTU21D/SHT21
        if (!sHTU_e.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid HTU21D ext sensor---");
                bHTU_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "HTU21D ext sensor finded&activated***");
                ESP_LOGI(TAG, "HTU21 Dev_ID %d,firmware %d", sHTU_e.readDeviceID(), sHTU_e.readFirmwareVersion());
                bHTU_e = true;
                vSensVal[10].unit = GRAD;
                vSensVal[11].unit = "%";
        }

        // ***SHT31
        if (!sSHT_e.begin(SHT31_ADDRESS))
        {
                ESP_LOGD(TAG, "Could not find a valid SHT31 ext sensor---");
                // Serial.println("*------> Could not find a valid SHT31 ext sensor, check wiring! ***!! ");
                bSHT_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "SHT31 ext sensor finded&activated***");
                bSHT_e = true;
                ESP_LOGI(TAG, "SHT31 status %X (Def 0x8010)", sSHT_e.readStatus());
                // bit - description
                // ==================
                // 15 Alert pending status
                //    '0': no pending alerts
                //    '1': at least one pending alert - default
                // 14 Reserved ‘0’
                // 13 Heater status
                //    '0’ : Heater OFF - default
                //    '1’ : Heater ON
                // 12 Reserved '0’
                // 11 Humidity tracking alert
                //    '0’ : no alert - default
                //    '1’ : alert
                // 10 Temp tracking alert
                //    '0’ : no alert - default
                //    '1’ : alert
                // 9:5 Reserved '00000’
                // 4 System reset detected
                //    '0': no reset since last ‘clear status register’ command
                //    '1': reset detected (hard or soft reset command or supply fail) - default
                // 3:2 Reserved ‘00’
                // 1 Command status
                //    '0': last cmd executed successfully
                //    '1': last cmd not processed. Invalid or failed checksum
                // 0 Write data checksum status
                //    '0': checksum of last write correct
                //    '1': checksum of last write transfer failed
                if (sSHT_e.isHeaterOn())
                {
                        sSHT_e.heatOff();
                }
                vSensVal[12].unit = GRAD;
                vSensVal[13].unit = "%";
        }

        // ***SCD30
        if (!sSCD30_i.begin())
        {
                ESP_LOGD(TAG, "Could not find a valid SCD30 sensor---");
                bHTU_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "SCD30 ext sensor finded&activated***");
                uint16_t settingVal;
                sSCD30_i.getFirmwareVersion(&settingVal);
                ESP_LOGI(TAG, "SCD30 firmware %d", settingVal);
                sSCD30_i.getTemperatureOffset(&settingVal);
                ESP_LOGI(TAG, "SCD30 Temperature offset (C) is %d", (((float)settingVal) / 100.0));
                sSCD30_i.getAltitudeCompensation(&settingVal);
                ESP_LOGI(TAG, "SCD30 Altitude offset (m) is %d", settingVal);
                if (sSCD30_i.getAutoSelfCalibration() == true)
                        ESP_LOGI(TAG, "SCD30 auto calibration is enable");
                else
                        ESP_LOGI(TAG, "SCD30 auto calibration is disable");
                bSCD30_i = true;
                vSensVal[14].unit = GRAD;
                vSensVal[15].unit = "%";
                vSensVal[16].unit = "ppm";
        }
}

//***********************************
// void SENSORS::getSensData() // read data from sensors
void getSensData(stSens *vSensVal) // read data from sensors
{
        resetActualSensVal(vSensVal);

        static const char *TAG = "sensors_values";

        if (bDS)
        {
                sDS.requestTemperatures(); // get data
                vTaskDelay(10);
                vSensVal[0].value = sDS.getTempC(sensorAddress); // read data
                vTaskDelay(10);
                vSensVal[0].value += vDS_fix; // fix
                ESP_LOGD(TAG, "DS  Temp=%f", vSensVal[0].value);

                //контроль корректности данных.
                if (vSensVal[0].value > -50 and vSensVal[0].value < 50)
                {
                        vSensVal[0].actual = true;
                }
                vTaskDelay(10);
        }

        if (bRAD)
        {
                if (sRadSens.getData())
                {
                        vTaskDelay(20);
                        vSensVal[1].value = sRadSens.getRadIntensyDyanmic();
                        // ESP_LOGD(TAG, "Rad Dyanmic: %f mRh", vRadD);
                        vSensVal[2].value = sRadSens.getRadIntensyStatic();
                        // ESP_LOGD(TAG, "Rad Static: %f mRh", vRadS);
                        vSensVal[3].value = sRadSens.getNumberOfPulses();
                        // ESP_LOGD(TAG, "Rad Pulses: %d ", vNumPulse);

                        ESP_LOGD(TAG, "Rad pulses: %d, dyanmic: %f mRh, static: %f mRh ", vNumPulse, vSensVal[1].value, vSensVal[2].value);

                        // vSensVal[1].value = vRadD;
                        // vSensVal[2].value = vRadS;
                        // vSensVal[3].value = vNumPulse;
                        //контроль корректности данных.
                        if (vNumPulse > 200)
                        {
                                vSensVal[1].actual = true;
                                vSensVal[2].actual = true;
                                vSensVal[3].actual = true;
                        }
                        vTaskDelay(50);
                }
        }

        if (bBME_i)
        {
                sBME_i.readSensor();                                 // get data
                vTaskDelay(20);                                      // delay(10);
                vSensVal[4].value = sBME_i.getTemperature_C();             // read data
                vSensVal[5].value = sBME_i.getHumidity();                  // read data
                vSensVal[6].value = (sBME_i.getPressure_MB() * 0.7500638); // read data
                ESP_LOGD(TAG, "BME_int Temp=%f, Humi=%f, Pres=%f", vSensVal[4].value,  vSensVal[5].value, vSensVal[6].value);

                //vSensVal[4].value = vBME_i_temp;
                //vSensVal[5].value = vBME_i_humi;
                //vSensVal[6].value = vBME_i_pres;
                //контроль корректности данных.
                if (vSensVal[4].value > -50 and vSensVal[4].value < 50)
                {
                        vSensVal[4].actual = true;
                }
                if (vSensVal[5].value > 5 and vSensVal[5].value < 101)
                {
                        vSensVal[5].actual = true;
                }
                if (vSensVal[6].value > 600 and vSensVal[6].value < 811)
                {
                        vSensVal[6].actual = true;
                }
        }
        if (bBME_e)
        {
                sBME_e.readSensor(); // get data

                vTaskDelay(20);                                      // delay(10);
                vSensVal[7].value = sBME_e.getTemperature_C();             // read data
                vSensVal[8].value = sBME_e.getHumidity();                  // read data
                vSensVal[9].value = (sBME_e.getPressure_MB() * 0.7500638); // read data
                ESP_LOGD(TAG, "BME_ext Temp=%f, Humi=%f, Pres=%f", vSensVal[7].value, vSensVal[8].value, vSensVal[9].value);

                //vSensVal[7].value = vBME_e_temp;
               // vSensVal[8].value = vBME_e_humi;
                //vSensVal[9].value = vBME_e_pres;
                //контроль корректности данных.

                if (vSensVal[7].value > -50 and vSensVal[7].value < 50)
                {
                        vSensVal[7].actual = true;
                }
                if (vSensVal[8].value > 5 and vSensVal[8].value < 101)
                {
                        vSensVal[8].actual = true;
                }
                if (vSensVal[9].value > 600 and vSensVal[9].value < 811)
                {
                        vSensVal[9].actual = true;
                }
        }

        if (bHTU_e)
        {
                vSensVal[10].value = sHTU_e.readTemperature(); // read data
                vSensVal[11].value = sHTU_e.readHumidity();
                ESP_LOGD(TAG, "HTU_ext Temp=%f, Humi=%f", vSensVal[10].value, vSensVal[11].value);

                //vSensVal[10].value = vHTU_e_temp;
                //vSensVal[11].value = vHTU_e_humi;
                //контроль корректности данных.
                if (vSensVal[10].value > -50 and vSensVal[10].value < 50)
                {
                        vSensVal[10].actual = true;
                }
                if (vSensVal[11].value > 5 and vSensVal[11].value < 101)
                {
                        vSensVal[11].actual = true;
                }
        }

        if (bSHT_e)
        {
                if (sSHT_e.read())
                {

                        vTaskDelay(50);
                        vSensVal[12].value = sSHT_e.getTemperature(); // read data
                        vSensVal[13].value = sSHT_e.getHumidity();
                        ESP_LOGD(TAG, "SHT_ext Temp=%f, Humi=%f", vSensVal[12].value, vSensVal[13].value);
                        //vSensVal[12].value = vSHT_e_temp;
                        //vSensVal[13].value = vSHT_e_humi;
                        //контроль корректности данных.
                        if ((vSensVal[12].value > -50 and vSensVal[12].value < 50) || (!sSHT_e.isHeaterOn()))
                        {
                                vSensVal[12].actual = true;
                        }
                        if (vSensVal[13].value > 5 and vSensVal[13].value < 101)
                        {
                                vSensVal[13].actual = true;
                        }
                        if (vSensVal[13].value >= 100)
                        {
                                sSHT_e.reset();
                        }
                }
        }

        if (bSCD30_i)
        {
                if (sSCD30_i.dataAvailable())
                {
                        vSensVal[14].value = sSCD30_i.getTemperature();
                        vSensVal[15].value = sSCD30_i.getHumidity();
                        vSensVal[16].value = sSCD30_i.getCO2();
                        ESP_LOGD(TAG, "HTU_ext Temp=%f, Humi=%f, CO2=%f", vSensVal[14].value, vSensVal[15].value, vSensVal[16].value);

                        //контроль корректности данных.
                        if (vSensVal[14].value > -50 and vSensVal[14].value < 50)
                        {
                                vSensVal[14].actual = true;
                        }
                        if (vSensVal[15].value > 5 and vSensVal[15].value < 101)
                        {
                                vSensVal[15].actual = true;
                        }
                        vSensVal[16].actual = true;
                }
        }
}

extern void heatSens() //прогрев датчиков для правильной влажности. (Может стоит проверить на температуру-влажность?)
{
        static const char *TAG = "heat";
        //#HTU21
        if (bHTU_e)
        {
                sHTU_e.setHeater(true);
                ESP_LOGD(TAG, "Heating HTU is ON");
        }

        //#SHT31
        if (bSHT_e)
        {
                sSHT_e.heatOn();
                ESP_LOGD(TAG, "Heating SHT is ON");
        }

        vTaskDelay(HEATTIME);

        if (bHTU_e)
        {
                sHTU_e.setHeater(false);
                ESP_LOGD(TAG, "Heating HTU is OFF");
        }
        if (bSHT_e)
        {
                sSHT_e.heatOff();
                ESP_LOGD(TAG, "Heating SHT is OFF");
        }
}

void resetActualSensVal(stSens *vSensVal)
{
        for (int i = 0; i < SensUnit; i++)
        {
                vSensVal[i].actual = false;
        }
}