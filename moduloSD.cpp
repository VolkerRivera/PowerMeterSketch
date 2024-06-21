#include "moduloSD.h"
#include <SD.h>
#include <SPI.h>
#include <string>

// Funcion para calcular el mes de hace seis meses (max. rango de almac. de datos)
uint8_t calcularMesHaceSeis(uint8_t mesActual){
  if (mesActual > 6) {
        return mesActual - 6;
    } else {
        return mesActual + 12 - 6;
    }
}

// Funcion para borrar una carpeta si existe
void borrarCarpeta(const char* ruta) {
    if (SD.exists(ruta)) {
        File dir = SD.open(ruta);
        while (true) {
            File entry = dir.openNextFile();
            if (!entry) {
                // No more files
                break;
            }
            entry.close();
            SD.remove(entry.name());
        }
        dir.close();
        SD.rmdir(ruta);
        Serial.print("Carpeta borrada: ");
        Serial.println(ruta);
    } else {
        Serial.print("La carpeta no existe: ");
        Serial.println(ruta);
    }
}

void saveEnergyPerHour(char carpeta[], char archivo[], String measurement) {

  //formar ruta a borrar
  uint8_t mesHaceSeis = calcularMesHaceSeis(atoi(carpeta));
  char rutaDetele[5];
  sprintf(rutaDetele, "/%02d", mesHaceSeis);

  //borrar carpeta
  borrarCarpeta(rutaDetele);

  // /carpeta/archivo.txt
  char path[11];  // n = 11
  sprintf(path, "/%s/%s.txt", carpeta, archivo);

  /* Comprobamos si la SD se encuentra insertada */
  if (SD.begin(SDCARD_CS_PIN)) {
    
    if (!SD.exists(path)) SD.mkdir(carpeta);  //Si no existe dicha direccion creamos la carpeta que la contendra si no existe aun, si no no pasa nada
    File myFile = SD.open(path, FILE_WRITE);  //una vez nos aseguramos de que existe la carpeta, abrimos o creamos el archivo en ella si no se encuentra aun
    myFile.println(measurement);
    myFile.close();

    Serial.println("Acceso a la SD: Se ha guardado la energia consumida durante la ultima hora.");

  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }
}

String readDataOfThisDay(String filePath){
  String data = "";
  if (SD.begin(SDCARD_CS_PIN)) {
    //Serial.println("La tarjeta microSD detectada correctamente.");
    File myFile = SD.open(filePath, FILE_READ);
    if (myFile) { // Check if file was opened successfully
      // Read timestamp (assuming valid format)
      data = myFile.readString();
      myFile.close();

    } else {
      Serial.println("Error: No se pudo abrir el archivo.");
    }
  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }
  return data;
}

void saveEnergyPerSecond(String timestamp, float energyAcum){
  if (SD.begin(SDCARD_CS_PIN)) {
    
    File myFile = SD.open("energyPerSecond.txt", FILE_WRITE);

    // Truncar el archivo (borrar su contenido)
    myFile.truncate(0);

    myFile.println(timestamp); //linea 0 "%04Y-%02m-%02d %02H:%02M:%02S"
    myFile.println(energyAcum, 20); //linea 1
    myFile.close();
    Serial.println("Acceso a la SD: acumulacion guardada.");

  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }
}

bool existEnergyPerSecondFile(){
  if (SD.begin(SDCARD_CS_PIN)) {
    if (SD.exists("energyPerSecond.txt")) {
      return true;
    } else {
      return false;
  }
  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
    return false;
  }
}

uint8_t readHour_LastMeasure() {
  uint8_t hora_reg = 99; // Default value for empty file

  if (SD.begin(SDCARD_CS_PIN)) {

    File myFile = SD.open("energyPerSecond.txt", FILE_READ);
    if (myFile) { // Check if file was opened successfully
      // Read timestamp (assuming valid format)
      String timestamp = myFile.readStringUntil('\r');
      myFile.close();

      // Extract hour only if data is available
      if (timestamp.length() > 11) {
        hora_reg = timestamp.substring(11, 13).toInt(); // 11 y 12 son las posiciones de los dos digitos de la hora dentro del timestamp empezando a contar desde 0
        Serial.println("Acceso a la SD: se ha leido la hora de la ultima medida.");

      } else {
        Serial.println("El archivo energyPerSecond.txt está vacío o tiene un formato inválido.");
      }
    } else {
      Serial.println("Error: No se pudo abrir el archivo energyPerSecond.txt");
    }
  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }

  return hora_reg;
}

