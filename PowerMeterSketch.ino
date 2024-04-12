/*
 * uMQTTBroker demo for Arduino (C++-style)
 * 
 * The program defines a custom broker class with callbacks, 
 * starts it, subscribes locally to anything, and publishs a topic every second.
 * Try to connect from a remote client and publish something - the console will show this as well.
 */

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "Network.h"

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
  static int last = 0;

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

  if ((millis () - last) > SHOW_TIME_PERIOD) {
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
    
  }
}

String readMeasure (void){
  String toPublish;
  JsonDocument doc;

  //doc["timestamp"] = NTP.getTimeDateStringUs ();
  doc["timestamp"] = NTP.getTimeDateStringForJS();
  doc["Vrms"] = (float)random(0.0, 230.0);
  doc["Irms"] = (float)random(0.0, 5.0);

  serializeJson(doc, toPublish); ///< from Json to String

return toPublish;
}
