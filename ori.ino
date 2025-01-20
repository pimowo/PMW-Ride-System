/*
01＿info Throttle Abnormality
03＿info Motor Hall Signal Abnormality
04＿info Torque sensor Signal Abnormality
05＿info Axis speed sensor Abnormality(only applied to torque sensor )
06＿info Motor or controller has short circuit Abnormality

01_info - Problem z Manetką
    W skrócie: Manetka nie działa prawidłowo
    Możliwe przyczyny: uszkodzenie manetki, złe połączenie, problemy z przewodami

03_info - Problem z Czujnikami w Silniku
    W skrócie: Silnik nie otrzymuje prawidłowych sygnałów od swoich wewnętrznych czujników
    Możliwe przyczyny: uszkodzenie czujników, problemy z okablowaniem silnika

04_info - Problem z Czujnikiem Siły Nacisku
    W skrócie: System nie wykrywa prawidłowo siły pedałowania
    Możliwe przyczyny: uszkodzenie czujnika, problemy z kalibracją

05_info - Problem z Czujnikiem Prędkości
    W skrócie: System ma problem z pomiarem prędkości
    Uwaga: Ten błąd pojawia się tylko w systemach z czujnikiem siły nacisku
    Możliwe przyczyny: uszkodzony czujnik, złe ustawienie magnesów

06_info - Wykryto Zwarcie
    W skrócie: Poważny problem elektryczny w silniku lub kontrolerze
    Możliwe przyczyny: uszkodzenie przewodów, zalanie wodą, wewnętrzne uszkodzenie
    UWAGA: Ten błąd wymaga natychmiastowej kontroli, aby uniknąć poważniejszych uszkodzeń!
*/

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
#include <map>

#include "Odometer.h"

// Dodaj globalną instancję
OdometerManager odometer;
Preferences preferences;

#define DEBUG

// --- Wersja systemu ---
const char* VERSION = "18.1.25";

// Stała z nazwą pliku konfiguracyjnego
const char* CONFIG_FILE = "/display_config.json";
const char* LIGHT_CONFIG_FILE = "/light_config.json";

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
        NONE,
        FRONT,
        REAR,
        BOTH
    };
    
    LightMode dayLights;      // Konfiguracja świateł dziennych
    LightMode nightLights;    // Konfiguracja świateł nocnych
    bool dayBlink;           // Miganie w trybie dziennym
    bool nightBlink;         // Miganie w trybie nocnym
    uint16_t blinkFrequency; // Częstotliwość migania
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

// Struktura dla parametrów sterownika
struct ControllerSettings {
    String type;  // "kt-lcd" lub "s866"
    int ktParams[23];  // P1-P5, C1-C15, L1-L3
    int s866Params[20];  // P1-P20
};

// Struktura przechowująca ustawienia ogólne
struct GeneralSettings {
    uint8_t wheelSize;  // Wielkość koła w calach (lub 0 dla 700C)
    
    // Konstruktor z wartościami domyślnymi
    GeneralSettings() : wheelSize(26) {} // Domyślnie 26 cali
};

// Struktura przechowująca ustawienia Bluetooth
struct BluetoothConfig {
    bool bmsEnabled;
    bool tpmsEnabled;
    
    BluetoothConfig() : bmsEnabled(false), tpmsEnabled(false) {}
};

// Deklaracje dla BMS
struct BmsData {
    float voltage;            // Napięcie całkowite [V]
    float current;            // Prąd [A]
    float remainingCapacity;  // Pozostała pojemność [Ah]
    float totalCapacity;      // Całkowita pojemność [Ah]
    uint8_t soc;              // Stan naładowania [%]
    uint8_t cycles;           // Liczba cykli
    float cellVoltages[16];   // Napięcia cel [V]
    float temperatures[4];    // Temperatury [°C]
    bool charging;            // Status ładowania
    bool discharging;         // Status rozładowania
};

BmsData bmsData;

