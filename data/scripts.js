// Funkcja konwersji wartości formularza na wartości API
function getLightMode(value) {
    debug('Konwersja wartości formularza:', value);
    switch(value) {
        case 'front-day':
        case 'front-normal':
            return "FRONT";
        case 'rear':
            return "REAR";
        case 'front-day-rear':
        case 'front-normal-rear':
            return "BOTH";
        case 'off':
        default:
            return "NONE";
    }
}

// Funkcja konwersji wartości z API na wartości formularza
function getFormValue(serverValue, isNightMode = false) {
    debug('Konwersja wartości z serwera:', serverValue);
    switch(serverValue) {
        case 'FRONT':
            return isNightMode ? 'front-normal' : 'front-day';
        case 'REAR':
            return 'rear';
        case 'BOTH':
            return isNightMode ? 'front-normal-rear' : 'front-day-rear';
        case 'NONE':
        default:
            return 'off';
    }
}

// Dodaj zmienną do kontroli debounce
let saveTimeout = null;

// Funkcja zapisywania konfiguracji z debounce
async function saveLightConfig() {
    debug('Rozpoczynam zapisywanie konfiguracji świateł');
    
    // Anuluj poprzedni timeout jeśli istnieje
    if (saveTimeout) {
        clearTimeout(saveTimeout);
    }

    // Ustaw nowy timeout
    saveTimeout = setTimeout(async () => {
        try {
            const elements = {
                dayLights: document.getElementById('day-lights'),
                nightLights: document.getElementById('night-lights'),
                dayBlink: document.getElementById('day-blink'),
                nightBlink: document.getElementById('night-blink'),
                blinkFrequency: document.getElementById('blink-frequency')
            };

            const lightConfig = {
                dayLights: getLightMode(elements.dayLights.value),
                nightLights: getLightMode(elements.nightLights.value),
                dayBlink: elements.dayBlink.checked,
                nightBlink: elements.nightBlink.checked,
                blinkFrequency: parseInt(elements.blinkFrequency.value)
            };

            debug('Przygotowane dane:', lightConfig);

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
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const result = await response.json();
            if (result.status === 'ok') {
                alert('Zapisano ustawienia świateł'); // Dodajemy to
                await loadLightConfig(); // Odświeżamy konfigurację
            } else {
                throw new Error(result.message || 'Nieznany błąd');
            }
        } catch (error) {
            console.error('Błąd podczas zapisywania:', error);
            alert('Błąd podczas zapisywania ustawień: ' + error.message);
        }
    }, 500); // Czekaj 500ms przed zapisem
}

// Dodaj debounce dla loadLightConfig
let loadTimeout = null;

// Funkcja wczytywania konfiguracji z debounce
async function loadLightConfig() {
    debug('Rozpoczynam wczytywanie konfiguracji świateł...');
    
    // Anuluj poprzedni timeout jeśli istnieje
    if (loadTimeout) {
        clearTimeout(loadTimeout);
    }

    // Ustaw nowy timeout
    loadTimeout = setTimeout(async () => {
        try {
            const response = await fetch('/api/status');
            const data = await response.json();
            debug('Otrzymane dane:', data);

            if (data.lights) {
                debug('Aktualizacja formularza, otrzymane dane:', data.lights);
                
                const elements = {
                    dayLights: document.getElementById('day-lights'),
                    nightLights: document.getElementById('night-lights'),
                    dayBlink: document.getElementById('day-blink'),
                    nightBlink: document.getElementById('night-blink'),
                    blinkFrequency: document.getElementById('blink-frequency')
                };

                elements.dayLights.value = getFormValue(data.lights.dayLights, false);
                elements.nightLights.value = getFormValue(data.lights.nightLights, true);
                elements.dayBlink.checked = Boolean(data.lights.dayBlink);
                elements.nightBlink.checked = Boolean(data.lights.nightBlink);
                elements.blinkFrequency.value = data.lights.blinkFrequency || 500;

                debug('Formularz zaktualizowany pomyślnie');
            }
        } catch (error) {
            console.error('Błąd podczas wczytywania konfiguracji świateł:', error);
        }
    }, 250); // Czekaj 250ms przed odświeżeniem
}

