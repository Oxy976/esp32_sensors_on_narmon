#pragma once

#ifndef sensors_h
#define sensors_h

#include <Arduino.h>
#include <M5Stack.h>
#include "strct.h"

// I2C
//Адрес на шине I2C для SHT21/HDC1080/HDC2080/HTU21D/Si7021 совпадает
#include <Wire.h>                 // i2c lib
//#include "CG_RadSens.h"           // ***Geiger https://github.com/climateguard/RadSens // со старой платой не дружит!!
#include "radSens1v2.h"
#include <BME280I2C.h>  //https://github.com/finitespace/BME280
#include <SHT2x.h>    //  https://github.com/RobTillaart/SHT2x  library for the SHT2x, HTU2x and Si70xx
#include "SHT31.h"              // + SHT31 https://github.com/RobTillaart/SHT31
#include "SparkFun_SCD30_Arduino_Library.h"  //SCD30 CO2 sensor https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library

// OneWire
#include <OneWire.h>           // 1-wire lib
#include "DallasTemperature.h" // Temp DS18B20   https://github.com/milesburton/Arduino-Temperature-Control-Library

// ****
extern void startSens(stSens *vSensVal);
extern void getSensData(stSens *vSensVal); //получить данные
extern void heatSens();
extern void resetActualSensVal(stSens *vSensVal); 
extern void SCD30Calibration();

#endif
