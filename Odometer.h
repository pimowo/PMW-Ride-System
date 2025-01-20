#ifndef ODOMETER_H
#define ODOMETER_H

#include <Preferences.h>

class OdometerManager {
private:
    Preferences preferences;
    const char* NAMESPACE = "odometer";
    const char* TOTAL_KEY = "total";
    const char* BACKUP_KEY = "total_bak";
    
    float currentTotal = 0;
    float lastSavedTotal = 0;
    unsigned long lastSaveTime = 0;
    bool isInitialized = false;
    
    const float MIN_DISTANCE_CHANGE = 0.1;    // 100 metrów
    const unsigned long MIN_SAVE_TIME = 300000; // 5 minut

    bool saveTotal() {
        #ifdef DEBUG
        Serial.println("Saving total distance...");
        Serial.printf("Current total: %.2f\n", currentTotal);
        #endif

        bool backupSuccess = preferences.putFloat(BACKUP_KEY, currentTotal);
        bool mainSuccess = preferences.putFloat(TOTAL_KEY, currentTotal);
        
        #ifdef DEBUG
        Serial.printf("Backup save: %s\n", backupSuccess ? "OK" : "Failed");
        Serial.printf("Main save: %s\n", mainSuccess ? "OK" : "Failed");
        #endif

        if (backupSuccess && mainSuccess) {
            lastSavedTotal = currentTotal;
            lastSaveTime = millis();
            return true;
        }
        return false;
    }

public:
    OdometerManager() {
        #ifdef DEBUG
        Serial.println("Initializing OdometerManager...");
        #endif

        isInitialized = preferences.begin(NAMESPACE, false);
        
        #ifdef DEBUG
        Serial.printf("Preferences begin: %s\n", isInitialized ? "OK" : "Failed");
        #endif

        if (isInitialized) {
            initialize();
        }
    }
    
    ~OdometerManager() {
        if (isInitialized) {
            preferences.end();
            isInitialized = false;
        }
    }

    void initialize() {
        float mainValue = preferences.getFloat(TOTAL_KEY, 0);
        float backupValue = preferences.getFloat(BACKUP_KEY, 0);
        
        #ifdef DEBUG
        Serial.printf("Read main value: %.2f\n", mainValue);
        Serial.printf("Read backup value: %.2f\n", backupValue);
        #endif
        
        currentTotal = max(mainValue, backupValue);
        lastSavedTotal = currentTotal;

        #ifdef DEBUG
        Serial.printf("Initialized with total: %.2f\n", currentTotal);
        #endif
    }

    float getRawTotal() const {
        return currentTotal;
    }

    void updateTotal(float newDistance) {
        if (newDistance > currentTotal) {
            #ifdef DEBUG
            Serial.printf("Updating distance from %.2f to %.2f\n", currentTotal, newDistance);
            #endif

            currentTotal = newDistance;
            
            float change = currentTotal - lastSavedTotal;
            unsigned long currentTime = millis();
            
            if (change >= MIN_DISTANCE_CHANGE && 
                currentTime - lastSaveTime >= MIN_SAVE_TIME) {
                #ifdef DEBUG
                Serial.println("Change threshold reached, saving...");
                #endif
                saveTotal();
            }
        }
    }

    bool setInitialValue(float initialKm) {
        if (!isInitialized) {
            #ifdef DEBUG
            Serial.println("Odometer not initialized!");
            #endif
            return false;
        }

        if (initialKm < 0) {
            #ifdef DEBUG
            Serial.println("Invalid initial value (negative)");
            #endif
            return false;
        }

        #ifdef DEBUG
        Serial.printf("Setting initial value to %.2f km\n", initialKm);
        #endif

        currentTotal = initialKm;
        lastSavedTotal = initialKm;
        
        bool backupSuccess = preferences.putFloat(BACKUP_KEY, initialKm);
        bool mainSuccess = preferences.putFloat(TOTAL_KEY, initialKm);
        
        #ifdef DEBUG
        Serial.printf("Backup save: %s\n", backupSuccess ? "OK" : "Failed");
        Serial.printf("Main save: %s\n", mainSuccess ? "OK" : "Failed");
        #endif
        
        return backupSuccess && mainSuccess;
    }

    void shutdown() {
        if (isInitialized) {
            #ifdef DEBUG
            Serial.println("Shutting down OdometerManager...");
            #endif
            saveTotal();
            preferences.end();
            isInitialized = false;
        }
    }

    bool isValid() const {
        return isInitialized;
    }
};

#endif
