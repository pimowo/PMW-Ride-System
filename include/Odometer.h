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