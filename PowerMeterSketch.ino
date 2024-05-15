
#include <ArduinoJson.h>
#include "Network.h"
#include "moduloSD.h"
#include "ADEDriver.h"
#define SDCARD_CS_PIN 3



struct EnergyRegs energyVals;  //Energy register values are read and stored in EnergyRegs structure
struct PowerRegs powerVals;    //Metrology data can be accessed from these structures
struct RMSRegs rmsVals;
struct PQRegs pqVals;
struct AcalRegs acalVals;
struct Temperature tempVal;

float precioPorHora[24];
String precioHoy = "";
extern uint8_t numDesconexiones;
extern bool NTPsincronizado;
float acumulador = 0;

String readMeasure(void);  ///< Simula las medidas de la ADE9153A, las asocia a un timestamp y crea un Json y String que lo contiene

/*
 * Your WiFi config here
 */
//char ssid[] = "DIGIFIBRA-hyZf";     // your network SSID (name)
//char pass[] = "FcfsPxPXHk4E"; // your network password
//bool WiFiAP = false;      // Do yo want the ESP as AP?

myMQTTBroker myBroker;  ///< Crea un objeto myMQTTBroker

/*
void startWiFiAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);
  Serial.println("AP started");
  Serial.println("IP address: " + WiFi.softAPIP().toString());
}*/

void setup() {
  pinMode(3, OUTPUT);
  WiFi.mode(WIFI_STA);  ///< explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(LED_BUILTIN, OUTPUT);     ///< Pin configurado como salida
  digitalWrite(LED_BUILTIN, HIGH);  ///< LED inicialmente apagado
  Serial.println();
  Serial.println();
  delay(3000);
  Serial.println("\n Starting");

  if (wm_nonblocking) {  ///< Configura el portal de forma bloqueante o no bloqueante €INIT
    wm.setConfigPortalBlocking(false);
  } else {
    wm.setConfigPortalBlocking(true);
  }

  //int customFieldLength = 40;

  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str);  // custom html input

  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);


  std::vector<const char*> menu = { "wifi", "info", "param", "sep", "restart", "exit" };  ///<Array de punteros const char*, estilo C++, en C habria que incluir ademas el tamaño
  wm.setMenu(menu);                                                                       ///< Establece el menu configurado anteriormente

  wm.setClass("invert");  ///< Sirve para activar el modo oscuro en el menu de configuración. Equivalente a wm.setDarkMode(true)

  wm.setConfigPortalTimeout(300);  // auto close configportal after n seconds

  bool res;

  //Dependiendo de si encuentra la red guardada o no vuelve a crear el portal o se reconecta IMPORTANTE CHECKEAR!!!!
  res = wm.autoConnect("AutoConnectAP", "password");  // password protected ap |||| SOLO inicia el cuadro de configuracion de red cuando no hay ninguna red guardada

  //res = wm.startConfigPortal("AutoConnectAP","password"); // Inicia siempre el cuadro de configuracion de red

  if (!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  initADE9153A(); //<Realizamos el setup del ADE9153A (Incluye reset_ADE9153A())
  autocalibrateADE9153A(); //< Necesario para que no salgan medidas raras

  // Iniciar mDNS a direccion esp8266.local
  if (!MDNS.begin("esp8266")) {
    Serial.println("Error iniciando mDNS");
  }
  Serial.println("mDNS iniciado");

  MDNS.addService("mqtt", "tcp", 1883);

  // Start WiFi
  /*if (WiFiAP)
    startWiFiAP();
  else
    startWiFiClient();*/


  NTP.onNTPSyncEvent([](NTPEvent_t event) {  ///< Callback para cada vez que se hace una sincronizacion
    ntpEvent = event;
    syncEventTriggered = true;
  });

  WiFi.onEvent(onWifiEvent);  //Ahora mismo no hace nada porque WIFIMANAGER es bloqueante?

  /* Sincronizar */
  doSyncNTP();

  delay(1000);

  // Start the broker
  Serial.println("Starting MQTT broker");
  myBroker.init();

  /*
 * Subscribe to anything
 */
  myBroker.subscribe("#");
}

int counter = 0;

