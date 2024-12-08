/* 
  GPIO  3 - DS18B20
  GPIO  4 - przycisk
  GPIO  5 - kadencja
  GPIO  6 - prędkość
  GPIO  8 - I2C SDA
  GPIO  9 - I2C SCL
  GPIO 10 - światło dzienne
  GPIO 11 - światło zwykłe
  GPIO 12 - światło tył
  GPIO 21 - LED PCB WS2812
*/

#define DEBUG 1 // 1 - włączone debugowanie, 0 - wyłączone debugowanie

// --- Biblioteki ---
#include <Wire.h>                 // Komunikacja I2C (do komunikacji z wyświetlaczem OLED i RTC)
#include <RTClib.h>               // Obsługa zegara czasu rzeczywistego (RTC) DS3231
#include <OneWire.h>              // Komunikacja 1-Wire (do obsługi czujnika temperatury DS18B20)
#include <DallasTemperature.h>    // Obsługa czujników temperatury DS18B20
#include <EEPROM.h>               // Zapisywanie i odczytywanie danych z pamięci EEPROM

#include <Arduino.h>              // Podstawowe funkcje i definicje dla Arduino
#include <CircularBuffer.hpp>     // Biblioteka do obsługi bufora cyklicznego (FIFO)

// --- Bluetooth --- 
#include <BLEDevice.h>            // Obsługa urządzeń Bluetooth Low Energy (BLE)
#include <BLEClient.h>            // Obsługa klienta BLE do komunikacji z innymi urządzeniami
#include <BLEAddress.h>           // Obsługa adresów BLE do identyfikacji urządzeń

// --- Serwer WWW ---
#include <WiFi.h>
#include <WebServer.h>

// --- OLED ---
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// Czcionki
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansOblique18pt7b.h>

// --- WDT ---
#include <esp_task_wdt.h>         // Watchdog Timer (WDT) dla ESP32 - zapobieganie zawieszaniu się systemu

// --- Wersja systemu --- 
const char systemVersion[] PROGMEM = "14.10.2024";

// --- Ustawienia wyświetlacza OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int currentScreen = 0;
int subScreen = 0; // Zmienna do obsługi pod-ekranów
const long screenInterval = 500;

// --- Światła ---
#define FrontDayPin 10 // światła dzienne
#define FrontPin 11    // światła zwykłe
#define RealPin 12     // tylne światło
bool rearBlinkState = false;
bool rearBlinkEnabled = true;

int currentLightMode = 2;  // po uruchomieniu wszystkie światła wyłączone
bool lightsOn = false;

// --- EEPROM - struktura do przechowywania ustawień ---
struct Settings {
  int wheelCircumference;
  float batteryCapacity;
  int daySetting;
  int nightSetting;
  bool dayRearBlink;
  bool nightRearBlink;
  unsigned long blinkInterval;
};

Settings bikeSettings;
Settings storedSettings;

// --- DS18B20 ---
#define PIN_ONE_WIRE_BUS 3                             // Definiuje pin, do którego podłączony jest czujnik temperatury DS18B20
OneWire oneWire(PIN_ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
unsigned long ds18b20RequestTime = 0;                  // Czas ostatniego żądania pomiaru DS18B20
const unsigned long DS18B20_CONVERSION_DELAY_MS = 750; // Czas potrzebny na konwersję (w ms)
float currentTemp = 0.0;                               // Aktualna temperatura

// --- Zmienne globalne ---
unsigned long previousMillis = 0;
unsigned long previousTempMillis = 0;
unsigned long previousScreenMillis = 0;
unsigned long noImpulseThreshold = 2000;  // Przykładowy próg w milisekundach
const long tempInterval = 1000;
const long dataInterval = 5000;

// --- RTC DS3231 ---
RTC_DS3231 rtc;

// --- Przycisk ---
const int buttonPin = 4;
unsigned long lastButtonPress = 0;
unsigned long buttonHoldStart = 0;
bool buttonPressed = false;        // Flaga przycisku dla krótkiego naciśnięcia
bool longPressHandled = false;     // Flaga dla długiego naciśnięcia
bool buttonReleased = true;        // Flaga sygnalizująca, że przycisk został zwolniony
unsigned long debounceDelay = 50; // Opóźnienie do debounce
unsigned long holdTime = 1000;     // Czas trzymania przycisku

// --- Kadencja ---
// Stałe dla kadencji
const uint8_t CADENCE_PIN = 5;  // Pin dla czujnika Hall
const uint16_t RPM_TIMEOUT = 3000;  // Timeout w ms (3 sekundy)
const uint8_t MAGNETS = 1;  // Liczba magnesów (domyślnie 1)
const uint16_t MIN_REVOLUTION_TIME = 150;  // Minimalny czas obrotu w ms (zabezpieczenie przed szumem)

// Zmienne dla kadencji
volatile uint32_t lastCadencePulseTime = 0;  // Czas ostatniego impulsu
volatile uint32_t cadencePulseInterval = 0;  // Interwał między impulsami
volatile bool newCadencePulse = false;       // Flaga nowego impulsu

const int cadenceMin = 60; // Minimalna kadencja
const int cadenceMax = 80; // Maksymalna kadencja

// --- Prędkość ---
// Stałe dla pomiaru prędkości
const uint8_t SPEED_PIN = 6;
const uint16_t WHEEL_CIRCUMFERENCE = 2100;  // mm
const uint16_t SPEED_TIMEOUT = 3000;        // ms
const uint16_t MIN_PULSE_TIME = 50;         // ms (eliminacja drgań)
const float KMH_CONVERSION = 3.6;           // Przelicznik m/s na km/h

// Zmienne dla pomiaru prędkości (volatile dla zmiennych używanych w przerwaniu)
volatile uint32_t lastSpeedPulseTime = 0;
volatile uint32_t speedPulseInterval = 0;
volatile bool newSpeedPulse = false;
float currentSpeed = 0.0;
float smoothedSpeed = 0.0;               

// --- Licznik kilometrów ---
float totalDistanceKm = 0;              
float nextDistanceCheckpoint = 0.1; 

// --- Debouncing ---
volatile unsigned long lastPulseTime = 0;
const unsigned long debounceInterval = 50;
volatile unsigned long lastInterruptTime = 0;  // Czas ostatniego przerwania

// --- Ładowarka USB ---
bool outUSB = false;

// --- BMS ---
#define MAX_BMS_BUFFER_SIZE 100                 // Maksymalny rozmiar bufora danych z BMS
#define RECONNECT_INTERVAL_MS 5000              // Interwał ponownego połączenia w ms
#define DATA_REQUEST_INTERVAL_MS 5000           // Interwał wysyłania zapytań do BMS w ms
BLEClient *bleClient;                           // Wskaźnik na klienta BLE
BLERemoteCharacteristic *bleCharacteristicTx;   // Charakterystyka do wysyłania danych
BLERemoteCharacteristic *bleCharacteristicRx;   // Charakterystyka do odbierania danych
BLERemoteService *bleService;                   // Zdalna usługa BLE
uint8_t bmsDataBuffer[MAX_BMS_BUFFER_SIZE];     // Bufor do przechowywania danych BMS
int bmsDataReceivedLength = 0;                  // Aktualna długość odebranych danych
int bmsExpectedDataLength = 0;                  // Oczekiwana długość danych
bool bmsDataErrorFlag = false;                  // Flaga błędu odbioru danych
unsigned long lastReconnectAttemptTime = 0;     // Czas ostatniej próby połączenia
unsigned long lastDataRequestTime = 0;          // Czas ostatniego wysłania zapytania
BLEAddress bmsMacAddress("a5:c2:37:05:8b:86");  // Adres MAC BMS
bool notificationReceived = false;              // Flaga powiadomień BLE

float voltage = 0;
float current = 0;
float remainingCapacity = 0;
float power = 0;
float remainingEnergy = 0;
float soc = 0;

// --- zasieg i srednie ---
#define BUFFER_SIZE 300  // 5 minut * 60 sek/min = 300 (zakładamy odczyty co sekundę)
#define DEFAULT_UPDATE_INTERVAL 1000  // Domyślny czas aktualizacji (1 sekunda)

unsigned long lastUpdateTime = 0;  // Zmienna do śledzenia czasu aktualizacji
unsigned long lastStationaryTime = 0;  // Czas, kiedy rower ostatnio się zatrzymał
unsigned long updateInterval = DEFAULT_UPDATE_INTERVAL;  // Dynamiczny interwał aktualizacji
bool stationary = false;  // Czy rower stoi w miejscu
float previousRange = 0;  // Poprzedni zasięg (do stabilizacji)

// --- Bufory do przechowywania danych historycznych ---
CircularBuffer<float, BUFFER_SIZE> energyHistory;  // Energia zużyta w Wh w ostatnich 5 minutach
CircularBuffer<float, BUFFER_SIZE> powerHistory;   // Moc w W w ostatnich 5 minutach
CircularBuffer<float, BUFFER_SIZE> speedHistory;   // Prędkość w km/h w ostatnich 5 minutach

// --- Zmienne do statystyk ---
float totalWh = 0;           // Zużycie energii od początku jazdy
float maxWh = 0;             // Maksymalne zużycie energii
float totalPower = 0;        // Suma mocy od początku jazdy
float maxPower = 0;          // Maksymalna moc
int dataCount = 0;           // Liczba próbek
float wh = 0;
float avgWh = 0;  // Średnie zużycie energii
float avgSpeed = 0;  // Średnia prędkość
float range = 0;

// --- Dane dla AP ---
const char* apSSID = "e-Bike System";
const char* apPassword = "#mamrower";

// --- WiFi serwer www ---
WebServer server(80);
// Deklaracja zmiennych globalnych dla czasu
int currentHour;
int currentMinute;
int currentDay;
int currentMonth;
int currentYear;

// --- Funkcje inicjalizujące i konfigurujące ---
void showWelcomeMessage();           // Wyświetlenie wiadomości powitalnej na ekranie OLED
void loadSettingsFromEEPROM();       // Wczytanie ustawień z pamięci EEPROM
void setLights();                    // Konfiguracja stanu świateł na rowerze
void connectToBms();                 // Inicjalizacja i połączenie z BMS przez BLE
// --- Funkcje związane z przyciskiem --- 
void handleButtonISR();              // Obsługa przerwania przycisku (debouncing i rejestrowanie naciśnięcia)
void handleButton();                 // Obsługa krótkiego naciśnięcia przycisku (przełączanie ekranów)
void handleLongPress();              // Obsługa długiego naciśnięcia przycisku (zmiana pod-ekranów, przełączanie trybów)
// --- Funkcje obsługi ekranu OLED ---
void showScreen(int screen);         // Aktualizacja wyświetlacza OLED z danymi zależnymi od aktualnego ekranu
int getSubScreenCount(int screen);   // Zwrócenie liczby pod-ekranów dla wybranego ekranu
// --- Funkcje związane z BLE i BMS ---
void notificationCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);  // Obsługa powiadomień BLE
bool appendBmsDataPacket(uint8_t *data, int dataLength);                                                                     // Dodawanie pakietu danych do bufora
void printBatteryStatistics(uint8_t *data);                                                                                  // Wyświetlanie statystyk baterii
// --- Funkcje sterowania akcesoriami ---
void toggleLightMode();              // Zmiana trybu świateł (dzienny/nocny/wyłączony)
void toggleUSBMode();                // Włączanie/wyłączanie zasilania USB
// --- Funkcje związane z czujnikami ---
void countPulse();                   // Obsługa impulsów z czujnika prędkości/kadencji
void updateTemperature();            // Aktualizacja temperatury z czujnika DS18B20
void calculateSpeed();               // Obliczanie aktualnej prędkości na podstawie impulsów
// --- Funkcje związane z danymi i zasięgiem ---
void updateData(float current, float voltage, float speedKmh);  // Aktualizacja danych, takich jak moc, energia i prędkość
void adjustUpdateInterval(float speedKmh);                      // Dynamiczna zmiana interwału aktualizacji na podstawie prędkości
float calculateRange();                                         // Obliczanie pozostałego zasięgu roweru
float getAverageWh();                                           // Obliczanie średniego zużycia energii (Wh)
float getAveragePower();                                        // Obliczanie średniej mocy (W)
void updateReadings(unsigned long currentTime);                 // Aktualizacja różnych odczytów systemu

