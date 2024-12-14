// Dodaj te definicje po innych #define, przed bibliotekami
#define BMS_BUFFER_SIZE 64
#define SERVICE_UUID "0000ff00-0000-1000-8000-00805f9b34fb"
#define CHAR_TX_UUID "0000ff02-0000-1000-8000-00805f9b34fb"
#define CHAR_RX_UUID "0000ff01-0000-1000-8000-00805f9b34fb"

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

// ---
#include "esp_timer.h"

// Mutexy dla bezpiecznego dostępu do współdzielonych zasobów
portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE cadenceMux = portMUX_INITIALIZER_UNLOCKED;

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

float displayedRange = 0.0f;                                  // Brakująca zmienna
bool stationary = false;                                     // Brakująca zmienna
uint32_t updateInterval = 1000;                             // Brakująca zmienna
#define STOP_TIMEOUT 3000                                    // Brakująca stała
#define DEFAULT_UPDATE_INTERVAL 1000                         // Brakująca stała

extern RTC_DS3231 rtc;
extern TemperatureSensor tempSensor;
extern BMSConnection bmsConnection;

uint8_t currentScreen = 0;
uint8_t subScreen = 0;

// Stałe czasowe (w mikrosekundach)
const uint32_t DEBOUNCE_TIME_US = 50000;  // 50ms
const uint32_t HOLD_TIME_US = 1000000;    // 1s

// --- Wersja systemu --- 
const char systemVersion[] PROGMEM = "8.12.2024";

// Dodaj definicje timeoutów
#define CADENCE_TIMEOUT_US (RPM_TIMEOUT * 1000)   // Konwersja ms na us

// Dodaj zmienne globalne
bool isMoving = false;      // Stan ruchu pojazdu

// --- Watchdog i logging ---
#define WDT_TIMEOUT 8000              // Timeout watchdoga (8 sekund)
#define LOG_BUFFER_SIZE 5            // Ilość ostatnich logów do przechowania
#define SYSTEM_CHECK_INTERVAL 5000    // Interwał sprawdzania systemu (5 sekund)

// Typy logów systemowych
enum LogType {
    LOG_INFO = 0,
    LOG_WARNING = 1,
    LOG_ERROR = 2,
    LOG_CRITICAL = 3
};

// Struktura logu systemowego
struct SystemLog {
    uint32_t timestamp;      // Czas zdarzenia
    LogType type;           // Typ logu
    uint16_t code;          // Kod błędu/zdarzenia
    float value;           // Dodatkowa wartość (np. napięcie, temperatura)
};

class SystemManager {
private:
    //CircularDataBuffer<SystemLog, LOG_BUFFER_SIZE> logs;
    CircularBuffer<SystemLog, LOG_BUFFER_SIZE> logs;
    uint32_t lastSystemCheck;
    uint32_t systemStartTime;
    uint16_t resetCount;
    
    // Flagi stanu systemu
    struct {
        bool rtcOk : 1;
        bool displayOk : 1;
        bool bmsConnected : 1;
        bool tempSensorOk : 1;
        bool lowBattery : 1;
        bool systemOverheat : 1;
    } systemFlags;

public:
    SystemManager() : 
        lastSystemCheck(0),
        systemStartTime(0),
        resetCount(0) {
        memset(&systemFlags, 0, sizeof(systemFlags));
    }

    void begin() {
        systemStartTime = esp_timer_get_time() / 1000;
        loadSystemState();  // Wczytaj poprzedni stan z EEPROM
    }

    void addLog(LogType type, uint16_t code, float value = 0.0f) {
        SystemLog log = {
            .timestamp = esp_timer_get_time() / 1000,
            .type = type,
            .code = code,
            .value = value
        };
        logs.push(log);
        
        // Dla błędów krytycznych, zapisz natychmiast do EEPROM
        if (type == LOG_CRITICAL) {
            saveSystemState();
        }
    }

    bool checkSystem() {
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - lastSystemCheck < SYSTEM_CHECK_INTERVAL) {
            return true;
        }
        lastSystemCheck = now;

        // Sprawdź RTC
        if (!rtc.isrunning()) {
            systemFlags.rtcOk = false;
            addLog(LOG_ERROR, 1, 0);
        } else {
            systemFlags.rtcOk = true;
        }

        // Sprawdź temperaturę systemu
        float temp = tempSensor.readTemperature();
        if (temp > 80.0f) {  // Temperatura krytyczna
            systemFlags.systemOverheat = true;
            addLog(LOG_CRITICAL, 2, temp);
            return false;
        }

        // Sprawdź napięcie systemu
        if (voltage < 3.0f) {  // Krytycznie niskie napięcie
            systemFlags.lowBattery = true;
            addLog(LOG_CRITICAL, 3, voltage);
            return false;
        }

        return true;
    }

    void saveSystemState() {
        // Zapisz ważne dane systemu do EEPROM
        EEPROM.put(450, resetCount);  // Użyj adresu po innych danych
        EEPROM.commit();
    }

    void loadSystemState() {
        // Wczytaj dane systemu z EEPROM
        EEPROM.get(450, resetCount);
    }

    void handleSystemFailure() {
        saveSystemState();  // Zapisz stan przed resetem
        resetCount++;
        ESP.restart();  // Reset systemu
    }
};

TimeStampManager timeManager;

// --- Ustawienia wyświetlacza OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const long screenInterval = 500;

// Przeniesienie stałych tekstowych do pamięci programu
const char TEXT_SPEED[] PROGMEM = "Predkosc";
const char TEXT_CADENCE[] PROGMEM = "Kadencja";
const char TEXT_TEMPERATURE[] PROGMEM = "Temperatura";
const char TEXT_BATTERY[] PROGMEM = "Akumulator";
const char TEXT_POWER[] PROGMEM = "Moc";
const char TEXT_CONSUMPTION[] PROGMEM = "Zuzycie";
const char TEXT_RANGE[] PROGMEM = "Zasieg";
const char TEXT_ERROR[] PROGMEM = "Blad";

// Funkcja pomocnicza do odczytu tekstu z pamięci programu
char* getProgmemString(const char* str) {
    static char buffer[32];  // Bufor tymczasowy
    strcpy_P(buffer, str);
    return buffer;
}

class DisplayBuffer {
private:
    static const size_t BUFFER_SIZE = 128 * 64 / 8;  // Rozmiar bufora dla OLED 128x32
    uint8_t buffer[BUFFER_SIZE];
    bool dirty = false;

public:
    void setPixel(int16_t x, int16_t y, bool color) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
        
