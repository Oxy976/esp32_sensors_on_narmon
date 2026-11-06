#include "sensors.h"

extern void logToWeb(String text);

// *** Инициализация   датчиков ***
TMP117 sTMP117;
boolean bTMP117 = false;

boolean bSPS30 = false;

SHT4x sSHT40(0x44);
boolean bSHT40 = false;

// *** Существующие датчики ***
#define PIN_DS18B20 26
OneWire oneWire(PIN_DS18B20);
DallasTemperature sDS(&oneWire);
DeviceAddress sensorAddress;
boolean bDS = false;

ClimateGuard_RadSens1v2 sRadSens(RS_DEFAULT_I2C_ADDRESS);
boolean bRAD = false;
unsigned long RADstartTime = millis();

BME280I2C::Settings settings_i(BME280::OSR_X1, BME280::OSR_X1, BME280::OSR_X1, BME280::Mode_Forced, BME280::StandbyTime_1000ms, BME280::Filter_Off, BME280::SpiEnable_False, BME280I2C::I2CAddr_0x76);
BME280I2C::Settings settings_e(BME280::OSR_X1, BME280::OSR_X1, BME280::OSR_X1, BME280::Mode_Forced, BME280::StandbyTime_1000ms, BME280::Filter_Off, BME280::SpiEnable_False, BME280I2C::I2CAddr_0x77);
BME280I2C sBME_i(settings_i);
BME280I2C sBME_e(settings_e);
boolean bBME_i = false;
boolean bBME_e = false;

// Si7021 sHTU_e(&Wire);
HTU21 sHTU_e(&Wire);
boolean bHTU_e = false;

SCD30 sSCD30_i;
boolean bSCD30_i = false;

String sC = "C";
String GRAD = "\u00B0" + sC;

// Поправочные коэффициенты
float fTMP_Tfix = 0.0;
float fDS_Tfix = 0.0;
float fBME_e_Tfix = 1.3;
float fHTU_e_Tfix = -0.3;
float fSHT_e_Tfix = 0.0;

// Автомат асинхронного прогрева - параметры
#define HEATTIME 6000
#define COOLTIME 20000
enum HeatState
{
        HEAT_IDLE,
        HEAT_WARMING,
        HEAT_COOLING
};
static HeatState currentHeatState = HEAT_IDLE;
static unsigned long heatTimer = 0;
static unsigned long lastHeatTimestamp = 0;

// -------------------------------------------------

