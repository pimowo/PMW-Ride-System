// --- Biblioteki ---
#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

// Utworzenie serwera na porcie 80
bool configModeActive = false;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

// Struktury dla ustawień
struct TimeSettings {
  bool ntpEnabled;
  int8_t hours;
  int8_t minutes;
  int8_t seconds;
  int8_t day;
  int8_t month;
  int16_t year;
};

struct LightSettings {
  enum LightMode {
    FRONT,
    REAR,
    BOTH
  };
  LightMode dayLights;
  bool dayBlink;
  LightMode nightLights;
  bool nightBlink;
  bool blinkEnabled;
  int blinkFrequency;  // ms
};

struct BacklightSettings {
  int dayBrightness;    // %
  int nightBrightness;  // %
  bool autoMode;
};

struct WiFiSettings {
  char ssid[32];
  char password[64];
};

// Globalne instancje ustawień
TimeSettings timeSettings;
LightSettings lightSettings;
BacklightSettings backlightSettings;
WiFiSettings wifiSettings;

// --- Definicje pinów ---
// Przyciski
#define BTN_UP 13
#define BTN_DOWN 14
#define BTN_SET 12
// Światła
#define FrontDayPin 5  // światła dzienne
#define FrontPin 18    // światła zwykłe
#define RealPin 19     // tylne światło
// Ładowarka USB
#define UsbPin 32  // ładowarka USB
// Czujnik temperatury powietrza
#define ONE_WIRE_BUS 15  // Pin do którego podłączony jest DS18B20

const uint8_t* czcionka_duza = u8g2_font_fur17_tr;
const uint8_t* czcionka_srednia = u8g2_font_pxplusibmvga9_mf; // górna belka
const uint8_t* czcionka_mala = u8g2_font_profont11_tr;  // opis ekranów

bool legalMode = false;  // false = normalny tryb, true = tryb legal
uint8_t displayBrightness = 16; // Wartość od 0 do 255
bool welcomeAnimationDone = false;  // Dodaj na początku pliku
void toggleLegalMode();            // Zdefiniuj tę funkcję
void showWelcomeMessage();         // Zdefiniuj tę funkcję

// Dodaj stałe jeśli ich nie ma
#define LEGAL_MODE_TIME 2000      // Czas przytrzymania dla trybu legal
// --- Stałe czasowe ---
const unsigned long DEBOUNCE_DELAY = 25;
const unsigned long BUTTON_DELAY = 200;
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long DOUBLE_CLICK_TIME = 300;
const unsigned long GOODBYE_DELAY = 3000;
const unsigned long SET_LONG_PRESS = 2000;
const unsigned long TEMP_REQUEST_INTERVAL = 1000;
const unsigned long DS18B20_CONVERSION_DELAY_MS = 750;

// --- Struktury i enumy ---
struct Settings {
  int wheelCircumference;
  float batteryCapacity;
  int daySetting;
  int nightSetting;
  bool dayRearBlink;
  bool nightRearBlink;
  unsigned long blinkInterval;
};

enum MainScreen {
  SPEED_SCREEN,      // Ekran prędkości
  CADENCE_SCREEN,    // Ekran kadencji
  TEMP_SCREEN,       // Ekran temperatur
  RANGE_SCREEN,      // Ekran zasięgu
  BATTERY_SCREEN,    // Ekran baterii
  POWER_SCREEN,      // Ekran mocy
  PRESSURE_SCREEN,   // Ekran ciśnienia
  USB_SCREEN,        // Ekran sterowania USB
  MAIN_SCREEN_COUNT  // Liczba głównych ekranów
};

enum SpeedSubScreen {
  SPEED_KMH,        // Aktualna prędkość
  SPEED_AVG_KMH,    // Średnia prędkość
  SPEED_MAX_KMH,    // Maksymalna prędkość
  SPEED_SUB_COUNT
};

enum CadenceSubScreen {
  CADENCE_RPM,      // Aktualna kadencja
  CADENCE_AVG_RPM,  // Średnia kadencja
  CADENCE_SUB_COUNT
};

enum TempSubScreen {
  TEMP_AIR,
  TEMP_CONTROLLER,
  TEMP_MOTOR,
  TEMP_SUB_COUNT
};

enum RangeSubScreen {
  RANGE_KM,
  DISTANCE_KM,
  ODOMETER_KM,
  RANGE_SUB_COUNT
};

enum BatterySubScreen {
  BATTERY_VOLTAGE,
  BATTERY_CURRENT,
  BATTERY_CAPACITY_WH,
  BATTERY_CAPACITY_AH,
  BATTERY_CAPACITY_PERCENT,
  BATTERY_SUB_COUNT
};

enum PowerSubScreen {
  POWER_W,
  POWER_AVG_W,
  POWER_MAX_W,
  POWER_SUB_COUNT
};

enum PressureSubScreen {
  PRESSURE_BAR,
  PRESSURE_VOLTAGE,
  PRESSURE_TEMP,
  PRESSURE_SUB_COUNT
};

// --- Zmienne stanu ekranu ---
MainScreen currentMainScreen = SPEED_SCREEN;
int currentSubScreen = 0;
bool inSubScreen = false;
bool displayActive = false;
bool showingWelcome = false;

#define PRESSURE_LEFT_MARGIN 70
#define PRESSURE_TOP_LINE 62
#define PRESSURE_BOTTOM_LINE 62

// --- Zmienne pomiarowe ---
float speed_kmh = 0;
int cadence_rpm = 0;
float temp_air = 0;
float temp_controller = 0;
float temp_motor = 0;
float range_km = 0;
float distance_km = 0;
float odometer_km = 0;
float battery_voltage = 0;
float battery_current = 0;
float battery_capacity_wh = 0;
float battery_capacity_ah = 0;
int battery_capacity_percent = 0;
int power_w = 0;
int power_avg_w = 0;
int power_max_w = 0;
float speed_avg_kmh = 0;
float speed_max_kmh = 0;
int cadence_avg_rpm = 0;
// Zmienne dla czujników ciśnienia
float pressure_bar = 0;           // przednie koło
float pressure_rear_bar = 0;      // tylne koło
float pressure_voltage = 0;       // napięcie przedniego czujnika
float pressure_rear_voltage = 0;  // napięcie tylnego czujnika
float pressure_temp = 0;          // temperatura przedniego czujnika
float pressure_rear_temp = 0;     // temperatura tylnego czujnika

// --- Zmienne dla czujnika temperatury ---
#define TEMP_ERROR -999.0
float currentTemp = DEVICE_DISCONNECTED_C;
bool temperatureReady = false;
bool conversionRequested = false;
unsigned long lastTempRequest = 0;
unsigned long ds18b20RequestTime;

// --- Zmienne dla przycisków ---
unsigned long lastClickTime = 0;
unsigned long lastButtonPress = 0;
unsigned long lastDebounceTime = 0;
unsigned long upPressStartTime = 0;
unsigned long downPressStartTime = 0;
unsigned long setPressStartTime = 0;
unsigned long messageStartTime = 0;
bool firstClick = false;
bool upLongPressExecuted = false;
bool downLongPressExecuted = false;
bool setLongPressExecuted = false;

// --- Zmienne konfiguracyjne ---
int assistLevel = 3;
bool assistLevelAsText = false;
int lightMode = 0;        // 0=off, 1=dzień, 2=noc
int assistMode = 0;       // 0=PAS, 1=STOP, 2=GAZ, 3=P+G
bool usbEnabled = false;  // Stan wyjścia USB

// --- Obiekty ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
RTC_DS3231 rtc;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Settings bikeSettings;
Settings storedSettings;