        if (color) {
            buffer[x + (y/8)*128] |= (1 << (y&7));
        } else {
            buffer[x + (y/8)*128] &= ~(1 << (y&7));
        }
        dirty = true;
    }

    void clear() {
        memset(buffer, 0, BUFFER_SIZE);
        dirty = true;
    }

    bool isDirty() const { return dirty; }
    void clearDirty() { dirty = false; }
    const uint8_t* getBuffer() const { return buffer; }
};

// Buforowanie i aktualizacja tylko zmienionych części ekranu
class OptimizedDisplay {
    private:
        uint8_t oldBuffer[SCREEN_WIDTH * SCREEN_HEIGHT / 8];
        bool needsFullUpdate;
    
    public:
        void updateIfNeeded() {
            if (memcmp(display.getBuffer(), oldBuffer, sizeof(oldBuffer)) != 0) {
                display.display();
                memcpy(oldBuffer, display.getBuffer(), sizeof(oldBuffer));
            }
        }
};

struct SystemState {
    uint8_t currentScreen : 4;    // 4 bity (16 ekranów max)
    uint8_t subScreen : 3;        // 3 bity (8 pod-ekranów max)
    uint8_t lightMode : 2;        // 2 bity (4 tryby świateł)
    bool isMoving : 1;            // 1 bit
    bool usbEnabled : 1;          // 1 bit
    bool displayNeedsUpdate : 1;  // 1 bit
} state;

// --- Światła ---
#define FrontDayPin 5 // światła dzienne
#define FrontPin 18    // światła zwykłe
#define RealPin 19     // tylne światło

enum LightMode {
    LIGHTS_OFF = 0,
    LIGHTS_DAY = 1,
    LIGHTS_NIGHT = 2
};

enum LightConfig {
    FRONT_DAY = 0,
    FRONT_NORMAL = 1, 
    REAR_ONLY = 2,
    FRONT_DAY_AND_REAR = 3,
    FRONT_NORMAL_AND_REAR = 4
};

bool rearBlinkState = false;
int currentLightMode = LIGHTS_OFF;  // domyślnie wyłączone
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
#define PIN_ONE_WIRE_BUS 15                             // Definiuje pin, do którego podłączony jest czujnik temperatury DS18B20
OneWire oneWire(PIN_ONE_WIRE_BUS);
DallasTemperature sensors5(&oneWire);
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
const int buttonPin = 12;
unsigned long lastButtonPress = 0;
unsigned long buttonHoldStart = 0;
bool buttonPressed = false;        // Flaga przycisku dla krótkiego naciśnięcia
bool longPressHandled = false;     // Flaga dla długiego naciśnięcia
bool buttonReleased = true;        // Flaga sygnalizująca, że przycisk został zwolniony
unsigned long debounceDelay = 50; // Opóźnienie do debounce
unsigned long holdTime = 1000;     // Czas trzymania przycisku

#define NUM_READINGS 10
unsigned long lastMeasurementTime = 0;  // Czas ostatniego pomiaru
int readIndex = 0;                      // Indeks dla tablicy odczytów
float totalDistance = 0.0f;  // Całkowita przebyta odległość
const uint32_t MEASUREMENT_INTERVAL = 500; // Interwał pomiarów (ms)

// Stałe definiujące bezpieczne zakresy
constexpr float MAX_SPEED = 99.9f;         // Maksymalna prędkość w km/h
constexpr float MAX_CADENCE = 200;         // Maksymalna kadencja w RPM
constexpr float MAX_RANGE = 200.0f;        // Maksymalny zasięg w km
constexpr float MAX_POWER = 2000.0f;       // Maksymalna moc w W
constexpr float MIN_VOLTAGE = 1.0f;        // Minimalne napięcie w V
constexpr float MAX_VOLTAGE = 100.0f;      // Maksymalne napięcie w V
constexpr float MAX_CURRENT = 50.0f;       // Maksymalny prąd w A

// Zmienne volatile dla bezpiecznej komunikacji między ISR a główną pętlą
static volatile bool buttonState = true;
static volatile uint32_t lastButtonChangeTime = 0;

// Struktury do przechowywania danych czujników
struct SensorData {
    volatile uint32_t lastPulseTime;
    volatile uint32_t pulseInterval;
    volatile uint32_t pulseCount;
    volatile bool newPulse;
};

// Dane czujników
static SensorData speedSensor = {0, 0, 0, false};
static SensorData cadenceSensor = {0, 0, 0, false};

// --- Kadencja ---
// Stałe dla kadencji
const uint8_t CADENCE_PIN = 27;  // Pin dla czujnika Hall
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
const uint16_t WHEEL_CIRCUMFERENCE = 2100;  // mm
const uint16_t SPEED_TIMEOUT = 3000;        // ms
const uint16_t MIN_PULSE_TIME = 50;         // ms (eliminacja drgań)
const float KMH_CONVERSION = 3.6;           // Przelicznik m/s na km/h          

// --- Licznik kilometrów ---
float totalDistanceKm = 0;              
float nextDistanceCheckpoint = 0.1; 

// --- Debouncing ---
volatile unsigned long lastPulseTime = 0;
const unsigned long debounceInterval = 50;
volatile unsigned long lastInterruptTime = 0;  // Czas ostatniego przerwania

// Dodanie flag do obsługi przerwań
static volatile uint32_t interruptFlags = 0;
#define FLAG_CADENCE  0x02
#define FLAG_BUTTON   0x04

// --- Ładowarka USB ---
bool outUSB = false;

// --- BMS ---
#define BUFFER_SIZE 200
#define MAX_BMS_BUFFER_SIZE 32
#define WATERMARK_HIGH 90  // 90% pojemności
#define WATERMARK_LOW 70   // 70% pojemności
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

// Struktura do przechowywania danych kadencji
struct CadenceData {
    uint32_t lastValidCadence;
    uint32_t totalCadence;
    uint32_t cadenceReadings;
    uint32_t maxCadence;
    float averageCadence;

    CadenceData() : 
        lastValidCadence(0),
        totalCadence(0),
        cadenceReadings(0),
        maxCadence(0),
        averageCadence(0.0f) {}
};

static CadenceData cadenceData;

class BMSData {
private:
    static const size_t MAX_PACKETS = 4;
    uint8_t dataBuffer[BMS_BUFFER_SIZE];
    size_t dataSize;
    uint32_t lastUpdateTime;
    bool dataValid;
    
public:
    BMSData() : dataSize(0), lastUpdateTime(0), dataValid(false) {}

