#ifndef ODOMETER_H
#define ODOMETER_H

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

class OdometerManager {
private:
    const char* filename = "/odometer.json";
    float currentTotal = 0;
    bool initialized = false;

    void saveToFile() {
        File file = LittleFS.open(filename, "w");
        if (!file) {
            #ifdef DEBUG
            Serial.println("Błąd otwarcia pliku licznika do zapisu");
            #endif
            return;
        }

        StaticJsonDocument<128> doc;
        doc["total"] = currentTotal;
        
        if (serializeJson(doc, file) == 0) {
            #ifdef DEBUG
            Serial.println("Błąd zapisu do pliku licznika");
            #endif
        } else {
            file.flush(); // Wymuszenie zapisu na kartę
            #ifdef DEBUG
            Serial.printf("Zapisano licznik: %.2f\n", currentTotal);
            #endif
        }
        
        file.close();
    }

    void loadFromFile() {
        if (!initialized) {
            File file = LittleFS.open(filename, "r");
            if (!file) {
                #ifdef DEBUG
                Serial.println("Brak pliku licznika - tworzę nowy");
                #endif
                currentTotal = 0;
                saveToFile();
                initialized = true;
                return;
            }

            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, file);
            
            if (!error) {
                currentTotal = doc["total"] | 0.0f;
                #ifdef DEBUG
                Serial.printf("Wczytano licznik: %.2f\n", currentTotal);
                #endif
            } else {
                #ifdef DEBUG
                Serial.println("Błąd odczytu pliku licznika");
                #endif
                currentTotal = 0;
            }
            
            file.close();
            initialized = true;
        }
    }

public:
    OdometerManager() {
        loadFromFile();
    }

    void initialize() {
        loadFromFile();
    }

    float getRawTotal() const {
        return currentTotal;
    }

    bool setInitialValue(float value) {
        if (value < 0) {
            #ifdef DEBUG
            Serial.println("Błędna wartość początkowa (ujemna)");
            #endif
            return false;
        }
        
        #ifdef DEBUG
        Serial.printf("Ustawianie początkowej wartości licznika: %.2f\n", value);
        #endif

        currentTotal = value;
        saveToFile();
        return true;
    }

    void updateTotal(float newValue) {
        if (newValue > currentTotal) {
            #ifdef DEBUG
            Serial.printf("Aktualizacja licznika z %.2f na %.2f\n", currentTotal, newValue);
            #endif

            currentTotal = newValue;
            saveToFile();
        }
    }

    bool isValid() const {
        return initialized;
    }

    void shutdown() {
        saveToFile();
    }
};

#endif