// --- Obiekty BLE ---
BLEClient* bleClient;
BLEAddress bmsMacAddress("a5:c2:37:05:8b:86");
BLERemoteService* bleService;
BLERemoteCharacteristic* bleCharacteristicTx;
BLERemoteCharacteristic* bleCharacteristicRx;

// --- Klasy pomocnicze ---
class TimeoutHandler {
private:
  uint32_t startTime;
  uint32_t timeoutPeriod;
  bool isRunning;

public:
  TimeoutHandler(uint32_t timeout_ms = 0)
    : startTime(0),
      timeoutPeriod(timeout_ms),
      isRunning(false) {}

  void start(uint32_t timeout_ms = 0) {
    if (timeout_ms > 0) timeoutPeriod = timeout_ms;
    startTime = millis();
    isRunning = true;
  }

  bool isExpired() {
    if (!isRunning) return false;
    return (millis() - startTime) >= timeoutPeriod;
  }

  void stop() {
    isRunning = false;
  }

  uint32_t getElapsed() {
    if (!isRunning) return 0;
    return (millis() - startTime);
  }
};

class TemperatureSensor {
private:
  static constexpr float INVALID_TEMP = -999.0f;
  static constexpr float MIN_VALID_TEMP = -50.0f;
  static constexpr float MAX_VALID_TEMP = 100.0f;
  bool conversionRequested = false;
  unsigned long lastRequestTime = 0;

public:
  void requestTemperature() {
    if (millis() - lastRequestTime >= TEMP_REQUEST_INTERVAL) {
      sensors.requestTemperatures();
      conversionRequested = true;
      lastRequestTime = millis();
    }
  }

  bool isValidTemperature(float temp) {
    return temp >= MIN_VALID_TEMP && temp <= MAX_VALID_TEMP;
  }

  float readTemperature() {
    if (!conversionRequested) return INVALID_TEMP;

    if (millis() - lastRequestTime < DS18B20_CONVERSION_DELAY_MS) {
      return INVALID_TEMP;  // Konwersja jeszcze trwa
    }

    float temp = sensors.getTempCByIndex(0);
    conversionRequested = false;
    return isValidTemperature(temp) ? temp : INVALID_TEMP;
  }
};

TemperatureSensor tempSensor;

// --- Deklaracje funkcji ---
void handleSettings(AsyncWebServerRequest *request);
void handleSaveClockSettings(AsyncWebServerRequest *request);
void handleSaveBikeSettings(AsyncWebServerRequest *request);

// void toggleLegalMode() {
//     legalMode = !legalMode;
    
//     display.clearBuffer();
//     display.setFont(czcionka_srednia);
    
//     // Obliczamy szerokość każdego tekstu
//     int width1 = display.getStrWidth("Tryb legalny");
//     int width2 = display.getStrWidth("zostal");
//     int width3 = display.getStrWidth(legalMode ? "wlaczony" : "wylaczony");
    
//     // Obliczamy pozycje x dla wycentrowania (szerokość ekranu - szerokość tekstu) / 2
//     int x1 = (128 - width1) / 2;
//     int x2 = (128 - width2) / 2;
//     int x3 = (128 - width3) / 2;
    
//     // Wyświetlamy wycentrowane teksty
//     display.drawStr(x1, 23, "Tryb legalny");
//     display.drawStr(x2, 38, "zostal");
//     display.drawStr(x3, 53, legalMode ? "wlaczony" : "wylaczony");
    
//     display.sendBuffer();
//     delay(2000);  // Pokazuj informację przez 2 sekundy
    
//     display.clearBuffer();
//     display.sendBuffer();
// }

void toggleLegalMode() {
    legalMode = !legalMode;
    
    display.clearBuffer();
        
    drawCenteredText("Tryb legalny", 20, czcionka_srednia);
    drawCenteredText("zostal", 35, czcionka_srednia);
    drawCenteredText(legalMode ? "wlaczony" : "wylaczony", 50, czcionka_srednia);
    
    display.sendBuffer();
    delay(1500);
    
    display.clearBuffer();
    display.sendBuffer();
}

// Funkcje BLE
void notificationCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  // Twoja funkcja obsługi powiadomień
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

void setDisplayBrightness(uint8_t brightness) {
    displayBrightness = brightness;
    display.setContrast(displayBrightness);
    // Opcjonalnie: zapisz wartość do EEPROM aby zapamiętać ustawienie
}

// Funkcje ustawień
// Wczytywanie ustawień z EEPROM
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

// Funkcje wyświetlacza
void drawHorizontalLine() {
  display.drawHLine(4, 15, 122);
  display.drawHLine(4, 50, 122);
}

void drawVerticalLine() {
  display.drawVLine(30, 20, 25);
  display.drawVLine(73, 20, 25);
}

void drawTopBar() {
  static bool colonVisible = true;
  static unsigned long lastColonToggle = 0;
  const unsigned long COLON_TOGGLE_INTERVAL = 500;  // Miganie co 500ms (pół sekundy)

  display.setFont(czcionka_srednia);

  // Pobierz aktualny czas z RTC
  DateTime now = rtc.now();

  // Czas z migającym dwukropkiem
  char timeStr[6];
  if (colonVisible) {
    sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
  } else {
    sprintf(timeStr, "%02d %02d", now.hour(), now.minute());
  }
  display.drawStr(0, 13, timeStr);

  // Przełącz stan dwukropka co COLON_TOGGLE_INTERVAL
  if (millis() - lastColonToggle >= COLON_TOGGLE_INTERVAL) {
    colonVisible = !colonVisible;
    lastColonToggle = millis();
  }

  // Bateria
  char battStr[5];
  sprintf(battStr, "%d%%", battery_capacity_percent);
  display.drawStr(60, 13, battStr);

  // Napięcie
  char voltStr[6];
  sprintf(voltStr, "%.0fV", battery_voltage);
  display.drawStr(100, 13, voltStr);
}

void drawLightStatus() {
  display.setFont(czcionka_mala);

  switch (lightMode) {
    case 1:
      display.drawStr(35, 36, "X");
      display.drawStr(35, 46, "Dzien");
      break;
    case 2:
      display.drawStr(35, 46, "Noc");
      break;
  }
}

// void drawAssistLevel() {
//     display.setFont(czcionka_duza);

//     if (assistLevelAsText) {
//         display.drawStr(8, 38, "T");  // Tryb tekstowy - tylko litera T
//     } else {
//         // Wyświetlanie poziomu asysty
//         if (legalMode) {
//             // Tryb legal - wyświetlanie w negatywie
//             int x = 15;
//             int y = 35;
//             int width = 18;   // Szerokość tła - dostosuj według potrzeb
//             int height = 28;  // Wysokość tła - dostosuj według potrzeb

//             // Rysuj białe tło
//             display.setDrawColor(1);
//             display.drawBox(x-2, y-height+5, width, height);

//             // Wyświetl tekst w negatywie (czarny na białym)
//             display.setDrawColor(0);
//             char levelStr[2];
//             sprintf(levelStr, "%d", assistLevel);
//             display.drawStr(x, y, levelStr);

//             // Przywróć normalny tryb rysowania
//             display.setDrawColor(1);
//         } else {
//             // Normalny tryb
//             char levelStr[2];
//             sprintf(levelStr, "%d", assistLevel);
//             display.drawStr(8, 38, levelStr);
//         }
//     }

//     display.setFont(czcionka_mala);
//     const char* modeText;
//     switch (assistMode) {
//         case 0:
//             modeText = "PAS";
//             break;
//         case 1:
//             modeText = "STOP";
//             break;
//         case 2:
//             modeText = "GAZ";
//             break;
//         case 3:
//             modeText = "P+G";
//             break;
//     }
//     display.drawStr(35, 26, modeText);
// }

