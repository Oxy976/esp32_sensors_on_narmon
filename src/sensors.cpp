/*
 работа с датчиками - инициализация, получение данных

 ### vSensVal[i].unit имеет значение только при инициализации соответствующего датчика. Или переписывать... ###

*/

#include "sensors.h"

extern void logToWeb(String text);

// **** Temp DS18B20
// Номер пина Arduino с подключенным датчиком| рекомендовано - 16,17,26
#define PIN_DS18B20 26

// Создаем объект OneWire
OneWire oneWire(PIN_DS18B20);
// Создаем объект DallasTemperature для работы с сенсорами, передавая ему ссылку на объект для работы с 1-Wire.
DallasTemperature sDS(&oneWire);
// Специальный объект для хранения адреса устройства
DeviceAddress sensorAddress;

boolean bDS = false;

// ***Geiger
// RadSens
// CG_RadSens sRadSens(RS_DEFAULT_I2C_ADDRESS); // Constructor of the class ClimateGuard_RadSens1v2,
ClimateGuard_RadSens1v2 sRadSens(RS_DEFAULT_I2C_ADDRESS);
boolean bRAD = false; // b - датчик найден и инициализирован корректно
int vNumPulse = 0;

unsigned long RADstartTime = millis();

// ***BME280
// Шаблоны настроек для BME280
BME280I2C::Settings settings_i(
    BME280::OSR_X1, BME280::OSR_X1, BME280::OSR_X1,
    BME280::Mode_Forced, BME280::StandbyTime_1000ms,
    BME280::Filter_Off, BME280::SpiEnable_False, BME280I2C::I2CAddr_0x76);

BME280I2C::Settings settings_e(
    BME280::OSR_X1, BME280::OSR_X1, BME280::OSR_X1,
    BME280::Mode_Forced, BME280::StandbyTime_1000ms,
    BME280::Filter_Off, BME280::SpiEnable_False, BME280I2C::I2CAddr_0x77);

BME280I2C sBME_i(settings_i);
BME280I2C sBME_e(settings_e);

boolean bBME_i = false;
boolean bBME_e = false;

// ***HTU21D/SHT21/SI7021
// Адрес на шине I2C для SHT21/HDC1080/HDC2080/HTU21D/Si7021 совпадает
Si7021 sHTU_e(&Wire);

boolean bHTU_e = false;
float vHTU_e = 0.0;

//*** SHT31 ****
#define SHT31_ADDRESS 0x44 // used in driver!
// SHT31 sSHT_e;
SHT31 sSHT_e(SHT31_ADDRESS);
boolean bSHT_e = false;

#define HEATTIME 7000 // сколько держать прогрев

//*** SCD30  (углекислый газ)
SCD30 sSCD30_i;
boolean bSCD30_i = false;

String sC = "C";
String GRAD = "\u00B0" + sC;

// ###################
// переменные коррекции
float fDS_Tfix = -0.8;   // fix  data from sensor (°C)
float fBME_e_Tfix = 0.0; // fix  data from sensor (°C)
float fHTU_e_Tfix = -1.5; // fix  data from sensor (°C)
float fSHT_e_Tfix = -1.8; // fix  data from sensor (°C)
// ####################

