#include "OdometerManager.h"

OdometerManager::OdometerManager() {
    odometer = new Odometer();
    preferences.begin(PREF_NAMESPACE, false);
}

OdometerManager::~OdometerManager() {
    delete odometer;
}

void OdometerManager::begin() {
    assert(odometer != nullptr);  // Dodaj asercję dla bezpieczeństwa
    loadFromPreferences();
    
    // Inicjalizacja ostatnich zapisanych wartości
    lastSavedTotal = odometer->getTotalDistance();
    lastSavedTrip = odometer->getTripDistance();
    lastSaveTime = millis();
}

void OdometerManager::update() {
    assert(odometer != nullptr);
    
    odometer->update();
    
    float currentTotal = odometer->getTotalDistance();
    float currentTrip = odometer->getTripDistance();
    unsigned long currentTime = millis();
    
    bool shouldSave = false;
    
    if (currentTime - lastSaveTime >= SAVE_INTERVAL) {
        shouldSave = true;
    }
    
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
    assert(odometer != nullptr);
    return odometer->getTotalDistance();
}

float OdometerManager::getTripDistance() const {
    assert(odometer != nullptr);
    return odometer->getTripDistance();
}

void OdometerManager::resetTrip() {
    assert(odometer != nullptr);
    odometer->resetTrip();
    saveToPreferences();
    lastSavedTrip = 0.0f;
    lastSaveTime = millis();
}

void OdometerManager::calibrate(float actualDistance) {
    assert(odometer != nullptr);
    odometer->calibrate(actualDistance);
    saveToPreferences();
    lastSavedTotal = odometer->getTotalDistance();
    lastSavedTrip = odometer->getTripDistance();
    lastSaveTime = millis();
}

void OdometerManager::saveToPreferences() {
    assert(odometer != nullptr);
    preferences.putFloat(TOTAL_DISTANCE_KEY, odometer->getTotalDistance());
    preferences.putFloat(TRIP_DISTANCE_KEY, odometer->getTripDistance());
}

void OdometerManager::loadFromPreferences() {
    assert(odometer != nullptr);
    float totalDistance = preferences.getFloat(TOTAL_DISTANCE_KEY, 0.0f);
    float tripDistance = preferences.getFloat(TRIP_DISTANCE_KEY, 0.0f);
    
    odometer->setTotalDistance(totalDistance);
    odometer->setTripDistance(tripDistance);
}

void OdometerManager::forceSave() {
    assert(odometer != nullptr);
    saveToPreferences();
    lastSavedTotal = odometer->getTotalDistance();
    lastSavedTrip = odometer->getTripDistance();
    lastSaveTime = millis();
}