/********************************************************************
 * BIBLIOTEKI
 ********************************************************************/

// --- Podstawowe biblioteki systemowe ---
#include <Wire.h>              // Biblioteka do komunikacji I2C
#include <EEPROM.h>           // Biblioteka do obsługi pamięci EEPROM

// --- Biblioteki wyświetlacza ---
#include <U8g2lib.h>          // Biblioteka do obsługi wyświetlacza OLED

// --- Biblioteki czasu ---
#include <RTClib.h>           // Biblioteka do obsługi zegara czasu rzeczywistego (RTC)
#include <TimeLib.h>          // Biblioteka do obsługi funkcji czasowych

// --- Biblioteki czujników temperatury ---
#include <OneWire.h>          // Biblioteka do komunikacji z czujnikami OneWire
#include <DallasTemperature.h> // Biblioteka do obsługi czujników temperatury DS18B20

// --- Biblioteki Bluetooth ---
#include <BLEDevice.h>        // Główna biblioteka BLE
#include <BLEServer.h>        // Biblioteka do tworzenia serwera BLE
#include <BLEUtils.h>         // Narzędzia BLE
#include <BLE2902.h>          // Deskryptor powiadomień BLE

// --- Biblioteki sieciowe ---
#include <WiFi.h>             // Biblioteka do obsługi WiFi
#include <AsyncTCP.h>         // Biblioteka do asynchronicznej komunikacji TCP
#include <ESPAsyncWebServer.h> // Biblioteka do obsługi serwera WWW

// --- Biblioteki systemu plików i JSON ---
#include <LittleFS.h>         // System plików dla ESP32
#include <ArduinoJson.h>      // Biblioteka do obsługi formatu JSON
#include <map>                // Biblioteka do obsługi map (kontenerów)

// --- Biblioteki własne ---
#include "Odometer.h"         // Własna biblioteka do obsługi licznika kilometrów

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
#define BTN_UP 13              // przycisk w górę
#define BTN_DOWN 14            // przycisk w dół
#define BTN_SET 12             // przycisk menu
#define FrontDayPin 5          // światła dzienne
#define FrontPin 18            // światła zwykłe
#define RealPin 19             // tylne światło
#define UsbPin 32              // ładowarka USB
#define TEMP_AIR_PIN 15        // temperatutra powietrza (DS18B20)
#define TEMP_CONTROLLER_PIN 4  // temperatura sterownika (DS18B20)

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

// callback dla BLE
void notificationCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                        uint8_t* pData, size_t length, bool isNotify) {
   
}

// wysyłanie zapytania do BMS
void requestBmsData(const uint8_t* command, size_t length) {
    
}

// aktualizacja danych BMS
void updateBmsData() {
    
}

// połączenie z BMS
void connectToBms() {
    
}

// --- Funkcje konfiguracji ---

// zapis ustawień świateł
void saveLightSettings() {
    
}

// wczytywanie ustawień świateł
void loadLightSettings() {
    
}

// ustawianie jasności wyświetlacza
void setDisplayBrightness(uint8_t brightness) {
    
}

// zapis ustawień podświetlenia
void saveBacklightSettingsToFile() {
    
}

// wczytywanie ustawień podświetlenia
void loadBacklightSettingsFromFile() {
    
}

// zapis ustawień ogólnych
void saveGeneralSettingsToFile() {
    
}

// zapis konfiguracji Bluetooth
void saveBluetoothConfigToFile() {
    
}

// wczytywanie konfiguracji Bluetooth
void loadBluetoothConfigFromFile() {
    
}

// wczytywanie ustawień ogólnych
void loadGeneralSettingsFromFile() {
    
}

// wczytywanie ustawień z EEPROM
void loadSettingsFromEEPROM() {
    
}

// zapis ustawień do EEPROM
void saveSettingsToEEPROM() {
    
}

// --- Funkcje wyświetlacza ---

// rysowanie linii poziomej
void drawHorizontalLine() {
    
}

// rysowanie linii pionowej
void drawVerticalLine() {
    
}

// rysowanie górnego paska
void drawTopBar() {
    
}

// wyświetlanie statusu świateł
void drawLightStatus() {
    
}

// Implementacja wyświetlania poziomu wspomagania
void drawAssistLevel() {
    
}

// Implementacja wyświetlania wartości i jednostki
void drawValueAndUnit(const char* valueStr, const char* unitStr) {
    
}

// Implementacja głównego ekranu
void drawMainDisplay() {
    
}

// Implementacja wyświetlania wycentrowanego tekstu
void drawCenteredText(const char* text, int y, const uint8_t* font) {
    
}

// Implementacja wyświetlania wiadomości powitalnej
void showWelcomeMessage() {
    
}

// --- Funkcje obsługi przycisków ---

// Implementacja obsługi przycisków
void handleButtons() {
    
}

// Implementacja sprawdzania trybu konfiguracji
void checkConfigMode() {
    
}

// Implementacja aktywacji trybu konfiguracji
void activateConfigMode() {
    
}

// Implementacja dezaktywacji trybu konfiguracji
void deactivateConfigMode() {
    
}

// Implementacja przełączania trybu legal
void toggleLegalMode() {
    
}

// --- Funkcje pomocnicze ---

// Implementacja sprawdzania pod-ekranów
bool hasSubScreens(MainScreen screen) {
    
}

// Implementacja liczenia pod-ekranów
int getSubScreenCount(MainScreen screen) {
    
}

// Implementacja trybu uśpienia
void goToSleep() {
    
}

// Implementacja ustawiania świateł
void setLights() {
    
}

// Implementacja ustawień podświetlenia
void applyBacklightSettings() {
    
}

// Implementacja sprawdzania poprawności temperatury
bool isValidTemperature(float temp) {
    
}

// Implementacja obsługi temperatury
void handleTemperature() {
    
}

// --- Funkcje konfiguracji systemu ---

// Implementacja wczytywania wszystkich ustawień
void loadSettings() {
    
}

// Implementacja zapisu wszystkich ustawień
void saveSettings() {
    
}

// Implementacja konwersji parametru na indeks
int getParamIndex(const String& param) {
    
}

// Implementacja aktualizacji parametrów kontrolera
void updateControllerParam(const String& param, int value) {
    
}

// Implementacja konwersji trybu świateł na string
const char* getLightModeString(LightSettings::LightMode mode) {
    
}

// --- Funkcje serwera WWW ---

// Implementacja konfiguracji serwera WWW
void setupWebServer() {
    
}

// Implementacja obsługi ustawień przez WWW
void handleSettings(AsyncWebServerRequest *request) {
    
}

// Implementacja zapisu ustawień zegara
void handleSaveClockSettings(AsyncWebServerRequest *request) {
    
}

// Implementacja zapisu ustawień roweru
void handleSaveBikeSettings(AsyncWebServerRequest *request) {
    
}

// --- Funkcje systemu plików ---

// Implementacja inicjalizacji systemu plików
void initLittleFS() {
    
}

// Implementacja listowania plików
void listFiles() {
    
}

// Implementacja wczytywania konfiguracji
bool loadConfig() {
    
}

// --- Funkcje czasu ---

// Implementacja synchronizacji RTC z NTP
void syncRTCWithNTP() {
    
}

// Implementacja synchronizacji czasu
void synchronizeTime() {
    
}

// Implementacja aktualizacji podświetlenia
void updateBacklight() {
    
}

// --- Główne funkcje programu ---

// Implementacja funkcji setup
void setup() {
    
}

// Implementacja funkcji loop
void loop() {
    
}
