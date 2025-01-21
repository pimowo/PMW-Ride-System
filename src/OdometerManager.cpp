#include "OdometerManager.h"

OdometerManager::OdometerManager() : odometer(nullptr) {
}

OdometerManager::~OdometerManager() {
    if (odometer != nullptr) {
        delete odometer;
    }
}

void OdometerManager::begin() {
    odometer = new Odometer();
    preferences.begin(PREF_NAMESPACE, false);
    loadFromPreferences();
    
    // Inicjalizacja ostatnich zapisanych wartości
    lastSavedTotal = odometer->getTotalDistance();
    lastSavedTrip = odometer->getTripDistance();
    lastSaveTime = millis();
}

void OdometerManager::update() {
    if (odometer == nullptr) return;
    
    odometer->update();
    
    // Pobierz aktualne wartości
    float currentTotal = odometer->getTotalDistance();
    float currentTrip = odometer->getTripDistance();
    unsigned long currentTime = millis();
    
    // Sprawdź, czy należy zapisać stan:
    bool shouldSave = false;
    
    // 1. Zapisz jeśli minęło wystarczająco dużo czasu
    if (currentTime - lastSaveTime >= SAVE_INTERVAL) {
        shouldSave = true;
    }
    
    // 2. Zapisz jeśli zmiana dystansu przekroczyła próg
    if (abs(currentTotal - lastSavedTotal) >= DISTANCE_THRESHOLD ||
        abs(currentTrip - lastSavedTrip) >= DISTANCE_THRESHOLD) {
        shouldSave = true;
    }
    
    if (shouldSave) {
        saveToPreferences();
        lastSaveTime = currentTime;
        lastSavedTotal = currentTotal;
        lastSavedTrip = currentTrip;
    }
}

float OdometerManager::getTotalDistance() const {
    return (odometer != nullptr) ? odometer->getTotalDistance() : 0.0f;
}

float OdometerManager::getTripDistance() const {
    return (odometer != nullptr) ? odometer->getTripDistance() : 0.0f;
}

void OdometerManager::resetTrip() {
    if (odometer == nullptr) return;
    
    odometer->resetTrip();
    // Zawsze zapisz po resecie
    saveToPreferences();
    lastSavedTrip = 0.0f;
    lastSaveTime = millis();
}

void OdometerManager::calibrate(float actualDistance) {
    if (odometer == nullptr) return;
    
    odometer->calibrate(actualDistance);
    // Zawsze zapisz po kalibracji
    saveToPreferences();
    lastSavedTotal = odometer->getTotalDistance();
    lastSavedTrip = odometer->getTripDistance();
    lastSaveTime = millis();
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

void OdometerManager::forceSave() {
    if (odometer == nullptr) return;
    
    saveToPreferences();
    lastSavedTotal = odometer->getTotalDistance();
    lastSavedTrip = odometer->getTripDistance();
    lastSaveTime = millis();
}