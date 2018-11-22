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
//    DarwiKuk 1.4_5 2018-02-22
String ver = "5";
//      * Optimalizace proměnných
//      * Zkrácení čekání před odpojením WiFi na 5 sekund
//      * LEDka svítí krátce jen během připojování WiFi, případně stále, pokud je nastaven mód pro vložení SSID a hesla
//      * Zpřesnění měření času potřebného pro spánek
//      * Nahrazní funkce delay(), která v některých případech dělala WDT reset
//      * Odstranění ověřování dostupnosti serveru, použije se přímo zápis měření
//      * Přidaná prodleva 5 sekund pro zpracování http get dotazu
 
 
//prodleva mezi měřeními v sekundach
unsigned int prodleva = 60 * 5 + 21; //5 minut, +21 korekce
//unsigned int prodleva = 30; //pro testování
unsigned int poWiFi = 5 * 1000; //čas na odpojení WiFi před restartem
String newData = "http://iot.darwiniana.cz/new_data.php"; //sem posílá data
const unsigned int pocetMereni = 16; //počet měření kvůli přesnoti, musí být dělitelný 4
 
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
 
#define LED_PIN 2 // LED_PIN 2 neboli GPI02 neboli D4, interní LED která oznamuje komunikaci a nedostupnost internetu
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
 
//================================================
//vrací MAC adrresu WiFi
String macRead() {
  String m = WiFi.macAddress();
  m.remove(2, 1);  m.remove(4, 1);  m.remove(6, 1);  m.remove(8, 1);  m.remove(10, 1);
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
void setup() {
  Serial.begin(SER); //nastaví sériovou linku pro debug informace
  Serial.println(); Serial.println(); Serial.println();
 
  pinMode(LED_PIN, OUTPUT); //oznamovací LED
  digitalWrite(LED_PIN, HIGH); // hned po startu zhasne LEDku, ta bude svítit jen při nastavování WiFi
 
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
  Serial.println();
 
  //nepokračuje, dokud není připojený na WiFi, případně vyvolá mód pro vložení SSID a hesla
  digitalWrite(LED_PIN, LOW); //rozsvítí LEDku, pokud svítí stále, uživatel ví, že má zadat heslo
  wifiManager.autoConnect("DarwiKuk");
  digitalWrite(LED_PIN, HIGH); //zhasne LEDku
  HTTPClient http;
 
  //pošle data jako GET
  String data = newData + "?mac=" + MAC + "&t=" + String(t) + "&h=" + String(h) + "&p=" + String(p) + "&l=" + String(l) + "&ver=" + ver;
  Serial.println(data);
  http.begin(data);
  Serial.print("Dame serveru cas na vyrizeni pozadavku ");
  pockat (5 * 1000); //dáme serveru chvilku na odpověď
  boolean zapsano = false;
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
    Serial.println("Chyba http");
  }
 
  if (!zapsano) {
    WiFi.mode(WIFI_OFF); //vypne WiFi
    Serial.print("Nepovedlo se připojit se na server. Restart za ");
    pockat(poWiFi);//počká poWiFI sekund na vypnutí WiFi
    ESP.deepSleep(1e6);//za jednu sekundu se restartuje
  }
 
  http.end();   //uzavře HTTP spojení
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
