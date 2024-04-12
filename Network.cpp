#include "Network.h"

/********************************************/
             /* BROKER MQTT */
/********************************************/

bool myMQTTBroker::onConnect(IPAddress addr, uint16_t client_count) {  
  Serial.println(addr.toString()+" connected");
  return true;
}


void myMQTTBroker::onDisconnect(IPAddress addr, String client_id) {
  Serial.println(addr.toString()+" ("+client_id+") disconnected");
}


bool myMQTTBroker::onAuth(String username, String password, String client_id) {
  Serial.println("Username/Password/ClientId: "+username+"/"+password+"/"+client_id);
  return true;
}


void myMQTTBroker::onData(String topic, const char *data, uint32_t length) {  
  char data_str[length+1];
  os_memcpy(data_str, data, length);
  data_str[length] = '\0';
      
  Serial.println("received topic '"+topic+"' with data '"+(String)data_str+"'");
  //printClients();
}

    
// Sample for the usage of the client info methods
void myMQTTBroker::printClients() {
  for (int i = 0; i < getClientCount(); i++) {
    IPAddress addr;
    String client_id;
         
    getClientAddr(i, addr);
    getClientId(i, client_id);
    Serial.println("Client "+client_id+" on addr: "+addr.toString());
  }
}


/********************************************/
             /* SINCRONIZACION NTP */
/********************************************/
const PROGMEM char* ntpServer = "158.227.98.15";
bool wifiFirstConnected = true;
bool syncEventTriggered = false; // True if a time even has been triggered
NTPEvent_t ntpEvent; // Last triggered event

void onWifiEvent(WiFiEvent_t event){
  Serial.printf ("[WiFi-event] event: %d\n", event);
  switch (event) {
    case WIFI_EVENT_STAMODE_CONNECTED:
        Serial.printf ("Conectado a %s. Solicitando dirección IP...\r\n", WiFi.BSSIDstr().c_str());
        break;
    case WIFI_EVENT_STAMODE_GOT_IP:
        Serial.printf ("IP obtenida: %s\r\n", WiFi.localIP().toString().c_str ());
        Serial.printf ("Estado de conexión: %s\r\n", WiFi.status () == WL_CONNECTED ? "Conectado" : "No conectado");
        digitalWrite (LED_BUILTIN, LOW); // Turn on LED
        wifiFirstConnected = true;
        break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
        Serial.printf ("Desconectado de la SSID: %s\n", WiFi.BSSIDstr ().c_str ());
        //Serial.printf ("Reason: %d\n", info.disconnected.reason);
        digitalWrite (LED_BUILTIN, HIGH); // Turn off LED
        //NTP.stop(); // NTP sync can be disabled to avoid sync errors
        WiFi.reconnect ();
        break;
    default:
        break;

  }

}

void processSyncEvent (NTPEvent_t ntpEvent) {
    switch (ntpEvent.event) { ///< Distinción entre eventos para diferente procesado o depuración
        case timeSyncd:
        case partlySync:
        case syncNotNeeded:
        case accuracyError: ///< Información sobre el error de precisión
            Serial.printf ("[NTP-event] %s\n", NTP.ntpEvent2str (ntpEvent));
            break;
        default:
            break;
    }
}

void doSyncNTP(void){
  if (wifiFirstConnected) {
        wifiFirstConnected = false;
        NTP.setTimeZone (TZ_Europe_Madrid);
        NTP.setInterval (NTP_RESYNC_SEC);
        NTP.setNTPTimeout (NTP_TIMEOUT);
        // NTP.setMinSyncAccuracy (5000);
        // NTP.settimeSyncThreshold (3000);
        NTP.begin (ntpServer);
    }
}

/******************************************************/
                    /* WIFI MANAGER */
/******************************************************/

bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(void){
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
}

