
#include <ArduinoJson.h>
#include "Network.h"
#include "moduloSD.h"
#include <SD.h>
#include "ADEDriver.h"
//#include <TimerEvent.h>
#include <TaskScheduler.h>
#define _TASK_SELF_DESTRUCT     // Enable tasks to "self-destruct" after disable
#define SDCARD_CS_PIN 3

struct EnergyRegs energyVals;  //Energy register values are read and stored in EnergyRegs structure
struct PowerRegs powerVals;    //Metrology data can be accessed from these structures
struct RMSRegs rmsVals;
struct PQRegs pqVals;
struct AcalRegs acalVals;
struct Temperature tempVal;


extern uint8_t numDesconexiones;
extern bool NTPsincronizado;
float acumulador = 0;
bool transmissionFinished = true;
bool mdnsUsed = false;

// funciones a ejecutar
void registerFunction();
void powerFunction();
void sendInfo();

Task powertask(1000, TASK_FOREVER, &powerFunction);
Task graphicstask(250, TASK_FOREVER, &registerFunction);
Task sendtask(0, 1, &sendInfo);

Scheduler organizador;

myMQTTBroker myBroker;  ///< Crea un objeto myMQTTBroker

void setup() {
  pinMode(SDCARD_CS_PIN, OUTPUT); //GPIO3
  WiFi.mode(WIFI_STA);  ///< explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(LED_BUILTIN, OUTPUT);     ///< Pin configurado como salida
  digitalWrite(LED_BUILTIN, HIGH);  ///< LED inicialmente apagado
  Serial.println();
  Serial.println();
  delay(3000);
  Serial.println("\n Starting");

  // ADE config antes de iniciar redes para no gastar tantos recursos
  initADE9153A(); // <<<< si ADE9153A conectada
  autocalibrateADE9153A(); // <<<< si ADE9153A conectada

  if (wm_nonblocking) {  ///< Configura el portal de forma bloqueante o no bloqueante €INIT
    wm.setConfigPortalBlocking(false);
  } else {
    wm.setConfigPortalBlocking(true);
  }

  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str);  // custom html input

  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);


  std::vector<const char*> menu = { "wifi", "info", "param", "sep", "restart", "exit" };  ///<Array de punteros const char*, estilo C++, en C habria que incluir ademas el tamaño
  wm.setMenu(menu);                                                                       ///< Establece el menu configurado anteriormente

  wm.setClass("invert");  ///< Sirve para activar el modo oscuro en el menu de configuración. Equivalente a wm.setDarkMode(true)

  wm.setConfigPortalTimeout(300);  // auto close configportal after n seconds

  bool res;

  //Dependiendo de si encuentra la red guardada o no vuelve a crear el portal o se reconecta 
  res = wm.autoConnect("AutoConnectAP", "password");  // password protected ap |||| SOLO inicia el cuadro de configuracion de red cuando no hay ninguna red guardada

  //res = wm.startConfigPortal("AutoConnectAP","password"); // Inicia siempre el cuadro de configuracion de red

  if (!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  // Iniciar mDNS a direccion esp8266.local
  if (!MDNS.begin("esp8266")) {
    Serial.println("Error iniciando mDNS");
  }
  Serial.println("mDNS iniciado");

  MDNS.addService("mqtt", "tcp", 1883);

  NTP.onNTPSyncEvent([](NTPEvent_t event) {  ///< Callback para cada vez que se hace una sincronizacion
    ntpEvent = event;
    syncEventTriggered = true;
  });

  WiFi.onEvent(onWifiEvent);  //Ahora mismo no hace nada porque WIFIMANAGER es bloqueante

  /* Comienza sincronizacion NTP */
  doSyncNTP();

  delay(1000);

  /* tasks */
  organizador.init();
  organizador.addTask(powertask);
  organizador.addTask(graphicstask);

  powertask.enable();
  graphicstask.enable();

  // Start the broker
  Serial.println("Starting MQTT broker");
  myBroker.init();
  myBroker.subscribe("#");

  delay(1000);
}

void loop() {  //<--------------------- LOOP
  organizador.execute();
  MDNS.update();

  /*
   * Si salta algún evento SNTP se procesa
   */

  if (syncEventTriggered) {
    syncEventTriggered = false;
    processSyncEvent(ntpEvent);
  }
}

