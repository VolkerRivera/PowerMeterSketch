/*
 * uMQTTBroker demo for Arduino (C++-style)
 * 
 * The program defines a custom broker class with callbacks, 
 * starts it, subscribes locally to anything, and publishs a topic every second.
 * Try to connect from a remote client and publish something - the console will show this as well.
 */

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

double precioPorHora[24];
char ultimoDiaGuardado[3];
extern uint8_t numDesconexiones;

String readMeasure(void); ///< Simula las medidas de la ADE9153A, las asocia a un timestamp y crea un Json y String que lo contiene

/*
 * Your WiFi config here
 */
//char ssid[] = "DIGIFIBRA-hyZf";     // your network SSID (name)
//char pass[] = "FcfsPxPXHk4E"; // your network password
//bool WiFiAP = false;      // Do yo want the ESP as AP?

myMQTTBroker myBroker; ///< Crea un objeto myMQTTBroker

/*
void startWiFiAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);
  Serial.println("AP started");
  Serial.println("IP address: " + WiFi.softAPIP().toString());
}*/

void setup()
{
  pinMode(3, OUTPUT);
  WiFi.mode(WIFI_STA); ///< explicitly set mode, esp defaults to STA+AP  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode (LED_BUILTIN, OUTPUT); ///< Pin configurado como salida
  digitalWrite (LED_BUILTIN, HIGH); ///< LED inicialmente apagado
  Serial.println();
  Serial.println();
  delay(3000);
  Serial.println("\n Starting");

  if(wm_nonblocking){ ///< Configura el portal de forma bloqueante o no bloqueante €INIT
    wm.setConfigPortalBlocking(false);
  }else{
    wm.setConfigPortalBlocking(true);
  }

  //int customFieldLength = 40;

  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input
  
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);


  std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};///<Array de punteros const char*, estilo C++, en C habria que incluir ademas el tamaño
  wm.setMenu(menu); ///< Establece el menu configurado anteriormente

  wm.setClass("invert"); ///< Sirve para activar el modo oscuro en el menu de configuración. Equivalente a wm.setDarkMode(true)

  wm.setConfigPortalTimeout(300); // auto close configportal after n seconds

  bool res;

  //Dependiendo de si encuentra la red guardada o no vuelve a crear el portal o se reconecta IMPORTANTE CHECKEAR!!!!
  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap |||| SOLO inicia el cuadro de configuracion de red cuando no hay ninguna red guardada

  //res = wm.startConfigPortal("AutoConnectAP","password"); // Inicia siempre el cuadro de configuracion de red

  if(!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
  }

  // Iniciar mDNS a direccion esp8266.local
   if (!MDNS.begin("esp8266")) 
   {             
     Serial.println("Error iniciando mDNS");
   }
   Serial.println("mDNS iniciado");

   MDNS.addService("mqtt", "tcp", 1883);

  // Start WiFi
  /*if (WiFiAP)
    startWiFiAP();
  else
    startWiFiClient();*/

  
  NTP.onNTPSyncEvent ([] (NTPEvent_t event) { ///< Callback para cada vez que se hace una sincronizacion
        ntpEvent = event;
        syncEventTriggered = true;
    });

  WiFi.onEvent (onWifiEvent);//Ahora mismo no hace nada porque WIFIMANAGER es bloqueante?

  /* Sincronizar */
  doSyncNTP();

  delay(1000);

  //initADE9153A(); //Realizamos el setup del ADE9153A

  // Start the broker
  Serial.println("Starting MQTT broker");
  myBroker.init();

/*
 * Subscribe to anything
 */
  myBroker.subscribe("#");
}

int counter = 0;

