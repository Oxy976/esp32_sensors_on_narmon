#ifndef strct_h
#define strct_h

#include <Arduino.h>

#define SensUnit 17 // к-во параметров с датчиков (нумерация с 0)

struct stSens
{
    boolean actual = false;     // значение прочиталось, актуально и входит в разрешенный диапазон
    float value = 0.0;          // значение датчика
    const char* id_name = "";   // Уникальный текстовый ID (хранится во Flash)
    const char* name = "";      // Красивое название для Web (теперь хранится во Flash!)
    const char* mqttId = "";    // ID для сервера/narodmon (теперь хранится во Flash!)
    String unit = "";           // Единица измерения (оставляем String, так как формируется динамически)
};

#endif