// Funkcja pomocnicza do debugowania
function debug(...args) {
    if (typeof console !== 'undefined') {
        console.log('[DEBUG]', ...args);
    }
}

document.addEventListener('DOMContentLoaded', async function() {
    debug('Inicjalizacja aplikacji...');

    let clockInterval;
    
    try {
        // Inicjalizacja zegara
        clockInterval = initializeClock();
        
        // Poczekaj na załadowanie DOM
        await new Promise(resolve => setTimeout(resolve, 200));

        // Inicjalizacja pozostałych modułów
        if (document.querySelector('.light-config')) {
            await loadLightConfig();
        }

        await Promise.all([
            fetchDisplayConfig(),
            fetchControllerConfig(),
            fetchSystemVersion()
        ]);

        // Inicjalizacja WebSocket i UI
        setupWebSocket();
        setupModal();
        setupFormListeners();

        debug('Inicjalizacja zakończona pomyślnie');
    } catch (error) {
        console.error('Błąd podczas inicjalizacji:', error);
        // W przypadku błędu, zatrzymaj interval zegara
        if (clockInterval) clearInterval(clockInterval);
    }
});

function checkAPIResponse(response, errorMessage = 'Błąd API') {
    if (!response.ok) {
        throw new Error(`${errorMessage}: ${response.status}`);
    }
    return response.json();
}

function showModal(title, description) {
    const modal = document.getElementById('info-modal');
    const modalTitle = document.getElementById('modal-title');
    const modalDescription = document.getElementById('modal-description');
    
    if (modal && modalTitle && modalDescription) {
        modalTitle.textContent = title;
        modalDescription.textContent = description;
        modal.style.display = 'block';
    }
}

let ws = null;

function setupWebSocket() {
    debug('Inicjalizacja WebSocket...');
    function connect() {
        ws = new WebSocket('ws://' + window.location.hostname + '/ws');
        
        ws.onopen = () => {
            debug('WebSocket połączony');
            // Pobierz aktualny stan po połączeniu
            fetchCurrentState();
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                debug('Otrzymano dane WebSocket:', data);
                if (data.lights) {
                    updateLightStatus(data.lights);
                    updateLightForm(data.lights); // Aktualizuj też formularz
                }
            } catch (error) {
                console.error('Błąd podczas przetwarzania danych WebSocket:', error);
            }
        };

        ws.onclose = () => {
            debug('WebSocket rozłączony, próba ponownego połączenia za 5s');
            setTimeout(connect, 5000);
        };

        ws.onerror = (error) => {
            console.error('Błąd WebSocket:', error);
        };
    }

    connect();
}

