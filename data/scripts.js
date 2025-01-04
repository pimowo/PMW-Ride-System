// Konfiguracja debugowania
const DEBUG_CONFIG = {
    ENABLED: true,
    LOG_LEVELS: {
        INFO: 'INFO',
        WARNING: 'WARNING',
        ERROR: 'ERROR',
        DEBUG: 'DEBUG'
    }
};

// Konfiguracja WebSocket
const WS_CONFIG = {
    RECONNECT_DELAY: 5000,
    MAX_RETRIES: 3,
    PING_INTERVAL: 30000,
    CONNECTION_TIMEOUT: 10000
};

// Stałe dla elementów formularza
const FORM_ELEMENTS = {
    dayLights: 'day-lights',
    nightLights: 'night-lights',
    dayBlink: 'day-blink',
    nightBlink: 'night-blink',
    blinkFrequency: 'blink-frequency'
};

// Stałe dla typów błędów
const ERROR_TYPES = {
    SAVE: 'zapisywania',
    LOAD: 'wczytywania',
    VALIDATION: 'walidacji',
    NETWORK: 'połączenia sieciowego'
};

// Obiekt z informacjami dla każdego parametru
const infoContent = {

    // Sekcja zegara //

    'rtc-info': {
        title: '⏰ Konfiguracja zegara',
        description: `Ustawia czas systemowy RTC (Real Time Clock).

    Aby zsynchronizować czas:
    1. Sprawdź czy czas na twoim urządzeniu jest prawidłowy
    2. Kliknij przycisk "Ustaw aktualny czas"
    3. System pobierze czas z twojego urządzenia
    
    ⚠️ WAŻNE: 
    Zegar jest podtrzymywany bateryjnie i działa 
    nawet po odłączeniu zasilania głównego.`
    },

    // Sekcja świateł //

    'light-config-info': {
        title: '💡 Konfiguracja świateł',
        description: `Ustawienia dotyczące świateł rowerowych. 

    Możesz skonfigurować różne tryby świateł:
      - dla jazdy dziennej 
      - dla jazdy nocnej`
    },

    'day-lights-info': {
        title: '☀️ Światła dzienne',
        description: `Wybór konfiguracji świateł dla jazdy w dzień:

      - Wyłączone: wszystkie światła wyłączone 
      - Przód dzień: przednie światło w trybie dziennym 
      - Przód zwykłe: przednie światło w trybie normalnym 
      - Tył: tylko tylne światło 
      - Przód dzień + tył: przednie światło dzienne i tylne 
      - Przód zwykłe + tył: przednie światło normalne i tylne`
    },

    'day-blink-info': {
        title: 'Mruganie tylnego światła (dzień)',
        description: `Włącza lub wyłącza funkcję mrugania tylnego światła podczas jazdy w dzień. 

    Mrugające światło może być bardziej widoczne 
    dla innych uczestników ruchu.`
    },

    'night-lights-info': {
        title: '🌙 Światła nocne',
        description: `Wybór konfiguracji świateł dla jazdy w nocy:
 
      - Wyłączone: wszystkie światła wyłączone 
      - Przód dzień: przednie światło w trybie dziennym 
      - Przód zwykłe: przednie światło w trybie normalnym 
      - Tył: tylko tylne światło 
      - Przód dzień + tył: przednie światło dzienne i tylne 
      - Przód zwykłe + tył: przednie światło normalne i tylne`
    },

    'night-blink-info': {
        title: 'Mruganie tylnego światła (noc)',
        description: `Włącza lub wyłącza funkcję mrugania tylnego światła podczas jazdy w nocy. 
    
    Należy rozważnie używać tej funkcji, gdyż w niektórych warunkach migające światło może być bardziej dezorientujące niż pomocne.`
    },

    'blink-frequency-info': {
        title: '⚡ Częstotliwość mrugania',
        description: `Określa częstotliwość mrugania tylnego światła w milisekundach. 
        
    Mniejsza wartość oznacza szybsze mruganie, a większa - wolniejsze. Zakres: 100-2000ms.`
    },

    // Sekcja wyświetlacza //

    'brightness-info': {
        title: '🔆 Podświetlenie wyświetlacza',
        description: `Ustaw jasność podświetlenia wyświetlacza w zakresie od 0% do 100%. 
        
    Wyższa wartość oznacza jaśniejszy wyświetlacz. Zalecane ustawienie to 50-70% dla optymalnej widoczności.`
    },

    'auto-mode-info': {
        title: '🤖 Tryb automatyczny',
        description: `Automatycznie przełącza jasność wyświetlacza w zależności od ustawionych świateł dzień/noc. W trybie dziennym używa jaśniejszego podświetlenia, a w nocnym - przyciemnionego. Gdy światła nie są włączone to jasność jest ustawiona jak dla dnia`
    },

    'day-brightness-info': {
        title: '☀️ Jasność dzienna',
        description: `Poziom jasności wyświetlacza używany w ciągu dnia (0-100%). Zalecana wyższa wartość dla lepszej widoczności w świetle słonecznym.`
    },

    'night-brightness-info': {
        title: '🌙 Jasność nocna',
        description: `Poziom jasności wyświetlacza używany w nocy (0-100%). Zalecana niższa wartość dla komfortowego użytkowania w ciemności.`
    },

    'auto-off-time-info': {
        title: '⏰ Czas automatycznego wyłączenia',
        description: `Określa czas bezczynności, po którym system automatycznie się wyłączy.

        Zakres: 0-60 minut
        0: Funkcja wyłączona (system nie wyłączy się automatycznie)
        1-60: Czas w minutach do automatycznego wyłączenia

        💡 WSKAZÓWKA:
          - Krótszy czas oszczędza baterię
          - Dłuższy czas jest wygodniejszy przy dłuższych postojach
        
        ⚠️ UWAGA:
        System zawsze zapisze wszystkie ustawienia przed wyłączeniem`
    },

    // Sekcja sterownika //

    'display-type-info': {
        title: '🔍 Wybór typu wyświetlacza',
        description: `Wybierz odpowiedni model wyświetlacza LCD zainstalowanego w Twoim rowerze.

        🟦 KT-LCD:
        • Standardowy wyświetlacz z serii KT
        • Obsługuje parametry P1-P5, C1-C15, L1-L3
        • Kompatybilny z większością kontrolerów KT
        
        🟨 S866:
        • Wyświetlacz z serii Bigstone/S866
        • Obsługuje parametry P1-P20
        • Posiada dodatkowe funkcje konfiguracyjne
        
        ⚠️ UWAGA: 
        Wybór niewłaściwego typu wyświetlacza może 
        spowodować nieprawidłowe działanie systemu.
        Upewnij się, że wybrany model odpowiada 
        fizycznie zainstalowanemu wyświetlaczowi.`
    },

    // Parametry sterownika KT-LCD //

    // Parametry P sterownika //

    'kt-p1-info': {
        title: '⚙️ P1 - Przełożenie silnika',
        description: `Obliczane ze wzoru: ilość magnesów X przełożenie

    Dla silników bez przekładni (np. 30H): przełożenie = 1 (P1 = 46)
    Dla silników z przekładnią (np. XP07): przełożenie > 1 (P1 = 96)

    Parametr wpływa tylko na wyświetlanie prędkości - nieprawidłowa wartość nie wpłynie na jazdę, jedynie na wskazania prędkościomierza`
    },

    'kt-p2-info': {
        title: 'P2 - Sposób odczytu prędkości',
        description: `Wybierz:
        
    0: Dla silnika bez przekładni
      - Prędkość z czujników halla silnika
      - Biały przewód do pomiaru temperatury

    1: Dla silnika z przekładnią
      - Prędkość z dodatkowego czujnika halla
      - Biały przewód do pomiaru prędkości

    2-6: Dla silników z wieloma magnesami pomiarowymi
      - Prędkość z dodatkowego czujnika halla
      - Biały przewód do pomiaru prędkości
      *używane rzadko, ale gdy pokazuje zaniżoną prędkość spróbuj tej opcji`
    },

    'kt-p3-info': {
        title: 'P3 - Tryb działania czujnika PAS',
        description: `Pozwala ustawić jak ma się zachowywać wspomaganie z czujnikiem PAS podczas używania biegów 1-5
      – 0: Tryb sterowania poprzez prędkość
      – 1: Tryb sterowania momentem obrotowym`
    },

    'kt-p4-info': {
        title: 'P4 - Ruszanie z manetki',
        description: `Pozwala ustawić sposób ruszania rowerem:

    0: Można ruszyć od zera używając samej manetki
    1: Manetka działa dopiero po ruszeniu z PAS/nóg`
    },

    'kt-p5-info': {
        title: 'P5 - Sposób obliczania poziomu naładowania akumulatora',
        description: `Pozwala dostosować czułość wskaźnika naładowania akumulatora
      - niższa wartość: szybsza reakcja na spadki napięcia
      - wyższa wartość: wolniejsza reakcja, uśrednianie wskazań

    Zalecane zakresy wartości:
      - 24V: 4-11
      - 36V: 5-15
      - 48V: 6-20
      - 60V: 7-30

    Uwaga: Zbyt wysokie wartości mogą opóźnić ostrzeżenie o niskim poziomie baterii.

    Jeśli wskaźnik pokazuje stale 100%, wykonaj:
    1. Reset do ustawień fabrycznych
    2. Ustaw podstawowe parametry
    3. Wykonaj pełny cykl ładowania-rozładowania`
    },

    // Parametry C sterownika //

    'kt-c1-info': {
        title: 'C1 - Czujnik PAS',
        description: `Konfiguracja czułości czujnika asysty pedałowania (PAS). Wpływa na to, jak szybko system reaguje na pedałowanie.`
    },

    'kt-c2-info': {
        title: 'C2 - Typ silnika',
        description: `Ustawienia charakterystyki silnika i jego podstawowych parametrów pracy.`
    },

    'kt-c3-info': {
        title: 'C3 - Tryb wspomagania',
        description: `Konfiguracja poziomów wspomagania i ich charakterystyki (eco, normal, power).`
    },

    'kt-c4-info': {
        title: 'C4 - Manetka i PAS',
        description: `Określa sposób współdziałania manetki z czujnikiem PAS i priorytety sterowania.`
    },

    'kt-c5-info': {
        title: '⚠️ C5 - Regulacja prądu sterownika',
        description: `Pozwala dostosować maksymalny prąd sterownika do możliwości akumulatora.
    
    Wartości:
    3:  Prąd zmniejszony o 50% (÷2.0)
    4:  Prąd zmniejszony o 33% (÷1.5) 
    5:  Prąd zmniejszony o 25% (÷1.33)
    6:  Prąd zmniejszony o 20% (÷1.25)
    7:  Prąd zmniejszony o 17% (÷1.20)
    8:  Prąd zmniejszony o 13% (÷1.15)
    9:  Prąd zmniejszony o 9%  (÷1.10)
    10: Pełny prąd sterownika

    Przykład dla sterownika 25A:
      - C5=3 → max 12.5A
      - C5=5 → max 18.8A
      - C5=10 → max 25A

    ⚠️ WAŻNE
    Używaj niższych wartości gdy:
      - Masz słaby akumulator z mocnym silnikiem
      - Chcesz wydłużyć żywotność akumulatora
      - Występują spadki napięcia podczas przyśpieszania`
    },

    'kt-c6-info': {
        title: 'C6 - Jasność wyświetlacza',
        description: `Ustawienie domyślnej jasności podświetlenia wyświetlacza LCD.`
    },

    'kt-c7-info': {
        title: 'C7 - Tempomat',
        description: `Konfiguracja tempomatu - utrzymywania stałej prędkości.`
    },

    'kt-c8-info': {
        title: 'C8 - Silnik',
        description: `Dodatkowe parametry silnika, w tym temperatura i zabezpieczenia.`
    },

    'kt-c9-info': {
        title: 'C9 - Zabezpieczenia',
        description: `Ustawienia kodów PIN i innych zabezpieczeń systemowych.`
    },

    'kt-c10-info': {
        title: 'C10 - Ustawienia fabryczne',
        description: `Opcje przywracania ustawień fabrycznych i kalibracji systemu.`
    },

    'kt-c11-info': {
        title: 'C11 - Komunikacja',
        description: `Parametry komunikacji między kontrolerem a wyświetlaczem.`
    },

    'kt-c12-info': {
        title: '🔋 C12 - Regulacja minimalnego napięcia wyłączenia (LVC)',
        description: `Pozwala dostosować próg napięcia, przy którym sterownik się wyłącza (Low Voltage Cutoff).

    Wartości względem napięcia domyślnego:
    0: -2.0V     
    1: -1.5V     
    2: -1.0V     
    3: -0.5V
    4: domyślne (40V dla 48V, 30V dla 36V, 20V dla 24V)
    5: +0.5V
    6: +1.0V
    7: +1.5V

    Przykład dla sterownika 48V:
      - Domyślnie (C12=4): wyłączenie przy 40V
      - C12=0: wyłączenie przy 38V
      - C12=7: wyłączenie przy 41.5V

    ⚠️ WAŻNE WSKAZÓWKI:
    1. Obniżenie progu poniżej 42V w sterowniku 48V może spowodować:
      - Błędne wykrycie systemu jako 36V
      - Nieprawidłowe wskazania poziomu naładowania (stałe 100%)
    2. Przy częstym rozładowywaniu akumulatora:
      - Zalecane ustawienie C12=7
      - Zapobiega przełączaniu na tryb 36V
      - Chroni ostatnie % pojemności akumulatora

    ZASTOSOWANIE:
      - Dostosowanie do charakterystyki BMS
      - Optymalizacja wykorzystania pojemności akumulatora
      - Ochrona przed głębokim rozładowaniem`
    },

    'kt-c13-info': {
        title: '🔄 C13 - Hamowanie regeneracyjne',
        description: `Pozwala ustawić siłę hamowania regeneracyjnego i efektywność odzysku energii.

    USTAWIENIA:
    0: Wyłączone (brak hamowania i odzysku)
    1: Słabe hamowanie + Najwyższy odzysk energii
    2: Umiarkowane hamowanie + Średni odzysk
    3: Średnie hamowanie + Umiarkowany odzysk
    4: Mocne hamowanie + Niski odzysk
    5: Najmocniejsze hamowanie + Minimalny odzysk

    ZASADA DZIAŁANIA:
      - Niższe wartości = lepszy odzysk energii
      - Wyższe wartości = silniejsze hamowanie
      - Hamowanie działa na klamki hamulcowe
      - W niektórych modelach działa też na manetkę

    ⚠️ WAŻNE OSTRZEŻENIA:
    1. Hamowanie regeneracyjne może powodować obluzowanie osi silnika
      - ZAWSZE używaj 2 blokad osi
      - Regularnie sprawdzaj dokręcenie
    2. Wybór ustawienia:
      - Priorytet odzysku energii → ustaw C13=1
      - Priorytet siły hamowania → ustaw C13=5
      - Kompromis → ustaw C13=2 lub C13=3

    💡 WSKAZÓWKA: Zacznij od niższych wartości i zwiększaj stopniowo, obserwując zachowanie roweru i efektywność odzysku energii.`
    },

    'kt-c14-info': {
        title: 'C14 - Poziomy PAS',
        description: `Konfiguracja poziomów wspomagania i ich charakterystyk.`
    },

    'kt-c15-info': {
        title: 'C15 - Prowadzenie',
        description: `Ustawienia trybu prowadzenia roweru (walk assist).`
    },

    // Parametry L sterownika //
    
    'kt-l1-info': {
        title: '🔋 L1 - Napięcie minimalne (LVC)',
        description: `Ustawienie minimalnego napięcia pracy sterownika (Low Voltage Cutoff).

    Dostępne opcje:
    0: Automatyczny dobór progu przez sterownik
      - 24V → wyłączenie przy 20V
      - 36V → wyłączenie przy 30V      
      - 48V → wyłączenie przy 40V
      
    Wymuszenie progu wyłączenia:
    1: 20V
    2: 30V
    3: 40V

    ⚠️ UWAGA: 
    Ustawienie zbyt niskiego progu może prowadzić do uszkodzenia akumulatora!`
    },

    'kt-l2-info': {
        title: '⚡ L2 - Silniki wysokoobrotowe',
        description: `Parametr dla silników o wysokich obrotach (>5000 RPM).

    Wartości:
    0: Tryb normalny
    1: Tryb wysokoobrotowy - wartość P1 jest mnożona ×2

    📝 UWAGA:
      - Parametr jest powiązany z ustawieniem P1
      - Używaj tylko dla silników > 5000 RPM`
    },

    'kt-l3-info': {
        title: '🔄 L3 - Tryb DUAL',
        description: `Konfiguracja zachowania dla sterowników z podwójnym kompletem czujników halla.

    Opcje:
    0: Tryb automatyczny
      - Automatyczne przełączenie na sprawny komplet czujników
      - Kontynuacja pracy po awarii jednego kompletu

    1: Tryb bezpieczny
      - Wyłączenie przy awarii czujników
      - Sygnalizacja błędu

    ⚠️ WAŻNE: 
    Dotyczy tylko sterowników z funkcją DUAL (2 komplety czujników halla)`
    },

    // Parametry sterownika S866 //

    's866-p1-info': {
        title: 'P1 - Jasność podświetlenia',
        description: `Regulacja poziomu podświetlenia wyświetlacza.

    Dostępne poziomy:
    1: Najciemniejszy
    2: Średni
    3: Najjaśniejszy`
    },

    's866-p2-info': {
        title: 'P2 - Jednostka pomiaru',
        description: `Wybór jednostki wyświetlania dystansu i prędkości.

    Opcje:
    0: Kilometry (km)
    1: Mile`
    },

    's866-p3-info': {
        title: 'P3 - Napięcie nominalne',
        description: `Wybór napięcia nominalnego systemu.

    Dostępne opcje:
    - 24V
    - 36V
    - 48V
    - 60V`
    },

    's866-p4-info': {
        title: 'P4 - Czas automatycznego uśpienia',
        description: `Czas bezczynności po którym wyświetlacz przejdzie w stan uśpienia.

    Zakres: 0-60 minut
    0: Funkcja wyłączona (brak auto-uśpienia)
    1-60: Czas w minutach do przejścia w stan uśpienia`
    },

    's866-p5-info': {
        title: 'P5 - Tryb wspomagania PAS',
        description: `Wybór liczby poziomów wspomagania.

    Opcje:
    0: Tryb 3-biegowy
    1: Tryb 5-biegowy`
    },

    's866-p6-info': {
        title: 'P6 - Rozmiar koła',
        description: `Ustawienie średnicy koła dla prawidłowego obliczania prędkości.

    Zakres: 5.0 - 50.0 cali
    Dokładność: 0.1 cala

    ⚠️ WAŻNE 
    Ten parametr jest kluczowy dla prawidłowego wyświetlania prędkości.`
    },

    's866-p7-info': {
        title: 'P7 - Liczba magnesów czujnika prędkości',
        description: `Konfiguracja czujnika prędkości.

    Zakres: 1-100 magnesów

    Dla silnika z przekładnią:
    Wartość = Liczba magnesów × Przełożenie

    Przykład:
    - 20 magnesów, przełożenie 4.3
    - Wartość = 20 × 4.3 = 86`
    },

    's866-p8-info': {
        title: 'P8 - Limit prędkości',
        description: `Ustawienie maksymalnej prędkości pojazdu.

    Zakres: 0-100 km/h
    100: Brak limitu prędkości

    ⚠️ UWAGA: 
    - Dokładność: ±1 km/h
    - Limit dotyczy zarówno mocy jak i skrętu
    - Wartości są zawsze w km/h, nawet przy wyświetlaniu w milach`
    },

    's866-p9-info': {
        title: 'P9 - Tryb startu',
        description: `Wybór sposobu uruchamiania wspomagania.

    0: Start od zera (zero start)
    1: Start z rozbiegu (non-zero start)`
    },

    's866-p10-info': {
        title: 'P10 - Tryb jazdy',
        description: `Wybór trybu wspomagania.

    0: Wspomaganie PAS (moc zależna od siły pedałowania)
    1: Tryb elektryczny (sterowanie manetką)
    2: Tryb hybrydowy (PAS + manetka)`
    },

    's866-p11-info': {
        title: 'P11 - Czułość PAS',
        description: `Regulacja czułości czujnika wspomagania.

    Zakres: 1-24
    - Niższe wartości = mniejsza czułość
    - Wyższe wartości = większa czułość`
    },

    's866-p12-info': {
        title: 'P12 - Siła startu PAS',
        description: `Intensywność wspomagania przy rozpoczęciu pedałowania.

    Zakres: 1-5
    1: Najsłabszy start
    5: Najmocniejszy start`
    },

    's866-p13-info': {
        title: 'P13 - Typ czujnika PAS',
        description: `Wybór typu czujnika PAS według liczby magnesów.

    Dostępne opcje:
    - 5 magnesów
    - 8 magnesów
    - 12 magnesów`
    },

    's866-p14-info': {
        title: 'P14 - Limit prądu kontrolera',
        description: `Ustawienie maksymalnego prądu kontrolera.

    Zakres: 1-20A`
    },

    's866-p15-info': {
        title: 'P15 - Napięcie odcięcia',
        description: `Próg napięcia przy którym kontroler wyłączy się.`
    },

    's866-p16-info': {
        title: 'P16 - Reset licznika ODO',
        description: `Resetowanie licznika całkowitego przebiegu.

    Aby zresetować:
    Przytrzymaj przycisk przez 5 sekund`
    },

    's866-p17-info': {
        title: 'P17 - Tempomat',
        description: `Włączenie/wyłączenie funkcji tempomatu.

    0: Tempomat wyłączony
    1: Tempomat włączony

    ⚠️ Uwaga
    Działa tylko z protokołem 2`
    },

    's866-p18-info': {
        title: 'P18 - Kalibracja prędkości',
        description: `Współczynnik korekcji wyświetlanej prędkości.

    Zakres: 50% - 150%`
    },

    's866-p19-info': {
        title: 'P19 - Bieg zerowy PAS',
        description: `Konfiguracja biegu zerowego w systemie PAS.

    0: Z biegiem zerowym
    1: Bez biegu zerowego`
    },

    's866-p20-info': {
        title: 'P20 - Protokół komunikacji',
        description: `Wybór protokołu komunikacji sterownika.

    0: Protokół 2
    1: Protokół 5S
    2: Protokół Standby
    3: Protokół Standby alternatywny`
    }
};