    bool addPacket(const uint8_t* data, size_t length) {
        if (!data || length == 0 || (dataSize + length) > BMS_BUFFER_SIZE) {
            systemManager.addLog(LOG_ERROR, 7); // Log błędu danych
            return false;
        }

        memcpy(dataBuffer + dataSize, data, length);
        dataSize += length;
        lastUpdateTime = millis();
        return true;
    }

    void processData() {
        if (dataSize < 7 || dataBuffer[0] != 0xDD) {
            reset();
            return;
        }

        // Sprawdzenie sumy kontrolnej
        uint8_t checksum = 0;
        for (size_t i = 0; i < dataSize - 1; i++) {
            checksum += dataBuffer[i];
        }
        
        if (checksum != dataBuffer[dataSize - 1]) {
            systemManager.addLog(LOG_ERROR, 8); // Log błędu sumy kontrolnej
            reset();
            return;
        }

        dataValid = true;
    }

    void reset() {
        dataSize = 0;
        dataValid = false;
    }

    bool isValid() const { return dataValid && (millis() - lastUpdateTime < 5000); }
    const uint8_t* getData() const { return dataBuffer; }
    size_t getSize() const { return dataSize; }
};

class OptimizedBMSConnection {
private:
    BMSData bmsData;
    uint8_t txBuffer[32];
    uint8_t rxBuffer[64];
    bool dataReady;
    TimeoutHandler operationTimeout;
    uint8_t retryCount;
    static const uint8_t MAX_RETRIES = 3;
    
public:
    OptimizedBMSConnection() : 
        dataReady(false), 
        retryCount(0),
        operationTimeout(1000) {} // 1 sekunda timeout

    void handleData() {
        if (!dataReady) return;

        // Przetwarzanie tylko gdy są nowe dane
        if (bmsData.isValid()) {
            processReceivedData();
        }
        dataReady = false;
    }

    void processReceivedData() {
        const uint8_t* data = bmsData.getData();
        size_t size = bmsData.getSize();

        if (size < 7) return; // Minimalny rozmiar pakietu

        // Parsowanie danych BMS
        if (data[1] == 0x03) {  // Sprawdzenie typu pakietu
            // Odczyt napięcia (jednostka: 10 mV)
            uint16_t voltageRaw = (data[4] << 8) | data[5];
            voltage = SafeMath::clamp(voltageRaw / 100.0f, MIN_VOLTAGE, MAX_VOLTAGE);

            // Odczyt natężenia (jednostka: 10 mA)
            int16_t currentRaw = (data[6] << 8) | data[7];
            current = SafeMath::clamp(currentRaw / 100.0f, -MAX_CURRENT, MAX_CURRENT);

            // Odczyt pozostałej pojemności (jednostka: 10 mAh)
            uint16_t remainingCapacityRaw = (data[8] << 8) | data[9];
            remainingCapacity = SafeMath::clamp(remainingCapacityRaw / 100.0f, 0.0f, bikeSettings.batteryCapacity);

            // Odczyt SOC
            soc = SafeMath::clamp(data[23], 0.0f, 100.0f);

            systemManager.addLog(LOG_INFO, 3); // Log sukcesu przetwarzania danych
        }
    }

    bool sendRequest(uint8_t* data, size_t length) {
        if (retryCount >= MAX_RETRIES) {
            systemManager.addLog(LOG_ERROR, 9); // Log przekroczenia liczby prób
            return false;
        }

        operationTimeout.start();
        bool success = bmsConnection.sendRequest(data, length);
        
        if (!success) {
            retryCount++;
            return false;
        }

        retryCount = 0;
        return true;
    }
};

// --- zasieg i srednie ---
// Stałe dla obliczania zasięgu
const float MIN_SPEED_FOR_RANGE = 0.1;        // Minimalna prędkość do obliczeń [km/h]
const float MIN_POWER_FOR_RANGE = 1.0;        // Minimalna moc do obliczeń [W]
const float RANGE_SMOOTH_FACTOR = 0.2;        // Współczynnik wygładzania (0-1)
const uint16_t RANGE_UPDATE_INTERVAL = 5000;  // Interwał aktualizacji [ms]

// Zmienne dla obliczania zasięgu
float smoothedRange = 0.0;           // Wygładzony zasięg
float lastValidRange = 0.0;          // Ostatni prawidłowy zasięg
unsigned long lastRangeUpdate = 0;   // Czas ostatniej aktualizacji

// --- Bufory do przechowywania danych historycznych ---
// Zmiana rozmiaru buforów na stałe wartości
const size_t BUFFER_SIZE = 100;  // Ustalony rozmiar dla buforów danych
const size_t BMS_BUFFER_SIZE = 32;  // Zoptymalizowany rozmiar dla danych BMS

// Zoptymalizowana struktura dla danych historycznych
template<typename T, size_t S>
class CircularDataBuffer {
private:
    T data[S];
    size_t head = 0;
    size_t count = 0;

public:
    void push(T value) {
        data[head] = value;
        head = (head + 1) % S;
        if (count < S) count++;
    }

    T get(size_t index) const {
        if (index >= count) return T();
        return data[(S + head - count + index) % S];
    }

    size_t size() const { return count; }
    void clear() { count = 0; head = 0; }
};

// Zmodyfikowana klasa SafeBuffer z dodatkowym mechanizmem zabezpieczeń
template<typename T, size_t MAX_SIZE>
class SafeBuffer {
private:
    T data[MAX_SIZE];
    size_t head;
    size_t count;
    bool overflowFlag;
    const size_t watermarkHigh;
    const size_t watermarkLow;

public:
    SafeBuffer() : 
        head(0), 
        count(0), 
        overflowFlag(false),
        watermarkHigh(MAX_SIZE * WATERMARK_HIGH / 100),
        watermarkLow(MAX_SIZE * WATERMARK_LOW / 100) {}

    bool push(const T& value) {
        if (count >= MAX_SIZE) {
            overflowFlag = true;
            systemManager.addLog(LOG_WARNING, 5, static_cast<float>(MAX_SIZE)); // Log przepełnienia
            return false;
        }

        data[head] = value;
        head = (head + 1) % MAX_SIZE;
        if (count < MAX_SIZE) count++;

        // Sprawdzenie wysokiego poziomu zapełnienia
        if (count >= watermarkHigh && !overflowFlag) {
            systemManager.addLog(LOG_WARNING, 6, static_cast<float>(count)); // Log ostrzeżenia
        }

        return true;
    }