// Komendy BMS
const uint8_t BMS_BASIC_INFO[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
const uint8_t BMS_CELL_INFO[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
const uint8_t BMS_TEMP_INFO[] = {0xDD, 0xA5, 0x08, 0x00, 0xFF, 0xF8, 0x77};

// Globalne instancje ustawień
ControllerSettings controllerSettings;
TimeSettings timeSettings;
LightSettings lightSettings;
BacklightSettings backlightSettings;
WiFiSettings wifiSettings;
GeneralSettings generalSettings;
BluetoothConfig bluetoothConfig;

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
// Czujniki temperatury
#define TEMP_AIR_PIN 15        // temperatutra powietrza (DS18B20)
#define TEMP_CONTROLLER_PIN 4  // temperatura sterownika (DS18B20)

// Zmienne do obsługi mrugania światła
unsigned long lastBlinkTime = 0;  // Czas ostatniego mrugania
bool blinkState = false;          // Stan mrugania (włączone/wyłączone)

const uint8_t* czcionka_mala = u8g2_font_profont11_mf;  // opis ekranów
const uint8_t* czcionka_srednia = u8g2_font_pxplusibmvga9_mf; // górna belka
const uint8_t* czcionka_duza = u8g2_font_fub20_tr;

bool legalMode = false;             // false = normalny tryb, true = tryb legal
uint8_t displayBrightness = 16;     // Wartość od 0 do 255
bool welcomeAnimationDone = false;  // Dodaj na początku pliku
void toggleLegalMode();             // Zdefiniuj tę funkcję
void showWelcomeMessage();          // Zdefiniuj tę funkcję

// Dodaj stałe jeśli ich nie ma
//#define LEGAL_MODE_TIME 2000      // Czas przytrzymania dla trybu legal

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
    SPEED_KMH,       // Aktualna prędkość
    SPEED_AVG_KMH,   // Średnia prędkość
    SPEED_MAX_KMH,   // Maksymalna prędkość
    SPEED_SUB_COUNT  
};

enum CadenceSubScreen {
    CADENCE_RPM,       // Aktualna kadencja
    CADENCE_AVG_RPM,   // Średnia kadencja
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
float speed_kmh;
int cadence_rpm;
float temp_air;
float temp_controller;
float temp_motor;
float range_km;
float distance_km;
float battery_voltage;
float battery_current;
float battery_capacity_wh;
float battery_capacity_ah;
int battery_capacity_percent;
int power_w;
int power_avg_w;
int power_max_w;
float speed_avg_kmh;
float speed_max_kmh;
int cadence_avg_rpm;
// Zmienne dla czujników ciśnienia
float pressure_bar;           // przednie koło
float pressure_rear_bar;      // tylne koło
float pressure_voltage;       // napięcie przedniego czujnika
float pressure_rear_voltage;  // napięcie tylnego czujnika
float pressure_temp;          // temperatura przedniego czujnika
float pressure_rear_temp;     // temperatura tylnego czujnika

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
OneWire oneWireAir(TEMP_AIR_PIN);
OneWire oneWireController(TEMP_CONTROLLER_PIN);
DallasTemperature sensorsAir(&oneWireAir);
DallasTemperature sensorsController(&oneWireController);
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

// 

class TemperatureSensor {
    private:
        static constexpr float INVALID_TEMP = -999.0f;
        static constexpr float MIN_VALID_TEMP = -50.0f;
        static constexpr float MAX_VALID_TEMP = 100.0f;
        bool conversionRequested = false;
        unsigned long lastRequestTime = 0;
        DallasTemperature* airSensor;
        DallasTemperature* controllerSensor;

    public:
        TemperatureSensor(DallasTemperature* air, DallasTemperature* controller) 
            : airSensor(air), controllerSensor(controller) {}

        void requestTemperature() {
            if (millis() - lastRequestTime >= TEMP_REQUEST_INTERVAL) {
                airSensor->requestTemperatures();
                controllerSensor->requestTemperatures();
                conversionRequested = true;
                lastRequestTime = millis();
            }
        }

        bool isValidTemperature(float temp) {
            return temp >= MIN_VALID_TEMP && temp <= MAX_VALID_TEMP;
        }

        float readAirTemperature() {
            if (!conversionRequested) return INVALID_TEMP;

            if (millis() - lastRequestTime < DS18B20_CONVERSION_DELAY_MS) {
                return INVALID_TEMP;  // Konwersja jeszcze trwa
            }

            float temp = airSensor->getTempCByIndex(0);
            return isValidTemperature(temp) ? temp : INVALID_TEMP;
        }

        float readControllerTemperature() {
            if (!conversionRequested) return INVALID_TEMP;

            if (millis() - lastRequestTime < DS18B20_CONVERSION_DELAY_MS) {
                return INVALID_TEMP;  // Konwersja jeszcze trwa
            }

            float temp = controllerSensor->getTempCByIndex(0);
            return isValidTemperature(temp) ? temp : INVALID_TEMP;
        }
};

// Callback dla powiadomień BLE
void notificationCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                        uint8_t* pData, size_t length, bool isNotify) {
    if (length < 2) return;  // Sprawdzenie minimalnej długości pakietu

    switch (pData[1]) {  // Sprawdź typ pakietu
        case 0x03:  // Basic info
            if (length >= 34) {
                // Napięcie całkowite (0.1V)
                bmsData.voltage = (float)((pData[4] << 8) | pData[5]) / 10.0;
                
                // Prąd (0.1A, wartość ze znakiem)
                int16_t current = (pData[6] << 8) | pData[7];
                bmsData.current = (float)current / 10.0;
                
                // Pozostała pojemność (0.1Ah)
                bmsData.remainingCapacity = (float)((pData[8] << 8) | pData[9]) / 10.0;
                
                // SOC (%)
                bmsData.soc = pData[23];
                
                // Status ładowania/rozładowania
                uint8_t status = pData[22];
                bmsData.charging = (status & 0x01);
                bmsData.discharging = (status & 0x02);
                
                #ifdef DEBUG
                Serial.printf("Voltage: %.1fV, Current: %.1fA, SOC: %d%%\n", 
                            bmsData.voltage, bmsData.current, bmsData.soc);
                #endif
            }
            break;

        case 0x04:  // Cell info
            if (length >= 34) {  // Sprawdź czy mamy kompletny pakiet
                for (int i = 0; i < 16; i++) {
                    bmsData.cellVoltages[i] = (float)((pData[4 + i*2] << 8) | pData[5 + i*2]) / 1000.0;
                }
                #ifdef DEBUG
                Serial.println("Cell voltages updated");
                #endif
            }
            break;

        case 0x08:  // Temperature info
            if (length >= 12) {
                for (int i = 0; i < 4; i++) {
                    // Konwersja z K na °C
                    int16_t temp = ((pData[4 + i*2] << 8) | pData[5 + i*2]) - 2731;
                    bmsData.temperatures[i] = (float)temp / 10.0;
                }
                #ifdef DEBUG
                Serial.println("Temperatures updated");
                #endif
            }
            break;
    }
}

// Funkcja wysyłająca zapytanie do BMS
void requestBmsData(const uint8_t* command, size_t length) {
    if (bleClient && bleClient->isConnected() && bleCharacteristicTx) {
//        bleCharacteristicTx->writeValue(command, length);
    }
}

// Funkcja aktualizująca dane BMS
void updateBmsData() {
    static unsigned long lastBmsUpdate = 0;
    const unsigned long BMS_UPDATE_INTERVAL = 1000; // Aktualizuj co 1 sekundę

    if (millis() - lastBmsUpdate >= BMS_UPDATE_INTERVAL) {
        if (bleClient && bleClient->isConnected()) {
            requestBmsData(BMS_BASIC_INFO, sizeof(BMS_BASIC_INFO));
            delay(100);  // Krótkie opóźnienie między zapytaniami
            requestBmsData(BMS_CELL_INFO, sizeof(BMS_CELL_INFO));
            delay(100);
            requestBmsData(BMS_TEMP_INFO, sizeof(BMS_TEMP_INFO));
            lastBmsUpdate = millis();
        }
    }
}

// Funkcja zapisująca ustawienia świateł do pliku
void saveLightSettings() {
    #ifdef DEBUG
    Serial.println("Zapisywanie ustawień świateł");
    #endif

    // Przygotuj dokument JSON
    StaticJsonDocument<256> doc;
    
    // Zapisz ustawienia
    doc["dayLights"] = lightSettings.dayLights;
    doc["nightLights"] = lightSettings.nightLights;
    doc["dayBlink"] = lightSettings.dayBlink;
    doc["nightBlink"] = lightSettings.nightBlink;
    doc["blinkFrequency"] = lightSettings.blinkFrequency;

    // Otwórz plik do zapisu
    File file = LittleFS.open("/lights.json", "w");
    if (!file) {
        #ifdef DEBUG
        Serial.println("Błąd otwarcia pliku do zapisu");
        #endif
        return;
    }

    // Zapisz JSON do pliku
    if (serializeJson(doc, file) == 0) {
        #ifdef DEBUG
        Serial.println("Błąd podczas zapisu do pliku");
        #endif
    }

    file.close();

    #ifdef DEBUG
    Serial.println("Ustawienia świateł zapisane");
    Serial.print("dayLights: "); Serial.println(lightSettings.dayLights);
    Serial.print("nightLights: "); Serial.println(lightSettings.nightLights);
    #endif

    // Od razu zastosuj nowe ustawienia
    setLights();
}

// Funkcja wczytująca ustawienia świateł z pliku
void loadLightSettings() {
    #ifdef DEBUG
    Serial.println("Wczytywanie ustawień świateł");
    #endif

    if (LittleFS.exists("/lights.json")) {
        File file = LittleFS.open("/lights.json", "r");
        if (file) {
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();

            if (!error) {
                const char* dayLightsStr = doc["dayLights"] | "FRONT";
                const char* nightLightsStr = doc["nightLights"] | "BOTH";

                // Konwersja stringów na enum
                if (strcmp(dayLightsStr, "FRONT") == 0) 
                    lightSettings.dayLights = LightSettings::FRONT;
                else if (strcmp(dayLightsStr, "REAR") == 0) 
                    lightSettings.dayLights = LightSettings::REAR;
                else if (strcmp(dayLightsStr, "BOTH") == 0) 
                    lightSettings.dayLights = LightSettings::BOTH;
                else 
                    lightSettings.dayLights = LightSettings::NONE;

                if (strcmp(nightLightsStr, "FRONT") == 0) 
                    lightSettings.nightLights = LightSettings::FRONT;
                else if (strcmp(nightLightsStr, "REAR") == 0) 
                    lightSettings.nightLights = LightSettings::REAR;
                else if (strcmp(nightLightsStr, "BOTH") == 0) 
                    lightSettings.nightLights = LightSettings::BOTH;
                else 
                    lightSettings.nightLights = LightSettings::NONE;

                lightSettings.dayBlink = doc["dayBlink"] | false;
                lightSettings.nightBlink = doc["nightBlink"] | false;
                lightSettings.blinkFrequency = doc["blinkFrequency"] | 500;
            }
        }
    } else {
        // Ustawienia domyślne
        lightSettings.dayLights = LightSettings::FRONT;
        lightSettings.nightLights = LightSettings::BOTH;
        lightSettings.dayBlink = false;
        lightSettings.nightBlink = false;
        lightSettings.blinkFrequency = 500;
    }
}

// --- Połączenie z BMS ---
void connectToBms() {
    if (!bleClient->isConnected()) {
        #ifdef DEBUG
        Serial.println("Próba połączenia z BMS...");
        #endif

        if (bleClient->connect(bmsMacAddress)) {
            #ifdef DEBUG
            Serial.println("Połączono z BMS");
            #endif

            bleService = bleClient->getService("0000ff00-0000-1000-8000-00805f9b34fb");
            if (bleService == nullptr) {
                #ifdef DEBUG
                Serial.println("Nie znaleziono usługi BMS");
                #endif
                bleClient->disconnect();
                return;
            }

            bleCharacteristicTx = bleService->getCharacteristic("0000ff02-0000-1000-8000-00805f9b34fb");
            if (bleCharacteristicTx == nullptr) {
                #ifdef DEBUG
                Serial.println("Nie znaleziono charakterystyki Tx");
                #endif
                bleClient->disconnect();
                return;
            }

            bleCharacteristicRx = bleService->getCharacteristic("0000ff01-0000-1000-8000-00805f9b34fb");
            if (bleCharacteristicRx == nullptr) {
                #ifdef DEBUG
                Serial.println("Nie znaleziono charakterystyki Rx");
                #endif
                bleClient->disconnect();
                return;
            }

            // Rejestracja funkcji obsługi powiadomień BLE
            if (bleCharacteristicRx->canNotify()) {
                bleCharacteristicRx->registerForNotify(notificationCallback);
                #ifdef DEBUG
                Serial.println("Zarejestrowano powiadomienia dla Rx");
                #endif
            } else {
                #ifdef DEBUG
                Serial.println("Charakterystyka Rx nie obsługuje powiadomień");
                #endif
                bleClient->disconnect();
                return;
            }
        } else {
          #ifdef DEBUG
          Serial.println("Nie udało się połączyć z BMS");
          #endif
        }
    }
}

void setDisplayBrightness(uint8_t brightness) {
    displayBrightness = brightness;
    display.setContrast(displayBrightness);
}

// Funkcja zapisująca ustawienia do pliku
void saveBacklightSettingsToFile() {
    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        #ifdef DEBUG
        Serial.println("Nie można otworzyć pliku do zapisu");
        #endif
        return;
    }

    StaticJsonDocument<200> doc;
    doc["dayBrightness"] = backlightSettings.dayBrightness;
    doc["nightBrightness"] = backlightSettings.nightBrightness;
    doc["autoMode"] = backlightSettings.autoMode;

    // Zapisz JSON do pliku
    if (serializeJson(doc, file)) {
        #ifdef DEBUG
        Serial.println("Zapisano ustawienia do pliku");
        #endif
    } else {
        #ifdef DEBUG
        Serial.println("Błąd podczas zapisu do pliku");
        #endif
    }
    
    file.close();
}