void startSens(stSens *vSensVal)
{
        static const char *TAG = "sensors_init";
        ESP_LOGD(TAG, "Start init sensors");

        // ================= СТРУКТУРА МАССИВА (21 параметр) =================
        // [0] TMP117 (Новая позиция 0)
        vSensVal[0].name = "extTMP_Temp";
        vSensVal[0].id_name = "temp_ext_tmp";
        vSensVal[0].mqttId = "";

        // [1] - [3] Гейгер
        vSensVal[1].name = "Rad_dyn";
        vSensVal[1].id_name = "rad_dyn";
        vSensVal[1].mqttId = "";

        vSensVal[2].name = "Rad_stat";
        vSensVal[2].id_name = "rad_stat";
        vSensVal[2].mqttId = "R0";

        vSensVal[3].name = "Rad_pulses";
        vSensVal[3].id_name = "rad_pulses";
        vSensVal[3].mqttId = "";

        // [4] - [6] BME280 ext
        vSensVal[4].name = "extBME_Temp";
        vSensVal[4].id_name = "temp_ext_bme";
        vSensVal[4].mqttId = "";

        vSensVal[5].name = "extBME_Hum";
        vSensVal[5].id_name = "hum_ext_bme";
        vSensVal[5].mqttId = "";

        vSensVal[6].name = "extBME_Press";
        vSensVal[6].id_name = "press_ext_bme";
        vSensVal[6].mqttId = "";

        // [7] - [8] HTU21D ext
        vSensVal[7].name = "extHTU_Temp";
        vSensVal[7].id_name = "temp_ext_htu";
        vSensVal[7].mqttId = "";

        vSensVal[8].name = "extHTU_Hum";
        vSensVal[8].id_name = "hum_ext_htu";
        vSensVal[8].mqttId = "";

        // [9] - [10] SHT40 (Заменил SHT31)
        vSensVal[9].name = "extSHT_Temp";
        vSensVal[9].id_name = "temp_ext_sht";
        vSensVal[9].mqttId = "T1";

        vSensVal[10].name = "extSHT_Hum";
        vSensVal[10].id_name = "hum_ext_sht";
        vSensVal[10].mqttId = "H1";

        // [11] - [13] BME280 int
        vSensVal[11].name = "intBME_Temp";
        vSensVal[11].id_name = "temp_int_bme";
        vSensVal[11].mqttId = "T2";

        vSensVal[12].name = "intBME_Hum";
        vSensVal[12].id_name = "hum_int_bme";
        vSensVal[12].mqttId = "H2";

        vSensVal[13].name = "intBME_Press";
        vSensVal[13].id_name = "press_int_bme";
        vSensVal[13].mqttId = "P2";

        // [14] - [16] SCD30 int
        vSensVal[14].name = "intSCD30_Temp";
        vSensVal[14].id_name = "temp_int_scd30";
        vSensVal[14].mqttId = "";

        vSensVal[15].name = "intSCD30_Hum";
        vSensVal[15].id_name = "hum_int_scd30";
        vSensVal[15].mqttId = "";

        vSensVal[16].name = "intSCD30_CO2";
        vSensVal[16].id_name = "co2_int_scd30";
        vSensVal[16].mqttId = "CO2";

        // [17] - [19] SPS30 (Новые датчики пыли)
        vSensVal[17].name = "SPS_PM1_0";
        vSensVal[17].id_name = "sps_pm1_0";
        vSensVal[17].mqttId = "";

        vSensVal[18].name = "SPS_PM2_5";
        vSensVal[18].id_name = "sps_pm2_5";
        vSensVal[18].mqttId = "";

        vSensVal[19].name = "SPS_PM10";
        vSensVal[19].id_name = "sps_pm10";
        vSensVal[19].mqttId = "";

        // [20] DS18B20 (Перемещен в самый низ)
        vSensVal[20].name = "extDS_Temp";
        vSensVal[20].id_name = "temp_ext_ds";
        vSensVal[20].mqttId = "T0";

        // ------------------ ИНИЦИАЛИЗАЦИЯ ------------------

        // 0. TMP117
        if (!sTMP117.begin())
        {
                ESP_LOGD(TAG, "--- TMP117 sensor not found");
                logToWeb("--- TMP117 not found");
                bTMP117 = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ TMP117 sensor found");
                logToWeb("+++ TMP117 found");
                bTMP117 = true;
                vSensVal[0].unit = GRAD;
        }

        // 1. Гейгер RadSens
        if (!sRadSens.radSens_init())
        {
                ESP_LOGD(TAG, "--- RadSens not found ");
                bRAD = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ RadSens found");
                logToWeb("+++ RadSens found");
                bRAD = true;
                sRadSens.setSensitivity(105);
                sRadSens.setHVGeneratorState(true);
                RADstartTime = millis();
                vSensVal[1].unit = "mRg/h";
                vSensVal[2].unit = "mRg/h";
                vSensVal[3].unit = "#";
        }

        // 2. BME280 ext
        if (!sBME_e.begin())
        {
                bBME_e = false;
                ESP_LOGD(TAG, "--- BME280_ext not found ");
        }
        else
        {
                ESP_LOGI(TAG, "+++ BME280_ext found");
                logToWeb("+++ BME280_ext found");

                bBME_e = true;
                vSensVal[4].unit = GRAD;
                vSensVal[5].unit = "%";
                vSensVal[6].unit = "mmHg";
        }

        // 3. Si7021 / HTU21D ext
        sHTU_e.begin();
        if (!sHTU_e.isConnected())
        {
                ESP_LOGD(TAG, "--- HTU21/Si7021 ext sensor not found ");
                bHTU_e = false;
        }
        else
        {
                // Просто пишем в лог то, что он отдал, не ломая датчик сменой разрешений
                logToWeb("+++ HTU21/Si7021 found on 0x40. Reported FW code: " + String(sHTU_e.getFirmwareVersion()));
                bHTU_e = true;
                vSensVal[7].unit = GRAD;
                vSensVal[8].unit = "%";
        }
        bHTU_e = false; // #### Принудительно отключен т.к. не отдает данные, но адрес занимает


        // 4. SHT40 (Взамен SHT31)
        if (!sSHT40.begin())
        {
                ESP_LOGD(TAG, "--- SHT40 ext sensor not found");
                logToWeb("--- SHT40 not found");
                bSHT40 = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ SHT40 ext sensor found");
                logToWeb("+++ SHT40 found");
                bSHT40 = true;
                vSensVal[9].unit = GRAD;
                vSensVal[10].unit = "%";
        }

        // 5. BME280 int
        if (!sBME_i.begin())
        {
                bBME_i = false;
                ESP_LOGD(TAG, "--- BME280_int not found ");
        }
        else
        {
                bBME_i = true;
                ESP_LOGI(TAG, "+++ BME280_int found");
                logToWeb("+++ BME280_int found");
                vSensVal[11].unit = GRAD;
                vSensVal[12].unit = "%";
                vSensVal[13].unit = "mmHg";
        }

        // 6. SCD30 int
        if (!sSCD30_i.begin(Wire, false))
        {
                ESP_LOGD(TAG, "--- SCD30 not found ");
                bSCD30_i = false;
        }
        else
        {
                bSCD30_i = true;
                ESP_LOGI(TAG, "+++ SCD30 found");
                logToWeb("+++ SCD30 found");
                vSensVal[14].unit = GRAD;
                vSensVal[15].unit = "%";
                vSensVal[16].unit = "ppm";
        }

        // 7. SPS30
        sensirion_i2c_init();
        if (sps30_probe() != 0)
        {
                bSPS30 = false;
                ESP_LOGD(TAG, "--- Датчик частиц SPS30 не обнаружен");
                logToWeb("--- SPS30 not found");
        }
        else
        {
                if (sps30_start_measurement() != 0)
                {
                        bSPS30 = false;
                }
                else
                {
                        bSPS30 = true;
                        ESP_LOGI(TAG, "+++ Датчик частиц SPS30 успешно запущен");
                        logToWeb("+++ SPS30 activated");
                        vSensVal[17].unit = "ug/m3";
                        vSensVal[18].unit = "ug/m3";
                        vSensVal[19].unit = "ug/m3";
                }
        }

        // 8. Dallas DS18B20
        sDS.begin();
        if (sDS.getDeviceCount() == 0 || !sDS.getAddress(sensorAddress, 0))
        {
                ESP_LOGD(TAG, "--- DS18B20 not found ");
                bDS = false;
        }
        else
        {
                ESP_LOGI(TAG, "+++ DS18B20 found");
                logToWeb("+++ DS18B20 found");

                bDS = true;
                sDS.setResolution(sensorAddress, 12);
                sDS.setWaitForConversion(false);
                sDS.requestTemperatures();
                vSensVal[20].unit = GRAD;
        }
        logToWeb("End initing sensors");
}

