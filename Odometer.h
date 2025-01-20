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
    
    const float MIN_DISTANCE_CHANGE = 0.1;    // 100 metrów
    const unsigned long MIN_SAVE_TIME = 300000; // 5 minut

    void saveTotal() {
        preferences.putFloat(BACKUP_KEY, currentTotal);
        preferences.putFloat(TOTAL_KEY, currentTotal);
        lastSavedTotal = currentTotal;
        lastSaveTime = millis();
    }

public:
    OdometerManager() {
        preferences.begin(NAMESPACE, false);
        initialize();
    }
    
    ~OdometerManager() {
        preferences.end();
    }

    void initialize() {
        float mainValue = preferences.getFloat(TOTAL_KEY, 0);
        float backupValue = preferences.getFloat(BACKUP_KEY, 0);
        
        // Użyj większej wartości (w przypadku awarii podczas zapisu)
        currentTotal = max(mainValue, backupValue);
        lastSavedTotal = currentTotal;
    }

    // Zmiana nazwy z getDisplayTotal na getRawTotal
    float getRawTotal() const {
        return currentTotal;
    }

    //void updateDistance(float newDistance) {
    void updateTotal(float newDistance) {
        if (newDistance > currentTotal) {
            currentTotal = newDistance;
            
            float change = currentTotal - lastSavedTotal;
            unsigned long currentTime = millis();
            
            if (change >= MIN_DISTANCE_CHANGE && 
                currentTime - lastSaveTime >= MIN_SAVE_TIME) {
                saveTotal();
            }
        }
    }

    bool setInitialValue(float initialKm) {
        if (initialKm < 0) {
            return false;
        }

        currentTotal = initialKm;
        lastSavedTotal = initialKm;
        
        preferences.putFloat(BACKUP_KEY, initialKm);
        preferences.putFloat(TOTAL_KEY, initialKm);
        
        return true;
    }

    void shutdown() {
        saveTotal();
        preferences.end();
    }
};

#endif
