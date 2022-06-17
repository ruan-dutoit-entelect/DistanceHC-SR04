#include <Arduino.h>
#include <Ultrasonic.h>
#include <Arduino.h>
#include <Ethernet.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <avr/pgmspace.h>
#include <PubSubClient.h>

#define   trigPin     D5
#define   echoPin     D7
#define   ledRed      D2
#define   ledBlue     D1
#define   ledGreen    D4
unsigned long timeOut = 20000UL;

#define SECRET_SSID "du_Toit" // Your network SSID (name)    
#define SECRET_PASS "Master101!" // Your network password
const char *host = "DistanceInterface";
const char *mqtt_server = "192.168.0.109";
long lastReconnectAttempt = 0;
bool clientConnected = false;
bool enableWritingToClient = true;
unsigned int distanceCm = 0;

Ultrasonic ultrasonic1(trigPin, echoPin, timeOut);	// An ultrasonic sensor HC-04
int port = 8888; // Port number
WiFiServer server(port);
WiFiClient wifiClient;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setupWifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SECRET_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(SECRET_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // server.on("/", handleRoot); //Which routine to handle at root location. This is display page
  server.begin();
  Serial.println("Web server started!");
}

void setupOta()
{
  Serial.println("setupOta");

  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
    Serial.println("onStart");
    enableWritingToClient = false;
  });

  ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
    Serial.println("onEnd");
  });

  ArduinoOTA.onError([](ota_error_t error)
                     {
                       (void)error;
                       Serial.println("onError");
                       ESP.restart(); });

  /* setup the OTA server */
  ArduinoOTA.begin();
  Serial.println("Ready");
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqttSetup()
{
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  lastReconnectAttempt = 0;
}

void setupWatchdog()
{
  wdt_disable();  /* Disable the watchdog and wait for more than 2 seconds */
  delay(3000);  /* Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */
  wdt_enable(WDTO_4S);  /* Enable the watchdog with a timeout of 2 seconds */
}

void setup() {
  Serial.begin(9600);
  setupWifi();
  pinMode(ledRed, OUTPUT);
  pinMode(ledBlue, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  mqttSetup();
  setupOta();
  setupWatchdog();
}

void mqttReconnect()
{
  if (!mqttClient.connected())
  {
    long now = millis();
    if (now - lastReconnectAttempt > 10000)
    {
      lastReconnectAttempt = now;
      if (mqttClient.connect("DistanceNode", "ruan", "ruan"))
      {
        Serial.println("Mqtt connected");
        mqttClient.publish("mqtt/distance", "hello world");
        // mqttClient.subscribe("inTopic");
      }
    }
  }
  else
  {
    lastReconnectAttempt = 0;
  }
}

void loopSocket()
{
  if (clientConnected == false)
  {
    wifiClient = server.available();

    if (wifiClient)
    {
      if (wifiClient.connected())
      {
        Serial.println("Client Connected");
        clientConnected = true;
      }
    }
  }
  else
  {
    if (wifiClient.connected())
    {
      if (wifiClient.available() > 0)
      {
        // read data from the connected wifiClient
        int readChar = wifiClient.read();
        wifiClient.write(readChar);
        if (readChar == 'q')
        {
          wifiClient.stop();
          Serial.println("Client disconnected");
          clientConnected = false;
        }
        if (readChar == 'd')
        {
          Serial.println("Stop writing");
          enableWritingToClient = false;
        }
        if (readChar == 'p')
        {
          Serial.println("Hello world");
          mqttClient.publish("mqtt/dsc", "hello world");
        }
      }
    }
  }
}


int toggle = 1;
unsigned int relativeDistance = 0; 
void loop() {
  ArduinoOTA.handle();
  Serial.print("Sensor 01: ");
  distanceCm = ultrasonic1.read();
  Serial.print(distanceCm); // Prints the distance on the default unit (centimeters)
  Serial.println("cm");
  toggle = toggle ^ 1;
  if (distanceCm >= 200 && distanceCm <= 357)
  {
    relativeDistance = (distanceCm - 200) * 255 / (357 - 200);
    analogWrite(ledRed, 256 - relativeDistance);
    analogWrite(ledBlue, 0);
    analogWrite(ledGreen, 0);
  }
  else if (distanceCm < 200 && distanceCm > 100)
  {
    relativeDistance = (distanceCm - 100) * 255 / 100;
    analogWrite(ledBlue, 256 - relativeDistance);
    analogWrite(ledRed, 0);
    analogWrite(ledGreen, 0);
  }
  else if (distanceCm < 100)
  {
    relativeDistance = distanceCm * 255 / 100;
    analogWrite(ledGreen, 256 - relativeDistance);
    analogWrite(ledBlue, 0);
    analogWrite(ledRed, 0);
  }

  if (clientConnected == true && enableWritingToClient == true)
  {
    char distanceString[5] = {0};
    itoa(distanceCm,distanceString,10);
    wifiClient.write(distanceString);
    wifiClient.write("cm\r\n");
  }
  mqttReconnect();
  mqttClient.loop();
  loopSocket();
  wdt_reset();  /* Reset the watchdog */
  delay(1000);
}