void loop()
{
  MDNS.update();
  static int last = 0;
  //readandwrite(); //Leemos cada 250 us

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
        processSyncEvent (ntpEvent);
    }

  if(numDesconexiones < 5){
    if ((millis () - last) > SHOW_TIME_PERIOD) { //actualizacion cada segundo
    last = millis ();
    Serial.print (counter); Serial.print (" ");
    Serial.print (NTP.getTimeDateStringUs ()); Serial.print (" ");
    Serial.print ("WiFi is ");
    Serial.println (WiFi.isConnected () ? "connected" : "not connected"); Serial.print (". ");
    Serial.print ("Tiempo ESP8266 encendido: ");
    Serial.print (NTP.getUptimeString ()); Serial.print (" Última sincronización: ");
    Serial.println (NTP.getTimeDateString (NTP.getFirstSyncUs ()));
    Serial.printf ("Memoria RAM libre: %u\n", ESP.getFreeHeap ());
    /*
    * Publish the counter value as String
    */
    myBroker.publish("broker/counter", (String)counter++);
    myBroker.publish("broker/measure", readMeasure());
    Serial.println("---------------------------------------");
    myBroker.printClients();
    Serial.println(myBroker.getClientCount());
    Serial.println();
    Serial.println("---------------------------------------");
    }
  }else{
    numDesconexiones = 0;
    ESP.reset(); //Hacemos un reset ya que de otra forma el Broker no volvera a aceptar mas conexiones
  }
  
}

String readMeasure (void){
  String toPublish; //String que contiene lo que se publicara en el topic
  JsonDocument doc; //Servira para dar formato json a las medidas
  String json; //JSON que nos devuelve la API de precios
  const size_t capacity = JSON_OBJECT_SIZE(24) + 24*JSON_OBJECT_SIZE(7) + 1024;
  StaticJsonDocument<capacity> precioDoc; //JSON solo con los precios
  char* date  = NTP.getTimeDateStringForJS(); //obtenemos el timestamp 
  char mes[3];
  char dia[3];
  sprintf(mes,"%c%c",date[0],date[1]); //obtenemos el mes y dia
  sprintf(dia,"%c%c",date[3],date[4]); //obtenemos el mes y dia
  //double es mas preciso que float, pero ocupa el doble de bytes (8)
  Serial.print("Dia actual: ");
  Serial.print(dia);
  Serial.print(" Ultimo dia guardado: ");
  Serial.println(ultimoDiaGuardado);


  if(strcmp(dia, ultimoDiaGuardado) != 0){
    json = callAPI();
    if(json.isEmpty()){
      Serial.println("callAPI no ha devuelto nada");
    }
    DeserializationError error = deserializeJson(precioDoc, json);
    if (error) {
      Serial.println("deserializeJson() failed: ");
    }
    for(uint8_t i = 0; i<24; i++){
      // Construir la clave para cada hora, por ejemplo "00-01", "01-02", etc.
      char entry[6]; 
      sprintf(entry, "%02d-%02d", i, (i + 1) % 24); //itera entradas de "00-01" a "23-00"

      // Acceder al objeto anidado usando la clave
      JsonObject nested = precioDoc[entry]; // Obtiene el JSON dentro de esa entry

      // Extraer el valor de "price" y almacenarlo en el array
      precioPorHora[i] = nested["price"]; 
      Serial.print(precioPorHora[i]/1000,4); //< Dividimos entre 1000 para pasar el precio de €/MWh a €/kWh
    }
  }

  strcpy(ultimoDiaGuardado, dia);

  doc["timestamp"] = date;
  doc["Vrms"] = serialized(String((float)random(0, 23000)/100.0, 2));
  doc["Irms"] = serialized(String((float)random(0, 500)/100.0, 2));
  doc["W"] = serialized(String((float)random(0, 115000)/100.0, 2));
  doc["VAR"] = serialized(String((float)random(0, 115000)/100.0, 2));
  doc["VA"] = serialized(String((float)random(0, 115000)/100.0, 2));
  doc["PF"] = serialized(String((float)random(0, 100)/100.0, 2));
  /*doc["Vrms"] = serialized(String(rmsVals.VoltageRMSValue, 2));
  doc["Irms"] = serialized(String(rmsVals.CurrentRMSValue, 2));
  doc["W"] = serialized(String(powerVals.ActivePowerValue, 2));
  doc["VAR"] = serialized(String(powerVals.FundReactivePowerValue, 2));
  doc["VA"] = serialized(String(powerVals.ApparentPowerValue, 2));
  doc["PF"] = serialized(String(pqVals.PowerFactorValue, 2));*/

  serializeJson(doc, toPublish); ///< from Json to String
  char measurement [toPublish.length() + 1];
  toPublish.toCharArray(measurement, sizeof(measurement));
  saveMeasure(mes,dia,measurement);

  Serial.println("Número de conexiones hechas: " + (String) numDesconexiones);

  return toPublish;
}




