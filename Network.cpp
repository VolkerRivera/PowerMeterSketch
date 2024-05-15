#include "Network.h"
#include <memory>
uint8_t numDesconexiones = 0;
/********************************************/
/* BROKER MQTT */
/********************************************/

bool myMQTTBroker::onConnect(IPAddress addr, uint16_t client_count) {

  Serial.println(addr.toString() + " connected");
  return true;
}


void myMQTTBroker::onDisconnect(IPAddress addr, String client_id) {
  numDesconexiones++;
  Serial.println(addr.toString() + " (" + client_id + ") disconnected");
}


bool myMQTTBroker::onAuth(String username, String password, String client_id) {
  Serial.println("Username/Password/ClientId: " + username + "/" + password + "/" + client_id);
  return true;
}


void myMQTTBroker::onData(String topic, const char* data, uint32_t length) {
  char data_str[length + 1];
  os_memcpy(data_str, data, length);
  data_str[length] = '\0';

  Serial.println("received topic '" + topic + "' with data '" + (String)data_str + "'");
  //printClients();
}


// Sample for the usage of the client info methods
void myMQTTBroker::printClients() {
  for (int i = 0; i < getClientCount(); i++) {
    IPAddress addr;
    String client_id;

    getClientAddr(i, addr);
    getClientId(i, client_id);
    Serial.println("Client " + client_id + " on addr: " + addr.toString());
  }
}


/********************************************/
/* SINCRONIZACION NTP */
/********************************************/
const PROGMEM char* ntpServer = "158.227.98.15";
bool wifiFirstConnected = true;
bool syncEventTriggered = false;  // True if a time even has been triggered
NTPEvent_t ntpEvent;              // Last triggered event
void onWifiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.printf("Conectado a %s. Solicitando dirección IP...\r\n", WiFi.BSSIDstr().c_str());
      break;
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.printf("IP obtenida: %s\r\n", WiFi.localIP().toString().c_str());
      Serial.printf("Estado de conexión: %s\r\n", WiFi.status() == WL_CONNECTED ? "Conectado" : "No conectado");
      digitalWrite(LED_BUILTIN, LOW);  // Turn on LED
      wifiFirstConnected = true;
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.printf("Desconectado de la SSID: %s\n", WiFi.BSSIDstr().c_str());
      //Serial.printf ("Reason: %d\n", info.disconnected.reason);
      digitalWrite(LED_BUILTIN, HIGH);  // Turn off LED
      //NTP.stop(); // NTP sync can be disabled to avoid sync errors
      WiFi.reconnect();
      break;
    default:
      break;
  }
}

bool NTPsincronizado = false;

void processSyncEvent(NTPEvent_t ntpEvent) {
  switch (ntpEvent.event) {  ///< Distinción entre eventos para diferente procesado o depuración
    case timeSyncd:
    case partlySync:
      NTPsincronizado = true;
    case syncNotNeeded:
    case accuracyError:  ///< Información sobre el error de precisión
      Serial.printf("[NTP-event] %s\n", NTP.ntpEvent2str(ntpEvent));
      break;
    default:
      break;
  }
}

void doSyncNTP(void) {
  if (wifiFirstConnected) {
    wifiFirstConnected = false;
    NTP.setTimeZone(TZ_Europe_Madrid);
    NTP.setInterval(NTP_RESYNC_SEC);
    NTP.setNTPTimeout(NTP_TIMEOUT);
    // NTP.setMinSyncAccuracy (5000);
    // NTP.settimeSyncThreshold (3000);
    NTP.begin(ntpServer);
  }
}

/******************************************************/
/* WIFI MANAGER */
/******************************************************/

bool wm_nonblocking = false;  // change to true to use non blocking

WiFiManager wm;                     // global wm instance
WiFiManagerParameter custom_field;  // global param ( for non blocking w params )

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(void) {
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
}

/******************************************************/
/* HTTP GET */
/******************************************************/