String readTimestamp_LastMeasure(){
  String timestamp = "";
  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("La tarjeta microSD detectada correctamente.");
    File myFile = SD.open("energyPerSecond.txt", FILE_READ);
    if (myFile) { // Check if file was opened successfully
      // Read timestamp (assuming valid format)
      timestamp = myFile.readStringUntil('\r');
      myFile.close();

      Serial.println("Acceso a la SD: se ha leido el timestamp del registro de energia.");

    } else {
      Serial.println("Error: No se pudo abrir el archivo energyPerSecond.txt");
    }
  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }

  return timestamp;
}


float readEnergy_Accumulation() {
  float energy_reg = 0.0; // Initialize with default value (0.0)
  String line;

  if (SD.begin(SDCARD_CS_PIN)) {
    
    File myFile = SD.open("energyPerSecond.txt", FILE_READ);
    if (myFile) { // Check if file was opened successfully
      if (myFile.available()) { // Check if there's data to read
        myFile.readStringUntil('\r'); // Discard first line (timestamp)
        line = myFile.readStringUntil('\r');
        myFile.close();
        energy_reg = line.toFloat(); // Convert only if there's content
        Serial.println("Acceso a la SD: valor de acumulacion leido.");

      } else {
        Serial.println("El archivo energyPerSecond.txt está vacío.");
      }
    } else {
      Serial.println("Error: No se pudo abrir el archivo energyPerSecond.txt");
    }
  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }

  return energy_reg;
}



bool existPriceToday(char timestamp_now[]) {
  String lineaLeida = "0";
  char mes_sd[3];
  char dia_sd[3];
  char hora_sd[3]; //new

  char mes_app[3];
  char dia_app[3];
  char hora_app[3]; //new

  //1 leer la primera linea del txt
  /* Comprobamos si la SD se encuentra insertada */
  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("La tarjeta microSD fue detectada correctamente.");
    File myFile = SD.open("/precios_hoy.txt", FILE_READ);  //una vez nos aseguramos de que existe la carpeta, abrimos o creamos el archivo en ella si no se encuentra aun
    lineaLeida = myFile.readStringUntil('\n'); //< LEE ABSOLUTAMENTE TOOOODO EL FICHERO
    Serial.print("Linea leida del txt: ");
    Serial.println(lineaLeida);
    myFile.close();
    //2 extraer el numero de dia del timestamp del txt
    sprintf(mes_sd, "%c%c", lineaLeida[5], lineaLeida[6]);  //obtenemos el mes del timestamp
    sprintf(dia_sd, "%c%c", lineaLeida[8], lineaLeida[9]);  //obtenemos el dia del timestamp
    sprintf(hora_sd, "%c%c", lineaLeida[11], lineaLeida[12]); //new

    //3 compararlo con el numero de dia extraido del timestamp que se ha introducido por parametro
    sprintf(mes_app, "%c%c", timestamp_now[5], timestamp_now[6]);  //obtenemos el mes del timestamp
    sprintf(dia_app, "%c%c", timestamp_now[8], timestamp_now[9]);  //obtenemos el dia del timestamp
    sprintf(hora_app, "%c%c", timestamp_now[11], timestamp_now[12]); //new

    //4 devolver si es el mismo o no con un bool
    if ((strcmp(mes_sd, mes_app) == 0) && strcmp(dia_sd, dia_app) == 0 && strcmp(hora_sd, hora_app) == 0) { // new
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

float readPrice(uint8_t newHour){ //rangeHourFinished [0, 23] --> i.e : we want to know the price from 01:00 - 02:00 -> newHour = 2 and we want to know the price before this hour
  String precioLeido = "";

  if (SD.begin(SDCARD_CS_PIN)) { //comprobamos si esta conectada la sd
    File myFile = SD.open("/precios_hoy.txt", FILE_READ); //abrimos el fichero
    uint8_t numLineaLeida = 0;

    while(myFile.available()){ // leemos lineas hasta llegar a la del precio en cuestion
      precioLeido = myFile.readStringUntil('\n'); // aqui no hay \r porque lo hemos escrito manualmente con \n
      if(numLineaLeida == (newHour+1)){ // numLineaLeida = 0 -> DateTime last sync API prices **** numLineaLeida = 1 -> price for 00:00 - 01:00 **** numLineaLeida = 2 -> price for 01:00 - 02:00 ... and so on
        break;
      }
      numLineaLeida++;
    }
  } else {
    Serial.println("ATENCIÓN: No se ha detectado la tarjeta microSD.");
  }

  return precioLeido.toFloat(); //if returns empty string -> check http request or file line index handling
}