void getSensData(stSens *vSensVal)
{
        resetActualSensVal(vSensVal);
        static const char *TAG = "sensors_values";
        bool skipWeather = (currentHeatState != HEAT_IDLE);

        // [0] Чтение TMP117
        if (bTMP117 && sTMP117.dataReady())
        {
                vSensVal[0].value = sTMP117.readTempC() + fTMP_Tfix;
                if (vSensVal[0].value > -50.0 && vSensVal[0].value < 100.0)
                {
                        vSensVal[0].actual = true;
                }
        }

        // [1]-[3] Чтение RadSens
        if (bRAD && sRadSens.getData())
        {
                vSensVal[1].value = sRadSens.getRadIntensyDyanmic();
                vSensVal[2].value = sRadSens.getRadIntensyStatic();
                vSensVal[3].value = sRadSens.getNumberOfPulses();
                if (vSensVal[3].value >= 0)
                {
                        vSensVal[1].actual = vSensVal[2].actual = vSensVal[3].actual = true;
                }
        }

        // [4]-[6] Чтение BME280 ext
        if (bBME_e)
        {
                BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
                BME280::PresUnit presUnit(BME280::PresUnit_Pa);
                sBME_e.read(vSensVal[6].value, vSensVal[4].value, vSensVal[5].value, tempUnit, presUnit);
                vSensVal[6].value *= 0.007500638; // Pa -> mmHg
                vSensVal[4].value += fBME_e_Tfix;
                if (vSensVal[4].value > -50 && vSensVal[4].value < 50)
                        vSensVal[4].actual = true;
                if (vSensVal[5].value > 5 && vSensVal[5].value < 101)
                        vSensVal[5].actual = true;
                if (vSensVal[6].value > 600 && vSensVal[6].value < 811)
                        vSensVal[6].actual = true;
        }

        //- Чтение совместимого HTU21 / Клона ext
        if (bHTU_e && !skipWeather)
        {
                if (sHTU_e.read())
                {
                        vSensVal[7].value = sHTU_e.getTemperature() + fHTU_e_Tfix;
                        vSensVal[8].value = sHTU_e.getHumidity();

                        if (vSensVal[7].value > -50 && vSensVal[7].value < 50)
                                vSensVal[7].actual = true;
                        if (vSensVal[8].value > 5 && vSensVal[8].value < 101)
                                vSensVal[8].actual = true;
                }
                else
                {
                        uint8_t errCode = sHTU_e.getError();

                        char errBuffer[64];
                        snprintf(errBuffer, sizeof(errBuffer), " [ERROR] Sensor 0x40 read error code: 0x%02X", errCode);
                        ESP_LOGW(TAG, "%s", errBuffer);
                        logToWeb(String(errBuffer)); // ###### УДАЛИТЬ!!! ################

                        // >>> ИСПРАВЛЕНИЕ: Если поймали ошибку записи (0x81) — бьем датчик софт-ресетом
                        if (errCode == 0x81)
                        {
                                ESP_LOGW(TAG, "Попытка реанимации датчика 0x40 через Soft Reset...");
                                logToWeb("HTU RESET");
                                sHTU_e.reset(); // Принудительный сброс автомата состояний чипа
                        }
                }
        }

        // [9]-[10] Чтение SHT40 (Вместо SHT31)
        if (bSHT40 && !skipWeather)
        {
                if (sSHT40.read())
                {
                        vSensVal[9].value = sSHT40.getTemperature() + fSHT_e_Tfix;
                        vSensVal[10].value = sSHT40.getHumidity();
                        if (vSensVal[9].value > -50 && vSensVal[9].value < 60)
                                vSensVal[9].actual = true;
                        if (vSensVal[10].value > 0 && vSensVal[10].value <= 100)
                                vSensVal[10].actual = true;
                }
        }
        // [11]-[13] Чтение BME280 int
        if (bBME_i)
        {
                BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
                BME280::PresUnit presUnit(BME280::PresUnit_Pa);
                sBME_i.read(vSensVal[13].value, vSensVal[11].value, vSensVal[12].value, tempUnit, presUnit);
                vSensVal[13].value *= 0.007500638;
                if (vSensVal[11].value > -50 && vSensVal[11].value < 50)
                        vSensVal[11].actual = true;
                if (vSensVal[12].value > 5 && vSensVal[12].value < 101)
                        vSensVal[12].actual = true;
                if (vSensVal[13].value > 600 && vSensVal[13].value < 811)
                        vSensVal[13].actual = true;
        }
        // [14]-[16] Чтение SCD30 int
        if (bSCD30_i && sSCD30_i.dataAvailable())
        {
                vSensVal[14].value = sSCD30_i.getTemperature();
                vSensVal[15].value = sSCD30_i.getHumidity();
                vSensVal[16].value = sSCD30_i.getCO2();
                if (vSensVal[14].value > -50 && vSensVal[14].value < 50)
                        vSensVal[14].actual = true;
                if (vSensVal[15].value > 5 && vSensVal[15].value < 101)
                        vSensVal[15].actual = true;
                vSensVal[16].actual = true;
        }
        // [17]-[19] Чтение SPS30
        if (bSPS30)
        {
                struct sps30_measurement measurement;
                if (sps30_read_measurement(&measurement) == 0)
                {
                        vSensVal[17].value = measurement.mc_1p0;
                        vSensVal[17].actual = true;
                        vSensVal[18].value = measurement.mc_2p5;
                        vSensVal[18].actual = true;
                        vSensVal[19].value = measurement.mc_4p0;
                        vSensVal[19].actual = true;
                }
        }

        // [20] Чтение смещенного DS18B20
        if (bDS)
        {
                vSensVal[20].value = sDS.getTempC(sensorAddress) + fDS_Tfix;
                sDS.requestTemperatures(); // Запускаем асинхронный замер на следующий цикл
                if (vSensVal[20].value > -50 && vSensVal[20].value < 50)
                {
                        vSensVal[20].actual = true;
                }
        }
}
void resetActualSensVal(stSens *vSensVal)
{
        for (int i = 0; i < SensUnit; i++)
        {
                vSensVal[i].actual = false;
        }
}
extern void SCD30Calibration()
{
        sSCD30_i.setForcedRecalibrationFactor(415);
}