void drawAssistLevel() {
    display.setFont(czcionka_duza);

    if (assistLevelAsText) {
        display.drawStr(8, 38, "T");  // Tryb tekstowy - tylko litera T
    } else {
        // Wyświetlanie poziomu asysty
        if (legalMode) {
            // Tryb legal - wyświetlanie w negatywie
            int x = 8;    // Ta sama pozycja x co w trybie normalnym
            int y = 38;   // Ta sama pozycja y co w trybie normalnym
            int width = 15;   // Szerokość tła
            int height = 25;  // Wysokość tła

            // Rysuj białe tło
            display.setDrawColor(1);
            display.drawBox(x-2, y-height+5, width, height);

            // Wyświetl tekst w negatywie (czarny na białym)
            display.setDrawColor(0);
            char levelStr[2];
            sprintf(levelStr, "%d", assistLevel);
            display.drawStr(x, y, levelStr);

            // Przywróć normalny tryb rysowania
            display.setDrawColor(1);
        } else {
            // Normalny tryb
            char levelStr[2];
            sprintf(levelStr, "%d", assistLevel);
            display.drawStr(8, 38, levelStr);
        }
    }

    display.setFont(czcionka_mala);
    const char* modeText;
    switch (assistMode) {
        case 0:
            modeText = "PAS";
            break;
        case 1:
            modeText = "STOP";
            break;
        case 2:
            modeText = "GAZ";
            break;
        case 3:
            modeText = "P+G";
            break;
    }
    display.drawStr(35, 26, modeText);
}

void drawValueAndUnit(const char* valueStr, const char* unitStr) {
    display.setFont(czcionka_mala);
    int unitWidth = display.getStrWidth(unitStr);
    int valueWidth = display.getStrWidth(valueStr);
    int spaceWidth = display.getStrWidth(" ");  // szerokość spacji
    
    // Całkowita szerokość = wartość + spacja + jednostka
    int totalWidth = valueWidth + spaceWidth + unitWidth;
    
    // Pozycja początkowa dla wartości (od prawej strony)
    int xPosValue = 128 - totalWidth;
    
    // Pozycja początkowa dla jednostki
    int xPosUnit = 128 - unitWidth;
    
    // Rysowanie wartości i jednostki
    display.drawStr(xPosValue, 62, valueStr);
    display.drawStr(xPosUnit, 62, unitStr);
}

void drawMainDisplay() {
    display.setFont(czcionka_mala);
    char valueStr[10];
    const char* unitStr;
    const char* descText;

    if (inSubScreen) {

        switch (currentMainScreen) {
      
            case SPEED_SCREEN:
                switch (currentSubScreen) {
                    case SPEED_KMH:
                        sprintf(valueStr, "%4.1f", speed_kmh);
                        unitStr = "km/h";
                        descText = "> Predkosc";
                        break;
                    case SPEED_AVG_KMH:
                        sprintf(valueStr, "%4.1f", speed_avg_kmh);
                        unitStr = "km/h";
                        descText = "> Pred. AVG";
                        break;
                    case SPEED_MAX_KMH:
                        sprintf(valueStr, "%4.1f", speed_max_kmh);
                        unitStr = "km/h";
                        descText = "> Pred. MAX";
                        break;
                }
                break;

            case CADENCE_SCREEN:
                switch (currentSubScreen) {
                    case CADENCE_RPM:
                        sprintf(valueStr, "%4d", cadence_rpm);
                        unitStr = "RPM";
                        descText = "> Kadencja";
                        break;
                    case CADENCE_AVG_RPM:
                        sprintf(valueStr, "%4d", cadence_avg_rpm);
                        unitStr = "RPM";
                        descText = "> Kad. AVG";
                        break;
                }
                break;

            case TEMP_SCREEN:
                switch (currentSubScreen) {
                    case TEMP_AIR:
                        if (currentTemp != TEMP_ERROR && currentTemp != DEVICE_DISCONNECTED_C) {
                            sprintf(valueStr, "%4.1f", currentTemp);
                        } else {
                            strcpy(valueStr, "---");
                        }
                        unitStr = "°C";
                        descText = "> Powietrze";
                        break;
                    case TEMP_CONTROLLER:
                        sprintf(valueStr, "%4.1f", temp_controller);
                        unitStr = "°C";
                        descText = "> Sterownik";
                        break;
                    case TEMP_MOTOR:
                        sprintf(valueStr, "%4.1f", temp_motor);
                        unitStr = "°C";
                        descText = "> Silnik";
                        break;
                }
                break;

            case RANGE_SCREEN:
                switch (currentSubScreen) {
                    case RANGE_KM:
                        sprintf(valueStr, "%4.1f", range_km);
                        unitStr = "km";
                        descText = "> Zasieg";
                        break;
                    case DISTANCE_KM:
                        sprintf(valueStr, "%4.1f", distance_km);
                        unitStr = "km";
                        descText = "> Dystans";
                        break;
                    case ODOMETER_KM:
                        sprintf(valueStr, "%4.0f", odometer_km);
                        unitStr = "km";
                        descText = "> Przebieg";
                        break;
                }
                break;

            case BATTERY_SCREEN:
                switch (currentSubScreen) {
                    case BATTERY_VOLTAGE:
                        sprintf(valueStr, "%4.1f", battery_voltage);
                        unitStr = "V";
                        descText = "> Napiecie";
                        break;
                    case BATTERY_CURRENT:
                        sprintf(valueStr, "%4.1f", battery_current);
                        unitStr = "A";
                        descText = "> Natezenie";
                        break;
                    case BATTERY_CAPACITY_WH:
                        sprintf(valueStr, "%4.0f", battery_capacity_wh);
                        unitStr = "Wh";
                        descText = "> Energia";
                        break;
                    case BATTERY_CAPACITY_AH:
                        sprintf(valueStr, "%4.1f", battery_capacity_wh);
                        unitStr = "Ah";
                        descText = "> Pojemnosc";
                        break;
                    case BATTERY_CAPACITY_PERCENT:
                        sprintf(valueStr, "%3d", battery_capacity_percent);
                        unitStr = "%";
                        descText = "> Bateria";
                        break;
                }
                break;

            case POWER_SCREEN:
                switch (currentSubScreen) {
                    case POWER_W:
                        sprintf(valueStr, "%4d", power_w);
                        unitStr = "W";
                        descText = "> Moc";
                        break;
                    case POWER_AVG_W:
                        sprintf(valueStr, "%4d", power_avg_w);
                        unitStr = "W";
                        descText = "> Moc AVG";
                        break;
                    case POWER_MAX_W:
                        sprintf(valueStr, "%4d", power_max_w);
                        unitStr = "W";
                        descText = "> Moc MAX";
                        break;
                }
                break;

            case PRESSURE_SCREEN:
                char combinedStr[16];
                switch (currentSubScreen) {
                    case PRESSURE_BAR:
                        sprintf(combinedStr, "%.2f/%.2f", pressure_bar, pressure_rear_bar);
                        strcpy(valueStr, combinedStr);
                        unitStr = "bar";
                        descText = "> Cis.";
                        break;
                    case PRESSURE_VOLTAGE:
                        sprintf(combinedStr, "%.2f/%.2f", pressure_voltage, pressure_rear_voltage);
                        strcpy(valueStr, combinedStr);
                        unitStr = "V";
                        descText = "> Bateria";
                        break;
                    case PRESSURE_TEMP:
                        sprintf(combinedStr, "%.1f/%.1f", pressure_temp, pressure_rear_temp);
                        strcpy(valueStr, combinedStr);
                        unitStr = "°C";
                        descText = "> Temp.";
                        break;
                }
                break;
        }

    } else {
        // Wyświetlanie głównych ekranów
        switch (currentMainScreen) {

            case SPEED_SCREEN:
                sprintf(valueStr, "%4.1f", speed_kmh);
                unitStr = "km/h";
                descText = "Predkosc";
                break;
          
            case CADENCE_SCREEN:
                sprintf(valueStr, "%4d", cadence_rpm);
                unitStr = "RPM";
                descText = "Kadencja";
                break;

            case TEMP_SCREEN:
                if (currentTemp != TEMP_ERROR && currentTemp != DEVICE_DISCONNECTED_C) {
                    sprintf(valueStr, "%4.1f", currentTemp);
                } else {
                    strcpy(valueStr, "---");
                }
                unitStr = "°C";
                descText = "Temperatura";
                break;

            case RANGE_SCREEN:
                sprintf(valueStr, "%4.1f", range_km);
                unitStr = "km";
                descText = "Zasieg";
                break;

            case BATTERY_SCREEN:
                sprintf(valueStr, "%3d", battery_capacity_percent);
                unitStr = "%";
                descText = "Bateria";
                break;

            case POWER_SCREEN:
                sprintf(valueStr, "%4d", power_w);
                unitStr = "W";
                descText = "Moc";
                break;

            case PRESSURE_SCREEN:
                sprintf(valueStr, "%.1f/%.1f", pressure_bar, pressure_rear_bar);
                unitStr = "bar";
                descText = "Kola";
                break;

            case USB_SCREEN:
                display.setFont(czcionka_mala);
                display.drawStr(73, 62, usbEnabled ? "Wlaczone" : "Wylaczone");
                descText = "Wyjscie USB";
                break;
        }
    }

    // Stały odczyt prędkości
    char speedStr[10];
    sprintf(speedStr, "%4.1f", speed_kmh);  // Formatowanie prędkości
    
    // Wyświetl prędkość dużą czcionką
    display.setFont(czcionka_duza);
    display.drawStr(80, 38, speedStr);
    
    // Wyświetl jednostkę małą czcionką pod prędkością
    display.setFont(czcionka_mala);
    display.drawStr(105, 47, "km/h");

    // Wyświetl wartości tylko jeśli nie jesteśmy na ekranie USB
    if (currentMainScreen != USB_SCREEN) {
        drawValueAndUnit(valueStr, unitStr);
    }

    display.setFont(czcionka_mala);
    display.drawStr(0, 62, descText);
}

