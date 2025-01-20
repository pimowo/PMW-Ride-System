OdometerManager odometer;
Preferences preferences;

bool configModeActive = false;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

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

unsigned long lastBlinkTime = 0;  // Czas ostatniego mrugania
bool blinkState = false;          // Stan mrugania (włączone/wyłączone)

bool legalMode = false;             // false = normalny tryb, true = tryb legal
uint8_t displayBrightness = 16;     // Wartość od 0 do 255
bool welcomeAnimationDone = false;  // Dodaj na początku pliku
void toggleLegalMode();             // Zdefiniuj tę funkcję
void showWelcomeMessage();          // Zdefiniuj tę funkcję

MainScreen currentMainScreen = SPEED_SCREEN;
int currentSubScreen = 0;
bool inSubScreen = false;
bool displayActive = false;
bool showingWelcome = false;

#define PRESSURE_LEFT_MARGIN 70
#define PRESSURE_TOP_LINE 62
#define PRESSURE_BOTTOM_LINE 62

float currentTemp = DEVICE_DISCONNECTED_C;
bool temperatureReady = false;
bool conversionRequested = false;
unsigned long lastTempRequest = 0;
unsigned long ds18b20RequestTime;

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