function showRTCInfo() {
    showModal(infoContent['rtc-info'].title, infoContent['rtc-info'].description);
}

// Funkcja pobierająca wersję systemu
async function fetchSystemVersion() {
    try {
        const response = await fetch('/api/version');
        const data = await response.json();
        if (data.version) {
            document.getElementById('system-version').textContent = data.version;
        }
    } catch (error) {
        console.error('Błąd podczas pobierania wersji systemu:', error);
        document.getElementById('system-version').textContent = 'N/A';
    }
}

// Funkcja debounce
function debounce(fn, time) {
    let timeout;
    return function(...args) {
        clearTimeout(timeout);
        timeout = setTimeout(() => fn.apply(this, args), time);
    };
}

// Funkcja getFormElements z cachowaniem
function getFormElements() {
    if (!getFormElements.cache) {
        getFormElements.cache = Object.entries(FORM_ELEMENTS).reduce((acc, [key, id]) => {
            acc[key] = document.getElementById(id);
            return acc;
        }, {});
    }
    return getFormElements.cache;
}

// Funkcje debugowania
const debug = (() => {
    if (!DEBUG_CONFIG.ENABLED) return () => {};
    return (message, data = null, level = DEBUG_CONFIG.LOG_LEVELS.DEBUG) => {
        const timestamp = new Date().toISOString();
        const prefix = `[${level}][${timestamp}]`;
        if (data) {
            console.log(prefix, message, data);
        } else {
            console.log(prefix, message);
        }
    };
})();

