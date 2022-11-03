#pragma once

#ifndef sensors_h
#define sensors_h

#include <Arduino.h>
#include <M5Stack.h>
#include "strct.h"

//#include "sens_sett.h"

// I2C
//Адрес на шине I2C для SHT21/HDC1080/HDC2080/HTU21D/Si7021 совпадает
#include <Wire.h>                 // i2c lib
#include "CG_RadSens.h"           // ***Geiger https://github.com/climateguard/RadSens
#include "cactus_io_BME280_I2C.h" // ***BME280  http://static.cactus.io/downloads/library/bme280/cactus_io_BME280_I2C.zip
#include "HTU2xD_SHT2x_Si70xx.h"  // ***HTU21D https://github.com/enjoyneering/HTU2xD_SHT2x_Si70xx
#include "SHT31.h"              // + SHT31 https://github.com/RobTillaart/SHT31

// OneWire
#include <OneWire.h>           // 1-wire lib
#include "DallasTemperature.h" // Temp DS18B20   https://github.com/milesburton/Arduino-Temperature-Control-Library

// ****
extern void startSens();
extern void getSensData(stSens *vSensVal); //получить данные

#endif