// --- Funkcja wyświetlania animacji powitania ---
void showWelcomeMessage() {
    display.clearBuffer();
    display.setFont(czcionka_srednia);

    // Tekst "Witaj!" na środku
    String welcomeText = "Witaj!";
    int welcomeWidth = display.getStrWidth(welcomeText.c_str());
    int welcomeX = (128 - welcomeWidth) / 2;

    // Tekst przewijany
    String scrollText = "e-Bike System   ";
    int messageWidth = display.getStrWidth(scrollText.c_str());
    int x = 128; // Start poza prawą krawędzią

    unsigned long lastUpdate = millis();
    while (x > -messageWidth) { // Przewijaj aż tekst zniknie z lewej strony
        unsigned long currentMillis = millis();
        if (currentMillis - lastUpdate >= 5) { // Aktualizuj co 10ms dla płynności
            display.clearBuffer();
            
            // Statyczny tekst "Witaj!"
            display.drawStr(welcomeX, 32, welcomeText.c_str());
            
            // Przewijany tekst "e-Bike System"
            display.drawStr(x, 53, scrollText.c_str());
            display.sendBuffer();
            
            x--; // Prędkość przewijania
            lastUpdate = currentMillis;
        }
    }

    welcomeAnimationDone = true;
}

void handleButtons() {

    if (configModeActive) {
        return; // W trybie konfiguracji nie obsługuj normalnych funkcji przycisków
    }

    unsigned long currentTime = millis();
    bool setState = digitalRead(BTN_SET);
    bool upState = digitalRead(BTN_UP);
    bool downState = digitalRead(BTN_DOWN);

    static unsigned long lastButtonCheck = 0;
    static unsigned long bothPressStart = 0;
    static bool upPressed = false;
    static bool downPressed = false;
    static unsigned long upPressTime = 0;
    static unsigned long downPressTime = 0;
    const unsigned long buttonDebounce = 50;

    // Sprawdzanie trybu legal (UP + SET) - tylko gdy wyświetlacz jest aktywny
    static unsigned long legalModeStart = 0;
    if (displayActive && !showingWelcome && !upState && !setState) {
        if (legalModeStart == 0) {
            legalModeStart = currentTime;
        } else if ((currentTime - legalModeStart) > 1000) { // 2 sekundy  przytrzymania
            toggleLegalMode();
            while (!digitalRead(BTN_UP) || !digitalRead(BTN_SET)) {
                delay(10); // Czekaj na puszczenie przycisków
            }
            legalModeStart = 0;
            return;
        }
    } else {
        legalModeStart = 0;
    }

  // Obsługa włączania/wyłączania wyświetlacza
  if (!displayActive) {
    if (!setState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (!setPressStartTime) {
        setPressStartTime = currentTime;
      } else if (!setLongPressExecuted && (currentTime - setPressStartTime) > SET_LONG_PRESS) {
        if (!welcomeAnimationDone) {
            showWelcomeMessage();  // Pokaż animację powitania
        } 
        messageStartTime = currentTime;
        setLongPressExecuted = true;
        showingWelcome = true;
        displayActive = true;
      }
    } else if (setState && setPressStartTime) {
      setPressStartTime = 0;
      setLongPressExecuted = false;
      lastDebounceTime = currentTime;
    }
    return;
  }

  // Obsługa przycisków gdy wyświetlacz jest aktywny
  if (!showingWelcome) {
    // Obsługa przycisku UP (zmiana asysty)
    if (!upState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (!upPressStartTime) {
        upPressStartTime = currentTime;
      } else if (!upLongPressExecuted && (currentTime - upPressStartTime) > LONG_PRESS_TIME) {
        lightMode = (lightMode + 1) % 3;
        setLights();
        upLongPressExecuted = true;
      }
    } else if (upState && upPressStartTime) {
      if (!upLongPressExecuted && (currentTime - upPressStartTime) < LONG_PRESS_TIME) {
        if (assistLevel < 5) assistLevel++;
      }
      upPressStartTime = 0;
      upLongPressExecuted = false;
      lastDebounceTime = currentTime;
    }

    // Obsługa przycisku DOWN (zmiana asysty)
    if (!downState && (currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (!downPressStartTime) {
        downPressStartTime = currentTime;
      } else if (!downLongPressExecuted && (currentTime - downPressStartTime) > LONG_PRESS_TIME) {
        assistLevelAsText = !assistLevelAsText;
        downLongPressExecuted = true;
      }
    } else if (downState && downPressStartTime) {
      if (!downLongPressExecuted && (currentTime - downPressStartTime) < LONG_PRESS_TIME) {
        if (assistLevel > 0) assistLevel--;
      }
      downPressStartTime = 0;
      downLongPressExecuted = false;
      lastDebounceTime = currentTime;
    }

    // Obsługa przycisku SET
    static unsigned long lastSetRelease = 0;
    static bool waitingForSecondClick = false;

    if (!setState) {  // Przycisk wciśnięty
      if (!setPressStartTime) {
        setPressStartTime = currentTime;
      } else if (!setLongPressExecuted && (currentTime - setPressStartTime) > SET_LONG_PRESS) {
        // Długie przytrzymanie (>3s) - wyłączenie
        display.clearBuffer();
        display.setFont(czcionka_srednia);
        display.drawStr(5, 32, "Do widzenia :)");
        display.sendBuffer();
        messageStartTime = currentTime;
        setLongPressExecuted = true;
      }
    } else if (setPressStartTime) {  // Przycisk puszczony
      if (!setLongPressExecuted) {
        unsigned long releaseTime = currentTime;

        if (waitingForSecondClick && (releaseTime - lastSetRelease) < DOUBLE_CLICK_TIME) {
          // Podwójne kliknięcie
          if (currentMainScreen == USB_SCREEN) {
            // Przełącz stan USB
            usbEnabled = !usbEnabled;
            digitalWrite(UsbPin, usbEnabled ? HIGH : LOW);
          } else if (inSubScreen) {
            inSubScreen = false;  // Wyjście z pod-ekranów
          } else if (hasSubScreens(currentMainScreen)) {
            inSubScreen = true;  // Wejście do pod-ekranów
            currentSubScreen = 0;
          }
          waitingForSecondClick = false;
        } else {
          // Pojedyncze kliknięcie
          if (!waitingForSecondClick) {
            waitingForSecondClick = true;
            lastSetRelease = releaseTime;
          } else if ((releaseTime - lastSetRelease) >= DOUBLE_CLICK_TIME) {
            // Przełączanie ekranów/pod-ekranów
            if (inSubScreen) {
              currentSubScreen = (currentSubScreen + 1) % getSubScreenCount(currentMainScreen);
            } else {
              currentMainScreen = (MainScreen)((currentMainScreen + 1) % MAIN_SCREEN_COUNT);
            }
            waitingForSecondClick = false;
          }
        }
      }
      setPressStartTime = 0;
      setLongPressExecuted = false;
      lastDebounceTime = currentTime;
    }

    // Reset flagi oczekiwania na drugie kliknięcie po upływie czasu
    if (waitingForSecondClick && (currentTime - lastSetRelease) >= DOUBLE_CLICK_TIME) {
      // Wykonaj akcję pojedynczego kliknięcia
      if (inSubScreen) {
        currentSubScreen = (currentSubScreen + 1) % getSubScreenCount(currentMainScreen);
      } else {
        currentMainScreen = (MainScreen)((currentMainScreen + 1) % MAIN_SCREEN_COUNT);
      }
      waitingForSecondClick = false;
    }
  }

  // Obsługa komunikatów powitalnych/pożegnalnych
  if (messageStartTime > 0 && (currentTime - messageStartTime) >= GOODBYE_DELAY) {
    if (!showingWelcome) {
      displayActive = false;
      goToSleep();
    }
    messageStartTime = 0;
    showingWelcome = false;
    }
}

void checkConfigMode() {
    static unsigned long upDownPressTime = 0;
    static bool bothButtonsPressed = false;

    if (!digitalRead(BTN_UP) && !digitalRead(BTN_DOWN)) {
        if (!bothButtonsPressed) {
            bothButtonsPressed = true;
            upDownPressTime = millis();
        } else if ((millis() - upDownPressTime > 500) && !configModeActive) {
            activateConfigMode();
        }
    } else {
        bothButtonsPressed = false;
    }
}

void activateConfigMode() {
    configModeActive = true;
        
    WiFi.mode(WIFI_AP);
    WiFi.softAP("e-Bike System", "#mamrower");
    
    // Konfiguracja i uruchomienie serwera WWW
    setupWebServer();
    server.begin();
}

void deactivateConfigMode() {
    if (!configModeActive) return;

    WiFi.softAPdisconnect(true);
    server.end();  // Dla AsyncWebServer używamy end() zamiast stop()
    WiFi.mode(WIFI_OFF);
    
    configModeActive = false;
    
    display.clearBuffer();
    display.sendBuffer();
}

// Funkcja pomocnicza sprawdzająca czy ekran ma pod-ekrany
bool hasSubScreens(MainScreen screen) {
  switch (screen) {
    case SPEED_SCREEN: return SPEED_SUB_COUNT > 1;
    case CADENCE_SCREEN: return CADENCE_SUB_COUNT > 1;
    case TEMP_SCREEN: return TEMP_SUB_COUNT > 1;
    case RANGE_SCREEN: return RANGE_SUB_COUNT > 1;
    case BATTERY_SCREEN: return BATTERY_SUB_COUNT > 1;
    case POWER_SCREEN: return POWER_SUB_COUNT > 1;
    case PRESSURE_SCREEN: return PRESSURE_SUB_COUNT > 1;
    case USB_SCREEN: return false;
    default: return false;
  }
}

// Funkcja pomocnicza zwracająca liczbę pod-ekranów dla danego ekranu
int getSubScreenCount(MainScreen screen) {
  switch (screen) {
    case SPEED_SCREEN: return SPEED_SUB_COUNT;
    case CADENCE_SCREEN: return CADENCE_SUB_COUNT;
    case TEMP_SCREEN: return TEMP_SUB_COUNT;
    case RANGE_SCREEN: return RANGE_SUB_COUNT;
    case BATTERY_SCREEN: return BATTERY_SUB_COUNT;
    case POWER_SCREEN: return POWER_SUB_COUNT;
    case PRESSURE_SCREEN: return PRESSURE_SUB_COUNT;
    default: return 0;
  }
}

// Funkcje zarządzania energią
void goToSleep() {
  // Wyłącz wszystkie LEDy
  digitalWrite(FrontDayPin, LOW);
  digitalWrite(FrontPin, LOW);
  digitalWrite(RealPin, LOW);
  digitalWrite(UsbPin, LOW);

  delay(50);

  // Wyłącz OLED
  display.clearBuffer();
  display.sendBuffer();
  display.setPowerSave(1);  // Wprowadź OLED w tryb oszczędzania energii

  // Konfiguracja wybudzania przez przycisk SET
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0);  // GPIO12 (BTN_SET) stan niski

  // Wejście w deep sleep
  esp_deep_sleep_start();
}