const logInfo = (message, data) => debug(message, data, DEBUG_CONFIG.LOG_LEVELS.INFO);
const logWarning = (message, data) => debug(message, data, DEBUG_CONFIG.LOG_LEVELS.WARNING);
const logError = (message, data) => debug(message, data, DEBUG_CONFIG.LOG_LEVELS.ERROR);

// Funkcje obsługi błędów i walidacji
function handleError(error, context) {
    const errorMessage = error.message || 'Nieznany błąd';
    logError(`Błąd podczas ${context}:`, error);
    alert(`Błąd podczas ${context}: ${errorMessage}`);
}

function validateLightConfig(config) {
    const errors = [];
    if (!config.dayLights) errors.push('Nieprawidłowe światła dzienne');
    if (!config.nightLights) errors.push('Nieprawidłowe światła nocne');
    if (isNaN(config.blinkFrequency) || config.blinkFrequency < 100 || config.blinkFrequency > 2000) {
        errors.push('Nieprawidłowa częstotliwość mrugania (zakres 100-2000ms)');
    }
    return errors;
}

// Zachowaj oryginalną funkcję getLightMode
function getLightMode(value) {
    return parseInt(value);
}

// Zachowaj oryginalną funkcję getFormValue
function getFormValue(value, defaultValue) {
    return value === undefined ? defaultValue : value;
}