    T get(size_t index) const {
        if (index >= count) {
            systemManager.addLog(LOG_ERROR, 10, static_cast<float>(index)); // Log błędu indeksu
            return T();
        }
        return data[(MAX_SIZE + head - count + index) % MAX_SIZE];
    }

    void clear() {
        head = 0;
        count = 0;
        overflowFlag = false;
    }

    // Gettery
    size_t size() const { return count; }
    bool isEmpty() const { return count == 0; }
    bool isFull() const { return count >= MAX_SIZE; }
    bool isOverflow() const { return overflowFlag; }
    
    // Sprawdzenie poziomu zapełnienia
    bool isNearCapacity() const { return count >= watermarkHigh; }

    // Obliczenie średniej wartości w buforze
    float getAverage() const {
        if (count == 0) return 0.0f;
        
        float sum = 0.0f;
        for (size_t i = 0; i < count; i++) {
            sum += static_cast<float>(get(i));
        }
        return sum / static_cast<float>(count);
    }
};

// Zdefiniowanie bezpiecznych buforów
SafeBuffer<float, BUFFER_SIZE> energyHistory;
SafeBuffer<float, BUFFER_SIZE> powerHistory;
SafeBuffer<uint8_t, MAX_BMS_BUFFER_SIZE> bmsBuffer;

// Funkcja do bezpiecznego dodawania danych do buforów
void updateBuffers(float energy, float power, float speed) {
    bool energyPushed = energyHistory.push(energy);
    bool powerPushed = powerHistory.push(power);
    bool speedPushed = speedHistory.push(speed);

    // Sprawdzenie czy którykolwiek bufor jest bliski zapełnienia
    if (energyHistory.isNearCapacity() || 
        powerHistory.isNearCapacity() || 
        speedHistory.isNearCapacity()) {
        systemManager.addLog(LOG_WARNING, 7); // Ostrzeżenie o wysokim wykorzystaniu buforów
    }

    // Sprawdzenie błędów zapisu
    if (!energyPushed || !powerPushed || !speedPushed) {
        systemManager.addLog(LOG_ERROR, 11); // Błąd zapisu do buforów
    }
}

// Funkcja do czyszczenia wszystkich buforów
void clearAllBuffers() {
    energyHistory.clear();
    powerHistory.clear();
    speedHistory.clear();
    bmsBuffer.clear();
    systemManager.addLog(LOG_INFO, 4); // Log wyczyszczenia buforów
}

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

// Dodaj stałe dla kadencji jeśli jeszcze nie ma
const uint8_t MIN_VALID_CADENCE = 20;    // Minimalna sensowna kadencja
const uint8_t MAX_VALID_CADENCE = 150;   // Maksymalna sensowna kadencja

class SafeTimer {
private:
    uint32_t lastTime;
    bool firstRun;

public:
    SafeTimer() : lastTime(0), firstRun(true) {}

    void reset() {
        lastTime = esp_timer_get_time() / 1000; // Konwersja na milisekundy
        firstRun = false;
    }

    bool elapsed(uint32_t interval) {
        if (firstRun) {
            reset();
            return false;
        }

        uint32_t currentTime = esp_timer_get_time() / 1000;
        
        // Obsługa przepełnienia
        if (currentTime < lastTime) {
            // Przepełnienie licznika - resetujemy
            lastTime = currentTime;
            return true;
        }

        if (currentTime - lastTime >= interval) {
            lastTime = currentTime;
            return true;
        }
        return false;
    }

    uint32_t getElapsedTime() {
        if (firstRun) return 0;
        
        uint32_t currentTime = esp_timer_get_time() / 1000;
        if (currentTime < lastTime) {
            return 0; // W przypadku przepełnienia
        }
        return currentTime - lastTime;
    }
};

// Deklaracja timerów dla różnych funkcji
SafeTimer screenTimer;        // Timer dla aktualizacji ekranu
SafeTimer tempTimer;          // Timer dla odczytu temperatury
SafeTimer updateTimer;        // Timer dla ogólnych aktualizacji
SafeTimer stationaryTimer;    // Timer dla wykrywania postoju
SafeTimer ds18b20Timer;       // Timer dla czujnika temperatury
SafeTimer rangeUpdateTimer;   // Timer dla aktualizacji zasięgu

// Zmienne dla interwałów czasowych (w ms)
const uint32_t SCREEN_UPDATE_INTERVAL = 500;
const uint32_t TEMP_READ_INTERVAL = 1000;
const uint32_t GENERAL_UPDATE_INTERVAL = 5000;
const uint32_t STATIONARY_TIMEOUT = 10000;
const uint32_t DS18B20_CONVERSION_DELAY = 750;

// Struktura dla bezpiecznego licznika impulsów
struct SafeCounter {
    volatile uint32_t count;
    volatile uint32_t lastResetTime;
    
    void increment() {
        if (count < UINT32_MAX) {
            count++;
        }
    }
    
    uint32_t getAndReset() {
        uint32_t current = count;
        count = 0;
        lastResetTime = esp_timer_get_time() / 1000;
        return current;
    }
};

class TimeoutHandler {
private:
    uint32_t startTime;
    uint32_t timeoutPeriod;
    bool isRunning;

public:
    TimeoutHandler(uint32_t timeout_ms = 0) : 
        startTime(0), 
        timeoutPeriod(timeout_ms), 
        isRunning(false) {}

    void start(uint32_t timeout_ms = 0) {
        if (timeout_ms > 0) timeoutPeriod = timeout_ms;
        startTime = esp_timer_get_time() / 1000;
        isRunning = true;
    }

    bool isExpired() {
        if (!isRunning) return false;
        return (esp_timer_get_time() / 1000 - startTime) >= timeoutPeriod;
    }

    void stop() {
        isRunning = false;
    }

    uint32_t getElapsed() {
        if (!isRunning) return 0;
        return (esp_timer_get_time() / 1000 - startTime);
    }
};

// Stałe dla timeoutów BLE
const uint32_t BLE_CONNECT_TIMEOUT = 5000;    // 5 sekund na połączenie
const uint32_t BLE_OPERATION_TIMEOUT = 1000;  // 1 sekunda na operację
const uint8_t BLE_MAX_RETRIES = 3;           // Maksymalna liczba prób