void printRTCDateTime();
void handleSaveClockSettings();
void handleSaveBikeSettings();
//bool isValidDate(int year, int month, int day, int hour, int minute);
void htmlStyle();

void checkDaylightSavingTime();

// --- Funkcja wyświetlania animacji powitania ---
void showWelcomeMessage() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setFont(&FreeSansOblique18pt7b);

  String welcomeMessage = "e-Bike System         "; // napis animacji
  int messageWidth = welcomeMessage.length() * 14;  // Szerokość wiadomości
  int screenWidth = 128;                            // Szerokość wyświetlacza
  int x = screenWidth;                              // Początkowa pozycja x, poza ekranem

  // Animacja przewijania
  unsigned long lastUpdate = millis();
  while (x > -messageWidth) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate >= 1) { 
      display.clearDisplay();
      display.setCursor(x, 25);
      display.print(welcomeMessage);
      display.display();
      x--;
      esp_task_wdt_reset();  // Reset Watchdoga, aby uniknąć restartu
      lastUpdate = currentMillis;
    }
  }

  // Wyświetl "Witaj!" na dwie sekundy
  display.setCursor(18, 25);
  display.print("Witaj!");
  display.display();
  delay(2000); 
}

// --- Wczytywanie ustawień z EEPROM ---
void loadSettingsFromEEPROM() {
  // Wczytanie ustawień z EEPROM
  EEPROM.get(0, bikeSettings);

  // Skopiowanie aktualnych ustawień do storedSettings do późniejszego porównania
  storedSettings = bikeSettings;

  // Możesz dodać weryfikację wczytanych danych
  if (bikeSettings.wheelCircumference == 0) {
    bikeSettings.wheelCircumference = 2210;  // Domyślny obwód koła
    bikeSettings.batteryCapacity = 10.0;     // Domyślna pojemność baterii
    bikeSettings.daySetting = 0;
    bikeSettings.nightSetting = 0;
    bikeSettings.dayRearBlink = false;
    bikeSettings.nightRearBlink = false;
    bikeSettings.blinkInterval = 500;
  }
}

// --- Funkcja zapisująca ustawienia do EEPROM ---
void saveSettingsToEEPROM() {
  // Porównaj aktualne ustawienia z poprzednio wczytanymi
  if (memcmp(&storedSettings, &bikeSettings, sizeof(bikeSettings)) != 0) {
    // Jeśli ustawienia się zmieniły, zapisz je do EEPROM
    EEPROM.put(0, bikeSettings);
    EEPROM.commit();

    // Zaktualizuj storedSettings po zapisie
    storedSettings = bikeSettings;
  }
}