// Klasa WebSocketManager
class WebSocketManager {
    constructor(hostname) {
        this.hostname = hostname;
        this.ws = null;
        this.retries = 0;
        this.isConnecting = false;
        this.pingInterval = null;
        this.reconnectTimeout = null;
    }

    connect() {
        if (this.isConnecting || this.retries >= WS_CONFIG.MAX_RETRIES) {
            return;
        }

        this.isConnecting = true;
        logInfo('Próba połączenia WebSocket...');

        try {
            this.ws = new WebSocket(`ws://${this.hostname}/ws`);
            this.setupEventHandlers();
            this.setupConnectionTimeout();
        } catch (error) {
            logError('Błąd podczas tworzenia połączenia WebSocket', error);
            this.handleConnectionError();
        }
    }

    setupEventHandlers() {
        this.ws.onopen = () => {
            this.isConnecting = false;
            this.retries = 0;
            logInfo('WebSocket połączony');
            this.startPingInterval();
            loadLightConfig();
        };

        this.ws.onclose = () => {
            this.isConnecting = false;
            this.stopPingInterval();
            logWarning('Połączenie WebSocket zostało zamknięte');
            this.handleConnectionError();
        };

        this.ws.onerror = (error) => {
            logError('Błąd WebSocket', error);
            this.handleConnectionError();
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleMessage(data);
            } catch (error) {
                logError('Błąd podczas przetwarzania wiadomości WebSocket', error);
            }
        };
    }

    setupConnectionTimeout() {
        setTimeout(() => {
            if (this.ws && this.ws.readyState !== WebSocket.OPEN) {
                logWarning('Przekroczono limit czasu połączenia');
                this.ws.close();
                this.handleConnectionError();
            }
        }, WS_CONFIG.CONNECTION_TIMEOUT);
    }

    handleConnectionError() {
        if (this.retries < WS_CONFIG.MAX_RETRIES) {
            this.retries++;
            logInfo(`Próba ponownego połączenia (${this.retries}/${WS_CONFIG.MAX_RETRIES})`);
            this.reconnectTimeout = setTimeout(() => this.connect(), WS_CONFIG.RECONNECT_DELAY);
        } else {
            logError('Przekroczono maksymalną liczbę prób połączenia');
        }
    }

    startPingInterval() {
        this.pingInterval = setInterval(() => {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.ws.send(JSON.stringify({ type: 'ping' }));
                logInfo('Wysłano ping');
            }
        }, WS_CONFIG.PING_INTERVAL);
    }

    stopPingInterval() {
        if (this.pingInterval) {
            clearInterval(this.pingInterval);
            this.pingInterval = null;
        }
    }

    handleMessage(data) {
        switch (data.type) {
            case 'pong':
                logInfo('Otrzymano pong');
                break;
            case 'config_update':
                logInfo('Otrzymano aktualizację konfiguracji', data);
                loadLightConfig();
                break;
            case 'error':
                logError('Otrzymano błąd z serwera', data);
                handleError(new Error(data.message), ERROR_TYPES.NETWORK);
                break;
            default:
                logWarning('Otrzymano nieznany typ wiadomości', data);
        }
    }

    cleanup() {
        this.stopPingInterval();
        if (this.reconnectTimeout) {
            clearTimeout(this.reconnectTimeout);
        }
        if (this.ws) {
            this.ws.close();
        }
    }
}

