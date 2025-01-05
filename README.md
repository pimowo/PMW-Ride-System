# 🚲 e-Bike System PMW

## 📝 Przegląd
e-Bike System PMW to zaawansowany system zaprojektowany dla rowerów elektrycznych. Oferuje funkcje takie jak monitorowanie prędkości, pomiar temperatury, zarządzanie baterią, pomiar mocy i wiele innych. System wykorzystuje różne czujniki i komponenty, aby dostarczać dane w czasie rzeczywistym oraz opcje sterowania, które poprawiają doświadczenie jazdy na rowerze elektrycznym.

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
- **🖥️ Wyświetlacz**: SSD1306 128x64 OLED (0.96", 1.54" i 2.42")
- **🌡️ Czujnik temperatury**: 2x DS18B20 i NTC10k
- **⏰ Zegar czasu rzeczywistego (RTC)**: DS3231
- **📶 Bluetooth**: BLE do komunikacji z systemem zarządzania baterią (BMS) i TPMS
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
  - docelowo światła na osobnym uC
    ATtyny85 z możliwością przełącznia świateł, różnymi trybami mrugania tylnego światła
- **🔌 Ładowarka USB**:
  - `UsbPin`: GPIO 32
- **🌡️ Czujnik temperatury**:
  - `ONE_WIRE_BUS`: GPIO 15 (DS18B20)
  - `ONE_WIRE_BUS`: GPIO (DS18B20)
  - `ADC`: GPIO (NTC10k)

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
    a) BTN_SET:
    - Długie (3s) naciśnięcie `BTN_SET` włącza/wyłącza wyświetlacz (system e-bike)
    - Krótkie (0,1s) naciśnięcia `BTN_SET` do nawigacji po głównych ekranach i pod-ekranach
    - Podwójne naciśnięcie `BTN_SET` do wejścia/wyjścia w pod-ekrany
    - Podwójne kliknięcie `BTN_SET` na ekranie "USB" przełącza wyjście USB
    b) BTN_UP
    - Użyj przycisków `BTN_UP` do zwiększenia trybu wspomagania (0->5)
    - Długie (1s) naciśnięcie `BTN_UP` przełacza tryb świateł: dzień/noc/włączone
    c) BTN_DOWN
    - Użyj przycisków `BTN_DOWN` do zmniejszenia trybu wspomagania (5->0)
    - Długie (1s) naciśnięcie `BTN_DOWN` włącza tryb prowadzenia roweru
    - Długie (1s) naciśnięcie `BTN_DOWN` podczas jazdy włącza tempomat
    d) BTN
    - Użyj przycisków `BTN_UP` + `BTN_DOWN` do uruchomienia trybu konfiguracji
    - Użyj przycisków `BTN_UP` + `BTN_SET` do przełączenia trybu legalnego
      
4. **⚙️ Konfiguracja przez interfejs webowy**:
    - Połącz się z siecią WiFi utworzoną przez urządzenie
    - Otwórz przeglądarkę i przejdź pod adres IP urządzenia
    - Skonfiguruj wszystkie parametry przez intuicyjny interfejs

## 📦 Struktura kodu
- **📚 Biblioteki**:
  - `Wire.h`, `U8g2lib.h`, `RTClib.h`, `OneWire.h`, `DallasTemperature.h`, `EEPROM.h`, `WiFi.h`, `ESPAsyncWebServer.h`, `SPIFFS.h`
- **💾 System plików LitteFS**:
  - Przechowywanie plików interfejsu webowego
  - Konfiguracja systemu
  - Logi systemowe

## 📄 Licencja
Projekt jest licencjonowany na podstawie licencji MIT. Zobacz plik [LICENSE](LICENSE) dla szczegółów.

## 📧 Kontakt
W razie pytań lub wsparcia, prosimy o utworzenie "Issue" w repozytorium GitHub.