void setLights() {
    if (lightMode == 0) {
        digitalWrite(FrontDayPin, LOW);
        digitalWrite(FrontPin, LOW);
        digitalWrite(RealPin, LOW);
    } else if (lightMode == 1) {
        digitalWrite(FrontDayPin, HIGH);
        digitalWrite(FrontPin, LOW);
        digitalWrite(RealPin, HIGH);
    } else if (lightMode == 2) {
        digitalWrite(FrontDayPin, LOW);
        digitalWrite(FrontPin, HIGH);
        digitalWrite(RealPin, HIGH);
    }
}

// Funkcje czujnika temperatury
void initializeDS18B20() {
  sensors.begin();
}

void requestGroundTemperature() {
  sensors.requestTemperatures();
  ds18b20RequestTime = millis();
}

bool isGroundTemperatureReady() {
  return millis() - ds18b20RequestTime >= DS18B20_CONVERSION_DELAY_MS;
}

bool isValidTemperature(float temp) {
  return (temp >= -50.0 && temp <= 100.0);
}

float readGroundTemperature() {
  if (isGroundTemperatureReady()) {
    float temperature = sensors.getTempCByIndex(0);
    if (isValidTemperature(temperature)) {
      return temperature;
    } else {
      return -999.0;
    }
  }
  return -999.0;
}

void handleTemperature() {
  unsigned long currentMillis = millis();

  if (!conversionRequested && (currentMillis - lastTempRequest >= TEMP_REQUEST_INTERVAL)) {
    sensors.requestTemperatures();
    conversionRequested = true;
    lastTempRequest = currentMillis;
  }

  if (conversionRequested && (currentMillis - lastTempRequest >= 750)) {
    currentTemp = sensors.getTempCByIndex(0);
    conversionRequested = false;
  }
}