// Funkcja odczytująca ustawienia z pliku
void loadBacklightSettingsFromFile() {
    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        #ifdef DEBUG
        Serial.println("Brak pliku konfiguracyjnego, używam ustawień domyślnych");
        #endif
        // Ustaw wartości domyślne
        backlightSettings.dayBrightness = 100;
        backlightSettings.nightBrightness = 50;
        backlightSettings.autoMode = false;
        // Zapisz domyślne ustawienia do pliku
        saveBacklightSettingsToFile();
        return;
    }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        #ifdef DEBUG
        Serial.println("Błąd podczas parsowania JSON, używam ustawień domyślnych");
        #endif
        // Ustaw wartości domyślne
        backlightSettings.dayBrightness = 100;
        backlightSettings.nightBrightness = 50;
        backlightSettings.autoMode = false;
        return;
    }

    // Wczytaj ustawienia
    backlightSettings.dayBrightness = doc["dayBrightness"] | 100;
    backlightSettings.nightBrightness = doc["nightBrightness"] | 50;
    backlightSettings.autoMode = doc["autoMode"] | false;

    #ifdef DEBUG
    Serial.println("Wczytano ustawienia z pliku:");
    Serial.print("Day Brightness: "); Serial.println(backlightSettings.dayBrightness);
    Serial.print("Night Brightness: "); Serial.println(backlightSettings.nightBrightness);
    Serial.print("Auto Mode: "); Serial.println(backlightSettings.autoMode);
    #endif
}