// --- Ustawiania świateł ---
void setLights() {
  if (!lightsOn) {
    // Wyłącz wszystkie światła
    digitalWrite(FrontDayPin, LOW);
    digitalWrite(FrontPin, LOW);
    digitalWrite(RealPin, LOW);
    rearBlinkState = false;
    return;
  }

  // Wybierz ustawienia świateł w zależności od trybu (dzienny/nocny)
  int lightSetting = (currentLightMode == 0) ? bikeSettings.daySetting : bikeSettings.nightSetting;
  rearBlinkEnabled = (currentLightMode == 0) ? bikeSettings.dayRearBlink : bikeSettings.nightRearBlink;  // Czy mruganie tylnego światła ma być włączone

  // Obsługa ustawień świateł
  switch (lightSetting) {
    case 0: // Tylko przednie światło dzienne/nocne
      digitalWrite(FrontDayPin, HIGH);
      digitalWrite(FrontPin, LOW);
      digitalWrite(RealPin, LOW);  // Wyłącz tylne światło
      rearBlinkState = false;       // Wyłącz mruganie tylnego światła
      break;

    case 1: // Tylko przednie światło zwykłe
      digitalWrite(FrontDayPin, LOW);
      digitalWrite(FrontPin, HIGH);
      digitalWrite(RealPin, LOW);  // Wyłącz tylne światło
      rearBlinkState = false;       // Wyłącz mruganie tylnego światła
      break;

    case 2: // Tylne światło bez przedniego
      digitalWrite(FrontDayPin, LOW);
      digitalWrite(FrontPin, LOW);
      digitalWrite(RealPin, HIGH);  // Włącz tylne światło
      rearBlinkState = rearBlinkEnabled;  // Sprawdź, czy tylne światło ma mrugać
      break;

    case 3: // Przednie dzienne/nocne + tylne światło      
      digitalWrite(FrontDayPin, HIGH);
      digitalWrite(FrontPin, LOW);      
      digitalWrite(RealPin, HIGH);  // Włącz tylne światło
      rearBlinkState = rearBlinkEnabled;  // Sprawdź, czy tylne światło ma mrugać
      break;

    case 4: // Przednie zwykłe + tylne światło      
      digitalWrite(FrontDayPin, LOW);
      digitalWrite(FrontPin, HIGH);
      digitalWrite(RealPin, HIGH);  // Włącz tylne światło
      rearBlinkState = rearBlinkEnabled;  // Sprawdź, czy tylne światło ma mrugać
      break;
  }
}

// --- Połączenie z BMS ---
void connectToBms() {
  if (!bleClient->isConnected()) {
    #if DEBUG
    Serial.println("Próba połączenia z BMS...");
    #endif

    if (bleClient->connect(bmsMacAddress)) {
      #if DEBUG
      Serial.println("Połączono z BMS");
      #endif

      bleService = bleClient->getService("0000ff00-0000-1000-8000-00805f9b34fb");
      if (bleService == nullptr) {
        #if DEBUG
        Serial.println("Nie znaleziono usługi BMS");
        #endif
        bleClient->disconnect();
        return;
      }

      bleCharacteristicTx = bleService->getCharacteristic("0000ff02-0000-1000-8000-00805f9b34fb");
      if (bleCharacteristicTx == nullptr) {
        #if DEBUG
        Serial.println("Nie znaleziono charakterystyki Tx");
        #endif
        bleClient->disconnect();
        return;
      }

      bleCharacteristicRx = bleService->getCharacteristic("0000ff01-0000-1000-8000-00805f9b34fb");
      if (bleCharacteristicRx == nullptr) {
        #if DEBUG
        Serial.println("Nie znaleziono charakterystyki Rx");
        #endif
        bleClient->disconnect();
        return;
      }

      // Rejestracja funkcji obsługi powiadomień BLE
      if (bleCharacteristicRx->canNotify()) {
        bleCharacteristicRx->registerForNotify(notificationCallback);
        #if DEBUG
        Serial.println("Zarejestrowano powiadomienia dla Rx");
        #endif
      } else {
        #if DEBUG
        Serial.println("Charakterystyka Rx nie obsługuje powiadomień");
        #endif
        bleClient->disconnect();
        return;
      }

    } else {
      #if DEBUG
      Serial.println("Nie udało się połączyć z BMS");
      #endif
    }
  }
}

// --- Obsługa przerwania przycisku ---
void IRAM_ATTR handleButtonISR() {
  unsigned long currentMillis = millis();

  if (digitalRead(buttonPin) == LOW) {  // Jeśli przycisk jest naciśnięty
    if (currentMillis - lastButtonPress >= debounceDelay && buttonReleased) {
      buttonHoldStart = currentMillis;  // Rozpocznij liczenie czasu przytrzymania
      buttonPressed = true;             // Ustaw flagę krótkiego naciśnięcia
      buttonReleased = false;           // Ustaw flagę, że przycisk jest naciśnięty
      longPressHandled = false;         // Zresetuj flagę obsługi długiego naciśnięcia
    }
  } else {  // Jeśli przycisk jest zwolniony
    buttonReleased = true;
    lastButtonPress = currentMillis; // Aktualizuj czas ostatniego naciśnięcia
  }
}

// --- Obsługa zmiany ekranu ---
void handleButton() {
  subScreen = 0;                            // Zresetuj pod-ekrany
  currentScreen = (currentScreen + 1) % 10; // Przełączanie głównych ekranów
  longPressHandled = false;                 // Reset flagi długiego naciśnięcia
}

// --- Obsługa długiego przytrzymania ---
void handleLongPress() {
  if (currentScreen == 8) {
    toggleLightMode(); // Zmień tryb świateł
  } else if (currentScreen == 9) {
    toggleUSBMode();   // Przełącz wyjście USB
  }

  // Obsługa pod-ekranów
  if (getSubScreenCount(currentScreen) > 0) {
    subScreen = (subScreen + 1) % getSubScreenCount(currentScreen);
  } else {
    subScreen = 0;
  }
}

