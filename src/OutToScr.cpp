#define USE_M5_FONT_CREATOR //работа с внешним шрифтом
#include <Arduino.h>
#include <M5Stack.h>
#include "strct.h"
#define SCRDELAY 5000 // сколько показывать картинку
// https://github.com/m5stack/M5Stack/blob/master/src/utility/In_eSPI.h
// тут используется исправленный шрифт с новыми символами и русскими буквами. Возможность вывода в UTF8 не используется. Вывод посимвольно.

//+Ru(192-255)+up/down[0-9](133-152)+sym(155-159)
#include "fonts/RobotoR16pt8b.h"
#define F_RR16 &RobotoR16pt8b
#include "fonts/RobotoR32pt8b.h"
#define F_RR32 &RobotoR32pt8b

int sym_dnum[] = {143, 144, 145, 146, 147, 148, 149, 150, 151, 152}; //!!only for font RobotoR !!
int sym_gradC = 159;                                                 //!!only for font RobotoR !!
int sym_uS = 155;                                                    //!!only for font RobotoR !!
int sym_uR = 156;                                                    //!!only for font RobotoR !!
int sym_mmR = 158;                                                   //!!only for font RobotoR !!

void OutStrToScr(String stb1, String stsr1, String stsr2, String stsd1, String stsd2, String stsd3)
{
  M5.Lcd.fillScreen(0);

  M5.Lcd.drawRoundRect(1, 1, 212, 160, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 1, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 81, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(1, 160, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(107, 160, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 160, 106, 80, 7, TFT_DARKGREY);

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(MC_DATUM); // Middle centre

  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.setFreeFont(F_RR32);
  M5.Lcd.drawString(stb1, 106, 70, 1); // Print the test text in the custom font

  M5.Lcd.setFreeFont(F_RR16);
  M5.Lcd.setTextColor(TFT_MAGENTA);
  M5.Lcd.drawString(stsr1, 267, 40, 1);
  M5.Lcd.drawString(stsr2, 267, 120, 1);

  // M5.Lcd.setFreeFont(F_RR16);
  M5.Lcd.setTextColor(TFT_YELLOW);
  M5.Lcd.drawString(stsd1, 53, 200, 1);
  M5.Lcd.drawString(stsd2, 159, 200, 1);
  M5.Lcd.drawString(stsd3, 265, 200, 1);
}

String TempToStr(float t)
{
  long T;
  int t1, t2, sym_t2;
  String strT;
  T = round(t * 10);
  t1 = T / 10;
  t2 = abs(T % 10);
  sym_t2 = sym_dnum[t2];
  strT = String(t1) + "," + String(char(sym_t2)) + " " + String(char(sym_gradC));
  return strT;
}

String PressToStr(float p)
{
  int p1;
  String strP;

  p1 = abs(p);
  strP = String(p1) + " " + String(char(sym_mmR));
  return strP;
}

String HumToStr(float h)
{
  long H;
  int h1, h2, sym_h2;
  String strH;
  H = round(h * 10);
  h1 = H / 10;
  h2 = abs(H % 10);
  sym_h2 = sym_dnum[h2];
  strH = String(h1) + "," + String(char(sym_h2)) + " %";
  return strH;
}

String RgToStr(float r)
{
  long R;
  int r1, r2, sym_r2;
  String strR;
  R = round(r * 10);
  r1 = R / 10;
  r2 = abs(R % 10);
  sym_r2 = sym_dnum[r2];
  strR = String(r1) + "," + String(char(sym_r2)) + " " + String(char(sym_uS));
  return strR;
}

void ScreenOff()
{
  // delay(2000);
  vTaskDelay(2000);
  for (int i = 255; i >= 0; i--)
  {
    M5.Lcd.setBrightness(i);
    // delay(10);
    vTaskDelay(10);
  }
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.writecommand(ILI9341_DISPOFF);
}

void ScreenOn()
{
  M5.Lcd.setBrightness(0);
  M5.Lcd.writecommand(ILI9341_DISPON);
  for (int i = 0; i <= 255; i++)
  {
    M5.Lcd.setBrightness(i);
    // delay(2);
    vTaskDelay(2);
  }
}

void OutToScr(stSens *vSensVal)
{
  ScreenOn();

  float b1, sr1, sr2, sd1, sd2, sd3;
  // b1
  if (vSensVal[13].actual) // DS extDS_Temp
  {
    b1 = vSensVal[13].value;
  }
  else
  {
    if (vSensVal[11].actual) // SHT31 extSHT_Temp
    {
      b1 = vSensVal[11].value;
    }
    else
    {
      if (vSensVal[9].actual) // HTU21 extHTU_Temp"
      {
        b1 = vSensVal[9].value;
      }
      else
      {
        if (vSensVal[3].actual) // BME_e_temp
        {
          b1 = vSensVal[3].value;
        }
        else
        {
          b1 = 888.8;
        }
      }
    }
  }
  // sr1  правый столбец влажность снаружи
  if (vSensVal[10].actual) // vSHT_e_humi
  {
    sr1 = vSensVal[10].value;
  }
  else
  {
    if (vSensVal[12].actual) // vHTU_e_humi
    {
      sr1 = vSensVal[12].value;
    }
    else
    {
      if (vSensVal[4].actual) // vBME_e_humi
      {
        sr1 = vSensVal[4].value;
      }
      else
      {
        sr1 = 0.0;
      }
    }
  }

  // sr2 правый столбец давление
  if (vSensVal[8].actual) // vBME_i_pres
  {
    sr2 = vSensVal[8].value;
  }
  else
  {
    if (vSensVal[5].actual) // vBME_e_pres
    {
      sr2 = vSensVal[5].value;
    }
    else
    {
      sr2 = 0.0;
    }
  }

  // sd1 нижняя строка температура внутри
  if (vSensVal[3].actual) // vBME_i_temp
  {
    sd1 = vSensVal[3].value;
  }
  else
  {
    sd1 = 888.0;
  }

  // sd2 нижняя строка влажность внутри
  if (vSensVal[4].actual) // vBME_i_humi
  {
    sd2 = vSensVal[4].value;
  }
  else
  {
    sd2 = 0.0;
  }

  // sd3 нижняя строка радиация
  if (vSensVal[0].actual) // RAD
  {
    sd3 = vSensVal[0].value;
  }
  else
  {
    sd3 = 0.0;
  }

  OutStrToScr(TempToStr(b1), HumToStr(sr1), PressToStr(sr2), TempToStr(sd1), HumToStr(sd2), RgToStr(sd3));
  vTaskDelay(SCRDELAY);
  ScreenOff();
}

void ShowTime()
{
  ScreenOn();
  struct tm timeinfo;

  getLocalTime(&timeinfo);

  char ctme[20], cdte[20];
  sprintf(ctme, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  sprintf(cdte, "%02d-%02d-%4d", timeinfo.tm_mday, (timeinfo.tm_mon + 1), (timeinfo.tm_year + 1900));
  String stme = ctme;
  String sdte = cdte;
  int wday = timeinfo.tm_wday;
  // String swday;

  // show screen with date-time
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextDatum(MC_DATUM); // Middle centre
  M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(stme, 160, 90, 7); // 7 - digital font
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setFreeFont(F_RR16);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString(sdte, 160, 170, 1);
  // M5.Lcd.drawString(swday, 160, 20, 1);

  switch (wday)
  {
  case 0:
    // swday = "Воскресенье";
    M5.Lcd.drawString(String(char(194)), 140, 210, 1);
    M5.Lcd.drawString(String(char(241)), 165, 210, 1);
    break;
  case 1:
    // swday = "Понедельник";
    M5.Lcd.drawString(String(char(207)), 140, 210, 1);
    M5.Lcd.drawString(String(char(237)), 160, 210, 1);
    break;
  case 2:
    // swday = "Вторник";
    M5.Lcd.drawString(String(char(194)), 140, 210, 1);
    M5.Lcd.drawString(String(char(242)), 164, 210, 1);
    break;
  case 3:
    // swday = "Среда";
    M5.Lcd.drawString(String(char(209)), 140, 210, 1);
    M5.Lcd.drawString(String(char(240)), 160, 210, 1);
    break;
  case 4:
    // swday = "Четверг";
    M5.Lcd.drawString(String(char(215)), 140, 210, 1);
    M5.Lcd.drawString(String(char(242)), 160, 210, 1);
    break;
  case 5:
    // swday = "Пятница";
    M5.Lcd.drawString(String(char(207)), 140, 210, 1);
    M5.Lcd.drawString(String(char(242)), 162, 210, 1);
    break;
  case 6:
    // swday = "Суббота";
    M5.Lcd.drawString(String(char(209)), 140, 210, 1);
    M5.Lcd.drawString(String(char(225)), 160, 210, 1);
    break;
  }
  vTaskDelay(SCRDELAY);
  ScreenOff();
}