// Główne funkcje programu

// Funkcja ładowania ustawień z LittleFS
void loadSettings() {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, configFile);

  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  // Wczytywanie ustawień czasu
  if (doc.containsKey("time")) {
    timeSettings.ntpEnabled = doc["time"]["ntpEnabled"] | false;
    timeSettings.hours = doc["time"]["hours"] | 0;
    timeSettings.minutes = doc["time"]["minutes"] | 0;
    timeSettings.seconds = doc["time"]["seconds"] | 0;
    timeSettings.day = doc["time"]["day"] | 1;
    timeSettings.month = doc["time"]["month"] | 1;
    timeSettings.year = doc["time"]["year"] | 2024;
  }

  // Wczytywanie ustawień świateł
  if (doc.containsKey("light")) {
    lightSettings.dayLights = static_cast<LightSettings::LightMode>(doc["light"]["dayLights"] | 0);
    lightSettings.dayBlink = doc["light"]["dayBlink"] | false;
    lightSettings.nightLights = static_cast<LightSettings::LightMode>(doc["light"]["nightLights"] | 0);
    lightSettings.nightBlink = doc["light"]["nightBlink"] | false;
    lightSettings.blinkEnabled = doc["light"]["blinkEnabled"] | false;
    lightSettings.blinkFrequency = doc["light"]["blinkFrequency"] | 500;
  }

  // Wczytywanie ustawień podświetlenia
  if (doc.containsKey("backlight")) {
    backlightSettings.dayBrightness = doc["backlight"]["dayBrightness"] | 100;
    backlightSettings.nightBrightness = doc["backlight"]["nightBrightness"] | 50;
    backlightSettings.autoMode = doc["backlight"]["autoMode"] | false;
  }

  // Wczytywanie ustawień WiFi
  if (doc.containsKey("wifi")) {
    strlcpy(wifiSettings.ssid, doc["wifi"]["ssid"] | "", sizeof(wifiSettings.ssid));
    strlcpy(wifiSettings.password, doc["wifi"]["password"] | "", sizeof(wifiSettings.password));
  }

  configFile.close();
}

// Funkcja zapisu ustawień do LittleFS
void saveSettings() {
  StaticJsonDocument<1024> doc;

  // Zapisywanie ustawień czasu
  JsonObject timeObj = doc.createNestedObject("time");
  timeObj["ntpEnabled"] = timeSettings.ntpEnabled;
  timeObj["hours"] = timeSettings.hours;
  timeObj["minutes"] = timeSettings.minutes;
  timeObj["seconds"] = timeSettings.seconds;
  timeObj["day"] = timeSettings.day;
  timeObj["month"] = timeSettings.month;
  timeObj["year"] = timeSettings.year;

  // Zapisywanie ustawień świateł
  JsonObject lightObj = doc.createNestedObject("light");
  lightObj["dayLights"] = static_cast<int>(lightSettings.dayLights);
  lightObj["dayBlink"] = lightSettings.dayBlink;
  lightObj["nightLights"] = static_cast<int>(lightSettings.nightLights);
  lightObj["nightBlink"] = lightSettings.nightBlink;
  lightObj["blinkEnabled"] = lightSettings.blinkEnabled;
  lightObj["blinkFrequency"] = lightSettings.blinkFrequency;

  // Zapisywanie ustawień podświetlenia
  JsonObject backlightObj = doc.createNestedObject("backlight");
  backlightObj["dayBrightness"] = backlightSettings.dayBrightness;
  backlightObj["nightBrightness"] = backlightSettings.nightBrightness;
  backlightObj["autoMode"] = backlightSettings.autoMode;

  // Zapisywanie ustawień WiFi
  JsonObject wifiObj = doc.createNestedObject("wifi");
  wifiObj["ssid"] = wifiSettings.ssid;
  wifiObj["password"] = wifiSettings.password;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to write config file");
  }

  configFile.close();
}

// W funkcji setup(), po inicjalizacji wyświetlacza, dodaj:
void setupWebServer() {
  // Serwowanie plików statycznych
  server.serveStatic("/", LittleFS, "/");

  // Endpoint dla aktualnego stanu
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
      StaticJsonDocument<512> doc;
      
      // Pobierz czas z RTC
      DateTime now = rtc.now();
      
      // Utwórz obiekt czasu
      JsonObject timeObj = doc.createNestedObject("time");
      
      // Format czasu dla wyświetlania
      char timeStr[64];
      snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());
      
      // Dodaj czas w dwóch formatach
      timeObj["formatted"] = timeStr;  // Format czytelny
      timeObj["timezone"] = "Europe/Warsaw";
      
      // Dodaj poszczególne komponenty czasu
      timeObj["hours"] = now.hour();
      timeObj["minutes"] = now.minute();
      timeObj["seconds"] = now.second();
      timeObj["day"] = now.day();
      timeObj["month"] = now.month();
      timeObj["year"] = now.year();

      // Dodaj stan świateł jako osobny obiekt
      JsonObject lightsObj = doc.createNestedObject("lights");
      lightsObj["frontDay"] = digitalRead(FrontDayPin);
      lightsObj["front"] = digitalRead(FrontPin);
      lightsObj["rear"] = digitalRead(RealPin);

      // Dodaj temperaturę
      doc["temperature"] = temperatureRead();

      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response);
  });

  // Endpoint dla ustawień świateł
  server.on("/api/lights", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("data", true)) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, request->getParam("data", true)->value());

      if (!error) {
        // Aktualizuj ustawienia świateł
        if (doc.containsKey("frontDay")) {
          digitalWrite(FrontDayPin, doc["frontDay"].as<bool>());
        }
        if (doc.containsKey("front")) {
          digitalWrite(FrontPin, doc["front"].as<bool>());
        }
        if (doc.containsKey("rear")) {
          digitalWrite(RealPin, doc["rear"].as<bool>());
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      }
    }
  });

  server.on("/api/time", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("data", true)) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, request->getParam("data", true)->value());

      if (!error) {
        int year = doc["year"] | 2024;
        int month = doc["month"] | 1;
        int day = doc["day"] | 1;
        int hour = doc["hour"] | 0;
        int minute = doc["minute"] | 0;
        int second = doc["second"] | 0;

        rtc.adjust(DateTime(year, month, day, hour, minute, second));
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      }
    }
  });

  ws.onEvent([](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
    }
    });
    server.addHandler(&ws);

  // Start serwera
  server.begin();
}

// Inicjalizacja domyślnych wartości
void initializeDefaultSettings() {
  // Czas
  timeSettings.ntpEnabled = false;
  timeSettings.hours = 0;
  timeSettings.minutes = 0;
  timeSettings.seconds = 0;
  timeSettings.day = 1;
  timeSettings.month = 1;
  timeSettings.year = 2024;

  // Światła
  lightSettings.dayLights = LightSettings::FRONT;
  lightSettings.dayBlink = false;
  lightSettings.nightLights = LightSettings::BOTH;
  lightSettings.nightBlink = false;
  lightSettings.blinkEnabled = false;
  lightSettings.blinkFrequency = 500;

  // Podświetlenie
  backlightSettings.dayBrightness = 100;
  backlightSettings.nightBrightness = 50;
  backlightSettings.autoMode = false;

  // WiFi - początkowo puste
  memset(wifiSettings.ssid, 0, sizeof(wifiSettings.ssid));
  memset(wifiSettings.password, 0, sizeof(wifiSettings.password));
}

