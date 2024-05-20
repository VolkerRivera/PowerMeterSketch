#ifndef __MODULOSD_H
#define __MODULOSD_H
#define SDCARD_CS_PIN 3
#include <Arduino.h>
//void saveEnergyPerHour(char carpeta[], char archivo[], char measurement[]);
void saveEnergyPerHour(char carpeta[], char archivo[], String measurement);
void savePriceToday(char timestamp[], char precios_hora[]);
bool existPriceToday(char timestamp_now[]);
float readPrice(uint8_t newHour); // permite leer el precio en la hora a la que se hace la consulta
void saveEnergyPerSecond(String timestamp, float energyAcum); // permite guardar un registro de una nueva medida y timestamp
float readEnergy_Accumulation(void); 
uint8_t readHour_LastMeasure(void);
String readTimestamp_LastMeasure(void);
String readDataOfThisDay(String filePath);
#endif /* __MODULOSD_H*/