// неблокирующий автомат асинхронного прогрева под особенности SHT40
void triggerHeatCycle()
{
        if (currentHeatState != HEAT_IDLE)
                return;

        // Защита: разрешаем греть датчики не чаще, чем раз в 15 минут (900 000 мс)
        if (millis() - lastHeatTimestamp < 900000)
                return;

        // 1. Прогрев HTU21D (включили регистром, выключим в фазе COOLING)
        if (bHTU_e)
        {
                sHTU_e.heatOn();
                logToWeb("[HEAT] Нагрев Si7021  включен.");
        }

        // 2. АСИНХРОННЫЙ Прогрев SHT40 (ИСПРАВЛЕНО!)
        // Метод requestData отправляет команду на нагрев/замер БЕЗ блокировки таски задержками delay().
        // Шина I2C освобождается мгновенно, что исключает зависание FreeRTOS.
        if (bSHT40)
        {
                sSHT40.requestData(SHT4x_MEASUREMENT_LONG_HIGH_HEAT);
                logToWeb("[HEAT] Запущен асинхронный импульсный прогрев SHT40 (1s).");
        }

        heatTimer = millis();
        lastHeatTimestamp = millis();
        currentHeatState = HEAT_WARMING;
}

void processAsyncHeating()
{
        switch (currentHeatState)
        {
        case HEAT_IDLE:
                break;

        case HEAT_WARMING:
                // HTU21D/Si7021 мы греем 6 секунд (HEATTIME).
                // Встроенный нагреватель SHT40 к этому моменту уже сам отключился (так как прошел заложенный в него 1 х секунда).
                if (millis() - heatTimer >= HEATTIME)
                {
                        // Принудительно гасим подогрев Si7021
                        if (bHTU_e)
                                sHTU_e.heatOff();

                        // ИСПРАВЛЕНО: Безопасно вычитываем буфер замера SHT40, который он сделал сразу после прогрева.
                        // Это очистит регистры датчика и подготовит его к нормальной работе.
                        if (bSHT40 && sSHT40.dataReady())
                        {
                                sSHT40.readData();
                        }

                        logToWeb("[HEAT] Фаза активного нагрева завершена. Датчики переходят в режим остывания...");
                        heatTimer = millis();
                        currentHeatState = HEAT_COOLING;
                }
                break;

        case HEAT_COOLING:
                // Ждем 20 секунд (COOLTIME), пока оба кристалла окончательно остынут.
                // Переменная skipWeather в getSensData() надежно защищает от чтения горячих датчиков.
                if (millis() - heatTimer >= COOLTIME)
                {
                        currentHeatState = HEAT_IDLE;
                        logToWeb("[HEAT] Цикл прогрева и охлаждения полностью завершен.");
                }
                break;
        default:
                break;
        }
}