// --- Funkcja do czyszczenia i aktualizacji wyświetlacza OLED tylko przy zmianach ---
void showScreen(int screen) {
  char buffer[16];  // Zwiększono rozmiar bufora, aby pomieścić większe teksty z PROGMEM

  display.clearDisplay();      // Wyczyść wyświetlacz
  display.setTextColor(SSD1306_WHITE); // Ustaw kolor tekstu na biały

  DateTime now = rtc.now();   // Odczytaj aktualny czas z zegara RTC

  const char* const daysOfWeek[] PROGMEM = {
    "Niedziela", "Poniedzialek", "Wtorek", "Sroda", 
    "Czwartek", "Piatek", "Sobota"
  };

  switch (currentScreen) {

    case 0: // Zegar
      switch (subScreen) {

        case 0:  // Zegar
          display.setFont(&FreeSansOblique18pt7b); // Ustaw odpowiednią czcionkę
          display.setCursor(10, 28);
          if (now.hour() < 10) display.print(' ');  // Dodaj spację dla jednocyfrowych godzin
          display.print(now.hour());
          display.print(':');
          if (now.minute() < 10) display.print('0');  // Dodaj "0" dla jednocyfrowych minut
          display.print(now.minute());
          break;

        case 1:  // Data
          display.setFont(&FreeSans9pt7b); // Ustaw odpowiednią czcionkę
          display.setCursor(2, 12);
          if (now.day() < 10) display.print('0');
          display.print(now.day());
          display.print('/');
          if (now.month() < 10) display.print('0');
          display.print(now.month());
          display.print('/');
          display.print(now.year());
          display.setCursor(2, 31);
          strcpy_P(buffer, (const char*)pgm_read_ptr(&(daysOfWeek[now.dayOfTheWeek()]))); 
          display.print(buffer);
          break;
      } 
      break;

    case 1: // Kadencja prędkość
    
        case 0:  // Kadencja          
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Kadencja");
          display.setCursor(2, 31);
          uint8_t cadence = calculateCadence();
          if (cadence < 10) {
            display.print(" ");
            display.print(cadence);
          } else if (cadence < 100) {
            display.print("  ");
            display.print(cadence);
          } else {
            display.print(cadence);
          }
          display.print(" RPM");

          // Wyświetlanie strzałki lub OK w zależności od kadencji
          if (cadence > 0) {
            if (cadence <= cadenceMin) {
              display.setCursor(100, 12);
              display.print("^");  // Strzałka w górę, kadencja za niska
            } else if (cadence >= cadenceMax) {
              display.setCursor(100, 12);
              display.print("v");  // Strzałka w dół, kadencja za wysoka
            } else {
              display.setCursor(100, 12);
              display.print("OK");  // Kadencja w zakresie, wyświetl OK
            }
          }
          break;

        case 1:  // Prędkość
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Predkosc");
          display.setCursor(2, 31);
          float speed = calculateSpeed();
          display.print(speed, 1);
          display.print(" km/h");
          break;
      }  
      break;

    case 2:  // Temperatura i wersja programu
      switch (subScreen) {

        case 0:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Temperatura");
          display.setCursor(2, 31);
          if (currentTemp == -999) {
            display.print("blad!");
          } else {
            display.print(currentTemp, 1);
            display.print(" C");
          }
          break;

        case 1:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("System ver.");
          display.setCursor(2, 31);
          strcpy_P(buffer, systemVersion);
          display.print(buffer);
          break;
      }
      break;

        case 3: // Zasięg i przebieg dzienny 
            switch (subScreen) {
                case 0:
                    display.setFont(&FreeSans9pt7b);             
                    display.setCursor(2, 12);
                    display.print("Zasieg");  // pozostały zasięg
                    display.setCursor(2, 31);
                    display.print(range, 1);
                    display.print(" km");
                    break;

                case 1: 
                    display.setFont(&FreeSans9pt7b);
                    display.setCursor(2, 12);
                    display.print("Przebieg");  // licznik dzienny
                    display.setCursor(2, 31);
                    display.print(totalDistanceKm, 1);
                    display.print(" km");
                    break;
            }    
            break;

    case 4: // Akumulator
      switch (subScreen) {

        case 0:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);          
          display.print("Akumulator");  // pojemność akumulatora w %
          display.setCursor(2, 31);
          display.print(soc, 0);
          display.print(" %");
          break;

        case 1:  
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Pojemnosc");  // pojemność akumulatora w Ah
          display.setCursor(2, 31);
          display.print(remainingCapacity, 1);
          display.print(" Ah");
          break;

        case 2:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Pojemnosc");  // pojemność akumulatora w Wh
          display.setCursor(2, 31);
          display.print(remainingEnergy, 1);
          display.print(" Wh");
          break;

        case 3:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Napiecie");  // napięcie w V
          display.setCursor(2, 31);
          display.print(voltage, 3);
          display.print(" V");
          break;

        case 4:   
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Natezenie");  // natężenie prądu w A
          display.setCursor(2, 31);
          display.print(current, 3);
          display.print(" A");
          break;
      }
      break;

    case 5: // Moc
      switch (subScreen) {

        case 0:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Moc");
          display.setCursor(2, 31);
          display.print(power, 1);
          display.print(" W");
          break;

        case 1:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Moc AVG");
          display.setCursor(2, 31);
          display.print(getAveragePower(), 1);
          display.print(" W");
          break;

        case 2:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Moc MAX");
          display.setCursor(2, 31);
          display.print(maxPower, 1);
          display.print(" W");
          break;
      }
      break;

    case 6: // Zużycie
      switch (subScreen) {

        case 0:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Zuzycie");
          display.setCursor(2, 31);
          display.print(wh, 1);
          display.print(" Wh");
          break;

        case 1:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Zuzycie AVG");
          display.setCursor(2, 31);
          display.print(getAverageWh(), 1);
          display.print(" Wh");
          break;

        case 2:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Zuzycie MAX");
          display.setCursor(2, 31);
          display.print(maxWh, 1);
          display.print(" Wh");
          break;          
      }
      break;

    case 7: // Czujniki ciśnienia kół
      switch (subScreen) {

        case 0:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("Cisnienie kol");
          display.setCursor(2, 31);
          display.print("3.2bar | 3.0bar");
          break;

        case 1:
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 12);
          display.print("25.6'C");
          display.setCursor(64, 12);
          display.print("| 23.4'C");
          display.setCursor(2, 31);
          display.print("2.99V");
          display.setCursor(64, 31);
          display.print("| 2.99V");
          break;     
      }
      break;

    case 8: // Wyświetlanie trybu świateł
      display.setFont(&FreeSans9pt7b);
      display.setCursor(2, 12);
      display.print("Tryb swiatel");
      display.setCursor(2, 30);
      if (!lightsOn) {
        display.print("Wylaczone");
      } else {
        display.print(currentLightMode == 0 ? "Dzien" : "Noc");
      }
      break;

    case 9: // Ładowarka USB
      display.setFont(&FreeSans9pt7b); 
      display.setCursor(0, 12); 
      display.print("Zasilanie USB"); 
      display.setCursor(0, 30); 
      display.print(outUSB == 0 ? "Wylaczone" : "Wlaczone");
      break;
  }
  display.display(); // Wyślij zawartość bufora do wyświetlacza
}

// --- ...
int getSubScreenCount(int screen) {
  switch (screen) {
    case 0: 
      return 2; // Zegar, Data
    case 1: 
      return 2; // Kadencja i Prędkość
    case 2: 
      return 2; // Temperatura, wersja systemu
    case 3: 
      return 2; // Zasięg, przebieg
    case 4:
      return 5; // Wh, Ah, %, A, V
    case 5: 
      return 3; // W, W AVG, W MAX
    case 6: 
      return 3; // Wh, Wh AVG, Wh MAX
    case 7: 
      return 2; // Ciśnienie, napięcie i temperatura
    default:
      return 0; // Brak pod-ekranów dla innych ekranów
  }
}

// --- Funkcja obsługi powiadomień z BMS ---
void notificationCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  if (length == 0 || pData == nullptr) {
    #if DEBUG
    Serial.println("Otrzymano niepoprawne dane lub brak danych.");
    #endif
    return;
  }

  // Sprawdzenie początku pakietu
  if (bmsDataReceivedLength == 0 && pData[0] == 0xDD) {
    bmsExpectedDataLength = pData[3];  // Oczekiwana długość danych z BMS
  }

  // Dodanie danych do bufora
  if (appendBmsDataPacket(pData, length)) {
    // Sprawdzenie, czy wszystkie dane zostały odebrane
    if (bmsDataReceivedLength == bmsExpectedDataLength + 7) {
      printBatteryStatistics(bmsDataBuffer, bmsDataReceivedLength);  // Wyświetlenie danych baterii
      bmsDataReceivedLength = 0;              // Resetowanie bufora po odebraniu pełnych danych
      bmsExpectedDataLength = 0;
    }
  } else {
    #if DEBUG
    Serial.println("Błąd podczas dodawania pakietu danych do bufora.");
    #endif
    bmsDataReceivedLength = 0;
    bmsExpectedDataLength = 0;
  }
}

// --- Funkcja dodająca pakiet danych do bufora ---
bool appendBmsDataPacket(uint8_t *data, int dataLength) {
  if (data == nullptr || dataLength <= 0) {
    #if DEBUG
    Serial.println("Niepoprawne dane do dodania do bufora.");
    #endif
    return false;
  }

  // Sprawdzenie, czy nie przekroczymy maksymalnego rozmiaru bufora
  if (dataLength + bmsDataReceivedLength >= MAX_BMS_BUFFER_SIZE) {
    #if DEBUG
    Serial.println("Błąd: Przekroczono maksymalny rozmiar bufora BMS.");
    #endif
    return false;  // Przekroczono pojemność bufora
  }

  // Kopiowanie danych do bufora
  memcpy(bmsDataBuffer + bmsDataReceivedLength, data, dataLength);
  bmsDataReceivedLength += dataLength;  // Aktualizacja odebranej długości

  return true;  // Sukces
}

