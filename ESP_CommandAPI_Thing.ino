#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <CommandHandler.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

#define WIFI_AP ""
#define WIFI_PASSWORD ""

String TOKEN = "8h4reFk42WTPM6HowTOQ";  //9jPma5EpXY0blSVNot3P
char thingsboardServer[] = "mqtt.thingcontrol.io";

static const char *fingerprint PROGMEM = "69 E5 FE 17 2A 13 9C 7C 98 94 CA E0 B0 A6 CB 68 66 6C CB 77";
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long period = 1000;  //the value is a number of milliseconds
CommandHandler cmdHdl("=", '\n');

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;
String downlink = "";

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {

  Serial.begin(9600);

  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("@Thingcontrol.io")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, 8883 );
  client.setCallback(callback);
  reconnectMqtt();

  cmdHdl.addCommand("AT", ATcheck);
  cmdHdl.addCommand("AT&MAC",   getMac);
  cmdHdl.addCommand("AT+MAC", processToken);
  cmdHdl.addCommand("AT&V",     viewActive);
  cmdHdl.addCommand("AT+SETWiFi", setWiFi);
  cmdHdl.addCommand("AT+ATTSEND", processAtt);
  cmdHdl.addCommand("AT+CALLED", processCall);
  cmdHdl.addCommand("AT+TELSEND", processTele);
  cmdHdl.addCommand("AT?", commandHelp);
  cmdHdl.setDefaultHandler(unrecognized);

  cmdHdl.setCmdHeader("+");
  cmdHdl.initCmd();
  cmdHdl.addCmdString("Boot..");
  cmdHdl.addCmdString("OK..");
  cmdHdl.addCmdString("RDY!!");
  cmdHdl.addCmdTerm();

  Serial.println(cmdHdl.getOutCmd());
  cmdHdl.sendCmdSerial();
  Serial.println();
    startMillis = millis();  //initial start time

}

void loop()
{
  status = WiFi.status();
  if ( status == WL_CONNECTED)
  {
    if ( !client.connected() )
    {
      reconnectMqtt();
    }
    client.loop();
  }

  cmdHdl.processSerial(Serial);
   currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (currentMillis - startMillis >= period)  //test whether the period has elapsed
  {
    processCall();
    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }
  
}

void ATcheck()
{
  Serial.println("OK");
}

void getMac()
{
  Serial.println("OK");
  Serial.print("+TOKEN: ");
  Serial.println(WiFi.macAddress());
}

void viewActive()
{
  Serial.println("OK");
  Serial.print("+:WiFi, ");
  Serial.println(WiFi.status());
  if (client.state() == 0)
  {
    Serial.print("+:MQTT, [CONNECT] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
  else
  {
    Serial.print("+:MQTT, [FAILED] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
}

void setWiFi()
{
  Serial.println("OK");
  Serial.println("+:Reconfig WiFi  Restart...");
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  if (!wifiManager.startConfigPortal("@ThingControl.io"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  Serial.println("OK");
  Serial.println("+:WiFi Connect");

  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, 8883 );
  client.setCallback(callback);
  reconnectMqtt();
}

void processCall()
{
  Serial.println("OK");
  Serial.print("+:");
  Serial.println(downlink);
}

void commandHelp()
{
  Serial.println("OK");
  Serial.println("\"AT&MAC\" :Get Token");
  Serial.println("\"AT+MAC\" :Set Token Ex. AT+MAC=9jPma5EpXY0blSVNot3P");
  Serial.println("\"AT&V\"   :View active configuration");
  Serial.println("\"AT+SETWiFi\"  :Redirect to AP WiFi Seting mode");
  Serial.println("\"AT+ATTSEND\"  :Send JSON to server topic v1/devices/me/attributes");
  Serial.println("\"AT+CALLED\"   :Get JSON Call back");
  Serial.println("\"AT+TELSEND\"  :Send JSON to server topic v1/devices/me/telemetry");
}

void processAtt()
{
  char *aString;

  aString = cmdHdl.readStringArg();
  if (cmdHdl.argOk) {
    Serial.println("OK");
    Serial.print("+:topic v1/devices/me/attributes , ");
    Serial.println(aString);
  }
  else {
    Serial.println("ERROR");
  }
  client.publish( "v1/devices/me/attributes", aString);
}

void processTele()
{
  char *aString;

  aString = cmdHdl.readStringArg();
  if (cmdHdl.argOk) {
    Serial.println("OK");
    Serial.print("+:topic v1/devices/me/telemetry , ");
    Serial.println(aString);
  }
  else {
    Serial.println("ERROR");
  }
  client.publish( "v1/devices/me/telemetry", aString);
}

void processToken()
{
  char *aString;

  aString = cmdHdl.readStringArg();
  if (cmdHdl.argOk) {
    Serial.println("OK");
    Serial.print("+:TOKEN , ");
    Serial.println(aString);
    TOKEN = aString;
  }
  else {
    Serial.println("ERROR");
  }
  reconnectMqtt();
}

void unrecognized(const char *command)
{
  Serial.println("ERROR");
}

void callback(char* topic, byte* payload, unsigned int length)
{
  /*Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    }
    Serial.println();*/

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  /*Serial.print("  Topic: ");
    Serial.println(topic);
    Serial.print("  Message: ");
    Serial.println(json);*/

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  String methodName = String((const char*)data["method"]);

  if (methodName.equals("setValue"))
  {
    char json[length + 1];
    strncpy (json, (char*)payload, length);
    json[length] = '\0';

    downlink = json;
  }
  else
  {
    downlink = "";
  }
}

void reconnectMqtt()
{
  if ( client.connect("Thingcontrol_AT", TOKEN.c_str(), NULL) )
  {
    Serial.println( "Connect MQTT Success." );
    client.subscribe("v1/devices/me/rpc/request/+");
  }
  /*else
    {
    Serial.print( "[FAILED] [ rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
    }*/
}
