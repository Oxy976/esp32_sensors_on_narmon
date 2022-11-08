#ifndef strct_h
#define strct_h

#define SensUnit 14 //  к-во параметров  с датчиков +1
struct stSens
{
        boolean actual = false; //значение прочиталось, актуально и входит в разрешенный диапазон
        float value = 0.0;      // значение датчика
        String name = "";       //название
        String unit = "";       //единица измерения
        String mqttId = "";     // MQTT ID
};

#endif