// --- Funkcja wyświetlająca dane statystyczne baterii ---
void printBatteryStatistics(uint8_t *data, size_t dataLength) {  
  if (data[1] == 0x03) {  // Sprawdzenie, czy dane są poprawne
    // Odczytanie napięcia (jednostka: 10 mV)
    uint16_t voltageRaw = (data[4] << 8) | data[5];
    voltage = voltageRaw / 100.0;

    // Odczytanie natężenia (jednostka: 10 mA)
    int16_t currentRaw = (data[6] << 8) | data[7];
    current = currentRaw / 100.0;

    // Odczytanie pozostałej pojemności (jednostka: 10 mAh)
    uint16_t remainingCapacityRaw = (data[8] << 8) | data[9];
    remainingCapacity = remainingCapacityRaw / 100.0;

    // Odczytanie poziomu naładowania (SOC)
    uint8_t socRAW = data[23];  // Zmieniamy typ na uint8_t, ponieważ SOC to pojedynczy bajt
    soc = socRAW; 

    // Obliczenie mocy (W)
    power = voltage * current;

    // Obliczenie pozostałej energii (Wh)
    remainingEnergy = remainingCapacity * voltage;
  }
}

// --- Zmiana trybu świateł ---
void toggleLightMode() {
  currentLightMode = (currentLightMode + 1) % 3;  // Cykl: dzień (0), noc (1), wyłączone (2)
  lightsOn = currentLightMode != 2;               // Jeśli tryb to "wyłączone", wyłącz światła       
  setLights();  // Aktualizuj stan świateł
}

//--- Wyjście USB ---
void toggleUSBMode() {
  outUSB = !outUSB; // Zmiana stanu na przeciwny
}

// --- 
void IRAM_ATTR countPulse() {
  unsigned long currentPulseTime = millis();
  if (currentPulseTime - lastPulseTime > debounceInterval) {
    pulseCount++;
    lastPulseTime = currentPulseTime;
  }
}

// --- Funkcje inicjalizujące i obsługujące DS18B20 ---

// Funkcja inicjalizująca DS18B20
// Inicjalizuje czujnik temperatury DS18B20, aby był gotowy do wykonywania pomiarów
void initializeDS18B20() {
  sensors.begin(); // Inicjalizacja czujnika DS18B20
}

// Funkcja inicjująca pomiar temperatury z DS18B20 (nieblokująca)
// Rozpoczyna asynchroniczny pomiar temperatury z czujnika DS18B20
void requestGroundTemperature() {
  sensors.requestTemperatures();
  ds18b20RequestTime = millis();
}

// Funkcja sprawdzająca, czy odczyt DS18B20 jest gotowy
// Zwraca true, jeśli minął wymagany czas konwersji temperatury od momentu rozpoczęcia pomiaru
bool isGroundTemperatureReady() {
  return millis() - ds18b20RequestTime >= DS18B20_CONVERSION_DELAY_MS;
}

// Funkcja sprawdzająca, czy odczytana temperatura jest poprawna
// Sprawdza, czy temperatura mieści się w realistycznym zakresie (-50 do 100°C)
// Zwraca true, jeśli wartość jest w zakresie, inaczej false
bool isValidTemperature(float temp) {
  return (temp >= -50.0 && temp <= 100.0);
}

void handleSaveBikeSettings() {
    // Sprawdzenie, czy są przekazywane odpowiednie argumenty
    if (server.hasArg("wheel")) bikeSettings.wheelCircumference = server.arg("wheel").toInt();
    if (server.hasArg("battery")) bikeSettings.batteryCapacity = server.arg("battery").toFloat();

    // Zapis do EEPROM
    EEPROM.put(0, bikeSettings);
    EEPROM.commit();

    // Po zapisaniu ustawień - przekierowanie na stronę główną
    server.sendHeader("Location", "/");
    server.send(303);
}

// Funkcja odczytuje temperaturę z DS18B20
// Jeśli odczyt jest gotowy i poprawny, zwraca aktualną temperaturę, 
// w przeciwnym razie zwraca -999.0 (błąd odczytu lub niegotowy pomiar)
float readGroundTemperature() {
  if (isGroundTemperatureReady()) {
    float temperature = sensors.getTempCByIndex(0); // Odczytaj temperaturę
    if (isValidTemperature(temperature)) {
      return temperature;
    } else {
      #if DEBUG
      Serial.println("Błąd: Temperatura poza zakresem");
      #endif
      return -999.0; // Zwraca -999.0, jeśli temperatura jest niepoprawna
    }
  }
  return -999.0; // Zwraca -999.0, jeśli odczyt nie jest gotowy
}

// --- Obliczenia prędkości ---
void setupSpeedSensor() {
    pinMode(SPEED_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SPEED_PIN), handleSpeedInterrupt, FALLING);
}

// Obsługa przerwania dla czujnika prędkości
void IRAM_ATTR handleSpeedInterrupt() {
    uint32_t currentTime = millis();
    uint32_t interval = currentTime - lastSpeedPulseTime;
    
    // Filtrowanie drgań styków
    if (interval > MIN_PULSE_TIME) {
        speedPulseInterval = interval;
        lastSpeedPulseTime = currentTime;
        newSpeedPulse = true;
    }
}

// Obliczanie prędkości
float calculateSpeed() {
    static uint32_t lastCalculation = 0;
    static float lastValidSpeed = 0.0;
    uint32_t currentTime = millis();

    // Sprawdzenie timeoutu - brak ruchu
    if (currentTime - lastSpeedPulseTime > SPEED_TIMEOUT) {
        lastValidSpeed = 0;
        newSpeedPulse = false;
        return 0;
    }

    // Aktualizacja tylko przy nowym impulsie
    if (newSpeedPulse && (currentTime - lastCalculation > 250)) {  // max 4 updates/sec
        // Prędkość = (obwód koła [mm] / 1000000) / (czas między impulsami [ms] / 3600000) [km/h]
        float speed = (float)WHEEL_CIRCUMFERENCE * 3.6 / speedPulseInterval;
        
        // Filtrowanie nierealistycznych wartości
        if (speed < 99.9) {  // Max 100 km/h
            lastValidSpeed = speed;
        }
        
        newSpeedPulse = false;
        lastCalculation = currentTime;
    }

    return lastValidSpeed;
}

// Aktualizacja prędkości z wygładzaniem
void updateSpeed() {
    static uint32_t lastUpdate = 0;
    uint32_t currentTime = millis();
    
    // Sprawdź czy pojazd stoi
    if (currentTime - lastSpeedPulseTime > STOP_TIMEOUT) {
        if (isMoving) {  // Zmiana stanu z ruchu na stop
            isMoving = false;
            currentSpeed = 0;
            smoothedSpeed = 0;
            // Można dodać zapis statystyk jazdy
        }
        return;  // Nie wykonuj dalszych obliczeń
    }

    // Aktualizuj co 250ms tylko jeśli są impulsy
    if (currentTime - lastUpdate >= 250 && newSpeedPulse) {
        float newSpeed = calculateSpeed();
        
        // Filtrowanie małych prędkości
        if (newSpeed < MIN_SPEED) {
            newSpeed = 0;
        }
        
        // Wygładzanie tylko dla rzeczywistego ruchu
        if (newSpeed > 0) {
            float alpha = (newSpeed > 25.0) ? 0.3 : 0.2;
            smoothedSpeed = alpha * newSpeed + (1.0 - alpha) * smoothedSpeed;
            isMoving = true;
        }
        
        currentSpeed = smoothedSpeed;
        newSpeedPulse = false;
        lastUpdate = currentTime;
    }
}

