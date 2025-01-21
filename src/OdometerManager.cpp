#include "OdometerManager.h"

OdometerManager::OdometerManager() : odometer(nullptr) {
}

OdometerManager::~OdometerManager() {
    if (odometer != nullptr) {
        delete odometer;
    }
}

void OdometerManager::begin() {
    // Inicjalizacja licznika
    odometer = new Odometer();
    
    // Otwieranie przestrzeni nazw w pamięci flash
    preferences.begin(PREF_NAMESPACE, false);
    
    // Wczytywanie zapisanych wartości
    loadFromPreferences();
}

void OdometerManager::update() {
    if (odometer != nullptr) {
        odometer->update();
        
        // Zapisujemy stan co jakiś czas lub przy znaczących zmianach
        // TODO: Zoptymalizować częstotliwość zapisu
        saveToPreferences();
    }
}

float OdometerManager::getTotalDistance() const {
    return (odometer != nullptr) ? odometer->getTotalDistance() : 0.0f;
}

float OdometerManager::getTripDistance() const {
    return (odometer != nullptr) ? odometer->getTripDistance() : 0.0f;
}

void OdometerManager::resetTrip() {
    if (odometer != nullptr) {
        odometer->resetTrip();
        saveToPreferences();
    }
}

void OdometerManager::calibrate(float actualDistance) {
    if (odometer != nullptr) {
        odometer->calibrate(actualDistance);
        saveToPreferences();
    }
}

void OdometerManager::saveToPreferences() {
    if (odometer != nullptr) {
        preferences.putFloat(TOTAL_DISTANCE_KEY, odometer->getTotalDistance());
        preferences.putFloat(TRIP_DISTANCE_KEY, odometer->getTripDistance());
    }
}

void OdometerManager::loadFromPreferences() {
    if (odometer != nullptr) {
        float totalDistance = preferences.getFloat(TOTAL_DISTANCE_KEY, 0.0f);
        float tripDistance = preferences.getFloat(TRIP_DISTANCE_KEY, 0.0f);
        
        // Ustawianie zapisanych wartości
        odometer->setTotalDistance(totalDistance);
        odometer->setTripDistance(tripDistance);
    }
}