// Zapisywanie ustawień ogólnych do pliku
void saveGeneralSettingsToFile() {
    File file = LittleFS.open("/general_config.json", "w");
    if (!file) {
        #ifdef DEBUG
        Serial.println("Nie można otworzyć pliku ustawień ogólnych do zapisu");
        #endif
        return;
    }

    StaticJsonDocument<64> doc;
    doc["wheelSize"] = generalSettings.wheelSize;

    if (serializeJson(doc, file) == 0) {
        #ifdef DEBUG
        Serial.println("Błąd podczas zapisu ustawień ogólnych");
        #endif
    }

    file.close();
}


// Funkcja zapisująca ustawienia Bluetooth do pliku
void saveBluetoothConfigToFile() {
    File file = LittleFS.open("/bluetooth_config.json", "w");
    if (!file) {
        #ifdef DEBUG
        Serial.println("Nie można otworzyć pliku konfiguracji Bluetooth");
        #endif
        return;
    }

    StaticJsonDocument<64> doc;
    doc["bmsEnabled"] = bluetoothConfig.bmsEnabled;
    doc["tpmsEnabled"] = bluetoothConfig.tpmsEnabled;

    serializeJson(doc, file);
    file.close();
}

// Funkcja wczytująca ustawienia Bluetooth z pliku
void loadBluetoothConfigFromFile() {
    File file = LittleFS.open("/bluetooth_config.json", "r");
    if (!file) {
        #ifdef DEBUG
        Serial.println("Nie znaleziono pliku konfiguracji Bluetooth, używam domyślnych");
        #endif
        return;
    }

    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, file);
    
    if (!error) {
        bluetoothConfig.bmsEnabled = doc["bmsEnabled"] | false;
        bluetoothConfig.tpmsEnabled = doc["tpmsEnabled"] | false;
    }
    
    file.close();
}

