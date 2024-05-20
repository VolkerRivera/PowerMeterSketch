#ifndef __NETWORK_H
#define __NETWORK_H

#include "uMQTTBroker.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESP8266WiFi.h>
#include <ESPNtpClient.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecureBearSSL.h>

/* MACROS */
#define PERIOD_POWER 1500 ///< Muestra la hora cada 5 segundos en el loop()
#define PERIOD_GRAPHICS 250
#define NTP_TIMEOUT 5000 ///< Tiempo que tiene el servidor NTP para responder
#define NTP_RESYNC_SEC 30

/* SINCRONIZACION NTP */
extern const PROGMEM char* ntpServer;
extern bool wifiFirstConnected;
extern bool syncEventTriggered; // True if a time event has been triggered
extern NTPEvent_t ntpEvent; // Last triggered event

void doSyncNTP(void);
void onWifiEvent(WiFiEvent_t event); ///< Permite saber los datos principales tras un intento de conexion
void processSyncEvent (NTPEvent_t ntpEvent); ///< Handler para cada sincronización NTP

/* WIFI MANAGER */

extern bool wm_nonblocking; // change to true to use non blocking

extern WiFiManager wm; // global wm instance
extern WiFiManagerParameter custom_field; // global param ( for non blocking w params )

String getParam(String name);
void saveParamCallback(void);

/* MQTT Broker */
class myMQTTBroker: public uMQTTBroker {
public:
  virtual bool onConnect(IPAddress addr, uint16_t client_count);
  virtual void onDisconnect(IPAddress addr, String client_id);
  virtual bool onAuth(String username, String password, String client_id);
  virtual void onData(String topic, const char *data, uint32_t length);
  virtual void printClients();
};

/* HTTPS */
String callAPI(void);

#endif /* __NETWORK_H*/