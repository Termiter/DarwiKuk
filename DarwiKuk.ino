unsigned long zacatek = millis(); //uloží si aktuální čas kvůli měření trvání doby běhu skriptu, o ten pak zkrátí čekání do dalšího cyklu
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
//    DarwiKuk 2.4_10 2018-12-04
int ver = 10;
//      * zpřesňuje hodnoty čidla vlhkosti půdy
 
//WEMOS D1 mini Pro drivery: https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers
 
//prodleva mezi měřeními v sekundach
const unsigned int prodleva = 60 * 5 + 22; //5 minut, +22 korekce
//const unsigned int prodleva = 30; //pro testování
const unsigned int poWiFi = 5 * 1000; //čas na odpojení WiFi před restartem
const String newData = "http://iot.darwiniana.cz/new_data.php"; //sem posílá data
const unsigned int pocetMereni = 16; //počet měření kvůli přesnoti, musí být dělitelný 4
unsigned int pocetOpakovaniGET = 16; //počet pokusů pro odeslání dat na server
const String updateBin = "http://www.darwiniana.cz/soubory/DarwiKuk/DarwiKuk.bin"; //adresa bin souboru pro update FW
const String updateVerze = "http://www.darwiniana.cz/soubory/DarwiKuk/verze.txt"; //adresa txt souboru s číslem aktuální verze
 
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
 
const byte LED_PIN = D4; // LED_PIN 2 neboli GPI02 neboli D4, interní LED která oznamuje komunikaci a nedostupnost internetu
// pro napájení nejde použít PIN D8, protože při startu se na něm může objevit HIGH, což způsobí boot z SD karty
const byte NAPAJENI_1_PIN = D6; //napájení čidla BMP280 a TSL2561
const byte NAPAJENI_2_PIN = D7; //napájení čidla vlhkosti půdy
 
#define SER 115200 // 74800 je rychlost na které NodeMCU komunikuje po startu. Na 115200 vypisuje chybové hlášky, takže je vidět všechno co se děje
 
String MAC(12); //proměnná kde se bude nacházet MAC adresa WiFi, používá se pro identifikaci měřidla v databázi
 
float mereni[pocetMereni]; //pro ukládání jednotlivých měření
 
// WiFiManager zajišťuje přepnutí na AP a vložení SSID a hesla
unsigned int wifiTimeout = 240; // po x sekundách zkusí znovu připojení k internetu, vhodné po výpadku WiFi, standardní hodnota 240s, 4 minuty
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
 
//kapacitní čidlo vlhkosti
const byte sPin = A0;
int s = 0;
 
//================================================
//vrací MAC adrresu WiFi
String macRead() {
  String m = WiFi.macAddress();
  m.remove(2, 1);  m.remove(4, 1);  m.remove(6, 1);  m.remove(8, 1);  m.remove(10, 1);
  return (m);
}
 