// Funkcja zwracająca średnią prędkość
float getAverageSpeed() {
    static uint32_t sampleCount = 0;
    static float speedSum = 0.0;
    
    if (currentSpeed > 0) {
        speedSum += currentSpeed;
        sampleCount++;
        return speedSum / sampleCount;
    }
    return (sampleCount > 0) ? speedSum / sampleCount : 0;
}

// --- Funkcja do obliczania i wyświetlania kadencji ---
// Przerwanie dla kadencji
void IRAM_ATTR handleCadenceInterrupt() {
    uint32_t currentTime = millis();
    uint32_t interval = currentTime - lastCadencePulseTime;
    
    // Filtrowanie szumów i odbić
    if (interval > MIN_REVOLUTION_TIME) {
        cadencePulseInterval = interval;
        lastCadencePulseTime = currentTime;
        newCadencePulse = true;
    }
}

// Funkcja obliczająca kadencję
uint8_t calculateCadence() {
    static uint32_t lastCalculation = 0;
    static uint8_t lastValidCadence = 0;
    uint32_t currentTime = millis();

    // Sprawdź czy nie minął timeout
    if (currentTime - lastCadencePulseTime > RPM_TIMEOUT) {
        lastValidCadence = 0;
        newCadencePulse = false;
        return 0;
    }

    // Aktualizuj tylko gdy jest nowy impuls
    if (newCadencePulse && currentTime - lastCalculation > 250) {  // Aktualizacja max 4 razy na sekundę
        // Obliczenie RPM: (60 sekund * 1000ms) / (interwał * liczba magnesów)
        uint8_t rpm = (uint32_t)60000 / (cadencePulseInterval * MAGNETS);
        
        // Filtr dla wartości odstających
        if (rpm < 200) {  // Maksymalna realna kadencja
            lastValidCadence = rpm;
        }
        
        newCadencePulse = false;
        lastCalculation = currentTime;
    }

    return lastValidCadence;
}

// --- Funkcja do aktualizacji bieżących danych ---
void updateData(float current, float voltage, float speedKmh) {
    power = current * voltage;  // Moc w W (watty)
    wh = power / 3600;    // Zużycie energii w Wh na sekundę

    // Zbieranie danych historycznych
    energyHistory.push(wh);
    powerHistory.push(power);
    speedHistory.push(speedKmh);

    // Aktualizacja statystyk
    totalWh += wh;
    totalPower += power;
    dataCount++;

    if (wh > maxWh) maxWh = wh;  // Maksymalne zużycie Wh
    if (power > maxPower) maxPower = power;  // Maksymalna moc

    // Aktualizacja przejechanych kilometrów
    totalDistanceKm += speedKmh * (updateInterval / 3600000.0);  // km = km/h * h
}

// Funkcja do dynamicznej aktualizacji interwału
void adjustUpdateInterval(float speedKmh) {
  if (speedKmh > 20) {
    updateInterval = 500;  // Szybsze aktualizacje przy wyższych prędkościach
  } else if (speedKmh < 5) {
    updateInterval = 2000;  // Wolniejsze aktualizacje przy niskich prędkościach
  } else {
    updateInterval = DEFAULT_UPDATE_INTERVAL;  // Standardowy interwał
  }
}

// --- Funkcja do obliczania zasięgu ---
float calculateRange() {
  avgWh = 0;  // Średnie zużycie energii
  avgSpeed = 0;  // Średnia prędkość

  if (energyHistory.size() > 0 && speedHistory.size() > 0) {
    // Obliczanie średniego zużycia energii (w Wh/h) i prędkości z ostatnich 5 minut
    for (int i = 0; i < energyHistory.size(); i++) {
      avgWh += energyHistory[i];
      avgSpeed += speedHistory[i];
    }
    avgWh = (avgWh / energyHistory.size()) * 3600;  // Przekształcamy Wh na godzinę
    avgSpeed /= speedHistory.size();
  } else {
    // Jeśli nie mamy danych historycznych, obliczamy na podstawie bieżącego zużycia
    avgWh = (power / 3600);  // Moc przeliczona na Wh/h
    avgSpeed = speedKmh;
  }

  // Unikanie nagłych zmian w obliczeniach zasięgu przy zerowej prędkości
  if (avgSpeed < 0.1) {
    // Zakładamy, że rower się zatrzymał, nie zmieniamy obliczeń zasięgu
    return previousRange;
  } else {
    previousRange = remainingEnergy / (avgWh / avgSpeed);  // Obliczenie zasięgu w km
    return previousRange;
  }
}

// --- Funkcja do obliczenia średniego zużycia Wh ---
float getAverageWh() {
  if (dataCount > 0) {
    return (totalWh / dataCount) * 3600;  // Zwracamy zużycie energii na godzinę
  } else {
    return 0;
  }
}

// --- Funkcja do obliczenia średniej mocy --- 
float getAveragePower() {
  if (dataCount > 0) {
    return totalPower / dataCount;  // Średnia moc, zakładamy, że totalPower jest w W
  } else {
    return 0;
  }
}

// --- Funkcja do aktualizacji odczytów kadencji ---
void updateReadings(unsigned long currentTime) {
  // Sprawdzenie, czy minął wystarczający czas od ostatniego pomiaru
  if (currentTime - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
    // Zapisz aktualny czas w tablicy odczytów kadencji
    cadenceReadings[readIndex] = currentTime; 
    readIndex = (readIndex + 1) % NUM_READINGS; // Zmieniaj indeks cyklicznie
    lastMeasurementTime = currentTime; // Uaktualnij czas ostatniego pomiaru

    // Oblicz i wyświetl kadencję
    calculateAndDisplayCadence();
  }
}