// Stałe dla mechanizmu recovery BLE
#define BLE_MAX_RECONNECT_ATTEMPTS 5     // Maksymalna liczba prób ponownego połączenia
#define BLE_RECONNECT_DELAY 1000         // Opóźnienie między próbami (ms)
#define BLE_RECOVERY_TIMEOUT 30000       // Timeout dla całego procesu recovery (ms)
#define BLE_DATA_TIMEOUT 5000            // Timeout dla oczekiwania na dane (ms)

class BMSConnection {
private:
    TimeoutHandler connectTimeout;
    TimeoutHandler operationTimeout;
    TimeoutHandler recoveryTimeout;
    uint8_t retryCount;
    uint8_t recoveryAttempts;
    bool isConnecting;
    bool inRecoveryMode;
    uint32_t lastConnectAttempt;

    // Bufor dla danych podczas recovery
    struct {
        float lastValidVoltage;
        float lastValidCurrent;
        float lastValidCapacity;
        uint8_t lastValidSoc;
        uint32_t timestamp;
    } recoveryData;

public:
    BMSConnection() : 
        connectTimeout(BLE_CONNECT_TIMEOUT),
        operationTimeout(BLE_OPERATION_TIMEOUT),
        recoveryTimeout(BLE_RECOVERY_TIMEOUT),
        retryCount(0),
        recoveryAttempts(0),
        isConnecting(false),
        inRecoveryMode(false),
        lastConnectAttempt(0) {
        memset(&recoveryData, 0, sizeof(recoveryData));
    }

    bool connect() {
        if (bleClient->isConnected()) {
            inRecoveryMode = false;
            return true;
        }

        uint32_t currentTime = millis();
        if (inRecoveryMode) {
            if (recoveryTimeout.isExpired()) {
                exitRecoveryMode();
                return false;
            }
            if (currentTime - lastConnectAttempt < BLE_RECONNECT_DELAY) {
                return false;
            }
        }

        if (isConnecting && !connectTimeout.isExpired()) return false;

        isConnecting = true;
        connectTimeout.start();
        lastConnectAttempt = currentTime;

        if (!inRecoveryMode) {
            recoveryAttempts = 0;
            inRecoveryMode = true;
            recoveryTimeout.start(BLE_RECOVERY_TIMEOUT);
            saveCurrentState(); // Zapisz ostatni znany stan
        }

        if (recoveryAttempts >= BLE_MAX_RECONNECT_ATTEMPTS) {
            systemManager.addLog(LOG_ERROR, 2, recoveryAttempts); // Log błędu
            exitRecoveryMode();
            return false;
        }

        // Próba połączenia
        if (bleClient->connect(bmsMacAddress)) {
            isConnecting = false;
            recoveryAttempts = 0;
            if (initializeServices()) {
                inRecoveryMode = false;
                systemManager.addLog(LOG_INFO, 1); // Log sukcesu
                return true;
            }
        }

        recoveryAttempts++;
        isConnecting = false;
        return false;
    }

    bool sendRequest(uint8_t* data, size_t length) {
        if (!bleClient->isConnected()) return false;

        operationTimeout.start();
        notificationReceived = false;

        if (!bleCharacteristicTx->writeValue(data, length)) {
            systemManager.addLog(LOG_ERROR, 5); // Log błędu wysyłania
            return false;
        }

        // Czekaj na odpowiedź z timeoutem
        while (!notificationReceived) {
            if (operationTimeout.isExpired()) {
                systemManager.addLog(LOG_ERROR, 6); // Log timeout odpowiedzi
                startRecovery();
                return false;
            }
            delay(1);
        }

        return true;
    }

    void startRecovery() {
        if (!inRecoveryMode) {
            inRecoveryMode = true;
            recoveryAttempts = 0;
            recoveryTimeout.start();
            saveCurrentState();
            systemManager.addLog(LOG_WARNING, 7); // Log rozpoczęcia recovery
        }
    }

    bool isRecovering() const {
        return inRecoveryMode;
    }

    uint8_t getRecoveryAttempts() const {
        return recoveryAttempts;
    }

private:
    void exitRecoveryMode() {
        inRecoveryMode = false;
        isConnecting = false;
        recoveryAttempts = 0;
        restoreLastState(); // Przywróć ostatni znany stan
        systemManager.addLog(LOG_INFO, 2); // Log zakończenia recovery
    }

    void saveCurrentState() {
        recoveryData.lastValidVoltage = voltage;
        recoveryData.lastValidCurrent = current;
        recoveryData.lastValidCapacity = remainingCapacity;
        recoveryData.lastValidSoc = soc;
        recoveryData.timestamp = millis();
    }

    void restoreLastState() {
        // Jeśli dane nie są zbyt stare (np. nie starsze niż 1 minuta)
        if (millis() - recoveryData.timestamp < 60000) {
            voltage = recoveryData.lastValidVoltage;
            current = recoveryData.lastValidCurrent;
            remainingCapacity = recoveryData.lastValidCapacity;
            soc = recoveryData.lastValidSoc;
        } else {
            // Dane są zbyt stare - ustaw wartości bezpieczne
            voltage = 0;
            current = 0;
            remainingCapacity = 0;
            soc = 0;
            systemManager.addLog(LOG_WARNING, 8); // Log o przestarzałych danych
        }
    }

    bool initializeServices() {
        operationTimeout.start();

        // Inicjalizacja usług BLE z timeoutem
        bleService = bleClient->getService(SERVICE_UUID);
        if (!bleService || operationTimeout.isExpired()) {
            systemManager.addLog(LOG_ERROR, 3); // Log błędu usługi
            return false;
        }

        // Inicjalizacja charakterystyk z timeoutem
        if (!initializeCharacteristics()) {
            systemManager.addLog(LOG_ERROR, 4); // Log błędu charakterystyk
            return false;
        }

        return true;
    }

    bool initializeCharacteristics() {
        if (operationTimeout.isExpired()) return false;

        bleCharacteristicTx = bleService->getCharacteristic(CHAR_TX_UUID);
        bleCharacteristicRx = bleService->getCharacteristic(CHAR_RX_UUID);

        if (!bleCharacteristicTx || !bleCharacteristicRx) {
            #if DEBUG
            Serial.println("Nie znaleziono charakterystyk BLE");
            #endif
            return false;
        }

        // Rejestracja notyfikacji
        if (bleCharacteristicRx->canNotify()) {
            bleCharacteristicRx->registerForNotify(notificationCallback);
            return true;
        }

        return false;
    }
};

