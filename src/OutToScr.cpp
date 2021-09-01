#include <Arduino.h>
#include <M5Stack.h>
// https://github.com/m5stack/M5Stack/blob/master/src/utility/In_eSPI.h

#include "fonts/RoboFix_24px.h"
#include "fonts/RoboFix_28px.h"
#include "fonts/RoboFix_72px.h"

int sym_dnum[] = {0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9 }; //!!only for font Robo !!
int sym_gradC = 0x00D0; //!!only for font Robo !!
int sym_uS = 0x00CC; //!!only for font Robo !!
int sym_uR = 0x00CD; //!!only for font Robo !!
int sym_mmR = 0x00CF; //!!only for font Robo !!
int sym_proc = 0x0025;


void OutStrToScr( String stb1, String  stsr1, String  stsr2, String  stsd1, String  stsd2, String  stsd3 )
{
  M5.Lcd.fillScreen(0);

// решетка
  M5.Lcd.drawRoundRect(1, 1, 212, 160, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 1, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 81, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(1, 160, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(107, 160, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 160, 106, 80, 7, TFT_DARKGREY);

// темп. улица
  M5.Lcd.setFreeFont(&RoboFix_72px);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_RED);
  
  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(stb1, 108, 80, 1);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.drawString(String(char(sym_gradC)), 136, 80, 1);

// даннце улицы, правый столбик
  M5.Lcd.setFreeFont(&RoboFix_28px);
  M5.Lcd.setTextSize(1);
  
  M5.Lcd.setTextColor(TFT_MAGENTA);
  
  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(stsr1, 265, 40, 1);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.drawString(String(char(sym_proc)), 279, 40, 1);

  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(stsr2, 270, 120, 1);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.drawString(String(char(sym_mmR)), 283, 120, 1);

//Внутри, нижняя строка
  M5.Lcd.setTextColor(TFT_YELLOW);

  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(stsd1, 55, 200, 1);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.drawString(String(char(sym_gradC)), 70, 200, 1);

  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(stsd2, 159, 200, 1);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.drawString(String(char(sym_proc)), 173, 200, 1);

  M5.Lcd.setTextDatum(MR_DATUM);
  M5.Lcd.drawString(stsd3, 258, 200, 1);
  M5.Lcd.setTextDatum(ML_DATUM);
  M5.Lcd.drawString(String(char(sym_uR)), 275, 200, 1);
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
  strT = String(t1) + "," + String(char(sym_t2));
  return strT;
}

String PressToStr(float p)
{
  int p1;
  String strP;
  p1 = abs(p);
  strP = String(p1) ;
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
  strH = String(h1) + "," + String(char(sym_h2));
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
  strR = String(r1) + "," + String(char(sym_r2));
  return strR;
}



void OutToScr( float b1, float  sr1, float  sr2, float  sd1, float  sd2, float  sd3 )
{
  OutStrToScr( TempToStr(b1), HumToStr(sr1), PressToStr(sr2), TempToStr(sd1), HumToStr(sd2), RgToStr(sd3) );
}




void ScreenOff()
{
  for (int i = 255; i >= 0; i--)
  {
    M5.Lcd.setBrightness(i);
    delay(10);
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
    delay(2);
  }
}


void ShowTime(struct tm timeinfo) {
  char ctme[20], cdte[20];
  sprintf(ctme, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  sprintf(cdte, "%02d-%02d-%4d", timeinfo.tm_mday, (timeinfo.tm_mon + 1), (timeinfo.tm_year + 1900));
  String stme = ctme;
  String sdte = cdte;
  int wday = timeinfo.tm_wday;
  String swday;
  switch ( wday) {
    case 0:
      swday = "Воскресенье";
      break;
    case 1:
      swday = "Понедельник";
      break;
    case 2:
      swday = "Вторник";
      break;
    case 3:
      swday = "Среда";
      break;
    case 4:
      swday = "Четверг";
      break;
    case 5:
      swday = "Пятница";
      break;
    case 6:
      swday = "Суббота";
      break;
  }

  //show screen with date-time
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextDatum(MC_DATUM); //Middle centre
  M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(stme, 160, 90, 7);  // 7 - digital font
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setFreeFont(&RoboFix_28px);
  M5.Lcd.drawString(sdte, 160, 170, 1);
  M5.Lcd.drawString(swday, 160, 205, 1);
}
