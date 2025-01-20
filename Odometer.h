#ifndef ODOMETER_H
#define ODOMETER_H

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

class OdometerManager {
private:
    const char* filename = "/odometer.json";
    float currentTotal = 0;

    void saveToFile() {
        #ifdef DEBUG
        Serial.println("Zapisywanie licznika do pliku...");
        #endif

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
            #ifdef DEBUG
            Serial.printf("Zapisano licznik: %.2f\n", currentTotal);
            #endif
        }
        
        file.close();
    }

    void loadFromFile() {
        #ifdef DEBUG
        Serial.println("Wczytywanie licznika z pliku...");
        #endif

        File file = LittleFS.open(filename, "r");
        if (!file) {
            #ifdef DEBUG
            Serial.println("Brak pliku licznika - tworzę nowy");
            #endif
            currentTotal = 0;
            saveToFile();
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
    }

public:
    OdometerManager() {
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
        return true; // Zawsze zwraca true, bo nie ma już inicjalizacji Preferences
    }
};

#endif
