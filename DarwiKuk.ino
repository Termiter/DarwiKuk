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
//    DarwiKuk 1.2-3 2018-01-25
//      * Mezi měřeními upadne do hlubokého spánku. Je třeba propojit PIN D0 a RST.
 
 
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
 
// LED_PIN 2 neboli GPI02 neboli D4, interní LED která oznamuje komunikaci a nedostupnost internetu
#define LED_PIN 2
 
// 74800 je rychlost na které NodeMCU komunikuje po startu a vypisuje chybové hlášky, takže je vidět všechno co se děje
#define SER 74880
 
long zacatek = millis(); //uloží si aktuální čas kvůli měření trvání doby běhu skriptu, o ten pak zkrátí čekání do dalšího cyklu
//prodleva mezi měřeními v sekundach
int prodleva = 60 * 5; //5 minut
//int prodleva = 30; //pro testování
int poWiFi = 10 * 1000; //čas na odpojení WiFi před restartem
 
String MAC(12); //proměnná kde se bude nacházet MAC adresa WiFi, používá se pro identifikaci měřidla v databázi
String testconn = "http://iot.darwiniana.cz/testconn.txt"; //stránka obsahuje znak 1 a slouží k otestování připojení
String newData = "http://iot.darwiniana.cz/new_data.php"; //sem posílá data
 
const int pocetMereni = 16; //počet měření kvůli přesnoti, musí být dělitelný 4
float mereni[pocetMereni]; //pro ukládání jednotlivých měření
 
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
//vraci hodnotu měření
float hodnotaMereni() {
  boolean trideno;
  do {
    trideno = false;
    for (int i = 0; i < pocetMereni - 1; i++ ) {
      if (mereni[i] > mereni[i + 1]) {
        long x = mereni[i];
        mereni[i] = mereni[i + 1];
        mereni[i + 1] = x;
        trideno = true;
      }
    }
  } while (trideno);
  float soucet = 0;
  for (int i = pocetMereni / 4; i < pocetMereni - (pocetMereni / 4); i++) {
    soucet += mereni[i];
  }
  return (soucet / (pocetMereni / 2));
}
 
//================================================
void setup() {
  Serial.begin(SER); //nastaví sériovou linku pro debug informace
  Serial.println(); Serial.println(); Serial.println();
 
  pinMode(LED_PIN, OUTPUT); //oznamovací LED
  digitalWrite(LED_PIN, LOW); // hned po startu rozsviti LEDku
 
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
 
  //nepokračuje, dokud není přístupný server pro ukládání údajů
  wifiManager.autoConnect("DarwiKuk");
  HTTPClient http;
  http.begin(testconn);
  boolean inet = false;
  int httpCode = http.GET(); //zavolá stránku pro test dostupnosti serveru
  String payload = http.getString();
  if (payload.charAt(0) == '1') {
    inet = true;
  }
 
  if (!inet) { //nepodařilo se připojit k AP, případně není dostupný server
    WiFi.mode(WIFI_OFF); //vypne WiFi
    delay(poWiFi);//počká poWiFI sekund na vypnutí WiFi
    ESP.deepSleep(1e6);//za jednu sekundu se restartuje
  }
 
  //začneme měřit
  //načte do h vlhkost
  for (int i = 0; i < pocetMereni; i++) {
    mereni[i] = bme.readHumidity();
  }
  float h = hodnotaMereni();
 
  //načte do t teplotu
  for (int i = 0; i < pocetMereni; i++) {
    mereni[i] = bme.readTemperature();
  }
  float t = hodnotaMereni();
 
  //načte tlak
  for (int i = 0; i < pocetMereni; i++) {
    mereni[i] = bme.readPressure();
  }
  float p = hodnotaMereni();
 
  //načte do l osvětlení
  for (int i = 0; i < pocetMereni; i++) {
    sensors_event_t event;
    tsl.getEvent(&event);
    mereni[i] = event.light;
  }
  float l = hodnotaMereni();
 
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
  String data = newData + "?mac=" + MAC + "&t=" + String(t) + "&h=" + String(h) + "&p=" + String(p) + "&l=" + String(l);
  Serial.println(data);
  http.begin(data);
  httpCode = http.GET();
  payload = http.getString();   //odešle data na server, zpět by se měla vrátit 1
  Serial.print("Server vratil: ");
  Serial.println(payload);
  if (payload == "1") {
    Serial.println("Data uspesne zapsana na server.");
  }
 
  else {
    Serial.println("Nepovedlo se připojit se na server. Restart!");
    WiFi.mode(WIFI_OFF); //vypne WiFi
  delay(poWiFi);//počká poWiFI sekund na vypnutí WiFi
    ESP.deepSleep(1e6);//za jednu sekundu se restartuje
  }
 
  http.end();   //uzavře HTTP spojení
  WiFi.mode(WIFI_OFF); //vypne WiFi
  delay(poWiFi);//počká poWiFI sekund na vypnutí WiFi
  digitalWrite(LED_PIN, HIGH); // zhasne LEDku
 
  int spat = prodleva - ((millis() - zacatek ) / 1000);
  Serial.print("Jdu spat na ");
  Serial.print(spat);
  Serial.println(" sekund");
  ESP.deepSleep(spat * 1e6);
}
 
//================================================
void loop() {
  // tady nic není, měření proběhne jen jednou a je ukončeno restartem po stanovené prodlevě
}