// void SENSORS::startSens() // init sensors
void startSens(stSens *vSensVal) // init sensors
{
        static const char *TAG = "sensors_init";
        ESP_LOGD(TAG, "Start init sensors");

        // -- наименование датчика (для сервера http)
        vSensVal[0].name = "extDS_Temp";
        vSensVal[1].name = "Rad_dyn";
        vSensVal[2].name = "Rad_stat";
        vSensVal[3].name = "Rad_pulses";
        vSensVal[4].name = "extBME_Temp";
        vSensVal[5].name = "extBME_Hum";
        vSensVal[6].name = "extBME_Press";
        vSensVal[7].name = "extHTU_Temp";
        vSensVal[8].name = "extHTU_Hum";
        vSensVal[9].name = "extSHT_Temp";
        vSensVal[10].name = "extSHT_Hum";
        vSensVal[11].name = "intBME_Temp";
        vSensVal[12].name = "intBME_Hum";
        vSensVal[13].name = "intBME_Press";
        vSensVal[14].name = "intSCD30_Temp";
        vSensVal[15].name = "intSCD30_Hum";
        vSensVal[16].name = "intSCD30_CO2";

        //-------- ID для публикации на narodmon(Какие заданы - будут отправлены, умолчательные значения "" пропущены)
        vSensVal[0].mqttId = "T0";
        vSensVal[2].mqttId = "R0";
        // vSensVal[7].mqttId = "T1";
        // vSensVal[8].mqttId = "H1";
        vSensVal[9].mqttId = "T1";
        vSensVal[10].mqttId = "H1";
        vSensVal[11].mqttId = "T2";
        vSensVal[12].mqttId = "H2";
        vSensVal[13].mqttId = "P2";
        // vSensVal[14].mqttId = "T3";
        // vSensVal[15].mqttId = "H3";
        vSensVal[16].mqttId = "CO2";

        // ***dallas DS18B20
        sDS.begin();

        // Поиск устройства:
        //  Быстро проверяем, есть ли физически хоть один прибор на шине OneWire.
        // Если устройств 0 — мы мгновенно выходим, не дожидаясь 5-секундного тайм-аута.
        if (sDS.getDeviceCount() == 0)
        {
                ESP_LOGW(TAG, "--- Dallas DS18B20 bus is empty (sensor disconnected)");
                logToWeb("--- Dallas DS18B20 disconnected");
                bDS = false;
        }
        // Ищем адрес устройства по порядку (индекс задается вторым параметром функции)
        else if (!sDS.getAddress(sensorAddress, 0))
        {
                ESP_LOGD(TAG, "--- Dallas DS18B20 sensor not found");
                logToWeb("--- Dallas DS18B20 sensor not found");
                bDS = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ Dallas DS18B20 sensor finded on address 0x%X  ", sensorAddress);
                logToWeb("+++ Dallas DS18B20 sensor finded");
                bDS = true;

                // Устанавливаем разрешение датчика в 12 бит (max) (при уменьшении точности скорость получения данных увеличится)
                sDS.setResolution(sensorAddress, 12);
                ESP_LOGI(TAG, "Разрешение датчика DS18B20: %d", sDS.getResolution(sensorAddress));
                vSensVal[0].unit = GRAD;
        }

        if (!sRadSens.radSens_init())
        // if (!sRadSens.init())
        {
                ESP_LOGD(TAG, "--- RadSens not found ");
                logToWeb("--- RadSens not found");
                bRAD = false;
        }
        else
        {

                vTaskDelay(pdMS_TO_TICKS(100));
                ESP_LOGI(TAG, "+++ RadSens  sensor finded  ");
                logToWeb("+++ RadSens  sensor finded");
                bRAD = true;
                ESP_LOGI(TAG, "Chip id:  %X", sRadSens.getChipId());
                logToWeb(" RadSens Chip id:" + String(sRadSens.getChipId()));
                ESP_LOGI(TAG, "Firmware version:  %d", sRadSens.getFirmwareVersion());
                logToWeb(" RadSensFirmware version: " + String(sRadSens.getFirmwareVersion()));
                vTaskDelay(pdMS_TO_TICKS(50));
                ESP_LOGI(TAG, "sensitivity get:  %d", sRadSens.getSensitivity());
                ESP_LOGI(TAG, "HV generator state:  %d", sRadSens.getHVGeneratorState());

                RADstartTime = millis();

                vSensVal[1].unit = "mRg/h";
                vSensVal[2].unit = "mRg/h";
                vSensVal[3].unit = "#";
        }

        if (!sBME_e.begin())
        {
                ESP_LOGD(TAG, "--- BME280 ext sensor not found");
                logToWeb("--- BME280 ext sensor not found");
                bBME_e = false;
        }
        else
        {
                switch (sBME_e.chipModel())
                {
                case BME280::ChipModel_BME280:
                        logToWeb("+++ BME280 ext sensor finded");
                        break;
                case BME280::ChipModel_BMP280:
                        logToWeb("+++ BMp280 ext sensor finded");
                        break;
                default:
                        Serial.println("[BME280]Found UNKNOWN sensor! Error!");
                }
                ESP_LOGI(TAG, "+++ BME(P)280 ext sensor finded***");
                //  logToWeb("+++ BME280 ext sensor finded&activated");
                bBME_e = true;
                // sBME_e.setTempCal(0); // correcting data, need calibrate this!!!   ************* old

                vSensVal[4].unit = GRAD;
                vSensVal[5].unit = "%";
                vSensVal[6].unit = "mmHg";
        }

        // ***HTU21D/SHT21/Si7021
        if (!sHTU_e.begin())
        {
                ESP_LOGD(TAG, "--- HTU21D/Si7021 ext sensor not found");
                logToWeb("--- HTU21D/Si7021 ext sensor not found");
                bHTU_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ HTU21D/Si7021 ext sensor finded&activated***");
                // ESP_LOGI(TAG, "+++ HTU21/Si7021 Dev_ID %d,firmware %d", sHTU_e.readDeviceID(), sHTU_e.readFirmwareVersion());
                logToWeb("+++ HTU21D/Si7021 ext sensor finded&activated");
                bHTU_e = true;
                vSensVal[7].unit = GRAD;
                vSensVal[8].unit = "%";
        }

        // ***SHT31
        if (!sSHT_e.begin())
        {
                ESP_LOGD(TAG, "--- SHT31 ext sensor not found");
                logToWeb("--- SHT31 ext sensor not found");
                bSHT_e = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ SHT31 ext sensor finded&activated***");
                logToWeb("+++ SHT31 ext sensor finded&activated");
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
                vSensVal[9].unit = GRAD;
                vSensVal[10].unit = "%";
        }

        // ***BME
        if (!sBME_i.begin())
        {
                ESP_LOGD(TAG, "--- BME280 int sensor not found");
                logToWeb("--- BME280 int sensor not found");
                bBME_i = false;
        }
        else
        {
                switch (sBME_i.chipModel())
                {
                case BME280::ChipModel_BME280:
                        logToWeb("+++ BME280 int sensor finded");
                        break;
                case BME280::ChipModel_BMP280:
                        logToWeb("+++ BMp280 int sensor finded");
                        break;
                default:
                        Serial.println("[BME280]Found UNKNOWN sensor! Error!");
                }
                ESP_LOGI(TAG, "+++ BME(P)280 int sensor finded***");
                bBME_i = true;
                // sBME_i.setTempCal(0); // correcting data, need calibrate this!!!   *************

                vSensVal[11].unit = GRAD;
                vSensVal[12].unit = "%";
                vSensVal[13].unit = "mmHg";
        }

        // ***SCD30
        if (!sSCD30_i.begin(Wire, false))
        {
                ESP_LOGD(TAG, "--- SCD30 ext sensor not found");
                logToWeb("--- SCD30 ext sensor not found");
                bSCD30_i = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ SCD30 ext sensor finded&activated***");
                logToWeb("+++ SCD30 ext sensor finded&activated");
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
        logToWeb("End initing sensors");
}

//***********************************
void getSensData(stSens *vSensVal) // read data from sensors
{
        resetActualSensVal(vSensVal);

        static const char *TAG = "sensors_values";

        if (bDS)
        {
                sDS.requestTemperatures(); // get data
                vTaskDelay(pdMS_TO_TICKS(10));
                vSensVal[0].value = sDS.getTempC(sensorAddress); // read data
                vTaskDelay(pdMS_TO_TICKS(10));
                vSensVal[0].value += fDS_Tfix; // fix
                ESP_LOGD(TAG, "DS  Temp=%f", vSensVal[0].value);

                // контроль корректности данных.
                if (vSensVal[0].value > -50 and vSensVal[0].value < 50)
                {
                        vSensVal[0].actual = true;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (bRAD)
        {
                if (sRadSens.getData())
                {
                        vTaskDelay(pdMS_TO_TICKS(20));
                        vSensVal[1].value = sRadSens.getRadIntensyDyanmic();
                        // ESP_LOGD(TAG, "Rad Dyanmic: %f mRh", vRadD);
                        vSensVal[2].value = sRadSens.getRadIntensyStatic();
                        // ESP_LOGD(TAG, "Rad Static: %f mRh", vRadS);
                        vSensVal[3].value = sRadSens.getNumberOfPulses();
                        // ESP_LOGD(TAG, "Rad Pulses: %d ", vNumPulse);

                        ESP_LOGD(TAG, "Rad pulses: %d, dyanmic: %f mRh, static: %f mRh ", vSensVal[3].value, vSensVal[1].value, vSensVal[2].value);

                        // контроль корректности данных.
                        if (vSensVal[3].value > 200)
                        {
                                vSensVal[1].actual = true;
                                vSensVal[2].actual = true;
                                vSensVal[3].actual = true;
                        }
                        // vTaskDelay(pdMS_TO_TICKS(50));
                }

                /*************************
                vTaskDelay(pdMS_TO_TICKS(20));
                vSensVal[1].value = sRadSens.getRadIntensyDynamic();
                // ESP_LOGD(TAG, "Rad Dyanmic: %f mRh", vRadD);
                vSensVal[2].value = sRadSens.getRadIntensyStatic();
                // ESP_LOGD(TAG, "Rad Static: %f mRh", vRadS);
                // функция getNumberOfPulses() возвращает количество импульсов, зарегистрированных с момента последнего чтения данных по I2C (а не накопительным итогом с момента старта прибора)
                vSensVal[3].value = sRadSens.getNumberOfPulses();
                // ESP_LOGD(TAG, "Rad Pulses: %d ", vNumPulse);

                ESP_LOGD(TAG, "Rad pulses: %d, dyanmic: %f mRh, static: %f mRh ", vSensVal[3].value, vSensVal[1].value, vSensVal[2].value);

                // vSensVal[1].value = vRadD;
                // vSensVal[2].value = vRadS;
                // vSensVal[3].value = vNumPulse;
                // контроль корректности данных.
                // 500 сек - время на набор статистики
                if (millis() - RADstartTime > 500000)
                {
                        vSensVal[2].actual = true;
                }
                else
                {
                        vSensVal[1].actual = true;
                        vSensVal[2].actual = true;
                }

                vTaskDelay(pdMS_TO_TICKS(50));
                */
        }

        if (bBME_e)
        {
                BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
                BME280::PresUnit presUnit(BME280::PresUnit_Pa);

                sBME_e.read(vSensVal[6].value, vSensVal[4].value, vSensVal[5].value, tempUnit, presUnit);
                vSensVal[6].value = (vSensVal[6].value * 0.007500638); // Pa ->mmHg
                vSensVal[4].value += fBME_e_Tfix;                      // fix

                ESP_LOGD(TAG, "BME_ext Temp=%f, Humi=%f, Pres=%f", vSensVal[4].value, vSensVal[5].value, vSensVal[6].value);

                // vSensVal[7].value = vBME_e_temp;
                // vSensVal[8].value = vBME_e_humi;
                // vSensVal[9].value = vBME_e_pres;

                // контроль корректности данных.
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

        if (bHTU_e)
        {

                if (sHTU_e.read())
                {
                        vSensVal[7].value = sHTU_e.getTemperature();
                        vSensVal[8].value = sHTU_e.getHumidity();
                        vSensVal[7].value += fHTU_e_Tfix; // fix

                        ESP_LOGD(TAG, "HTU_ext Temp=%f, Humi=%f", vSensVal[7].value, vSensVal[8].value);
                }
                else
                {
                        vSensVal[7].actual = false;
                        vSensVal[8].actual = false;
                }

                // контроль корректности данных.
                if (vSensVal[7].value > -50 and vSensVal[7].value < 50)
                {
                        vSensVal[7].actual = true;
                }
                if (vSensVal[8].value > 5 and vSensVal[8].value < 101)
                {
                        vSensVal[8].actual = true;
                }
        }

        if (bSHT_e)
        {
                if (sSHT_e.read())
                {

                        vTaskDelay(pdMS_TO_TICKS(20));
                        vSensVal[9].value = sSHT_e.getTemperature(); // read data
                        vSensVal[10].value = sSHT_e.getHumidity();
                        vSensVal[9].value += fSHT_e_Tfix; // fix

                        ESP_LOGD(TAG, "SHT_ext Temp=%f, Humi=%f", vSensVal[9].value, vSensVal[10].value);

                        // контроль корректности данных.
                        if ((vSensVal[9].value > -50 and vSensVal[9].value < 50) || (!sSHT_e.isHeaterOn()))
                        {
                                vSensVal[9].actual = true;
                        }
                        if (vSensVal[10].value > 5 and vSensVal[10].value < 101)
                        {
                                vSensVal[10].actual = true;
                        }
                        if (vSensVal[10].value >= 100)
                        {
                                sSHT_e.reset();
                        }
                }
        }

        if (bBME_i)
        {
                BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
                BME280::PresUnit presUnit(BME280::PresUnit_Pa);

                //                pres,                  temp,                 hum,
                sBME_i.read(vSensVal[13].value, vSensVal[11].value, vSensVal[12].value, tempUnit, presUnit);
                vSensVal[13].value = (vSensVal[13].value * 0.007500638); // Pa ->mmHg

                ESP_LOGD(TAG, "BME_int Temp=%f, Humi=%f, Pres=%f", vSensVal[11].value, vSensVal[12].value, vSensVal[13].value);

                // vSensVal[4].value = vBME_i_temp;
                // vSensVal[5].value = vBME_i_humi;
                // vSensVal[6].value = vBME_i_pres;
                // контроль корректности данных.
                if (vSensVal[11].value > -50 and vSensVal[11].value < 50)
                {
                        vSensVal[11].actual = true;
                }
                if (vSensVal[12].value > 5 and vSensVal[12].value < 101)
                {
                        vSensVal[12].actual = true;
                }
                if (vSensVal[13].value > 600 and vSensVal[13].value < 811)
                {
                        vSensVal[13].actual = true;
                }
        }

        if (bSCD30_i)
        {
                if (sSCD30_i.dataAvailable())
                {
                        vSensVal[14].value = sSCD30_i.getTemperature();
                        vSensVal[15].value = sSCD30_i.getHumidity();
                        vSensVal[16].value = sSCD30_i.getCO2();
                        ESP_LOGD(TAG, "SCD_int Temp=%f, Humi=%f, CO2=%f", vSensVal[14].value, vSensVal[15].value, vSensVal[16].value);

                        // контроль корректности данных.
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

        // ************************ TEST **********************
        // vSensVal[16].actual = true;
        // vSensVal[16].value = 333.33;
        // vSensVal[16].unit = "ppm";
        // vSensVal[7].actual = true;
        // vSensVal[7].value = 88.88;
        // vSensVal[0].actual = true;
        // vSensVal[0].value = 88.88;
        // *******************************************
}

extern void heatSens() // прогрев датчиков для правильной влажности. (Может стоит проверить на температуру-влажность?)
{
        static const char *TAG = "heat";
        // #HTU21
        if (bHTU_e)
        {
                sHTU_e.heatOn();
                ESP_LOGD(TAG, "Heating HTU is ON");
                logToWeb("Heating HTU is ON");
        }

        // #SHT31
        if (bSHT_e)
        {
                sSHT_e.heatOn();
                ESP_LOGD(TAG, "Heating SHT is ON");
                logToWeb("Heating SHT is ON");
        }

        vTaskDelay(pdMS_TO_TICKS(HEATTIME)); // время нагрева

        if (bHTU_e)
        {
                sHTU_e.heatOff();
                ESP_LOGD(TAG, "Heating HTU is OFF");
                logToWeb("Heating HTU is OFF");
        }
        if (bSHT_e)
        {
                sSHT_e.heatOff();
                ESP_LOGD(TAG, "Heating SHT is OFF");
                logToWeb("Heating SHT is OFF");
        }
        vTaskDelay(pdMS_TO_TICKS(HEATTIME)); // время охлаждения
}

void resetActualSensVal(stSens *vSensVal)
{
        for (int i = 0; i < SensUnit; i++)
        {
                vSensVal[i].actual = false;
        }
}

extern void SCD30Calibration() //  Принудительная калибровка по опорной точке  (FRC — Forced Recalibration)
{
        // Говорим датчику: "То, что ты сейчас измеряешь — это ровно 415 ppm"
        sSCD30_i.setForcedRecalibrationFactor(415);
}