//================================================
//vrací sílu signálu RSSI
int RSSI () {
  return(WiFi.RSSI());
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
// místo delay();
void pockat(unsigned long pockat) {
  Serial.print(pockat / 1000); Serial.println(" sekund");
  unsigned long i = millis();
  unsigned long j;
  do {
    j = millis();
    ESP.wdtFeed(); //vynuluje WDT, jinak dojde k WDT resetu
  }
  while (j - i < pockat);
}
 
//================================================
// kontrola verze FW a případně provede update
void kontrolaVerze() {
  HTTPClient http;
  Serial.println("Kontroluji dostupnost nove verze.");
  Serial.print("Bezim na verzi "); Serial.println(ver);
  http.begin(updateVerze);
  Serial.print("Dame serveru na reakci ");
  pockat(poWiFi);//náhradní funkce za delay()
  int httpCode = http.GET();
  String payload = http.getString();
  int novaVerze = payload.toInt();
  if (!novaVerze) {
    Serial.println("Nepodarilo se stahnout informace o dostupne verzi ze serveru.");
  }
  else {
    if (novaVerze > ver) {
      Serial.print("Je tu nova verze "); Serial.println(novaVerze);
      Serial.println("Zahajuji update systemu.");
      ESPhttpUpdate.update(updateBin);
      Serial.println("Z nejakeho duvodu se nepodarilo provest update. Zkusim to tedy priste...");
    }
    else {
      Serial.print("Neni nutny update, na serveru je k dispozici verze "); Serial.println(novaVerze);
    }
  }
  http.end();
}
 
//================================================
void setup() {
  Serial.begin(SER); //nastaví sériovou linku pro debug informace
  Serial.println(); Serial.println(); Serial.println();
 
  pinMode(LED_PIN, OUTPUT); //oznamovací LED
  pinMode(NAPAJENI_1_PIN, OUTPUT); // napájení čidel
  pinMode(NAPAJENI_2_PIN, OUTPUT); // napájení čidel
  digitalWrite(LED_PIN, HIGH); // hned po startu zhasne LEDku, ta bude svítit jen při nastavování WiFi
  digitalWrite(NAPAJENI_1_PIN, LOW); //vypneme napájení
  digitalWrite(NAPAJENI_2_PIN, LOW); //vypneme napájení
 
  //zapneme napájení čidel BMP280 a osvětlení
  digitalWrite(NAPAJENI_1_PIN, HIGH);
  delay(100);
 
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
 
  //začneme měřit
  Serial.println("Zaciname merit.");
  //načte do h vlhkost
  Serial.print("h = ");
  for (int i = 0; i < pocetMereni; i++) {
    mereni[i] = bme.readHumidity();
  }
  float h = hodnotaMereni();
  Serial.println(h);
 
  //načte do t teplotu
  Serial.print("t = ");
  for (int i = 0; i < pocetMereni; i++) {
    mereni[i] = bme.readTemperature();
  }
  float t = hodnotaMereni();
  Serial.println(t);
 
  //načte tlak
  Serial.print("p = ");
  for (int i = 0; i < pocetMereni; i++) {
    mereni[i] = bme.readPressure();
  }
  float p = hodnotaMereni();
  Serial.println(p);
 
  //načte do l osvětlení
  Serial.print("l = ");
  for (int i = 0; i < pocetMereni; i++) {
    sensors_event_t event;
    tsl.getEvent(&event);
    mereni[i] = event.light;
  }
  float l = hodnotaMereni();
  Serial.println(l);
 
  digitalWrite(NAPAJENI_1_PIN, LOW); //vypneme napájení čidel
 
  //načte vlhkost půdy
  digitalWrite(NAPAJENI_2_PIN, HIGH); //zapneme napájení čidla vlhkosti půdy
  delay(500);
  Serial.print ("s = ");
  for (int i = 0; i < pocetMereni; i++) {
    mereni[i] = analogRead(sPin);
  }
  digitalWrite(NAPAJENI_1_PIN, LOW); //vypneme napájení čidla vlhkosti půdy
  int s = hodnotaMereni();
  Serial.println(s);
 
  Serial.println();
 
  //nepokračuje, dokud není připojený na WiFi, případně vyvolá mód pro vložení SSID a hesla
  digitalWrite(LED_PIN, LOW); //rozsvítí LEDku, pokud svítí stále, uživatel ví, že má zadat heslo
  wifiManager.autoConnect("DarwiKuk");
  digitalWrite(LED_PIN, HIGH); //zhasne LEDku
  HTTPClient http;
 
  kontrolaVerze(); //zkontroluje verzi FW a případně provede update
 
  short rssi = RSSI();
  Serial.print("Síla signálu Wi-Fi je: ");
  Serial.print(rssi);
  Serial.print(" dB");
 
 
  //pošle data jako GET
  Serial.println(); Serial.println("Posilam data na server:");
  String data = newData + "?mac=" + MAC + "&t=" + String(t) + "&h=" + String(h) + "&p=" + String(p) + "&l=" + String(l) + "&rssi=" + String(rssi) + "&s=" + String(s) + "&ver=" + ver;
  Serial.println(data);
  boolean zapsano = false;
  do {
    http.begin(data);
    Serial.print("Dame serveru cas na vyrizeni pozadavku ");
    pockat (5 * 1000); //dáme serveru chvilku na odpověď
    int httpCode = http.GET();//odešle data na server, zpět by se měla vrátit 1
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.print("Server vratil: ");
      Serial.println(payload);//načte odeslaná data, měla by tm být 1
      if (payload == "1") {
        Serial.println("Data uspesne zapsana na server.");
        zapsano = true;
      }
    }
    else {
      Serial.print("Chyba http, zbyva "); Serial.print(pocetOpakovaniGET) ; Serial.println(" pokusu ");
      pocetOpakovaniGET--;
    }
    http.end();   //uzavře HTTP spojení
  } while (!zapsano and pocetOpakovaniGET); //opakuj GET dokud se nepovede, nebo dokud nevycerpa pocet pokusu
 
  if (!zapsano) { //když se nepovedlo data odeslat, tak restart
    WiFi.mode(WIFI_OFF); //vypne WiFi
    Serial.print("Nepovedlo se odeslat data na server. Restart za ");
    pockat(poWiFi);//počká poWiFI sekund na vypnutí WiFi
    ESP.deepSleep(1e6);//za jednu sekundu se restartuje
  }
 
  WiFi.mode(WIFI_OFF); //vypne WiFi
  Serial.print("Pro ukonceni WiFi cekam ");
  pockat(poWiFi);//náhradní funkce za delay()
 
  long spat = (prodleva * 1000) - ((millis() - zacatek ));
  if (spat < 1) {
    spat = 1;
  }
  Serial.print("Jdu spat na "); Serial.print(spat / 1000); Serial.println(" sekund");
  ESP.deepSleep(spat * 1000); //tady je to mikrosekundách
}
 
//================================================
void loop() {
  // tady nic není, měření proběhne jen jednou a je ukončeno restartem po stanovené prodlevě
}
