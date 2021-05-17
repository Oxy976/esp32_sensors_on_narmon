#include <Arduino.h>
#include <M5Stack.h>
// https://github.com/m5stack/M5Stack/blob/master/src/utility/In_eSPI.h

//** Переписать под UTF8 ! ***

/*#include "utf8rus.h"

#include "fonts/RobotoR12pt8b.h"
#define F_RR12 &RobotoR12pt8b
#include "fonts/RobotoR14pt8b.h"
#define F_RR14 &RobotoR14pt8b
#include "fonts/RobotoR16pt8b.h"
#define F_RR16 &RobotoR16pt8b
#include "fonts/RobotoR32pt8b.h"
#define F_RR32 &RobotoR32pt8b
*/

int sym_dnum[] = {143, 144, 145, 146, 147, 148, 149, 150, 151, 152 }; //!!only for font RobotoR !!
int sym_gradC = 159; //!!only for font RobotoR !!
int sym_uS = 155; //!!only for font RobotoR !!
int sym_uR = 156; //!!only for font RobotoR !!
int sym_mmR = 158; //!!only for font RobotoR !!


void OutStrToScr( String stb1, String  stsr1, String  stsr2, String  stsd1, String  stsd2, String  stsd3 )
{
  M5.Lcd.fillScreen(0);

  M5.Lcd.drawRoundRect(1, 1, 212, 160, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 1, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 81, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(1, 160, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(107, 160, 106, 80, 7, TFT_DARKGREY);
  M5.Lcd.drawRoundRect(213, 160, 106, 80, 7, TFT_DARKGREY);

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(MC_DATUM); //Middle centre

  M5.Lcd.setTextColor(TFT_RED);
 // M5.Lcd.setTextColor(0xfbe4);
  //  M5.Lcd.setTextColor(0xe8e4);
//  M5.Lcd.setFont(F_RR32);                 // Select the font

  M5.Lcd.drawString(stb1, 106, 80, 1);// Print the test text in the custom font

  //  M5.Lcd.setTextColor(0xfbe4);
  //M5.Lcd.setTextColor(0xe8e4);
  M5.Lcd.setTextColor(TFT_MAGENTA);
//  M5.Lcd.setFont(F_RR16);
  M5.Lcd.drawString(stsr1, 267, 40, 1);
  M5.Lcd.drawString(stsr2, 267, 120, 1);

  //M5.Lcd.setTextColor(0xff80);
  M5.Lcd.setTextColor(TFT_YELLOW);
//  M5.Lcd.setFont(F_RR16);
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
  strR = String(r1) + "," + String(char(sym_r2)) + " " + String(char(sym_uR));
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
//  M5.Lcd.setFont(F_RR14);                 // Select the font
  M5.Lcd.drawString(sdte, 160, 170, 1);
//  M5.Lcd.setFont(F_RR12);                 // Select the font
//  M5.Lcd.drawString(utf8rus(swday), 160, 210, 1);
}
