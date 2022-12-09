#ifndef strct_h
#define strct_h

#define SensUnit 17 //  к-во параметров  с датчиков (нумерация с 0)
struct stSens
{
        boolean actual = false; //значение прочиталось, актуально и входит в разрешенный диапазон
        float value = 0.0;      // значение датчика
        String name = "";       //название
        String unit = "";       //единица измерения
        String mqttId = "";     //ID для сервера (mqtt, tcp data)
};

#endif