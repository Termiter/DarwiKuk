//    =======================================================
//    =                                                     =
//    =                     DarwiKuk                        =
//    =                                                     =
//    =   https://www.darwiniana.cz/masozravky/iot:start    =
//    =                                                     =
//    =    https://creativecommons.org/licenses/by/3.0/     =
//    =                                                     =
//    =======================================================
//    
//    DarwiKuk 1.0-1 2018-11-10


#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>

// LED_PIN 2 neboli GPI02 neboli D4, interní LED která oznamuje komunikaci a nedostupnost internetu
#define LED_PIN 2

// 74800 je rychlost na které NodeMCU komunikuje po startu a vypisuje chybové hlášky, takže je vidět všechno co se děje
#define SER 74880

String MAC(12); //proměnná kde se bude nacházet MAC adresa WiFi, používá se pro identifikaci měřidla v databázi
String testconn = "http://iot.darwiniana.cz/testconn.txt"; //stránka obsahuje znak 1 a slouží k otestování připojení
String newData = "http://iot.darwiniana.cz/new_data.php"; //sem posílá data

//prodleva mezi měřeními v milisekundách, standardní hodnota je 5 minut - 1000 * 60 * 5
int prodleva = 1000 * 60 * 5;

// WiFiManager zajišťuje přepnutí na AP a vložení SSID a hesla
int wifiTimeout = 240; // po x sekundách zkusí znovu připojení k internetu, vhodné po výpadku WiFi, standardní hodnota 240s, 4 minuty
WiFiManager wifiManager;

// nastavení sběrnice SCL --> D1, SDA --> D2, default I2C PINy u NodeMCU
#include <Wire.h>
#include <Adafruit_Sensor.h> //obecná Adafruit knihovna pro senzory, dále jsou použity Adafruit knihovny

//TSL2561 - čidlo osvětlení
#include <Adafruit_TSL2561_U.h>
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

//BME280 - čidlo teploty, vlhkosti a tlaku
#include <Adafruit_BME280.h>
#define BME280_ADRESA (0x76)
Adafruit_BME280 bme;

//=======================================
//vrací TRUE když je připojení k serveru k dispozici
boolean inetCheck() {
  digitalWrite(LED_PIN, LOW); // rozsviti LEDku
  boolean inet = false;
  HTTPClient http;
  http.begin(testconn);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    if (payload.charAt(0) == '1') {
      inet = true;
      digitalWrite(LED_PIN, HIGH); // zhasne LEDku - pripojeno
    }
  }
  http.end();
  if (!inet and WiFi.status() == WL_CONNECTED) {
    ESP.restart(); //pokud dojde ke ztrate pripojeni, restart
  }
  return (inet);
}

//=======================================
//neodejde, dokud se nepřipojí
void inet() {
  while (!inetCheck()) {
    wifiManager.autoConnect("DarwiKuk");
    //ESP.restart();
  }
}

//================================================
//vrací MAC adrresu WiFi
String macRead() {
  byte mac[6];
  String m(12);
  WiFi.macAddress(mac);
  m = String(mac[5], HEX) + String(mac[4], HEX) + String(mac[3], HEX) + String(mac[2], HEX) + String(mac[1], HEX) + String(mac[0], HEX);
  return (m);
}

//================================================
void setup() {
  Serial.begin(SER); //nastaví sériovou linku pro debug informace
  Serial.println(); Serial.println(); Serial.println();

  pinMode(LED_PIN, OUTPUT); //oznamovací LED

  Wire.begin();

  bme.begin(BME280_ADRESA); // nastaví čidlo teploty, vlhkosti a tlaku

  //nastaví čidlo osvětlení
  tsl.enableAutoRange(true);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);
  tsl.begin();

  MAC = (macRead()); //uloží si MAC adresu

  //nastaví wifiManager pro uživatelské nastavení WiFi
  wifiManager.setAPStaticIPConfig(IPAddress(10, 10, 10, 10), IPAddress(10, 10, 10, 10), IPAddress(255, 255, 255, 0)); //nastaví adresu AP
  wifiManager.setConfigPortalTimeout(wifiTimeout);
}

//================================================
void loop() {
  int cas = millis(); //uloží si aktuální čas

  //nepokračuje, dokud není přístupný server pro ukládání údajů
  inet();

  //načte do h vlhkost
  float h = 0;
  for (int i = 0; i < 5; i++) {
    h += bme.readHumidity();
  }
  h = h / 5;

  //načte do t teplotu
  float t = 0;
  for (int i = 0; i < 5; i++) {
    t += bme.readTemperature();
  }
  t = t / 5;

  //načte tlak
  float p = 0;
  for (int i = 0; i < 5; i++) {
    p += bme.readPressure();
  }
  p = p / 5;


  //načte do l osvětlení
  sensors_event_t event;
  tsl.getEvent(&event);
  int l = event.light;

  //vypíše načtené údaje
  Serial.print("h = ");
  Serial.println(h);
  Serial.print("t = ");
  Serial.println(t);
  Serial.print("p = ");
  Serial.println(p);
  Serial.print("l = ");
  Serial.println(l);

  //pošle data jako GET
    HTTPClient http; //nasteví třídu http
    String data = newData + "?mac=" + MAC + "&t=" + String(t) + "&h=" + String(h) + "&p=" + String(p) + "&l=" + String(l);
    Serial.println(data);
    http.begin(data);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();   //Get the request response payload
      if (payload == "1") {
        Serial.println("Data uspesne zapsana na server.");
      }
      else {
        Serial.println("Neco se pokazilo, data nejsou na serveru.");
      }
    }
    http.end();   //Close connection
  
  //počká stanovený čas, než začne měřit znovu
  while (millis() < (cas + prodleva)) {
    delay(100);
  }

}