async function fetchRTCTime() {
    try {
        const timeElement = document.getElementById('rtc-time');
        const dateElement = document.getElementById('rtc-date');

        if (!timeElement || !dateElement) {
            debug('Elementy czasu nie są gotowe');
            return;
        }

        const response = await fetch('/api/time');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const data = await response.json();
        if (data && data.time) {
            const { hours, minutes, seconds, year, month, day } = data.time;
            
            const timeStr = `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
            const dateStr = `${String(day).padStart(2, '0')}/${String(month).padStart(2, '0')}/${year}`;
            
            timeElement.value = timeStr;
            dateElement.value = dateStr;
            
            debug('Zaktualizowano czas:', { time: timeStr, date: dateStr });
        }
    } catch (error) {
        console.error('Błąd podczas pobierania czasu RTC:', error);
    }
}

// Funkcja inicjalizacji zegara
function initializeClock() {
    debug('Inicjalizacja zegara');
    fetchRTCTime(); // Pierwsze pobranie
    return setInterval(fetchRTCTime, 1000); // Aktualizacja co sekundę
}

function checkElements(...ids) {
    const missing = [];
    const elements = {};

    for (const id of ids) {
        const element = document.getElementById(id);
        if (!element) {
            missing.push(id);
        }
        elements[id] = element;
    }

    if (missing.length > 0) {
        throw new Error(`Brak elementów: ${missing.join(', ')}`);
    }

    return elements;
}

// Funkcja zapisująca konfigurację RTC
async function saveRTCConfig() {
    // Pobierz aktualny czas z przeglądarki
    const now = new Date();
    
    try {
        const response = await fetch('/api/time', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                year: now.getFullYear(),
                month: now.getMonth() + 1, // getMonth() zwraca 0-11
                day: now.getDate(),
                hour: now.getHours(),
                minute: now.getMinutes(),
                second: now.getSeconds()
            })
        });

        const data = await response.json();
        if (data.status === 'ok') {
            alert('Ustawiono aktualny czas');
            fetchRTCTime(); // Pobierz zaktualizowany czas
        } else {
            alert('Błąd podczas ustawiania czasu');
        }
    } catch (error) {
        console.error('Błąd podczas ustawiania czasu RTC:', error);
        alert('Błąd podczas ustawiania czasu');
    }
}

// Funkcja pobierająca konfigurację świateł
async function fetchLightConfig() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        if (data.lights) {
            document.getElementById('day-lights').value = data.lights.dayLights;
            document.getElementById('night-lights').value = data.lights.nightLights;
            document.getElementById('day-blink').checked = data.lights.dayBlink;
            document.getElementById('night-blink').checked = data.lights.nightBlink;
            document.getElementById('blink-frequency').value = data.lights.blinkFrequency;
        }
    } catch (error) {
        console.error('Błąd podczas pobierania konfiguracji świateł:', error);
    }
}

function setupFormListeners() {
    const formElements = [
        'day-lights',
        'night-lights',
        'day-blink',
        'night-blink',
        'blink-frequency'
    ];

    formElements.forEach(elementId => {
        const element = document.getElementById(elementId);
        if (element) {
            element.addEventListener('change', () => {
                debug(`Zmieniono wartość ${elementId}:`, 
                    element.type === 'checkbox' ? element.checked : element.value);
            });
        }
    });

    // Dodaj listener do przycisku zapisu
    const saveButton = document.getElementById('save-lights-btn');
    if (saveButton) {
        saveButton.addEventListener('click', saveLightConfig);
    }
}

// Funkcja inicjalizacji formularza świateł
function initializeLightForm() {
    debug('Inicjalizacja formularza świateł');
    const form = document.getElementById('lights-form');
    if (!form) {
        console.error('Nie znaleziono formularza świateł');
        return false;
    }
    return true;
}

// Funkcja sprawdzająca elementy formularza
function getLightFormElements() {
    const elements = {
        dayLights: document.getElementById('day-lights'),
        nightLights: document.getElementById('night-lights'),
        dayBlink: document.getElementById('day-blink'),
        nightBlink: document.getElementById('night-blink'),
        blinkFrequency: document.getElementById('blink-frequency')
    };

    const missing = Object.entries(elements)
        .filter(([_, element]) => !element)
        .map(([name]) => name);

    if (missing.length > 0) {
        throw new Error(`Brak elementów formularza: ${missing.join(', ')}`);
    }

    return elements;
}

async function fetchCurrentState(retries = 3) {
    for (let i = 0; i < retries; i++) {
        try {
            debug(`Próba pobrania stanu (${i + 1}/${retries})`);
            const response = await fetch('/api/status');
            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const data = await response.json();
            debug('Otrzymane dane statusu:', data);

            if (data.lights) {
                updateLightStatus(data.lights);
                updateLightForm(data.lights);
                return true;
            }
            
            await new Promise(resolve => setTimeout(resolve, 1000));
        } catch (error) {
            console.error(`Błąd podczas próby ${i + 1}:`, error);
            if (i === retries - 1) throw error;
            await new Promise(resolve => setTimeout(resolve, 1000));
        }
    }
    return false;
}

async function fetchCurrentState() {
    try {
        debug('Pobieranie aktualnego stanu...');
        const response = await fetch('/api/status');
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const data = await response.json();
        debug('Otrzymane dane statusu:', data);

        if (data.lights) {
            // Aktualizacja interfejsu
            updateLightStatus(data.lights);
            // Aktualizacja formularza
            updateLightForm(data.lights);
        }
    } catch (error) {
        console.error('Błąd podczas pobierania stanu:', error);
    }
}

// Funkcja do aktualizacji formularza na podstawie otrzymanego stanu
function updateLightForm(lights) {
    debug('Aktualizacja formularza, otrzymane dane:', lights);
    
    try {
        const elements = {
            dayLights: document.getElementById('day-lights'),
            nightLights: document.getElementById('night-lights'),
            dayBlink: document.getElementById('day-blink'),
            nightBlink: document.getElementById('night-blink'),
            blinkFrequency: document.getElementById('blink-frequency')
        };

        // Konwersja LightMode na wartość selecta
        function getSelectValue(mode) {
            switch(mode) {
                case 'BOTH':
                    return 'front-day-rear';
                case 'FRONT':
                    return 'front-day';
                case 'REAR':
                    return 'rear';
                default:
                    return 'off';
            }
        }

        // Ustaw wartości formularza
        elements.dayLights.value = getSelectValue(lights.dayLights);
        elements.nightLights.value = getSelectValue(lights.nightLights);
        elements.dayBlink.checked = lights.dayBlink;
        elements.nightBlink.checked = lights.nightBlink;
        elements.blinkFrequency.value = lights.blinkFrequency || 500;

        debug('Formularz zaktualizowany pomyślnie');
    } catch (error) {
        console.error('Błąd podczas aktualizacji formularza:', error);
    }
}

function setupModal() {
    const modal = document.getElementById('info-modal');
    const modalTitle = document.getElementById('modal-title');
    const modalDescription = document.getElementById('modal-description');
    
    // Otwieranie modala przez info-icons
    document.querySelectorAll('.info-icon').forEach(button => {
        button.addEventListener('click', function() {
            const infoId = this.dataset.info;
            const info = infoContent[infoId];
            
            if (info) {
                modalTitle.textContent = info.title;
                modalDescription.textContent = info.description;               
                modal.style.display = 'block';
            } else {
                console.error('Nie znaleziono opisu dla:', infoId);
            }
        });
    });
    
    // Zamykanie modala
    document.querySelector('.close-modal').addEventListener('click', () => {
        modal.style.display = 'none';
    });
    
    // Zamykanie po kliknięciu poza modalem
    window.addEventListener('click', (event) => {
        if (event.target === modal) {
            modal.style.display = 'none';
        }
    });
}

// Funkcja aktualizacji statusu świateł
function updateLightStatus(lights) {
    try {
        const elements = getLightFormElements();
        if (!elements) return;

        // Aktualizacja klas CSS dla wskaźników świateł (jeśli są)
        const indicators = {
            frontDay: document.querySelector('.light-indicator.front-day'),
            front: document.querySelector('.light-indicator.front'),
            rear: document.querySelector('.light-indicator.rear')
        };

        for (const [key, indicator] of Object.entries(indicators)) {
            if (indicator) {
                indicator.classList.toggle('active', Boolean(lights[key]));
            }
        }

        debug('Status świateł zaktualizowany');
    } catch (error) {
        console.error('Błąd podczas aktualizacji statusu świateł:', error);
    }
}

// Funkcja pobierająca konfigurację wyświetlacza
async function fetchDisplayConfig() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        if (data.backlight) {
            document.getElementById('day-brightness').value = data.backlight.dayBrightness;
            document.getElementById('night-brightness').value = data.backlight.nightBrightness;
            document.getElementById('display-auto').value = data.backlight.autoMode.toString();
            // Ustawienie jasności normalnej na podstawie jasności dziennej w trybie manualnym
            document.getElementById('brightness').value = data.backlight.dayBrightness;
            // Wywołaj funkcję przełączania, aby odpowiednio pokazać/ukryć sekcje
            toggleAutoBrightness();
        }
    } catch (error) {
        console.error('Błąd podczas pobierania konfiguracji wyświetlacza:', error);
    }
}

// Funkcja zapisująca konfigurację wyświetlacza
async function saveDisplayConfig() {
    try {
        const autoMode = document.getElementById('display-auto').value === 'true';
        const data = {
            dayBrightness: parseInt(autoMode ? 
                document.getElementById('day-brightness').value : 
                document.getElementById('brightness').value),
            nightBrightness: parseInt(document.getElementById('night-brightness').value),
            autoMode: autoMode
        };

        console.log('Wysyłane dane:', data); // dla debugowania

        const response = await fetch('/api/display/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });

        const result = await response.json();
        console.log('Odpowiedź serwera:', result); // dla debugowania

        if (result.status === 'ok') {
            alert('Zapisano ustawienia wyświetlacza');
            await fetchDisplayConfig(); // odśwież wyświetlane ustawienia
        } else {
            throw new Error(result.message || 'Błąd odpowiedzi serwera');
        }
    } catch (error) {
        console.error('Błąd podczas zapisywania:', error);
        alert('Błąd podczas zapisywania ustawień: ' + error.message);
    }
}

function toggleAutoBrightness() {
    const autoMode = document.getElementById('display-auto').value === 'true';
    const autoBrightnessSection = document.getElementById('auto-brightness-section');
    const normalBrightness = document.getElementById('brightness').parentElement.parentElement;
    
    if (autoMode) {
        autoBrightnessSection.style.display = 'block';
        normalBrightness.style.display = 'none';
        // Ustaw jasność dzienną jako domyślną jasność
        document.getElementById('day-brightness').value = document.getElementById('brightness').value;
    } else {
        autoBrightnessSection.style.display = 'none';
        normalBrightness.style.display = 'flex';
        // Ustaw normalną jasność na wartość jasności dziennej
        document.getElementById('brightness').value = document.getElementById('day-brightness').value;
    }
}

// Walidacja dla pól numerycznych wyświetlacza
document.querySelectorAll('#day-brightness, #night-brightness').forEach(input => {
    input.addEventListener('input', function() {
        let value = parseInt(this.value);
        if (value < 0) this.value = 0;
        if (value > 100) this.value = 100;
    });
});

// Funkcja przełączająca widoczność parametrów w zależności od wybranego sterownika
function toggleControllerParams() {
    const controllerType = document.getElementById('controller-type').value;
    const ktLcdParams = document.getElementById('kt-lcd-params');
    const s866Params = document.getElementById('s866-lcd-params');

    if (controllerType === 'kt-lcd') {
        ktLcdParams.style.display = 'block';
        s866Params.style.display = 'none';
    } else if (controllerType === 's866') {
        ktLcdParams.style.display = 'none';
        s866Params.style.display = 'block';
    }
}

// Dodaj wywołanie przy załadowaniu strony
document.addEventListener('DOMContentLoaded', function() {
    toggleControllerParams();
});

// Funkcja pobierająca konfigurację sterownika
async function fetchControllerConfig() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        if (data.controller) {
            // Ustaw typ sterownika
            document.getElementById('controller-type').value = data.controller.type;
            toggleControllerParams();

            // Wypełnij parametry dla KT-LCD
            if (data.controller.type === 'kt-lcd') {
                // Parametry P
                for (let i = 1; i <= 5; i++) {
                    document.getElementById(`kt-p${i}`).value = data.controller[`p${i}`] || '';
                }
                // Parametry C
                for (let i = 1; i <= 15; i++) {
                    document.getElementById(`kt-c${i}`).value = data.controller[`c${i}`] || '';
                }
                // Parametry L
                for (let i = 1; i <= 3; i++) {
                    document.getElementById(`kt-l${i}`).value = data.controller[`l${i}`] || '';
                }
            }
            // Wypełnij parametry dla S866
            else if (data.controller.type === 's866') {
                for (let i = 1; i <= 20; i++) {
                    const input = document.getElementById(`s866-p${i}`);
                    if (input) {
                        input.value = data.controller[`p${i}`] || '';
                    }
                }
            }
        }
    } catch (error) {
        console.error('Błąd podczas pobierania konfiguracji sterownika:', error);
    }
}

// Funkcja zapisująca konfigurację sterownika
async function saveControllerConfig() {
    try {
        const controllerType = document.getElementById('controller-type').value;
        let data = {
            type: controllerType,
        };

        // Zbierz parametry w zależności od typu sterownika
        if (controllerType === 'kt-lcd') {
            // Parametry P
            for (let i = 1; i <= 5; i++) {
                const value = document.getElementById(`kt-p${i}`).value;
                if (value !== '') data[`p${i}`] = parseInt(value);
            }
            // Parametry C
            for (let i = 1; i <= 15; i++) {
                const value = document.getElementById(`kt-c${i}`).value;
                if (value !== '') data[`c${i}`] = parseInt(value);
            }
            // Parametry L
            for (let i = 1; i <= 3; i++) {
                const value = document.getElementById(`kt-l${i}`).value;
                if (value !== '') data[`l${i}`] = parseInt(value);
            }
        } else {
            // Parametry S866
            for (let i = 1; i <= 20; i++) {
                const value = document.getElementById(`s866-p${i}`).value;
                if (value !== '') data[`p${i}`] = parseInt(value);
            }
        }

        const response = await fetch('/api/controller/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });

        const result = await response.json();
        if (result.status === 'ok') {
            alert('Zapisano ustawienia sterownika');
            fetchControllerConfig();
        } else {
            throw new Error('Błąd odpowiedzi serwera');
        }
    } catch (error) {
        console.error('Błąd podczas zapisywania konfiguracji sterownika:', error);
        alert('Błąd podczas zapisywania ustawień: ' + error.message);
    }
}

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
    },

    'wheel-size-info': {
        title: 'Rozmiar koła',
        description: `Wybierz rozmiar koła swojego roweru. Jest to ważne dla prawidłowego obliczania prędkości i dystansu.`
    },

    'bms-info': {
        title: 'System zarządzania baterią (BMS)',
        description: `BMS (Battery Management System) to system monitorujący stan baterii. Po włączeniu tej opcji, urządzenie będzie odbierać dane o stanie baterii przez Bluetooth, takie jak:
             
    • Pojemność (Ah)
    • Energia (Wh)
    • Temperatura ogniw (°C)
    • Stan naładowania (SOC)`
    },

    // Opis dla TPMS
    'tpms-info': {
        title: 'System monitorowania ciśnienia w oponach (TPMS)',
        description: `TPMS (Tire Pressure Monitoring System) to system monitorujący ciśnienie w oponach. Po włączeniu tej opcji, urządzenie będzie odbierać dane z czujników przez Bluetooth, takie jak:
                
    • Ciśnienie w oponach (bar)
    • Temperatura opon (°C)
    • Stan baterii czujników (V)`
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

function saveGeneralSettings() {
    const wheelSize = document.getElementById('wheel-size').value;
    
    fetch('/save-general-settings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            wheelSize: wheelSize
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            console.log('Ustawienia ogólne zapisane pomyślnie');
        }
    })
    .catch(error => {
        console.error('Błąd podczas zapisywania ustawień ogólnych:', error);
    });
}

async function loadBluetoothConfig() {
    try {
        const response = await fetch('/get-bluetooth-config');
        const data = await response.json();
        if (data) {
            document.getElementById('bms-enabled').value = data.bmsEnabled.toString();
            document.getElementById('tpms-enabled').value = data.tpmsEnabled.toString();
        }
    } catch (error) {
        console.error('Błąd podczas pobierania konfiguracji Bluetooth:', error);
    }
}

window.onload = function() {
    loadGeneralSettings();
    loadBluetoothConfig();
    // ... (pozostałe istniejące wywołania)
};

function saveBluetoothConfig() {
    const bmsEnabled = document.getElementById('bms-enabled').value;
    const tpmsEnabled = document.getElementById('tpms-enabled').value;
    
    // Tutaj dodaj kod do zapisywania konfiguracji
    fetch('/save-bluetooth-config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            bmsEnabled: bmsEnabled === 'true',
            tpmsEnabled: tpmsEnabled === 'true'
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            console.log('Konfiguracja Bluetooth zapisana pomyślnie');
        }
    })
    .catch(error => {
        console.error('Błąd podczas zapisywania konfiguracji Bluetooth:', error);
    });
}

/*
WAŻNE KOMUNIKATY:
⚠️ - Ważne ostrzeżenia
💡 - Wskazówka
📝 - Uwaga

PARAMETRY TECHNICZNE:
⚡ - Ustawienia mocy/elektryczne
🔋 - Ustawienia baterii
🔌 - Ustawienia elektryczne
🌡️ - Parametry temperatury
📊 - Parametry pomiarowe

USTAWIENIA MECHANICZNE:
🚲 - Ogólne ustawienia roweru
⚙️ - Ustawienia mechaniczne
🔄 - Funkcje regeneracji

INTERFEJS I CZAS:
📱 - Ustawienia interfejsu
⏰ - Ustawienia czasowe
💾 - Opcje zapisu/resetu

BEZPIECZEŃSTWO I WYDAJNOŚĆ:
🔒 - Ustawienia zabezpieczeń
📈 - Parametry wydajności
🛠️ - Ustawienia serwisowe
🔧 - KONFIGURACJA
*/