// Funkcja synchronizacji czasu przez NTP
void synchronizeTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                      timeinfo.tm_mday, timeinfo.tm_hour,
                      timeinfo.tm_min, timeinfo.tm_sec));
}

// Funkcja aktualizacji podświetlenia
void updateBacklight() {
  if (backlightSettings.autoMode) {
    // Auto mode - ustaw jasność na podstawie trybu dzień/noc
    // Zakładam, że masz zmienną określającą tryb dzienny/nocny
    bool isDayMode = true;  // Tu trzeba dodać właściwą logikę
    int brightness = isDayMode ? backlightSettings.dayBrightness : backlightSettings.nightBrightness;
    // Tu dodaj kod ustawiający jasność wyświetlacza
  } else {
    // Manual mode - ustaw stałą jasność
    // Tu dodaj kod ustawiający jasność wyświetlacza
  }
}

// Funkcja do ustawienia jasności w %
// void ustawJasnosc(int jasnosc_procent) {
//   // Przekształcenie procentów na zakres 0-255
//   int kontrast = map(jasnosc_procent, 0, 100, 0, 255);

//   // Ustawienie kontrastu
//   u8g2.setContrast(kontrast);
// }

#include <esp_partition.h>

// Sprawdzenie i formatowanie systemu plików przy starcie
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    if (!LittleFS.format()) {
      Serial.println("LittleFS Format Failed");
      return;
    }
    if (!LittleFS.begin()) {
      Serial.println("LittleFS Mount Failed After Format");
      return;
    }
  }
  Serial.println("LittleFS Mounted Successfully");
}

void listFiles() {
  Serial.println("Files in LittleFS:");
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("- Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

bool loadConfig() {
    if(!LittleFS.exists("/config.json")) {
        Serial.println("Creating default config file...");
        // Tworzymy domyślną konfigurację
        StaticJsonDocument<512> defaultConfig;
        defaultConfig["version"] = "1.0.0";
        defaultConfig["timezone"] = "Europe/Warsaw";
        defaultConfig["lastUpdate"] = "";
        
        File configFile = LittleFS.open("/config.json", "w");
        if(!configFile) {
            Serial.println("Failed to create config file");
            return false;
        }
        serializeJson(defaultConfig, configFile);
        configFile.close();
    }
    
    // Czytamy konfigurację
    File configFile = LittleFS.open("/config.json", "r");
    if(!configFile) {
        Serial.println("Failed to open config file");
        return false;
    }
    
    Serial.println("Config file loaded successfully");
    return true;
}

// Synchronizacja RTC z NTP
void syncRTCWithNTP() {
    configTime(0, 0, "pool.ntp.org");
    delay(1000);
    time_t now;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        rtc.adjust(DateTime(
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec
        ));
    }
}

// void setup() {
//   // Sprawdź przyczynę wybudzenia
//   esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

//   Serial.begin(115200);

//   // Inicjalizacja I2C
//   Wire.begin();

//   // Inicjalizacja DS18B20
//   initializeDS18B20();
//   sensors.setWaitForConversion(false);  // Ważne - tryb nieblokujący
//   sensors.setResolution(12);            // Ustaw najwyższą rozdzielczość
//   tempSensor.requestTemperature();      // Pierwsze żądanie pomiaru

//   // Inicjalizacja RTC
//   if (!rtc.begin()) {
//     Serial.println("Couldn't find RTC");
//     while (1)
//       ;
//   }

//   // Sprawdzenie RTC
//   if (rtc.lostPower()) {
//     Serial.println("RTC lost power, lets set the time!");
//     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//   }

//   if (!LittleFS.begin(true)) {
//     Serial.println("An Error has occurred while mounting LittleFS");
//     return;
//   }

//   listFiles();

//   // Dodaj obsługę serwera plików
//   server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

//   // Dodaj obsługę MIME types
//   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
//       request->send(LittleFS, "/style.css", "text/css");
//   });

//   server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
//       request->send(LittleFS, "/script.js", "application/javascript");
//   });

//   // Ładujemy konfigurację
//   if(!loadConfig()) {
//       Serial.println("Config load failed - using defaults");
//   }

//   configTime(3600, 3600, "pool.ntp.org"); // GMT+1 + DST
//   setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Strefa czasowa dla Polski
//   tzset();

//   // Inicjalizacja domyślnych ustawień
//   initializeDefaultSettings();

//   // Wczytanie zapisanych ustawień
//   loadSettings();

//   // Endpoint testowy
//   server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request){
//       request->send(200, "text/plain", "Server is running!");
//   });

//   // Konfiguracja pinów
//   pinMode(BTN_UP, INPUT_PULLUP);
//   pinMode(BTN_DOWN, INPUT_PULLUP);
//   pinMode(BTN_SET, INPUT_PULLUP);

//   // Konfiguracja pinów LED
//   pinMode(FrontDayPin, OUTPUT);
//   pinMode(FrontPin, OUTPUT);
//   pinMode(RealPin, OUTPUT);
//   digitalWrite(FrontDayPin, LOW);
//   digitalWrite(FrontPin, LOW);
//   digitalWrite(RealPin, LOW);
//   setLights();

//   // Ładowarka USB
//   pinMode(UsbPin, OUTPUT);
//   digitalWrite(UsbPin, LOW);

//   // Inicjalizacja wyświetlacza
//   display.begin();
//   display.enableUTF8Print();
//   display.setFontDirection(0);

//   // Ustawienie jasności na 50%
//   //ustawJasnosc(50);

//   // Wyczyść wyświetlacz na starcie
//   display.clearBuffer();
//   display.sendBuffer();

//   // Konfiguracja WiFi - dodaj po inicjalizacji wyświetlacza
//   WiFi.mode(WIFI_AP);
//   WiFi.softAP("e-Bike System", "#mamrower");  // Zmień nazwę i hasło według potrzeb

//   // Konfiguracja serwera WWW
//   setupWebServer();

//   // Jeśli wybudzenie przez przycisk SET, poczekaj na długie naciśnięcie
//   if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
//     unsigned long startTime = millis();
//     while (!digitalRead(BTN_SET)) {  // Czekaj na puszczenie przycisku
//       if ((millis() - startTime) > SET_LONG_PRESS) {
//         displayActive = true;
//         showingWelcome = true;
//         messageStartTime = millis();

//         display.clearBuffer();
//         display.setFont(czcionka_srednia);
//         display.drawStr(40, 32, "Witaj!");
//         display.sendBuffer();

//         while (!digitalRead(BTN_SET)) {  // Czekaj na puszczenie przycisku
//           delay(10);
//         }
//         break;
//       }
//       delay(10);
//     }
//   }

//   // Ustaw aktualną jasność podświetlenia
//   updateBacklight();
// }

void handleSettings(AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html lang='pl'>";
    // ... (reszta kodu generującego stronę)
    request->send(200, "text/html", html);
}

void handleSaveClockSettings(AsyncWebServerRequest *request) {
    if (request->hasParam("hour")) {
        int hour = request->getParam("hour")->value().toInt();
        // ... (reszta kodu obsługi ustawień zegara)
    }
    request->redirect("/");
}

void handleSaveBikeSettings(AsyncWebServerRequest *request) {
    if (request->hasParam("wheel")) {
        bikeSettings.wheelCircumference = request->getParam("wheel")->value().toInt();
        // ... (reszta kodu obsługi ustawień roweru)
    }
    request->redirect("/");
}

void setup() {
    // Sprawdź przyczynę wybudzenia
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    Serial.begin(115200);
    
    // Inicjalizacja I2C
    Wire.begin();

    // Inicjalizacja DS18B20
    initializeDS18B20();
    sensors.setWaitForConversion(false);  // Tryb nieblokujący
    sensors.setResolution(12);            // Najwyższa rozdzielczość
    tempSensor.requestTemperature();      // Pierwsze żądanie pomiaru

    // Inicjalizacja RTC
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, lets set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Konfiguracja pinów
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SET, INPUT_PULLUP);

    // Konfiguracja pinów LED
    pinMode(FrontDayPin, OUTPUT);
    pinMode(FrontPin, OUTPUT);
    pinMode(RealPin, OUTPUT);
    digitalWrite(FrontDayPin, LOW);
    digitalWrite(FrontPin, LOW);
    digitalWrite(RealPin, LOW);
    setLights();

    // Ładowarka USB
    pinMode(UsbPin, OUTPUT);
    digitalWrite(UsbPin, LOW);

    // Inicjalizacja wyświetlacza
    display.begin();
    display.setContrast(displayBrightness); // Ustaw początkowy kontrast
    display.enableUTF8Print();
    display.setFontDirection(0);
    display.clearBuffer();
    display.sendBuffer();

    // Jeśli wybudzenie przez przycisk SET
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        unsigned long startTime = millis();
        while (!digitalRead(BTN_SET)) {  // Czekaj na puszczenie przycisku
            if ((millis() - startTime) > SET_LONG_PRESS) {
                displayActive = true;
                showingWelcome = true;
                messageStartTime = millis();
                if (!welcomeAnimationDone) {
                    showWelcomeMessage();  // Pokaż animację powitania
                }                
                while (!digitalRead(BTN_SET)) {  // Czekaj na puszczenie przycisku
                    delay(10);
                }
                break;
            }
            delay(10);
        }
    }
}