void sendInfo(){
  graphicstask.disable();
  File root = SD.open("/"); // abrimos raiz SD
    // Iterar sobre los archivos y directorios en la raíz
    while (File entry = root.openNextFile()) {
      if (entry.isDirectory() && entry.name() != "System Volume Information") {  // Ignorar carpetas del sistema
        String directorioMes = String("/") + entry.name();

        // Iterar sobre los archivos dentro de cada carpeta (mes)
        File file = SD.open(directorioMes);
        while (File diaFile = file.openNextFile()) {
          
          if (!diaFile.isDirectory()) {
            String rutaArchivo = directorioMes + "/" + diaFile.name();
            String dialeido = readDataOfThisDay(rutaArchivo);
            Serial.printf("%s -> %s\n", rutaArchivo, dialeido);
            myBroker.publish("broker/register", dialeido);
            delay(25); // delay para que no se llene la cola
          }
        }
      }
      // No es necesario hacer nada si es un archivo en la raíz, ya que queremos ignorarlos
    }
    transmissionFinished = true;
    sendtask.disable();
    organizador.deleteTask(sendtask);
    graphicstask.enable();
    Serial.println("sendtask disable and deleted");
}

void registerFunction() {  // GESTION GRAPHICS SCREEN
  if (!transmissionFinished) { // = false
    organizador.addTask(sendtask);
    sendtask.enable(); 
    sendtask.restart();
  }
}