// Inicjalizacja WebSocket
let wsManager = null;

function initializeWebSocket() {
    if (wsManager) {
        wsManager.cleanup();
    }
    wsManager = new WebSocketManager(window.location.hostname);
    wsManager.connect();
}

// Główne funkcje aplikacji
async function saveLightConfigImpl() {
    try {
        const elements = getFormElements();
        const lightConfig = {
            dayLights: getLightMode(elements.dayLights.value),
            nightLights: getLightMode(elements.nightLights.value),
            dayBlink: elements.dayBlink.checked,
            nightBlink: elements.nightBlink.checked,
            blinkFrequency: parseInt(elements.blinkFrequency.value, 10)
        };

        const validationErrors = validateLightConfig(lightConfig);
        if (validationErrors.length > 0) {
            logError('Błędy walidacji', validationErrors);
            throw new Error(`Błędy walidacji: ${validationErrors.join(', ')}`);
        }

        logInfo('Przygotowane dane konfiguracji', lightConfig);

        const formData = new URLSearchParams();
        formData.append('data', JSON.stringify(lightConfig));

        const response = await fetch('/api/lights/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: formData.toString()
        });

        if (!response.ok) {
            throw new Error(`Błąd HTTP: ${response.status}`);
        }

        const result = await response.json();
        if (result.status === 'ok') {
            logInfo('Zapisano ustawienia świateł');
            alert('Zapisano ustawienia świateł');
            await loadLightConfig();
        } else {
            throw new Error(result.message || 'Nieznany błąd odpowiedzi');
        }
    } catch (error) {
        handleError(error, ERROR_TYPES.SAVE);
    }
}