void loop() { //<--------------------- LOOP
  MDNS.update();
  static int last = 0;
  readandwrite(); //Leemos cada 250 us

  /*
   * Se ejecuta una sincronizacion SNTP siempre y cuando no se haya hecho previamente un conexión a 
   * red WiFi, tras esta sincronización no se hacen más rexonexiones SNTP hasta que no se vuelva a 
   * conectar/reconectar a una red WiFi de nuevo.  
   */

  doSyncNTP();

  /*
   * Si salta algún evento SNTP se procesa
   */

  if (syncEventTriggered) {
    syncEventTriggered = false;
    processSyncEvent(ntpEvent);
  }

  if (numDesconexiones < 5) { //< Solo funciona normalmente cuando han habido menos de 6 conexiones
    if ((millis() - last) > SHOW_TIME_PERIOD) {  //actualizacion cada segundo
      last = millis();
      Serial.print(counter);
      Serial.print(" ");
      Serial.print(NTP.getTimeDateStringUs());
      Serial.print(" ");
      Serial.print("WiFi is ");
      Serial.println(WiFi.isConnected() ? "connected" : "not connected");
      Serial.print(". ");
      Serial.print("Tiempo ESP8266 encendido: ");
      Serial.print(NTP.getUptimeString());
      Serial.print(" Última sincronización: ");
      Serial.println(NTP.getTimeDateString(NTP.getFirstSyncUs()));
      Serial.printf("Memoria RAM libre: %u\n", ESP.getFreeHeap());

      /*
      * Publish the counter value as String
      */
      myBroker.publish("broker/counter", (String)counter++);
      myBroker.publish("broker/measure", readMeasure()); //< Se ejecuta readMeasure() y se publica el resultado
      Serial.println("---------------------------------------");
      myBroker.printClients();
      Serial.println(myBroker.getClientCount());
      Serial.println();
      Serial.println("---------------------------------------");
    }
  } else {
    numDesconexiones = 0;
    ESP.reset();  //Hacemos un reset ya que de otra forma el Broker no volvera a aceptar mas conexiones
  }
}

