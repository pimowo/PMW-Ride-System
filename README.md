# 🚲 PMW Ride System (PMW RS)

## 📝 Przegląd
PMW Ride System to zaawansowany system zaprojektowany dla rowerów elektrycznych. Oferuje funkcje takie jak monitorowanie prędkości, pomiar temperatury, zarządzanie baterią, pomiar mocy i wiele innych. System wykorzystuje różne czujniki i komponenty, aby dostarczać dane w czasie rzeczywistym oraz opcje sterowania, które poprawiają doświadczenie jazdy na rowerze elektrycznym.

## ⚡ Funkcje
- **🔄 Monitorowanie prędkości**: Wyświetla aktualną, średnią i maksymalną prędkość
- **🌡️ Pomiar temperatury**: Monitoruje temperaturę powietrza, kontrolera i silnika
- **📏 Obliczanie zasięgu**: Wyświetla szacowany zasięg, przebyty dystans i wartość licznika kilometrów
- **🔋 Zarządzanie baterią**: Pokazuje napięcie, prąd, pojemność i procent naładowania baterii
- **⚡ Pomiar mocy**: Wyświetla aktualną, średnią i maksymalną moc wyjściową
- **💨 Monitorowanie ciśnienia**: Monitoruje ciśnienie w oponach, napięcie i temperaturę
- **🔌 Sterowanie USB**: Kontroluje port ładowania USB (5V)
- **💡 Sterowanie światłami**: Zarządza przednimi i tylnymi światłami z trybami dziennym i nocnym
- **📱 Interfejs użytkownika**: Interaktywny wyświetlacz OLED z wieloma ekranami i pod-ekranami dla szczegółowych informacji

## 🛠️ Komponenty
- **🧠 Mikrokontroler**: ESP32
- **🖥️ Wyświetlacz**: SSD1306 128x64 OLED
- **🌡️ Czujnik temperatury**: DS18B20
- **⏰ Zegar czasu rzeczywistego (RTC)**: DS3231
- **📶 Bluetooth**: BLE do komunikacji z systemem zarządzania baterią (BMS)
- **💾 EEPROM**: Do przechowywania ustawień użytkownika

## 📍 Definicje pinów
- **🔘 Przyciski**:
  - `BTN_UP`: GPIO 13
  - `BTN_DOWN`: GPIO 14
  - `BTN_SET`: GPIO 12
- **💡 Światła**:
  - `FrontDayPin`: GPIO 5
  - `FrontPin`: GPIO 18
  - `RearPin`: GPIO 19
- **🔌 Ładowarka USB**:
  - `UsbPin`: GPIO 32
- **🌡️ Czujnik temperatury**:
  - `ONE_WIRE_BUS`: GPIO 15

## 📱 Interfejs webowy
System oferuje intuicyjny interfejs webowy dostępny przez przeglądarkę, który umożliwia:
- Podgląd wszystkich parametrów systemu w czasie rzeczywistym
- Konfigurację ustawień systemu
- Ustawianie zegara RTC
- Sterowanie oświetleniem i portem USB
- Kalibrację czujników

## 💻 Użytkowanie
1. **⚙️ Instalacja**:
    - Podłącz wszystkie komponenty zgodnie z definicjami pinów
    - Wgraj dostarczony kod do mikrokontrolera ESP32
    - Skonfiguruj połączenie WiFi przez interfejs webowy

2. **🎮 Obsługa fizycznych przycisków**:
    - Użyj przycisku `BTN_SET` do nawigacji między głównymi ekranami
    - Użyj przycisków `BTN_UP` i `BTN_DOWN` do zmiany ustawień
    - Długie naciśnięcie `BTN_SET` włącza/wyłącza wyświetlacz
    - Podwójne kliknięcie `BTN_SET` przełącza wyjście USB

3. **⚙️ Konfiguracja przez interfejs webowy**:
    - Połącz się z siecią WiFi utworzoną przez urządzenie
    - Otwórz przeglądarkę i przejdź pod adres IP urządzenia
    - Skonfiguruj wszystkie parametry przez intuicyjny interfejs

## 📦 Struktura kodu
- **📚 Biblioteki**:
  - `Wire.h`, `U8g2lib.h`, `RTClib.h`, `OneWire.h`, `DallasTemperature.h`, `EEPROM.h`, `WiFi.h`, `ESPAsyncWebServer.h`, `SPIFFS.h`
- **💾 System plików SPIFFS**:
  - Przechowywanie plików interfejsu webowego
  - Konfiguracja systemu
  - Logi systemowe

## 📄 Licencja
Projekt jest licencjonowany na podstawie licencji MIT. Zobacz plik [LICENSE](LICENSE) dla szczegółów.

## 🤝 Wkład
Wkłady są mile widziane! Proszę rozwidlić repozytorium i złożyć pull request na wszelkie ulepszenia lub poprawki błędów.

## 📧 Kontakt
W razie pytań lub wsparcia, prosimy o utworzenie "Issue" w repozytorium GitHub.