// Wczytywanie ustawień ogólnych z pliku
void loadGeneralSettingsFromFile() {
    File file = LittleFS.open("/general_config.json", "r");
    if (!file) {
        #ifdef DEBUG
        Serial.println("Nie znaleziono pliku ustawień ogólnych, używam domyślnych");
        #endif
        generalSettings.wheelSize = 26; // Wartość domyślna
        saveGeneralSettingsToFile(); // Zapisz domyślne ustawienia
        return;
    }

    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        #ifdef DEBUG
        Serial.println("Błąd podczas parsowania JSON ustawień ogólnych");
        #endif
        generalSettings.wheelSize = 26; // Wartość domyślna
        saveGeneralSettingsToFile(); // Zapisz domyślne ustawienia
        return;
    }

    generalSettings.wheelSize = doc["wheelSize"] | 26; // Domyślnie 26 cali jeśli nie znaleziono

    #ifdef DEBUG
    Serial.print("Loaded wheel size: ");
    Serial.println(generalSettings.wheelSize);
    #endif
}

// Wczytywanie ustawień z EEPROM
void loadSettingsFromEEPROM() {
    // Wczytanie ustawień z EEPROM
    EEPROM.get(0, bikeSettings);

    // Skopiowanie aktualnych ustawień do storedSettings do późniejszego porównania
    storedSettings = bikeSettings;

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

    // Ustawienia świateł
    lightSettings.dayLights = LightSettings::FRONT;
    lightSettings.nightLights = LightSettings::BOTH;
    lightSettings.dayBlink = false;
    lightSettings.nightBlink = false;
    lightSettings.blinkFrequency = 500;

    // Podświetlenie
    backlightSettings.dayBrightness = 100;
    backlightSettings.nightBrightness = 50;
    backlightSettings.autoMode = false;

    // WiFi - początkowo puste
    memset(wifiSettings.ssid, 0, sizeof(wifiSettings.ssid));
    memset(wifiSettings.password, 0, sizeof(wifiSettings.password));
}

#include <esp_partition.h>