async function loadLightConfigImpl() {
    try {
        const response = await fetch('/api/status');
        
        if (!response.ok) {
            throw new Error(`Błąd HTTP: ${response.status}`);
        }

        const data = await response.json();
        logInfo('Otrzymane dane', data);

        if (!data.lights) {
            throw new Error('Brak danych konfiguracji świateł');
        }

        const elements = getFormElements();
        
        elements.dayLights.value = getFormValue(data.lights.dayLights, false);
        elements.nightLights.value = getFormValue(data.lights.nightLights, true);
        elements.dayBlink.checked = Boolean(data.lights.dayBlink);
        elements.nightBlink.checked = Boolean(data.lights.nightBlink);
        elements.blinkFrequency.value = data.lights.blinkFrequency || 500;
            
        logInfo('Formularz zaktualizowany pomyślnie');
    } catch (error) {
        handleError(error, ERROR_TYPES.LOAD);
    }
}

// Wersje z debounce
const saveLightConfig = debounce(saveLightConfigImpl, 500);
const loadLightConfig = debounce(loadLightConfigImpl, 250);

// Inicjalizacja przy starcie
document.addEventListener('DOMContentLoaded', () => {
    initializeWebSocket();
});

// Czyszczenie przy zamknięciu
window.addEventListener('beforeunload', () => {
    if (wsManager) {
        wsManager.cleanup();
    }
});
