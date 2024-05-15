#include "moduloSD.h"
#include <SD.h>
#include <SPI.h>
#include <string>

void saveEnergyPerHour(char carpeta[], char archivo[], char measurement[]) {
  // /carpeta/archivo.txt
  char path[11];  // n = 11
  sprintf(path, "/%s/%s.txt", carpeta, archivo);

  /* Comprobamos si la SD se encuentra insertada */
  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("La tarjeta microSD detectada correctamente.");
    if (!SD.exists(path)) SD.mkdir(carpeta);  //Si no existe dicha direccion creamos la carpeta que la contendra si no existe aun, si no no pasa nada
    File myFile = SD.open(path, FILE_WRITE);  //una vez nos aseguramos de que existe la carpeta, abrimos o creamos el archivo en ella si no se encuentra aun
    myFile.println(measurement);
    myFile.close();

  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }
}

void saveEnergyPerSecond(String timestamp, float energyAcum){
  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("La tarjeta microSD detectada correctamente.");
    File myFile = SD.open("energyPerSecond.txt", FILE_WRITE);

    // Truncar el archivo (borrar su contenido)
    myFile.truncate(0);

    myFile.println(timestamp); //linea 0 "%04Y-%02m-%02d %02H:%02M:%02S"
    myFile.println(energyAcum); //linea 1
    myFile.close();

  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }
}

uint8_t readEnergyPerSecondFile(uint8_t num_linea){
  uint8_t hora_reg;
  String timestamp;
  uint8_t index = 0;

  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("La tarjeta microSD detectada correctamente.");
    File myFile = SD.open("energyPerSecond.txt", FILE_READ);

    while(index <= num_linea){
      timestamp = myFile.readStringUntil('\n');
      index++;
    } 
    myFile.close();
    hora_reg = timestamp.substring(11,13).toInt();
    Serial.print("Hora leida del archivo: ");
    Serial.println(hora_reg);
    return hora_reg;

  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
    return 0;
  }
}

bool existPriceToday(char timestamp_now[]) {
  String lineaLeida = "0";
  char mes_sd[3];
  char dia_sd[3];

  char mes_app[3];
  char dia_app[3];

  //1 leer la primera linea del txt
  /* Comprobamos si la SD se encuentra insertada */
  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("La tarjeta microSD fue detectada correctamente.");
    File myFile = SD.open("/precios_hoy.txt", FILE_READ);  //una vez nos aseguramos de que existe la carpeta, abrimos o creamos el archivo en ella si no se encuentra aun
    lineaLeida = myFile.readString();
    Serial.print("Linea leida del txt: ");
    Serial.println(lineaLeida);
    myFile.close();
    //2 extraer el numero de dia del timestamp del txt
    sprintf(mes_sd, "%c%c", lineaLeida[0], lineaLeida[1]);  //obtenemos el mes del timestamp
    sprintf(dia_sd, "%c%c", lineaLeida[3], lineaLeida[4]);  //obtenemos el dia del timestamp

    //3 compararlo con el numero de dia extraido del timestamp que se ha introducido por parametro
    sprintf(mes_app, "%c%c", timestamp_now[0], timestamp_now[1]);  //obtenemos el mes del timestamp
    sprintf(dia_app, "%c%c", timestamp_now[3], timestamp_now[4]);  //obtenemos el dia del timestamp

    //4 devolver si es el mismo o no con un bool
    if ((strcmp(mes_sd, mes_app) == 0) && strcmp(dia_sd, dia_app) == 0) {
      return true;
    }

  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD en la función existPriceToday.");
  }
  return false;
}

void savePriceToday(char timestamp[], char precios_hora[]) {

  /* Comprobamos si la SD se encuentra insertada */
  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("La tarjeta microSD detectada correctamente.");
    File myFile = SD.open("/precios_hoy.txt", FILE_WRITE);  //una vez nos aseguramos de que existe la carpeta, abrimos o creamos el archivo en ella si no se encuentra aun
    /* Para actualizar primero hay que truncar el fichero porque si no se acumulara */
    myFile.seek(0);
    myFile.write(0);
    myFile.seek(0);
    myFile.println(timestamp);
    myFile.println(precios_hora);
    myFile.close();

  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }
}

/* IMPORTANT: BE CAREFUL WITH THE API SYNC AND THE LAST PRICE READING OF THE DAY */

float readPriceNow(uint8_t newHour){ //rangeHourFinished [00 - 23] --> i.e : we want to know the price from 01:00 - 02:00 -> newHour = 2 and we want to know the price before this hour
  String precioLeido = "";
  if (SD.begin(SDCARD_CS_PIN)) { //comprobamos si esta conectada la sd
    File myFile = SD.open("/precios_hoy.txt", FILE_READ); //abrimos el fichero
    uint8_t numLineaLeida = 0;

    while(myFile.available()){ // leemos lineas hasta llegar a la del precio en cuestion
      precioLeido = myFile.readStringUntil('\n'); // read until newline character is encountered
      if(numLineaLeida == (newHour)){ // numLineaLeida = 0 -> DateTime last sync API prices **** numLineaLeida = 1 -> price for 00:00 - 01:00 **** numLineaLeida = 2 -> price for 01:00 - 02:00 ... and so on
        break;
      }
      numLineaLeida++;
    }
  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }

  return precioLeido.toFloat(); //if returns empty string -> check http request or file line index handling
}