class TemperatureSensor {
private:
    TimeoutHandler conversionTimeout;
    TimeoutHandler readTimeout;
    bool conversionInProgress;

public:
    TemperatureSensor() : 
        conversionTimeout(DS18B20_CONVERSION_DELAY),
        readTimeout(1000),  // 1 sekunda na odczyt
        conversionInProgress(false) {}

    void requestTemperature() {
        if (conversionInProgress) return;
        
        sensors.requestTemperatures();
        conversionTimeout.start();
        conversionInProgress = true;
    }

    bool isReady() {
        if (!conversionInProgress) return false;
        if (conversionTimeout.isExpired()) {
            conversionInProgress = false;
            return true;
        }
        return false;
    }

    float readTemperature() {
        if (!conversionInProgress) return -999.0;

        readTimeout.start();
        float temp = sensors.getTempCByIndex(0);

        if (readTimeout.isExpired()) {
            #if DEBUG
            Serial.println("Timeout odczytu temperatury");
            #endif
            return -999.0;
        }

        conversionInProgress = false;
        return isValidTemperature(temp) ? temp : -999.0;
    }
};

// Utworzenie obiektów globalnych
BMSConnection bmsConnection;
TemperatureSensor tempSensor;
// Liczniki dla prędkości i kadencji
SafeCounter speedCounter;
SafeCounter cadenceCounter;

SystemManager systemManager;

// Klasa pomocnicza do bezpiecznych operacji matematycznych
class SafeMath {
public:
    // Bezpieczne dzielenie zmiennoprzecinkowe
    static float safeDivide(float numerator, float denominator, float defaultValue = 0.0f) {
        if (abs(denominator) < 0.000001f || !isfinite(denominator)) {
            return defaultValue;
        }
        float result = numerator / denominator;
        return isfinite(result) ? result : defaultValue;
    }

    // Bezpieczne dzielenie całkowitoliczbowe
    static uint32_t safeDivide(uint32_t numerator, uint32_t denominator, uint32_t defaultValue = 0) {
        if (denominator == 0) {
            return defaultValue;
        }
        return numerator / denominator;
    }

    // Sprawdzenie czy liczba jest w bezpiecznym zakresie
    static bool isInRange(float value, float min, float max) {
        if (!isfinite(value)) return false;
        return value >= min && value <= max;
    }

