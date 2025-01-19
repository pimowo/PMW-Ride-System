/******************************************
* Biblioteki
******************************************/
#include <TinyWireS.h>
#include <avr/wdt.h>

/******************************************
* Definicje sprzętowe
******************************************/
// Piny GPIO
#define FRONT_PIN 0  // PB0 - światło przednie
#define DAY_PIN   1  // PB1 - światło dzienne
#define REAR_PIN  2  // PB2 - światło tylne

// Adres I2C urządzenia
#define I2C_ADDRESS 0x20

/******************************************
* Definicje stanów i komend
******************************************/
// Bity stanu świateł
#define LIGHT_FRONT 0b00000001  // Bit dla światła przedniego
#define LIGHT_DAY   0b00000010  // Bit dla światła dziennego
#define LIGHT_REAR  0b00000100  // Bit dla światła tylnego

// Komendy I2C
#define CMD_FRONT_LIGHT 0x01  // Komenda przełączania światła przedniego
#define CMD_DAY_LIGHT   0x02  // Komenda przełączania światła dziennego
#define CMD_REAR_LIGHT  0x03  // Komenda przełączania światła tylnego
#define CMD_SET_EFFECT  0x04  // Komenda ustawiania efektu

/******************************************
* Definicje efektów i ich parametrów
******************************************/
// Stałe czasowe dla efektów (w milisekundach)
const uint32_t BLINK_INTERVAL = 500;         // Zwykłe miganie
const uint32_t FAST_BLINK_INTERVAL = 250;    // Szybkie miganie
const uint32_t SLOW_BLINK_INTERVAL = 1000;   // Wolne miganie
const uint32_t DOUBLE_BLINK_INTERVAL = 125;  // Podwójne miganie
const uint32_t PULSE_INTERVAL = 50;          // Interwał pulsowania

// Maksymalna liczba kroków dla efektów
#define MAX_PULSE_STEPS 8
#define MAX_DOUBLE_BLINK_STEPS 4
#define MAX_BLINK_STEPS 2

// Typy efektów dla światła tylnego
enum RearEffect {
    EFFECT_NONE = 0,      // Brak efektu
    EFFECT_BLINK,         // Zwykłe miganie
    EFFECT_DOUBLE_BLINK,  // Podwójne miganie
    EFFECT_PULSE,         // Pulsowanie
    EFFECT_FAST_BLINK,    // Szybkie miganie
    EFFECT_SLOW_BLINK     // Wolne miganie
};

// Tablica wartości dla efektu pulsowania (zapisana w pamięci programu)
const uint8_t PROGMEM pulseValues[] = {0, 36, 127, 218, 255, 218, 127, 36};

/******************************************
* Struktury i zmienne globalne
******************************************/
// Struktura przechowująca stan efektu
struct EffectState {
    uint32_t lastTime;  // Czas ostatniej aktualizacji efektu
    uint8_t step;       // Aktualny krok efektu
    bool active;        // Czy efekt jest aktywny
};

// Zmienne globalne
volatile uint8_t lightState = 0;                  // Stan wszystkich świateł
volatile RearEffect currentEffect = EFFECT_NONE;  // Aktualny efekt
EffectState effectState = {0, 0, false};          // Stan efektu

// Bufor dla komunikacji I2C
#define I2C_BUFFER_SIZE 8
volatile uint8_t i2cBuffer[I2C_BUFFER_SIZE];
volatile uint8_t i2cBufferIndex = 0;

/******************************************
* Funkcje pomocnicze
******************************************/
// Funkcja sprawdzająca poprawność komendy I2C
bool isValidCommand(uint8_t cmd) {
    return (cmd >= CMD_FRONT_LIGHT && cmd <= CMD_SET_EFFECT);
}

// Funkcja sprawdzająca poprawność wartości efektu
bool isValidEffect(uint8_t effect) {
    return (effect <= EFFECT_SLOW_BLINK);
}

/******************************************
* Funkcje obsługi efektów
******************************************/
// Obsługa efektów opartych na prostym interwale czasowym
void handleTimedEffect(uint32_t currentTime, uint32_t interval, uint8_t maxSteps) {
    if (currentTime - effectState.lastTime >= interval) {
        effectState.lastTime = currentTime;
        effectState.step = (effectState.step + 1) & (maxSteps - 1); // Zabezpieczenie przed przepełnieniem
        digitalWrite(REAR_PIN, effectState.step == 0);
    }
}

// Obsługa efektu podwójnego mignięcia
void handleDoubleBlinkEffect(uint32_t currentTime) {
    if (currentTime - effectState.lastTime >= DOUBLE_BLINK_INTERVAL) {
        effectState.lastTime = currentTime;
        switch(effectState.step) {
            case 0: digitalWrite(REAR_PIN, HIGH); break;
            case 1: digitalWrite(REAR_PIN, LOW); break;
            case 2: digitalWrite(REAR_PIN, HIGH); break;
            case 3: digitalWrite(REAR_PIN, LOW); break;
        }
        effectState.step = (effectState.step + 1) & (MAX_DOUBLE_BLINK_STEPS - 1); // Zabezpieczenie przed przepełnieniem
    }
}