// Rysuje wycentrowany tekst na wyświetlaczu OLED
void drawCenteredText(const char* text, int y, const uint8_t* font) {
    // Ustawia wybraną czcionkę dla tekstu
    display.setFont(font);
    
    // Oblicza szerokość tekstu w pikselach dla wybranej czcionki
    int textWidth = display.getStrWidth(text);
    
    // Oblicza pozycję X dla wycentrowania tekstu
    // display.getWidth() zwraca szerokość wyświetlacza (zazwyczaj 128 pikseli)
    // Odejmujemy szerokość tekstu i dzielimy przez 2, aby uzyskać lewy margines
    int x = (display.getWidth() - textWidth) / 2;
    
    // Rysuje tekst w obliczonej pozycji
    // x - pozycja pozioma (wycentrowana)
    // y - pozycja pionowa (określona przez parametr)
    display.drawStr(x, y, text);
}

void loop() {
    static unsigned long lastButtonCheck = 0;
    static unsigned long lastUpdate = 0;
    const unsigned long buttonInterval = 5;
    const unsigned long updateInterval = 2000;

    unsigned long currentTime = millis();

    if (configModeActive) {      
        display.clearBuffer();

        // Wycentruj każdą linię tekstu
        drawCenteredText("e-Bike System", 15, czcionka_srednia);
        drawCenteredText("Konfiguracja on-line", 25, czcionka_mala);
        drawCenteredText("siec: e-Bike System", 42, czcionka_mala);
        drawCenteredText("haslo: #mamrower", 52, czcionka_mala);
        drawCenteredText("IP: 192.168.4.1", 62, czcionka_mala);

        display.sendBuffer();

        // Sprawdź długie przytrzymanie SET do wyjścia
        static unsigned long setPressStartTime = 0;
        if (!digitalRead(BTN_SET)) { 
            if (setPressStartTime == 0) {
                setPressStartTime = millis();
            } else if (millis() - setPressStartTime > 50) {
                deactivateConfigMode();
                setPressStartTime = 0;
            }
        } else {
            setPressStartTime = 0;
        }
        return;
    }

    // Sprawdzaj czy nie trzeba włączyć trybu konfiguracji
    checkConfigMode();

    if (currentTime - lastButtonCheck >= buttonInterval) {
        handleButtons();
        lastButtonCheck = currentTime;
    }

    static unsigned long lastWebSocketUpdate = 0;
    if (currentTime - lastWebSocketUpdate >= 1000) { // Aktualizuj co sekundę
        if (ws.count() > 0) {
            String json = "{";
            json += "\"speed\":" + String(speed_kmh) + ",";
            json += "\"temperature\":" + String(currentTemp) + ",";
            json += "\"battery\":" + String(battery_capacity_percent) + ",";
            json += "\"power\":" + String(power_w);
            json += "}";
            ws.textAll(json);
        }
        lastWebSocketUpdate = currentTime;
    }

    // Aktualizuj wyświetlacz tylko jeśli jest aktywny i nie wyświetla komunikatów
    if (displayActive && messageStartTime == 0) {
        display.clearBuffer();
        drawTopBar();
        drawHorizontalLine();
        drawVerticalLine();
        drawAssistLevel();
        drawMainDisplay();
        drawLightStatus();
        display.sendBuffer();
        handleTemperature();

        if (currentTime - lastUpdate >= updateInterval) {
            speed_kmh = (speed_kmh >= 35.0) ? 0.0 : speed_kmh + 0.1;
            cadence_rpm = random(60, 90);
            temp_controller = 25.0 + random(15);
            temp_motor = 30.0 + random(20);
            range_km = 50.0 - (random(20) / 10.0);
            distance_km += 0.1;
            odometer_km += 0.1;
            power_w = 100 + random(300);
            power_avg_w = power_w * 0.8;
            power_max_w = power_w * 1.2;
            battery_current = random(50, 150) / 10.0;
            battery_capacity_wh = battery_voltage * battery_capacity_ah;
            pressure_bar = 2.0 + (random(20) / 10.0);
            pressure_voltage = 0.5 + (random(20) / 100.0);
            pressure_temp = 20.0 + (random(100) / 10.0);
            speed_kmh = (speed_kmh >= 35.0) ? 0.0 : speed_kmh + 0.1;
            distance_km += 0.1;
            odometer_km += 0.1;
            power_w = 100 + random(300);
            battery_capacity_wh = 14.5 - (random(20) / 10.0);
            battery_capacity_percent = (battery_capacity_percent <= 0) ? 100 : battery_capacity_percent - 1;
            battery_voltage = (battery_voltage <= 42.0) ? 50.0 : battery_voltage - 0.1;
            assistMode = (assistMode + 1) % 4;
            lastUpdate = currentTime;
            pressure_bar = 2.0 + (random(20) / 10.0);
            pressure_rear_bar = 2.0 + (random(20) / 10.0);
            pressure_voltage = 0.5 + (random(20) / 100.0);
            pressure_rear_voltage = 0.5 + (random(20) / 100.0);
            pressure_temp = 20.0 + (random(100) / 10.0);
            pressure_rear_temp = 20.0 + (random(100) / 10.0);
            speed_kmh = (speed_kmh >= 35.0) ? 0.0 : speed_kmh + 0.1;
            // Aktualizacja średniej prędkości (przykładowa implementacja)
            static float speed_sum = 0;
            static int speed_count = 0;
            speed_sum += speed_kmh;
            speed_count++;
            speed_avg_kmh = speed_sum / speed_count;
            // Aktualizacja maksymalnej prędkości
            if (speed_kmh > speed_max_kmh) {
                speed_max_kmh = speed_kmh;
            }

            // Aktualizacja kadencji
            cadence_rpm = random(60, 90);

            // Aktualizacja średniej kadencji (przykładowa implementacja)
            static int cadence_sum = 0;
            static int cadence_count = 0;
            cadence_sum += cadence_rpm;
            cadence_count++;
            cadence_avg_rpm = cadence_sum / cadence_count;
        }
    }
}
