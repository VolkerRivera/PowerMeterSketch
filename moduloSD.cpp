#include "moduloSD.h"
#include <SD.h>
#include <SPI.h>

void saveMeasure(char carpeta[], char archivo[], char measurement[]){
  // /carpeta/archivo.txt
  char path[11]; // n = 11
  sprintf(path,"/%s/%s.txt",carpeta,archivo);

  /* Comprobamos si la SD se encuentra insertada */
  if(SD.begin(SDCARD_CS_PIN)){
    Serial.println("La tarjeta microSD detectada correctamente.");
    if(!SD.exists(path)) SD.mkdir(carpeta); //Si no existe dicha direccion creamos la carpeta que la contendra si no existe aun, si no no pasa nada
    File myFile = SD.open(path, FILE_WRITE); //una vez nos aseguramos de que existe la carpeta, abrimos o creamos el archivo en ella si no se encuentra aun
    myFile.println(measurement);
    myFile.close();

  }else{
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }
}