void powerFunction() {
  /************************************************************
   *  Lectura de registros de medida y procesado de los mismos
   ************************************************************/
  readandwrite(); // <<<< si ADE9153A conectada

  if (numDesconexiones < 5) {  //< Solo funciona normalmente cuando han habido menos de 6 conexiones
    Serial.println("\n\n\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    String mqttViewToPublish;  //String que contiene lo que se publicara en el topic broker/measure
    String toSave;             //String que contiene el json que se guardara en la microSD en el cambio de hora
    JsonDocument mqttView;  //Servira para dar formato json a las medidas <--- para medidor de potencia
    char* date = NTP.getTimeDateString(time(NULL), "%04Y-%02m-%02d %02H:%02M:%02S");
    String date_reg = readTimestamp_LastMeasure();

    // mes, dia, hora NTP
    char mes[3];
    char dia[3];
    char hora[3];

    // mes, dia de la ultima acumulacion guardada en energyPerSecond.txt
    char mes_reg [3];
    char dia_reg [3];
    uint8_t hora_reg = date_reg.substring(11, 13).toInt();

    sprintf(mes, "%c%c", date[5], date[6]);     //obtenemos el mes
    sprintf(dia, "%c%c", date[8], date[9]);     //obtenemos el dia
    sprintf(hora, "%c%c", date[11], date[12]);  //obtenemos la hora

    sprintf(mes_reg, "%s", date_reg.substring(5, 7));
    sprintf(dia_reg, "%s", date_reg.substring(8, 10));

    Serial.printf("Mes NTP: %s, día NTP: %s , Hora NTP: %s \n", mes, dia, hora);

    if (NTPsincronizado) {
      //se actualiza acumulador con el ultimo valor de energia leido
      float ultima_acumulacion = readEnergy_Accumulation(); // acumulacion guardad en sd [dWh]
      acumulador = ultima_acumulacion + energyVals.ActiveEnergyValue;  //acumulacion guardada en sd + acumulacion ADE ultimo segundo [dWh] // <<<< si ADE9153A conectada
      //acumulador = ultima_acumulacion + (float)random(0, 3) / 100.0;  // <--- ACUMULACION ENERGIA <<<< si ADAE9153A no conectada
      Serial.printf("Ultima acumulacion guardada: %f dWh :: Acumulación último segundo: %f dWh ::  Acumulación actual: %f dWh \n", ultima_acumulacion, acumulador, energyVals.ActiveEnergyValue); // <<<< si ADE9153A conectada
      Serial.printf("Potencia activa: %f W\n", powerVals.ActivePowerValue/1000.0); // <<<< si ADE9153A conectada
      

      mqttView["timestamp"] = date;

      // numeros aleatorios <<<< si ADAE9153A no conectada
      /*mqttView["Vrms"] = serialized(String((float)random(22900, 23000) / 100.0, 2));
      mqttView["Irms"] = serialized(String((float)random(499, 500) / 100.0, 2));
      mqttView["W"] = serialized(String((float)random(114990, 115000) / 100.0, 2));
      mqttView["VAR"] = serialized(String((float)random(114990, 115000) / 100.0, 2));
      mqttView["VA"] = serialized(String((float)random(114990, 115000) / 100.0, 2));
      mqttView["PF"] = serialized(String((float)random(90, 100) / 100.0, 2));
      mqttView["frec"] = serialized("50.00");  */

      mqttView["Vrms"] = serialized(String(rmsVals.VoltageRMSValue/1000.0, 2));
      mqttView["Irms"] = serialized(String(rmsVals.CurrentRMSValue/1000.0, 2));
      mqttView["W"] = serialized(String(powerVals.ActivePowerValue/1000.0, 2));
      mqttView["VAR"] = serialized(String(powerVals.FundReactivePowerValue/1000.0, 2));
      mqttView["VA"] = serialized(String(powerVals.ApparentPowerValue/1000.0, 2));
      mqttView["PF"] = serialized(String(pqVals.PowerFactorValue, 2));
      mqttView["frec"] = serialized(String(pqVals.FrequencyValue, 2));

      serializeJson(mqttView, mqttViewToPublish);  ///< from Json to String

      Serial.println("Número de conexiones hechas: " + (String)numDesconexiones);

      myBroker.publish("broker/measure", mqttViewToPublish);  //<---- Broker publica

      Serial.println("---------------------------------------");  //<---- Muestra clientes mqtt conectados
      myBroker.printClients();
      Serial.println(myBroker.getClientCount());
      Serial.println();
      Serial.println("---------------------------------------");
      // si existe precio para hora NTP
      if (existPriceToday(date)) {  // Funcionamiento deseado <--- Si los precios estan actualizados

        // si la medida se ha hecho dentro del mismo rango de hora que la ultima medida registrada -> valido hasta xx:59:59
        // - seguimos acumulando
        Serial.printf("Hora NTP Server: %s , hora última acumulación: %d \n", hora, hora_reg);
        //yyyy-mm-dd hh:mm:ss <- date_reg
        if (atoi(hora) == hora_reg) {

          //se actualiza el registro de acumulacion
          saveEnergyPerSecond(date, acumulador); // dWh

          Serial.println("NTP sincronizado y precios actualizados. Lectura dentro de la misma hora.");

        } 

      } else {  // si no existe precio para hora NTP -> cambio de hora
          // si no tenemos precios porque no existia ninguno previamente
          // si energyPerSecond.txt no está vacío -> existe una medida correspondiente al ultimo instante de tiempo registrado
          // -> guardamos esa con su date_reg y reseteamos acumulador
          // - reseteamos el registro del acumulador
          // - guardamos medida

          if(!existEnergyPerSecondFile()){ //caso de la primera medida de todas donde no existe archivo de acumulacion ni precio
            // SI ENERGYPERSECOND.TXT NO EXISTE LO CREA Y GUARDA (ademas no exportara ningun consumo [Wh][€])
            saveEnergyPerSecond(date, acumulador);  //se actualiza el registro de acumulacion

          }else{
            // SI EXISTE, GUARDAR LA INFORMACION EN EL FICHERO CORRESPONDIENTE A LA ULTIMA ACUMULACION GUARDADA 
            JsonDocument energyMeasured;
            energyMeasured["amountWh"] = ultima_acumulacion/10; // dWh -> Wh
            energyMeasured["amountEuro"] = ultima_acumulacion * readPrice(hora_reg)/10000.0;  // la hora que se introduce es la actual, el precio que se lee es el de la hora que acaba de terminar
            energyMeasured["dateTime"] = date_reg; 
            serializeJson(energyMeasured, toSave);
            saveEnergyPerHour(mes_reg, dia_reg, toSave);
            acumulador = energyVals.ActiveEnergyValue; // <<<< si ADE9153A conectada
            //acumulador = (float)random(0, 3) / 100.0; // <<<< si ADAE9153A no conectada
            //se actualiza el registro de acumulacion
            saveEnergyPerSecond(date, acumulador);
          }
        Serial.println("NTP sincronizado y precios no actualizados. Lectura en cualquier otro caso.");

        // independientemente de los casos anteriores -> se ejecuta peticion https

        String json;  //JSON que nos devuelve la API de precios

        do {
          json = callAPI();  // https request a la api de precios
        } while (json.isEmpty());

        //se comprueba si esta vacio o no
        if (json.isEmpty()) {
          Serial.println("callAPI no ha devuelto nada");  // <--- Ahora mismo unicamente para debugear errores
        }
        //en caso de no estar vacio se forma JSON
        // from single price json to float
        float precioPorHora[24];
        memset(precioPorHora, 0, sizeof(precioPorHora));
        String precioHoy = "";
        StaticJsonDocument<256> responseJSON;
        DeserializationError error = deserializeJson(responseJSON, json);
        //si ocurre algun error al formar  JSON se reporta
        if (error) {
          Serial.println("deserializeJson() failed: ");
        }
        precioPorHora[atoi(hora)] = responseJSON["price"];
        for (uint8_t i = 0; i < 24; i++) {
          precioHoy = precioHoy + String(precioPorHora[i] / 1000.0, 5) + "\n";
        }
        ///////////////////////////////////////////
        int precioHoyLength = precioHoy.length();
        char precios_hora_array[precioHoyLength + 1];
        precioHoy.toCharArray(precios_hora_array, precioHoyLength + 1);
        savePriceToday(date, precios_hora_array);  //< GUARDA EL COSTE DE LA LUZ / DIA
      }
    }  // Si NTP no sincronizado no se hará nada ya que se necesita sincronizacion para llevar control de precios, medidas y guardado con timestamp
    Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");

  } else {                 // a la quinta desconexion
    numDesconexiones = 0;  //< no es necesario ya que se reseteara y volvera a 0 automaticamente
    ESP.reset();           //Hacemos un reset ya que de otra forma el Broker no volvera a aceptar mas conexiones
  }
}