// Obsługa efektu pulsowania
void handlePulseEffect(uint32_t currentTime) {
    if (currentTime - effectState.lastTime >= PULSE_INTERVAL) {
        effectState.lastTime = currentTime;
        effectState.step = (effectState.step + 1) & (MAX_PULSE_STEPS - 1); // Zabezpieczenie przed przepełnieniem
        analogWrite(REAR_PIN, pgm_read_byte(&pulseValues[effectState.step]));
    }
}

// Główna funkcja obsługująca efekty świetlne
void handleEffects() {
    if (!(lightState & LIGHT_REAR) || currentEffect == EFFECT_NONE) {
        digitalWrite(REAR_PIN, LOW);
        effectState.active = false;
        return;
    }

    uint32_t currentTime = millis();
    
    switch(currentEffect) {
        case EFFECT_BLINK:
            handleTimedEffect(currentTime, BLINK_INTERVAL, MAX_BLINK_STEPS);
            break;
        case EFFECT_DOUBLE_BLINK:
            handleDoubleBlinkEffect(currentTime);
            break;
        case EFFECT_PULSE:
            handlePulseEffect(currentTime);
            break;
        case EFFECT_FAST_BLINK:
            handleTimedEffect(currentTime, FAST_BLINK_INTERVAL, MAX_BLINK_STEPS);
            break;
        case EFFECT_SLOW_BLINK:
            handleTimedEffect(currentTime, SLOW_BLINK_INTERVAL, MAX_BLINK_STEPS);
            break;
    }
}

/******************************************
* Funkcje obsługi I2C
******************************************/
// Przetwarzanie otrzymanych komend I2C
void processI2CBuffer() {
    if (i2cBufferIndex < 1) return;
    
    uint8_t command = i2cBuffer[0];
    
    // Sprawdzenie poprawności komendy
    if (!isValidCommand(command)) {
        i2cBufferIndex = 0;  // Reset bufora w przypadku błędnej komendy
        return;
    }
    
    uint8_t value = (i2cBufferIndex > 1) ? i2cBuffer[1] : 0;
    
    switch(command) {
        case CMD_FRONT_LIGHT:
            lightState ^= LIGHT_FRONT; 
            break;
        case CMD_DAY_LIGHT:
            lightState ^= LIGHT_DAY; 
            break;
        case CMD_REAR_LIGHT:
            lightState ^= LIGHT_REAR; 
            break;
        case CMD_SET_EFFECT:
            if (isValidEffect(value)) {
                currentEffect = (RearEffect)value;
                effectState.step = 0;
                effectState.lastTime = millis();
                effectState.active = true;
            }
            break;
    }
    
    updateLights();
    i2cBufferIndex = 0;
}

// Obsługa zdarzenia otrzymania danych przez I2C
void receiveEvent(uint8_t howMany) {
    while (TinyWireS.available() && i2cBufferIndex < I2C_BUFFER_SIZE) {
        i2cBuffer[i2cBufferIndex++] = TinyWireS.receive();
    }
}

/******************************************
* Funkcje zarządzania stanem świateł
******************************************/
// Aktualizacja stanu wszystkich świateł
void updateLights() {
    digitalWrite(FRONT_PIN, (lightState & LIGHT_FRONT) ? HIGH : LOW);
    digitalWrite(DAY_PIN, (lightState & LIGHT_DAY) ? HIGH : LOW);
    
    if (!(lightState & LIGHT_REAR)) {
        digitalWrite(REAR_PIN, LOW);
        currentEffect = EFFECT_NONE;
        effectState.active = false;
    }
}

/******************************************
* Funkcje główne
******************************************/
void setup() {
    // Konfiguracja pinów jako wyjścia
    pinMode(FRONT_PIN, OUTPUT);
    pinMode(DAY_PIN, OUTPUT);
    pinMode(REAR_PIN, OUTPUT);
    
    // Wyłączenie wszystkich świateł na starcie
    digitalWrite(FRONT_PIN, LOW);
    digitalWrite(DAY_PIN, LOW);
    digitalWrite(REAR_PIN, LOW);
    
    // Inicjalizacja stanu efektu
    effectState.lastTime = 0;
    effectState.step = 0;
    effectState.active = false;
    
    // Inicjalizacja I2C
    TinyWireS.begin(I2C_ADDRESS);
    TinyWireS.onReceive(receiveEvent);
    
    // Włączenie watchdoga (restart po 1 sekundzie w przypadku zawieszenia)
    wdt_enable(WDTO_1S);
}

void loop() {
    wdt_reset();             // Reset licznika watchdoga
    TinyWireS_stop_check();  // Sprawdzenie stanu I2C
    
    processI2CBuffer();      // Przetworzenie otrzymanych komend I2C
    handleEffects();         // Obsługa efektów świetlnych
}
