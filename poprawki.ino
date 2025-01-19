/********************************************************************
 * BIBLIOTEKI
 ********************************************************************/
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

/********************************************************************
 * DEFINICJE I STAŁE GLOBALNE
 ********************************************************************/
#define DEBUG

// Wersja oprogramowania
const char* VERSION = "20.1.25";

// Nazwy plików konfiguracyjnych
const char* CONFIG_FILE = "/display_config.json";
const char* LIGHT_CONFIG_FILE = "/light_config.json";

// Definicje pinów
#define BTN_UP 13
#define BTN_DOWN 14
#define BTN_SET 12
#define FrontDayPin 5    // światła dzienne
#define FrontPin 18      // światła zwykłe
#define RealPin 19       // tylne światło
#define UsbPin 32        // ładowarka USB
#define TEMP_AIR_PIN 15          // temperatutra powietrza (DS18B20)
#define TEMP_CONTROLLER_PIN 4    // temperatura sterownika (DS18B20)

// Stałe czasowe
const unsigned long DEBOUNCE_DELAY = 25;
const unsigned long BUTTON_DELAY = 200;
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long DOUBLE_CLICK_TIME = 300;
const unsigned long GOODBYE_DELAY = 3000;
const unsigned long SET_LONG_PRESS = 2000;
const unsigned long TEMP_REQUEST_INTERVAL = 1000;
const unsigned long DS18B20_CONVERSION_DELAY_MS = 750;

// Stałe wyświetlacza
#define PRESSURE_LEFT_MARGIN 70
#define PRESSURE_TOP_LINE 62
#define PRESSURE_BOTTOM_LINE 62
#define TEMP_ERROR -999.0

/********************************************************************
 * STRUKTURY I TYPY WYLICZENIOWE
 ********************************************************************/
// Struktury konfiguracyjne
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

struct ControllerSettings {
    String type;  // "kt-lcd" lub "s866"
    int ktParams[23];  // P1-P5, C1-C15, L1-L3
    int s866Params[20];  // P1-P20
};

struct GeneralSettings {
    uint8_t wheelSize;  // Wielkość koła w calach (lub 0 dla 700C)
    
    // Konstruktor z wartościami domyślnymi
    GeneralSettings() : wheelSize(26) {} // Domyślnie 26 cali
};

struct BluetoothConfig {
    bool bmsEnabled;
    bool tpmsEnabled;
    
    BluetoothConfig() : bmsEnabled(false), tpmsEnabled(false) {}
};

struct BmsData {
    float voltage;            // Napięcie całkowite [V]
    float current;            // Prąd [A]
    float remainingCapacity;  // Pozostała pojemność [Ah]
    float totalCapacity;      // Całkowita pojemność [Ah]
    uint8_t soc;             // Stan naładowania [%]
    uint8_t cycles;          // Liczba cykli
    float cellVoltages[16];  // Napięcia cel [V]
    float temperatures[4];   // Temperatury [°C]
    bool charging;           // Status ładowania
    bool discharging;        // Status rozładowania
};

struct Settings {
    int wheelCircumference;
    float batteryCapacity;
    int daySetting;
    int nightSetting;
    bool dayRearBlink;
    bool nightRearBlink;
    unsigned long blinkInterval;
};

/********************************************************************
 * TYPY WYLICZENIOWE
 ********************************************************************/
// Ekrany główne
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

// Podekrany
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

/********************************************************************
 * ZMIENNE GLOBALNE
 ********************************************************************/
// Obiekty główne
OdometerManager odometer;
Preferences preferences;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

// Zmienne stanu systemu
bool configModeActive = false;
bool legalMode = false;
bool welcomeAnimationDone = false;
bool displayActive = false;
bool showingWelcome = false;

// Zmienne stanu ekranu
MainScreen currentMainScreen = SPEED_SCREEN;
int currentSubScreen = 0;
bool inSubScreen = false;

// Zmienne pomiarowe
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