// Funkcja obsługi strony ustawień
void handleSettings() {
  // Pobierz aktualny czas z zegara RTC
  DateTime now = rtc.now();

  // Dodaj logi diagnostyczne
  Serial.print("Odczyt z RTC: ");
  Serial.print(now.year());
  Serial.print("-");
  Serial.print(now.month());
  Serial.print("-");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.println();

  // Przygotowanie strony HTML z formularzami
  String html = "<!DOCTYPE html><html lang='pl'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";

  // Styl dla wyglądu responsywnego
  html += "<style>"
          "body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; margin: 0; padding: 0; }"
          "header { color: #007bff; padding: 10px; text-align: center; font-size: 36px; font-family: 'Verdana', sans-serif; }"
          "form { display: flex; flex-direction: column; align-items: center; padding: 20px; }"
          "label { margin: 10px 0 5px 0; }"
          ".row { display: flex; flex-wrap: wrap; justify-content: center; width: 100%; max-width: 600px; }"
          ".row input, .row select { flex: 1; padding: 10px; margin: 5px; box-sizing: border-box; max-width: 150px; text-align: center; }"
          ".row label { flex: 1; text-align: center; font-weight: bold; }"
          "button { background-color: #007bff; color: white; border: none; padding: 15px; width: 100%; max-width: 600px; margin-top: 20px; cursor: pointer; font-size: 16px; }"
          "button:hover { background-color: #0056b3; }"
          "h2 { margin-bottom: 0px; font-family: 'Verdana', sans-serif; font-size: 18px; }"
          "</style>";

  html += "</head><body>";
  html += "<header>e-Bike System</header>";

  // --- Formularz ustawień zegara ---
  html += "<form action='/saveClockSettings' method='GET'>";
  html += "<h2>Czas</h2>";
  html += "<div class='row'>";
  html += "<input type='number' name='hour' value='" + String(now.hour()) + "' placeholder='Godzina' min='0' max='23'>";
  html += "<input type='number' name='minute' value='" + String(now.minute()) + "' placeholder='Minuta' min='0' max='59'>";
  html += "<input type='number' name='second' value='" + String(now.second()) + "' placeholder='Sekunda' min='0' max='59'>"; // Dodane pole dla sekund
  html += "</div>";

  html += "<h2>Data</h2>";
  html += "<div class='row'>";
  html += "<input type='number' name='day' value='" + String(now.day()) + "' placeholder='Dzień' min='1' max='31'>";
  html += "<input type='number' name='month' value='" + String(now.month()) + "' placeholder='Miesiąc' min='1' max='12'>";
  html += "<input type='number' name='year' value='" + String(now.year()) + "' placeholder='Rok' min='2020' max='2100'>";
  html += "</div>";
  
  html += "<button type='submit'>Zapisz ustawienia zegara</button>";
  html += "</form>";

  // --- Formularz ustawień roweru ---
  html += "<form action='/saveBikeSettings' method='GET'>";
  html += "<h2>Ustawienia</h2>";
  html += "<div class='row'>";
  html += "<input type='number' name='wheel' value='" + String(bikeSettings.wheelCircumference) + "' placeholder='Obwód koła (mm)'>";
  html += "</div>";
  
  html += "<div class='row'>";
  html += "<input type='number' step='0.1' name='battery' value='" + String(bikeSettings.batteryCapacity, 1) + "' placeholder='Pojemność baterii (Ah)'>";
  html += "</div>";

  // Światła dzienne
  html += "<h2>Światła dzienne</h2>";
  html += "<div class='row'>";
  html += "<select name='daySetting'>";
  html += "<option value='0'" + String(bikeSettings.daySetting == 0 ? " selected" : "") + ">Dzienne</option>";
  html += "<option value='1'" + String(bikeSettings.daySetting == 1 ? " selected" : "") + ">Zwykłe</option>";
  html += "<option value='2'" + String(bikeSettings.daySetting == 2 ? " selected" : "") + ">Tył</option>";
  html += "<option value='3'" + String(bikeSettings.daySetting == 3 ? " selected" : "") + ">Dzienne + Tył</option>";
  html += "<option value='4'" + String(bikeSettings.daySetting == 4 ? " selected" : "") + ">Zwykłe + Tył</option>";
  html += "</select>";
  html += "</div>";

  // Mruganie tylnego światła dziennego
  html += "<div class='row'>";
  html += "<label>Mruganie tylnego światła (dzienny):</label>";
  html += "<select name='dayRearBlink'>";
  html += "<option value='true'" + String(bikeSettings.dayRearBlink ? " selected" : "") + ">Włącz</option>";
  html += "<option value='false'" + String(!bikeSettings.dayRearBlink ? " selected" : "") + ">Wyłącz</option>";
  html += "</select>";
  html += "</div>";

  // Światła nocne
  html += "<h2>Światła nocne</h2>";
  html += "<div class='row'>";
  html += "<select name='nightSetting'>";
  html += "<option value='0'" + String(bikeSettings.nightSetting == 0 ? " selected" : "") + ">Dzienne</option>";
  html += "<option value='1'" + String(bikeSettings.nightSetting == 1 ? " selected" : "") + ">Zwykłe</option>";
  html += "<option value='2'" + String(bikeSettings.nightSetting == 2 ? " selected" : "") + ">Tył</option>";
  html += "<option value='3'" + String(bikeSettings.nightSetting == 3 ? " selected" : "") + ">Dzienne + Tył</option>";
  html += "<option value='4'" + String(bikeSettings.nightSetting == 4 ? " selected" : "") + ">Zwykłe + Tył</option>";
  html += "</select>";
  html += "</div>";

  // Mruganie tylnego światła nocnego
  html += "<div class='row'>";
  html += "<label>Mruganie tylnego światła (nocny):</label>";
  html += "<select name='nightRearBlink'>";
  html += "<option value='true'" + String(bikeSettings.nightRearBlink ? " selected" : "") + ">Włącz</option>";
  html += "<option value='false'" + String(!bikeSettings.nightRearBlink ? " selected" : "") + ">Wyłącz</option>";
  html += "</select>";
  html += "</div>";

  // Częstotliwość mrugania tylnego światła
  html += "<div class='row'>";
  html += "<input type='number' name='blinkInterval' value='" + String(bikeSettings.blinkInterval) + "' placeholder='Częstotliwość mrugania (ms)'>";
  html += "</div>";

  html += "<button type='submit'>Zapisz ustawienia</button>";
  html += "</form>";

  html += "</body></html>";
  
  // Wyślij stronę do klienta
  server.send(200, "text/html", html);
}

// Funkcja zapisywania ustawień zegara
// Inicjalizacja RTC i inne funkcje...

void handleSaveClockSettings() {
  int year, month, day, hour, minute, second; // Sekundy zawsze ustawione na 0

  if (server.hasArg("year")) year = server.arg("year").toInt();        // Rok
  if (server.hasArg("month")) month = server.arg("month").toInt();     // Miesiąc
  if (server.hasArg("day")) day = server.arg("day").toInt();           // Dzień
  if (server.hasArg("hour")) hour = server.arg("hour").toInt();        // Godzina
  if (server.hasArg("minute")) minute = server.arg("minute").toInt();  // Minuty
  if (server.hasArg("second")) second = server.arg("second").toInt();  // Sekundy
  
    // Walidacja daty
    // if (!isValidDate(year, month, day, hour, minute, 0)) {
    //     server.send(400, "text/plain", "Invalid date");
    //     return;
    // }

    // Ustawienie RTC z pełnymi wartościami
    rtc.adjust(DateTime(year, month, day, hour, minute, second)); // Ustawienie czasu

    // Potwierdzenie zapisu ustawień zegara
    String html = "<!DOCTYPE html><html lang='pl'><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    //html += htmlStyle + "</head><body>";
    //html += generateNavigation("Ustawienia");
    html += "<h1>Ustawienia zegara zostały zapisane!</h1>";
    html += "</body></html>";

    server.send(200, "text/html", html);
}

void printRTCDateTime() {
  DateTime now = rtc.now();

  if (rtc.lostPower()) {
    Serial.println("Błąd: RTC stracił zasilanie!");
  } else {
    Serial.print("Odczyt z RTC: ");
    Serial.print(now.year());
    Serial.print("-");
    Serial.print(now.month());
    Serial.print("-");
    Serial.print(now.day());
    Serial.print(" ");
    Serial.print(now.hour());
    Serial.print(":");
    Serial.print(now.minute());
    Serial.print(":");
    Serial.println(now.second());
  }
}

void BLETask(void * pvParameters) {
  BLEDevice::init("ESP32-BLE");  // Inicjalizacja urządzenia BLE
  BLEServer *pServer = BLEDevice::createServer();  // Utworzenie serwera BLE
  BLEService *pService = pServer->createService("180A");  // Utworzenie usługi BLE
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         BLEUUID("2A29"),
                                         BLECharacteristic::PROPERTY_READ
                                       );
  pCharacteristic->setValue("ESP32 Device");
  pService->start();  // Uruchomienie usługi
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();  // Rozpoczęcie reklamowania BLE

  while (true) {
    if (pServer->getConnectedCount() > 0) {
      // BLE jest połączone, można dodać logikę
      Serial.println("BLE connected.");
    } else {
      Serial.println("Waiting for BLE connection...");
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Sprawdzanie co sekundę
    esp_task_wdt_reset();  // Reset Watchdoga, aby uniknąć restartu
  }
}