    // Bezpieczne ograniczenie wartości do zakresu
    static float clamp(float value, float min, float max) {
        if (!isfinite(value)) return min;
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
}; // Dodany średnik

// Stałe obliczane w czasie kompilacji - dodaj je na początku kodu
constexpr float SPEED_FACTOR = WHEEL_CIRCUMFERENCE * 3.6f / 1000000.0f;  // Przelicznik dla prędkości
constexpr float POWER_FACTOR = 1.0f / 3600.0f;                          // Przelicznik dla mocy

void toggleLightMode();
void toggleUSBMode();
uint8_t getSubScreenCount(int screen);
float calculateCadence();

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
        digitalWrite(FrontDayPin, LOW);
        digitalWrite(FrontPin, LOW);
        digitalWrite(RealPin, LOW);
        return;
    }

    // Wybierz ustawienia w zależności od trybu
    bool isDay = (currentLightMode == LIGHTS_DAY);
    int config = isDay ? bikeSettings.daySetting : bikeSettings.nightSetting;
    bool shouldBlink = isDay ? bikeSettings.dayRearBlink : bikeSettings.nightRearBlink;

    // Ustaw światła według konfiguracji
    digitalWrite(FrontDayPin, (config == FRONT_DAY || config == FRONT_DAY_AND_REAR));
    digitalWrite(FrontPin, (config == FRONT_NORMAL || config == FRONT_NORMAL_AND_REAR));
    
    // Obsługa tylnego światła
    if (config == REAR_ONLY || config == FRONT_DAY_AND_REAR || config == FRONT_NORMAL_AND_REAR) {
        if (shouldBlink) {
            static unsigned long lastBlinkTime = 0;
            if (millis() - lastBlinkTime >= bikeSettings.blinkInterval) {
                rearBlinkState = !rearBlinkState;
                lastBlinkTime = millis();
            }
            digitalWrite(RealPin, rearBlinkState);
        } else {
            digitalWrite(RealPin, HIGH);
        }
    } else {
        digitalWrite(RealPin, LOW);
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

// Zoptymalizowana obsługa przerwania przycisku
void IRAM_ATTR handleButtonISR() {
    static InterruptManager buttonInterruptManager;
    
    if (!buttonInterruptManager.shouldHandleInterrupt()) {
        return;
    }

    portENTER_CRITICAL_ISR(&buttonMux);
    buttonState = digitalRead(buttonPin);
    lastButtonChangeTime = timeManager.getTimestamp();
    portEXIT_CRITICAL_ISR(&buttonMux);
}

// Funkcja do przetwarzania stanu przycisku w głównej pętli
void processButtonState() {
    static uint32_t lastProcessTime = 0;
    uint32_t currentTime = esp_timer_get_time();
    
    portENTER_CRITICAL(&buttonMux);
    bool currentButtonState = buttonState;
    uint32_t changeTime = lastButtonChangeTime;
    portEXIT_CRITICAL(&buttonMux);
    
    // Debouncing i przetwarzanie w głównej pętli
    if ((currentTime - lastProcessTime) >= debounceDelay * 1000) { // konwersja na mikrosekundy
        if (!currentButtonState) { // Przycisk naciśnięty (aktywny LOW)
            if (!buttonPressed) {
                buttonPressed = true;
                buttonHoldStart = currentTime;
                longPressHandled = false;
            }
        } else { // Przycisk zwolniony
            if (buttonPressed) {
                // Obsługa zwolnienia przycisku
                if (!longPressHandled && (currentTime - buttonHoldStart < holdTime * 1000)) {
                    // Krótkie naciśnięcie
                    handleShortPress();
                }
                buttonPressed = false;
            }
        }
        lastProcessTime = currentTime;
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

// Obsługa przerwania kadencji
void handleCadence() {
    portENTER_CRITICAL(&cadenceMux);
    bool newData = cadenceSensor.newPulse;
    cadenceSensor.newPulse = false;
    portEXIT_CRITICAL(&cadenceMux);

    if (!newData) return;

    uint8_t currentCadence = calculateCadence();
    
    // Walidacja i aktualizacja danych kadencji
    if (currentCadence >= MIN_VALID_CADENCE && currentCadence <= MAX_VALID_CADENCE) {
        cadenceData.lastValidCadence = currentCadence;
        cadenceData.totalCadence += currentCadence;
        cadenceData.cadenceReadings++;
        
        // Aktualizacja maksymalnej kadencji
        if (currentCadence > cadenceData.maxCadence) {
            cadenceData.maxCadence = currentCadence;
        }
        
        // Obliczenie średniej kadencji
        cadenceData.averageCadence = SafeMath::safeDivide(
            static_cast<float>(cadenceData.totalCadence),
            static_cast<float>(cadenceData.cadenceReadings)
        );

        // Sprawdzenie czy kadencja jest w optymalnym zakresie
        if (currentCadence < cadenceMin) {
            systemManager.addLog(LOG_WARNING, 10); // Kadencja za niska
        } else if (currentCadence > cadenceMax) {
            systemManager.addLog(LOG_WARNING, 11); // Kadencja za wysoka
        }
    }

    // Reset liczników jeśli minął określony czas bez impulsów
    if (millis() - cadenceSensor.lastPulseTime > CADENCE_TIMEOUT_US/1000) {
        cadenceData.lastValidCadence = 0;
    }
}

// Funkcja do odczytu aktualnej kadencji
uint8_t getCurrentCadence() {
    return cadenceData.lastValidCadence;
}

// Funkcja do odczytu średniej kadencji
float getAverageCadence() {
    return cadenceData.averageCadence;
}

// Funkcja do odczytu maksymalnej kadencji
uint32_t getMaxCadence() {
    return cadenceData.maxCadence;
}

// Zmodyfikowana funkcja calculateAndDisplayCadence
void calculateAndDisplayCadence() {
    uint8_t currentCadence = getCurrentCadence();
    
    // Aktualizacja wyświetlacza tylko jeśli są nowe dane
    if (currentCadence > 0) {
        // Logika wyświetlania jest już w showScreen()
        state.displayNeedsUpdate = true;
    }
}

// Reset statystyk kadencji
void resetCadenceStats() {
    portENTER_CRITICAL(&cadenceMux);
    cadenceData = CadenceData(); // Reset wszystkich wartości
    portEXIT_CRITICAL(&cadenceMux);
}

// Obsługa krótkiego naciśnięcia przycisku
void handleShortPress() {
    handleButton();  // Wywołanie istniejącej funkcji obsługi przycisku
    state.displayNeedsUpdate = true;  // Oznaczenie że wyświetlacz wymaga aktualizacji
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
          //float speed = calculateSpeed();
          //display.print(speed, 1);
          display.print("25.5");          
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
    currentLightMode = (currentLightMode + 1) % 3;  // Przełączanie między LIGHTS_OFF (0), LIGHTS_DAY (1), LIGHTS_NIGHT (2)
    lightsOn = (currentLightMode != LIGHTS_OFF);    // Światła są włączone, gdy nie jesteśmy w trybie LIGHTS_OFF
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

// Dodaj na początku pliku po innych definicjach
#define TIMESTAMP_ROLLOVER_VALUE 0xFFFFFFFF
#define MIN_INTERRUPT_INTERVAL_US 50  // 50 mikrosekund minimalna przerwa między przerwaniami

// Klasa do bezpiecznego zarządzania znacznikami czasu
class TimeStampManager {
private:
    uint32_t lastRolloverCheck;
    uint32_t rollovers;

public:
    TimeStampManager() : lastRolloverCheck(0), rollovers(0) {}

    // Bezpieczne pobranie czasu z obsługą przepełnienia
    uint32_t getTimestamp() {
        uint32_t currentTime = esp_timer_get_time() / 1000; // Konwersja na ms
        
        // Sprawdź przepełnienie
        if (currentTime < lastRolloverCheck) {
            rollovers++;
            systemManager.addLog(LOG_INFO, 5); // Log przepełnienia licznika
        }
        
        lastRolloverCheck = currentTime;
        return currentTime;
    }

    // Bezpieczne obliczanie różnicy czasu
    uint32_t getElapsedTime(uint32_t startTime) {
        uint32_t currentTime = getTimestamp();
        
        if (currentTime < startTime) {
            // Obsługa przepełnienia
            return (TIMESTAMP_ROLLOVER_VALUE - startTime) + currentTime;
        }
        
        return currentTime - startTime;
    }
};

TimeStampManager timeManager;

// Klasa do zarządzania przerwaniami
class InterruptManager {
private:
    static const uint8_t MAX_DEBOUNCE_COUNT = 3;
    volatile uint32_t lastInterruptTime;
    volatile uint8_t debounceCount;
    TimeStampManager timeManager;

public:
    InterruptManager() : lastInterruptTime(0), debounceCount(0) {}

    // Sprawdź czy przerwanie powinno być obsłużone
    bool shouldHandleInterrupt() {
        uint32_t currentTime = timeManager.getTimestamp();
        uint32_t timeSinceLastInterrupt = timeManager.getElapsedTime(lastInterruptTime);

        if (timeSinceLastInterrupt < MIN_INTERRUPT_INTERVAL_US) {
            debounceCount++;
            if (debounceCount >= MAX_DEBOUNCE_COUNT) {
                systemManager.addLog(LOG_WARNING, 12); // Log zbyt częstych przerwań
                debounceCount = 0;
            }
            return false;
        }

        debounceCount = 0;
        lastInterruptTime = currentTime;
        return true;
    }
};

// Utworzenie globalnych instancji
TimeStampManager timeManager;
InterruptManager interruptManager;

// --- Funkcja do obliczania i wyświetlania kadencji ---
// Przerwanie dla kadencji
void IRAM_ATTR handleCadenceInterrupt() {
    static InterruptManager cadenceInterruptManager;
    
    if (!cadenceInterruptManager.shouldHandleInterrupt()) {
        return;
    }

    portENTER_CRITICAL_ISR(&cadenceMux);
    uint32_t currentTime = timeManager.getTimestamp();
    
    cadenceSensor.pulseInterval = timeManager.getElapsedTime(cadenceSensor.lastPulseTime);
    cadenceSensor.lastPulseTime = currentTime;
    cadenceSensor.pulseCount++;
    cadenceSensor.newPulse = true;
    
    portEXIT_CRITICAL_ISR(&cadenceMux);
}

// Funkcja do bezpiecznego odczytu danych czujnika
bool getSensorData(SensorData* sensor, portMUX_TYPE* mux, uint32_t* interval, uint32_t* count) {
    bool newData = false;
    
    portENTER_CRITICAL(mux);
    if (sensor->newPulse) {
        *interval = sensor->pulseInterval;
        *count = sensor->pulseCount;
        sensor->newPulse = false;
        newData = true;
    }
    portEXIT_CRITICAL(mux);
    
    return newData;
}

// Funkcja obliczająca kadencję
uint8_t calculateCadence() {
    static uint8_t lastValidCadence = 0;

    portENTER_CRITICAL(&cadenceMux);
    uint32_t interval = cadenceSensor.pulseInterval;
    portEXIT_CRITICAL(&cadenceMux);

    // Sprawdzenie warunków brzegowych
    if (interval == 0 || interval > CADENCE_TIMEOUT_US) {
        return 0;
    }

    // Obliczenie kadencji (RPM) z zabezpieczeniem
    // (60 * 1000000) / (interwał [us] * liczba magnesów)
    uint32_t rpm = SafeMath::safeDivide(
        60000000U,
        interval * MAGNETS,
        0
    );

    // Walidacja wyniku
    if (rpm < MAX_CADENCE) {
        lastValidCadence = rpm;
    }

    return lastValidCadence;
}

// --- Funkcja do aktualizacji bieżących danych ---
void updateData(float current, float voltage, float speedKmh) {
    power = current * voltage;  // Moc w W (watty)
    wh = power * POWER_FACTOR; // Zoptymalizowane obliczenie zużycia energii

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

// --- Funkcja do obliczania zasięgu ---

// Zmienne dla obliczania zasięgu
float smoothedRange = 0.0;           // Wygładzony zasięg
float lastValidRange = 0.0;          // Ostatni prawidłowy zasięg
unsigned long lastRangeUpdate = 0;   // Czas ostatniej aktualizacji

float calculateRange() {
    if (speedKmh < MIN_SPEED_FOR_RANGE || power < MIN_POWER_FOR_RANGE) {
        return lastValidRange;
    }

    float whPerKm = 0.0f;
    
    // Obliczenie zużycia energii na kilometr
    if (speedHistory.size() > 0 && energyHistory.size() > 0) {
        float totalEnergy = 0.0f;
        float totalDistance = 0.0f;
        
        // Sumowanie historii
        for (size_t i = 0; i < energyHistory.size(); i++) {
            totalEnergy += energyHistory[i];
            if (i < speedHistory.size()) {
                totalDistance += SafeMath::safeDivide(speedHistory[i], 3600.0f, 0.0f);
            }
        }
        
        // Obliczenie średniego zużycia na km
        if (totalDistance > 0.0f) {
            whPerKm = SafeMath::safeDivide(
                totalEnergy * 3600.0f,
                totalDistance,
                0.0f
            );
        }
    }

    // Obliczenie teoretycznego zasięgu
    float theoreticalRange = 0.0f;
    if (whPerKm > 0.0f) {
        theoreticalRange = SafeMath::safeDivide(
            remainingEnergy,
            whPerKm,
            0.0f
        );
    }

    // Walidacja i ograniczenie zasięgu
    if (SafeMath::isInRange(theoreticalRange, 0.0f, MAX_RANGE)) {
        lastValidRange = theoreticalRange;
    }

    return lastValidRange;
}

// Funkcja do resetowania obliczeń zasięgu
void resetRangeCalculations() {
    smoothedRange = 0;
    lastValidRange = 0;
    energyHistory.clear();
    speedHistory.clear();
    powerHistory.clear();
}

// --- Funkcja do obliczenia średniego zużycia Wh ---
float getAverageWh() {
    if (dataCount == 0) return 0.0f;
    
    return SafeMath::safeDivide(
        totalWh * 3600.0f,
        dataCount,
        0.0f
    );
}

// --- Funkcja do obliczenia średniej mocy --- 
float getAveragePower() {
    if (dataCount == 0) return 0.0f;
    
    return SafeMath::safeDivide(
        totalPower,
        dataCount,
        0.0f
    );
}

static unsigned long cadenceReadings[NUM_READINGS];

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
  Wire.begin(21, 22);  //SDA = GPIO 21, SCL = GPIO 22

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

    systemManager.begin();
    
    // Po każdej ważnej inicjalizacji dodaj log
    if (!rtc.begin()) {
        systemManager.addLog(LOG_CRITICAL, 1);  // Błąd RTC
        while (1);
    }
}

SystemManager systemManager;

// --- PĘTLA GŁÓWNA ---
void loop() {
    static uint32_t lastUpdate = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Obsługa przerwań za pomocą flag
    uint32_t flags;
    portENTER_CRITICAL(&mux);
    flags = interruptFlags;
    interruptFlags = 0;
    portEXIT_CRITICAL(&mux);
    
    if (flags & FLAG_CADENCE) handleCadence();
    if (flags & FLAG_BUTTON) handleButton();
    
    // Obsługa BMS
    if (!bmsConnection.connect()) {
        // Obsługa błędu połączenia
        #if DEBUG
        Serial.println("Nie można połączyć z BMS");
        #endif
    }

    // Obsługa czujnika temperatury
    if (!tempSensor.isReady()) {
        tempSensor.requestTemperature();
    } else {
        currentTemp = tempSensor.readTemperature();
    }
  
    // Aktualizacja ekranu
    if (screenTimer.elapsed(SCREEN_UPDATE_INTERVAL)) {
        showScreen(currentScreen);
    }

    // Sprawdzanie i odczyt temperatury
    if (tempTimer.elapsed(TEMP_READ_INTERVAL)) {
        if (isGroundTemperatureReady()) {
            currentTemp = readGroundTemperature();
            requestGroundTemperature();
        }
    }

    // Aktualizacja danych jeśli nie jest w trybie postoju
    if (!stationary && updateTimer.elapsed(updateInterval)) {
        // Dynamiczne dostosowanie interwału aktualizacji
        adjustUpdateInterval(speedKmh);
        
        // Aktualizacja danych
        updateData(current, voltage, speedKmh);
    }

    // Aktualizacja zasięgu
    if (rangeUpdateTimer.elapsed(RANGE_UPDATE_INTERVAL)) {
        if (!stationary) {
            float currentRange = calculateRange();
            if (abs(currentRange - displayedRange) > 0.1) {
                displayedRange = currentRange;
            }
        }
    }

    // Sprawdzanie stanu systemu
    if (!systemManager.checkSystem()) {
        systemManager.handleSystemFailure();
    }

    // Reset watchdoga
    esp_task_wdt_reset();
}
