#ifndef ODOMETER_MANAGER_H
#define ODOMETER_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Odometer.h"

class OdometerManager {
    private:
        Odometer* odometer;
        Preferences preferences;
        const char* PREF_NAMESPACE = "odometer";
        const char* TOTAL_DISTANCE_KEY = "total_dist";
        const char* TRIP_DISTANCE_KEY = "trip_dist";

        unsigned long lastSaveTime = 0;        // Czas ostatniego zapisu
        const unsigned long SAVE_INTERVAL = 300000; // Zapisuj co 5 minut (300000 ms)
        float lastSavedTotal = 0.0f;          // Ostatnio zapisana całkowita odległość
        float lastSavedTrip = 0.0f;           // Ostatnio zapisana odległość podróży
        const float DISTANCE_THRESHOLD = 0.1f; // Próg zmiany dystansu (100 metrów)
        
        // Metody pomocnicze
        void saveToPreferences();
        void loadFromPreferences();

    public:
        OdometerManager();
        ~OdometerManager();

        // Podstawowe operacje
        void begin();
        void update();
        
        // Gettery
        float getTotalDistance() const;
        float getTripDistance() const;
        
        // Resetowanie licznika podróży
        void resetTrip();
        
        // Kalibracja
        void calibrate(float actualDistance);
};

#endif // ODOMETER_MANAGER_H