void WiFiTask(void * pvParameters) {
  WiFi.softAP(apSSID, apPassword);  // Konfiguracja ESP32 jako punkt dostępowy
  Serial.println("ESP32 działa jako punkt dostępowy.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());  // Wyświetlenie IP punktu dostępowego

  // Rozpocznij serwer WWW
  server.begin();  // Użyj globalnej zmiennej serwera WWW

  while (true) {
    server.handleClient();  // Obsługuje żądania HTTP
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Krótkie opóźnienie dla lepszej obsługi tasków
    esp_task_wdt_reset();  // Reset Watchdoga, aby uniknąć restartu
  }
}

void checkDaylightSavingTime(DateTime now) {
  // Sprawdzanie, czy jest ostatnia niedziela marca
  if (now.month() == 3 && now.dayOfTheWeek() == 0 && (now.day() + 7 > 31) && now.hour() == 2) {
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour() + 1, now.minute(), now.second()));
    Serial.println("Przełączono na czas letni.");
  }

  // Sprawdzanie, czy jest ostatnia niedziela października
  if (now.month() == 10 && now.dayOfTheWeek() == 0 && (now.day() + 7 > 31) && now.hour() == 3) {
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour() - 1, now.minute(), now.second()));
    Serial.println("Przełączono na czas zimowy.");
  }
}

// --- Pętla konfiguracji ---
void setup() {
  Serial.begin(115200);
  
  // Tworzenie konfiguracji Watchdog Timer
  // Sprawdzenie, czy Watchdog nie jest już zainicjalizowany
  if (esp_task_wdt_status(NULL) == ESP_OK) {
    // Watchdog już zainicjowany, nie inicjalizujemy go ponownie
    #if DEBUG
    Serial.println("Watchdog już został zainicjalizowany.");
    #endif 
  } else {
    // Tworzenie konfiguracji Watchdog Timer
    const esp_task_wdt_config_t wdtConfig = {
      .timeout_ms = 8000,        // Timeout w milisekundach (8 sekund)
      .idle_core_mask = 0,       // Maskowanie rdzeni (nie maskujemy)
      .trigger_panic = true      // Resetowanie systemu w razie zablokowania
    };
    esp_task_wdt_init(&wdtConfig);  // Inicjalizacja Watchdog Timer
    esp_task_wdt_add(NULL);         // Dodanie głównego tasku do WDT
    #if DEBUG
    Serial.println("Watchdog zainicjalizowany.");
    #endif
  }

  // I2C
  Wire.begin(8, 9);  //SDA = GPIO 8, SCL = GPIO 9

  // Inicjalizacja RTC (zegar czasu rzeczywistego)
  if (!rtc.begin()) {  // Sprawdzenie, czy zegar RTC został poprawnie zainicjalizowany
    #if DEBUG
    Serial.println("Couldn't find RTC");
    #endif
    while (1);  // Zatrzymaj działanie w przypadku błędu
  }

  DateTime now = rtc.now();  // Odczyt aktualnego czasu
  if (now.year() < 2024) {  // Jeśli zegar nie jest prawidłowo ustawiony, ustaw domyślną datę
    #if DEBUG
    Serial.println("Zegar nie jest prawidłowo ustawiony. Ustawiam na 1 stycznia 2024 00:00:00.");
    #endif
    rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));  // Ustawienie domyślnej daty
  }
  
  // Inicjalizacja czujnika temperatury DS18B20
  initializeDS18B20();
  //sensors.begin();

  // Inicjalizacja EEPROM i wczytanie ustawień
  EEPROM.begin(512);
  loadSettingsFromEEPROM();  // Wczytaj ustawienia z EEPROM
  
  // Tworzenie taska BLE
  xTaskCreate(
    BLETask,           // Funkcja taska BLE
    "BLE Task",        // Nazwa taska
    4096,              // Rozmiar stosu (4 KB)
    NULL,              // Parametry taska (NULL, bo nie są potrzebne)
    1,                 // Priorytet taska
    NULL               // Uchwyt taska (NULL, jeśli nie potrzebujemy uchwytu)
  );

  // Tworzenie taska WiFi/WebServer
  xTaskCreate(
    WiFiTask,          // Funkcja taska WiFi/WebServer
    "WiFi Task",       // Nazwa taska
    4096,              // Rozmiar stosu (4 KB)
    NULL,              // Parametry taska (NULL, bo nie są potrzebne)
    1,                 // Priorytet taska
    NULL               // Uchwyt taska (NULL, jeśli nie potrzebujemy uchwytu)
  );

    // Kadencja
    pinMode(CADENCE_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CADENCE_PIN), handleCadenceInterrupt, FALLING);
  
    // Prędkość
    pinMode(SPEED_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SPEED_PIN), handleSpeedInterrupt, FALLING);

  // Inicjalizacja tablicy uśredniania prędkości
  for (int i = 0; i < numSpeedReadings; i++) {
    speedReadings[i] = 0;
  }

  // Inicjalizacja wyświetlacza OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Inicjalizacja wyświetlacza
    #if DEBUG
      Serial.println(F("SSD1306 allocation failed"));
    #endif
    for (;;);  // Zatrzymaj działanie w przypadku błędu
  }
  display.clearDisplay();
  display.display();
  showWelcomeMessage();  // Wyświetlanie powitania na OLED

  // Konfiguracja przycisków
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonISR, CHANGE);

  // Konfiguracja świateł
  pinMode(FrontDayPin, OUTPUT);
  pinMode(FrontPin, OUTPUT);
  pinMode(RealPin, OUTPUT);
  setLights();
}

// --- PĘTLA GŁÓWNA ---
void loop() {
    DateTime now = rtc.now();     // Odczyt aktualnego czasu z RTC
    checkDaylightSavingTime(now); // Sprawdzenie i ewentualna zmiana czasu

    // Aktualizacja danych z czujników
    unsigned long currentMillis = millis();
 
    // Resetuj Watchdoga co cykl pętli, aby uniknąć restartu
    static unsigned long lastWdtReset = 0;
    if (millis() - lastWdtReset >= 1000) {
        esp_task_wdt_reset();
        lastWdtReset = millis();
    }

  // Obsługa krótkiego naciśnięcia
  if (buttonPressed && !longPressHandled) {
    // Sprawdź, czy przycisk jest nadal trzymany przez określony czas
    if (currentMillis - buttonHoldStart >= holdTime) {
      handleLongPress();       // Obsługa długiego naciśnięcia
      longPressHandled = true; // Flaga, że długie naciśnięcie zostało obsłużone
    } else if (buttonReleased) {
      handleButton();          // Obsługa krótkiego naciśnięcia
      buttonPressed = false;   // Reset flagi krótkiego naciśnięcia
    }
  }
  
  // Sprawdzanie i odczyt temperatury
  if (isGroundTemperatureReady()) {
    // Odczyt temperatury tylko jeśli jest gotowy
    currentTemp = readGroundTemperature();
    if (currentTemp != -999.0) {
      #if DEBUG
      Serial.print("Aktualna temperatura: ");
      Serial.println(currentTemp);
      #endif
    }

    // Po odczytaniu temperatury inicjujemy nowy pomiar
    requestGroundTemperature();
  }

  // Aktualizacja ekranu
  if (currentMillis - previousScreenMillis >= screenInterval) {
    previousScreenMillis = currentMillis;
    showScreen(currentScreen);
  }

  // Detekcja postoju - gdy prędkość wynosi 0 przez dłuższy czas
  if (speedKmh < 0.1) {
    if (currentMillis - lastStationaryTime > 10000) {
      stationary = true;
    }
  } else {
    lastStationaryTime = currentMillis;
    stationary = false;
  }

  // Jeśli rower nie jest w trybie postoju, kontynuujemy aktualizację
  if (!stationary && currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;

    // Dynamiczne dostosowanie interwału aktualizacji
    adjustUpdateInterval(speedKmh);

    // Przykładowa aktualizacja danych (zastąp rzeczywistymi danymi)
    updateData(current, voltage, speedKmh);

    // Obliczanie zasięgu
    range = calculateRange();
  }
  calculateSpeed();
}
