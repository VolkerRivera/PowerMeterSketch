#ifndef __MODULOSD_H
#define __MODULOSD_H
#define SDCARD_CS_PIN 3
#include <Arduino.h>
void saveEnergyPerHour(char carpeta[], char archivo[], char measurement[]);
void savePriceToday(char timestamp[], char precios_hora[]);
bool existPriceToday(char timestamp_now[]);
float readPriceNow(uint8_t newHour);
void saveEnergyPerSecond(String timestamp, float energyAcum);
uint8_t readEnergyPerSecondFile(uint8_t linea);
#endif /* __MODULOSD_H*/