/*
const char IRG_Root_X1 [] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIGNDCCBRygAwIBAgISA/NVy5OWAsL0Uy2cxrAlkpX6MA0GCSqGSIb3DQEBCwUA
MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD
EwJSMzAeFw0yNDA0MTcwMjEyNThaFw0yNDA3MTYwMjEyNTdaMBwxGjAYBgNVBAMT
EXByZWNpb2RlbGFsdXoub3JnMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKC
AgEAnKQ+eVwrkc9Ahu4D5YZw3LC/ftw7vO2Pla+IdHy0iryASUHjXYshuQllfGe4
himtqp3suBIn8KvOOXrMmpe7AdxQ0AkqdpSJnfJdRTzZ9u2fAtDR5P3ae/OgFBqg
KMe0DwC1JzAIzK6q3tBwkjJ6sD1W3jINQHpA7FWdMPDQ9zmI1aK7zevpwSntE1pC
C3kaGp6mpV+nbGbqWNilcwe20aAQl8ShU6w0HnoBfuGvGHoviGh/6BZzTwxyvnC2
gmDoWEe8RVzhOvl4UqkIfmim2XO+tUAu11iz/dZJEiiEeKUJ090bP4WF15Qx5kmM
R27wayYCVlJ1WSR5QXGUkR9meBidMDmzMvwRojFHcKk8nMvnlnwm8bon+LPQGIdw
n1PJq5tU5/wiD08zV3b7QtF9K1mUA4hTBTfOG4YkydjPXNjkEgZz1OgRdcjf3Hbq
Q3txPse8FU5zpzEq2NO/6Juf0EAb8AhtwgZ4s8TgRLKJDvlxdU8G3CtomOLEL4aY
qMdmld9uDYBD0G8YqoniGBiv+HcHFDkG6UupbZDOh4h+y1TcRuXszZM4jWQ+dqo0
broyYDxVhKw+iOQ9/rPaTvdyMQKP2B5wx9RW4XzHkQ6WzoVyyDHucyjlGLMUTMvL
975p9wNK2+7InQPnqy45YxmrkaN5UpzWz22y9qv+jJGRLH0CAwEAAaOCAlgwggJU
MA4GA1UdDwEB/wQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIw
DAYDVR0TAQH/BAIwADAdBgNVHQ4EFgQUWN5lYgwcYCgQtSEj2jsunqaakSIwHwYD
VR0jBBgwFoAUFC6zF7dYVsuuUAlA5h+vnYsUwsYwVQYIKwYBBQUHAQEESTBHMCEG
CCsGAQUFBzABhhVodHRwOi8vcjMuby5sZW5jci5vcmcwIgYIKwYBBQUHMAKGFmh0
dHA6Ly9yMy5pLmxlbmNyLm9yZy8wYQYDVR0RBFowWIIVYXBpLnByZWNpb2RlbGFs
dXoub3JnghVhcHAucHJlY2lvZGVsYWx1ei5vcmeCEXByZWNpb2RlbGFsdXoub3Jn
ghV3d3cucHJlY2lvZGVsYWx1ei5vcmcwEwYDVR0gBAwwCjAIBgZngQwBAgEwggEE
BgorBgEEAdZ5AgQCBIH1BIHyAPAAdwBIsONr2qZHNA/lagL6nTDrHFIBy1bdLIHZ
u7+rOdiEcwAAAY7qCxRxAAAEAwBIMEYCIQDCVKROCeK1f+TCCXI+HVMb5IXJTsEJ
GepaBo+haqQHDAIhANTn3PdbW2RsRq8vTF8Exu6iDZR9yHzcVK6VdPopR7FfAHUA
O1N3dT4tuYBOizBbBv5AO2fYT8P0x70ADS1yb+H61BcAAAGO6gsUfAAABAMARjBE
AiAEAhVtNw4dD37kG0ClymNGbwjSFOz1jxu4oiywvX6ayAIgWzlf4kiArAbaAU4Q
BoGhXkkcPmXI9KWe89i32I8U3EIwDQYJKoZIhvcNAQELBQADggEBADOqr24al2aR
ZY6PFoadleC+a/vFfcJbZGqCb+iCIR4Z5cPj7T0s55iBPXXngkB8fu0BotAcI0yp
kKsYP1vs43+q5TCX3QRK7IT+vwBsdtNThwq70OV/3JK7ZQ84dTtP6K+fuslEVJuy
mr2rI7fa2IoJfDqKpQzakwp6sLgpjSH2yEo17LFBNI9ox/Vn6aNg31Gf9H3Q/CI9
lwrVOoQpGnvKtDnwsK6QPYx4JpPCioSG9BQKa+zVRrTng3APbZcmEyLodOQjlh+i
ao6YUD93NXEc3l+NXZqLk6OZfOnL8O7iplkHfLiZIenpwaagWN8brzsAFe9J6DX6
scxqrxeHfcA=
-----END CERTIFICATE-----
)CERT";

X509List cert(IRG_Root_X1);*/

/* MUY IMPORTANTE */
/* Es imprescindible que tanto el certificado como la lista no sean compiladas porque si no dará error */

String callAPI(void) {  //No funciona con el certificado
  String payload = "";

  if (WiFi.status() == WL_CONNECTED) {
    
    /* Necesario hacer una peticion segura para poder relacionar las medidas de consumo de energía por hora al precio de la energía por hora */
    
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    const String serverURL = "https://api.preciodelaluz.org/v1/prices/all?zone=PCB";

    client->setInsecure();
    HTTPClient https;
    
    https.begin(*client, serverURL); // String -> const char[]
    int httpCode = https.GET();

    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        payload = https.getString();
        https.end();  //Liberamos recursos
        Serial.print("Se ha obtenido la siguiente respuesta del servidor: ");
        Serial.println(payload);
        return payload;
        
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();  //Liberamos recursos

  } else {
    Serial.println("Error: WiFi desconectado");
  }
  return payload;
}