String readMeasure(void) { //< Se ejecuta cada segundo

  String toPublish;  //String que contiene lo que se publicara en el topic
  String toSave; //String que contiene el json que se guardara en la microSD
  float preciokWh;
  JsonDocument doc;  //Servira para dar formato json a las medidas
  String json;       //JSON que nos devuelve la API de precios
  const size_t capacity = JSON_OBJECT_SIZE(24) + 24 * JSON_OBJECT_SIZE(7) + 1024;
  StaticJsonDocument<capacity> precioDoc;     //JSON solo con los precios
  char* date = NTP.getTimeDateStringForJS();  //obtenemos el timestamp para medidor potencia en tiempo real
  char* timestamp = NTP.getTimeDateString (time (NULL), "%04Y-%02m-%02d %02H:%02M:%02S");
  char mes[3];
  char dia[3];
  char hora[3];
  //> 05/06/2024 15:17:21 [11][12] corresponden a la hora

  sprintf(mes, "%c%c", date[0], date[1]);  //obtenemos el mes
  sprintf(dia, "%c%c", date[3], date[4]);  //obtenemos el dia
  sprintf(hora, "%c%c", date[11], date[12]);  //obtenemos la hora
  Serial.print("Mes: ");Serial.print(mes);
  Serial.print(" Día: ");Serial.print(dia);
  Serial.print(" Hora: ");Serial.println(hora);
  //double es mas preciso que float, pero ocupa el doble de bytes (8)

  //Se comprueba si la ultima fecha anotada en el txt es la misma que se acaba de sincronizar
  if (!existPriceToday(date)) {
    //En caso de estar sincronizado con el servidor NTP se llama a la API
    if (NTPsincronizado) {
      NTPsincronizado = false; // necesario para que la siguiente medida, entre al else y se reinicie la esp
      //se obtiene el json
      do{
        json = callAPI(); // https request a la api de precios
      }while(json.isEmpty());
      //se comprueba si esta vacio o no
      if (json.isEmpty()) {
        Serial.println("callAPI no ha devuelto nada");
      }
      //en caso de no estar vacio se chopea
      DeserializationError error = deserializeJson(precioDoc, json);
      //si ocurre algun error al chopear se reporta
      if (error) {
        Serial.println("deserializeJson() failed: ");
      }
      //se extraen los 24 precios que contiene el json
      for (uint8_t i = 0; i < 24; i++) {
        // Construir la clave para cada hora, por ejemplo "00-01", "01-02", etc.
        char entry[6];
        //sprintf(entry, "%02d-%02d", i, (i + 1) % 24);  //itera entradas de "00-01" a "23-24"
        sprintf(entry, "%02d-%02d", i, (i + 1));
        // Acceder al objeto anidado usando la clave
        JsonObject nested = precioDoc[entry];  // Obtiene el JSON dentro de esa entry

        // Extraer el valor de "price" y almacenarlo en el array de 24 doubles precioPorHora
        precioPorHora[i] = nested["price"];
        //A su vez, concatenamos estos valores para diferentes lineas con 5 decimalaes
        precioHoy = precioHoy + String(precioPorHora[i]/1000, 5) + "\n";
      }
      int precioHoyLength = precioHoy.length();
      char precios_hora_array[precioHoyLength + 1];
      precioHoy.toCharArray(precios_hora_array, precioHoyLength + 1);
      /* 1 LINEA: TIMESTAMP 
       2,3,4.. : Precio_hora[i]*/
      savePriceToday(date, precios_hora_array); //< GUARDA EL COSTE DE LA LUZ / DIA
      ESP.reset(); //Reset ya que el broker es incompatible con la peticion https

    } else {
      Serial.println("No existe el precio para hoy pero NTP no sincronizado.");

    }
  } else { // existen los precios para hoy
    Serial.println("Precios diarios: actualizados.");
    // cuando hay un cambio en la hora respecto a la anterior registrada
    preciokWh = readPriceNow(3);
  }

  //El json es local, luego se puede pasar a string 2 veces?

  doc["timestamp"] = timestamp;

  /* Esta version contiene menos cosas y se usa para MQTT*/
  doc["Vrms"] = serialized(String((float)random(0, 23000) / 100.0, 2));
  doc["Irms"] = serialized(String((float)random(0, 500) / 100.0, 2));
  doc["W"] = serialized(String((float)random(0, 115000) / 100.0, 2));
  doc["VAR"] = serialized(String((float)random(0, 115000) / 100.0, 2));
  doc["VA"] = serialized(String((float)random(0, 115000) / 100.0, 2));
  doc["PF"] = serialized(String((float)random(0, 100) / 100.0, 2));
  

  /*
  doc["Vrms"] = serialized(String(rmsVals.VoltageRMSValue/1000, 2));
  doc["Irms"] = serialized(String(rmsVals.CurrentRMSValue/1000, 2));
  doc["W"] = serialized(String(powerVals.ActivePowerValue/1000, 2));
  doc["VAR"] = serialized(String(powerVals.FundReactivePowerValue/1000, 2));
  doc["VA"] = serialized(String(powerVals.ApparentPowerValue/1000, 2));
  doc["PF"] = serialized(String(pqVals.PowerFactorValue, 2));*/

  /*  1. leer registro de energia cada 1.024 seg
      2. acumular energia
      3. cuando se cambie de hora guardamos los siguientes datos en formato json:  
        - amountKWh = acumulador;
        - amountEuro = acumulador * precio;
        - timestamp = DateTime format -> DateTime.parse('1969-07-20 20:18:04Z');

      -> 24 lineas/dia * 31 dias * 6 meses = 

  */

  serializeJson(doc, toPublish);  ///< from Json to String
  //char measurement[toPublish.length() + 1];
  //toPublish.toCharArray(measurement, sizeof(measurement)); //< el broker publica toPublish luego no deberia hacer falta pasarlo a char[]

  /* Esta version contiene mas parametros y se guarda en la microSD */
  /* Se debería guardar una medida cada hora, acumulador de energia en la SD ??*/
  doc["frec"] = serialized("50.00");
  doc["Precio"] = serialized(String(precioPorHora[atoi(hora)]/1000.0, 5));
  serializeJson(doc, toSave);  ///< from Json to String
  char data[toSave.length() + 1];
  toSave.toCharArray(data, sizeof(data));
  saveEnergyPerHour(mes, dia, data); // Se guarda la energia consumida durante cada hora en un dia en ./mes/dia.txt

  Serial.println("Número de conexiones hechas: " + (String)numDesconexiones);

  return toPublish;
}