// Zmienne dla czujnika temperatury
float currentTemp = DEVICE_DISCONNECTED_C;
bool temperatureReady = false;
bool conversionRequested = false;
unsigned long lastTempRequest = 0;
unsigned long ds18b20RequestTime;

// Zmienne dla przycisków
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

// Zmienne konfiguracyjne
int assistLevel = 3;
bool assistLevelAsText = false;
int lightMode = 0;        // 0=off, 1=dzień, 2=noc
int assistMode = 0;       // 0=PAS, 1=STOP, 2=GAZ, 3=P+G
bool usbEnabled = false;  // Stan wyjścia USB

// Zmienne dla świateł
unsigned long lastBlinkTime = 0;  // Czas ostatniego mrugania
bool blinkState = false;          // Stan mrugania (włączone/wyłączone)

// Czcionki
const uint8_t* czcionka_mala = u8g2_font_profont11_mf;      // opis ekranów
const uint8_t* czcionka_srednia = u8g2_font_pxplusibmvga9_mf; // górna belka
const uint8_t* czcionka_duza = u8g2_font_fub20_tr;

// Stałe BMS
const uint8_t BMS_BASIC_INFO[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
const uint8_t BMS_CELL_INFO[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
const uint8_t BMS_TEMP_INFO[] = {0xDD, 0xA5, 0x08, 0x00, 0xFF, 0xF8, 0x77};

// Instancje obiektów globalnych
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
RTC_DS3231 rtc;
OneWire oneWireAir(TEMP_AIR_PIN);
OneWire oneWireController(TEMP_CONTROLLER_PIN);
DallasTemperature sensorsAir(&oneWireAir);
DallasTemperature sensorsController(&oneWireController);
Settings bikeSettings;
Settings storedSettings;

// Obiekty BLE
BLEClient* bleClient;
BLEAddress bmsMacAddress("a5:c2:37:05:8b:86");
BLERemoteService* bleService;
BLERemoteCharacteristic* bleCharacteristicTx;
BLERemoteCharacteristic* bleCharacteristicRx;

// Instancje struktur konfiguracyjnych
ControllerSettings controllerSettings;
TimeSettings timeSettings;
LightSettings lightSettings;
BacklightSettings backlightSettings;
WiFiSettings wifiSettings;
GeneralSettings generalSettings;
BluetoothConfig bluetoothConfig;
BmsData bmsData;

/********************************************************************
 * KLASY POMOCNICZE
 ********************************************************************/
// Klasa obsługująca timeout
class TimeoutHandler {
    // Implementacja klasy TimeoutHandler
};

// Klasa obsługująca czujnik temperatury
class TemperatureSensor {
    // Implementacja klasy TemperatureSensor
};

/********************************************************************
 * DEKLARACJE I IMPLEMENTACJE FUNKCJI
 ********************************************************************/

// --- Funkcje BLE ---
void notificationCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                        uint8_t* pData, size_t length, bool isNotify) {
    // Implementacja funkcji callback dla BLE
}

void requestBmsData(const uint8_t* command, size_t length) {
    // Implementacja wysyłania zapytania do BMS
}

void updateBmsData() {
    // Implementacja aktualizacji danych BMS
}

void connectToBms() {
    // Implementacja połączenia z BMS
}

// --- Funkcje konfiguracji ---
void saveLightSettings() {
    // Implementacja zapisu ustawień świateł
}

void loadLightSettings() {
    // Implementacja wczytywania ustawień świateł
}

void setDisplayBrightness(uint8_t brightness) {
    // Implementacja ustawiania jasności wyświetlacza
}

void saveBacklightSettingsToFile() {
    // Implementacja zapisu ustawień podświetlenia
}

void loadBacklightSettingsFromFile() {
    // Implementacja wczytywania ustawień podświetlenia
}

void saveGeneralSettingsToFile() {
    // Implementacja zapisu ustawień ogólnych
}

void saveBluetoothConfigToFile() {
    // Implementacja zapisu konfiguracji Bluetooth
}

void loadBluetoothConfigFromFile() {
    // Implementacja wczytywania konfiguracji Bluetooth
}

void loadGeneralSettingsFromFile() {
    // Implementacja wczytywania ustawień ogólnych
}

void loadSettingsFromEEPROM() {
    // Implementacja wczytywania ustawień z EEPROM
}

void saveSettingsToEEPROM() {
    // Implementacja zapisu ustawień do EEPROM
}

// --- Funkcje wyświetlacza ---
void drawHorizontalLine() {
    // Implementacja rysowania linii poziomej
}

void drawVerticalLine() {
    // Implementacja rysowania linii pionowej
}

void drawTopBar() {
    // Implementacja rysowania górnego paska
}

void drawLightStatus() {
    // Implementacja wyświetlania statusu świateł
}

void drawAssistLevel() {
    // Implementacja wyświetlania poziomu wspomagania
}

void drawValueAndUnit(const char* valueStr, const char* unitStr) {
    // Implementacja wyświetlania wartości i jednostki
}

void drawMainDisplay() {
    // Implementacja głównego ekranu
}

void drawCenteredText(const char* text, int y, const uint8_t* font) {
    // Implementacja wyświetlania wycentrowanego tekstu
}

void showWelcomeMessage() {
    // Implementacja wyświetlania wiadomości powitalnej
}

// --- Funkcje obsługi przycisków ---
void handleButtons() {
    // Implementacja obsługi przycisków
}

void checkConfigMode() {
    // Implementacja sprawdzania trybu konfiguracji
}

void activateConfigMode() {
    // Implementacja aktywacji trybu konfiguracji
}

void deactivateConfigMode() {
    // Implementacja dezaktywacji trybu konfiguracji
}

void toggleLegalMode() {
    // Implementacja przełączania trybu legal
}

// --- Funkcje pomocnicze ---
bool hasSubScreens(MainScreen screen) {
    // Implementacja sprawdzania pod-ekranów
}

int getSubScreenCount(MainScreen screen) {
    // Implementacja liczenia pod-ekranów
}

void goToSleep() {
    // Implementacja trybu uśpienia
}

void setLights() {
    // Implementacja ustawiania świateł
}

void applyBacklightSettings() {
    // Implementacja ustawień podświetlenia
}

bool isValidTemperature(float temp) {
    // Implementacja sprawdzania poprawności temperatury
}

void handleTemperature() {
    // Implementacja obsługi temperatury
}

// --- Funkcje konfiguracji systemu ---
void loadSettings() {
    // Implementacja wczytywania wszystkich ustawień
}

void saveSettings() {
    // Implementacja zapisu wszystkich ustawień
}

int getParamIndex(const String& param) {
    // Implementacja konwersji parametru na indeks
}

void updateControllerParam(const String& param, int value) {
    // Implementacja aktualizacji parametrów kontrolera
}

const char* getLightModeString(LightSettings::LightMode mode) {
    // Implementacja konwersji trybu świateł na string
}

// --- Funkcje serwera WWW ---
void setupWebServer() {
    // Implementacja konfiguracji serwera WWW
}

void handleSettings(AsyncWebServerRequest *request) {
    // Implementacja obsługi ustawień przez WWW
}

void handleSaveClockSettings(AsyncWebServerRequest *request) {
    // Implementacja zapisu ustawień zegara
}

void handleSaveBikeSettings(AsyncWebServerRequest *request) {
    // Implementacja zapisu ustawień roweru
}

// --- Funkcje systemu plików ---
void initLittleFS() {
    // Implementacja inicjalizacji systemu plików
}

void listFiles() {
    // Implementacja listowania plików
}

bool loadConfig() {
    // Implementacja wczytywania konfiguracji
}

// --- Funkcje czasu ---
void syncRTCWithNTP() {
    // Implementacja synchronizacji RTC z NTP
}

void synchronizeTime() {
    // Implementacja synchronizacji czasu
}

void updateBacklight() {
    // Implementacja aktualizacji podświetlenia
}

// --- Główne funkcje programu ---
void setup() {
    // Implementacja funkcji setup
}

void loop() {
    // Implementacja funkcji loop
}
