document.addEventListener('DOMContentLoaded', function() {
    // Pobierz aktualny czas z RTC i konfiguracje przy starcie
    fetchRTCTime();
    fetchLightConfig();
    fetchDisplayConfig();
    fetchControllerConfig();

    // Obsługa WebSocket dla danych w czasie rzeczywistym
    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    
    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        updateDashboard(data);
    };

    // Aktualizacja danych na dashboardzie
    function updateDashboard(data) {
        // Aktualizacja mocy
        if(data.power !== undefined) {
            document.querySelector('.power .value').textContent = data.power;
        }
    }
});

// Funkcja pobierająca czas z RTC
async function fetchRTCTime() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        if (data.time) {
            // Format czasu dla input type="time"
            const timeStr = String(data.time.hours).padStart(2, '0') + ':' +
                          String(data.time.minutes).padStart(2, '0') + ':' +
                          String(data.time.seconds).padStart(2, '0');
            document.getElementById('rtc-time').value = timeStr;
            
            // Format daty dla input type="date"
            const dateStr = data.time.year + '-' +
                          String(data.time.month).padStart(2, '0') + '-' +
                          String(data.time.day).padStart(2, '0');
            document.getElementById('rtc-date').value = dateStr;
        }
    } catch (error) {
        console.error('Błąd podczas pobierania czasu RTC:', error);
    }
}

// Funkcja zapisująca konfigurację RTC
async function saveRTCConfig() {
    const timeValue = document.getElementById('rtc-time').value;
    const dateValue = document.getElementById('rtc-date').value;
    
    // Rozdziel wartości czasu i daty
    const [hours, minutes, seconds] = timeValue.split(':').map(Number);
    const [year, month, day] = dateValue.split('-').map(Number);

    try {
        const response = await fetch('/api/time', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                year: year,
                month: month,
                day: day,
                hour: hours,
                minute: minutes,
                second: seconds || 0
            })
        });

        const data = await response.json();
        if (data.status === 'ok') {
            alert('Zapisano ustawienia zegara');
            fetchRTCTime();
        } else {
            alert('Błąd podczas zapisywania ustawień');
        }
    } catch (error) {
        console.error('Błąd podczas zapisywania konfiguracji RTC:', error);
        alert('Błąd podczas zapisywania ustawień');
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

// Funkcja zapisująca konfigurację świateł
async function saveLightConfig() {
    try {
        const data = {
            dayLights: document.getElementById('day-lights').value,
            nightLights: document.getElementById('night-lights').value,
            dayBlink: document.getElementById('day-blink').checked,
            nightBlink: document.getElementById('night-blink').checked,
            blinkFrequency: parseInt(document.getElementById('blink-frequency').value)
        };

        const response = await fetch('/api/lights/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });

        const result = await response.json();
        if (result.status === 'ok') {
            alert('Zapisano ustawienia świateł');
            fetchLightConfig();
        } else {
            throw new Error('Błąd odpowiedzi serwera');
        }
    } catch (error) {
        console.error('Błąd podczas zapisywania konfiguracji świateł:', error);
        alert('Błąd podczas zapisywania ustawień: ' + error.message);
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
            document.getElementById('auto-mode').checked = data.backlight.autoMode;
        }
    } catch (error) {
        console.error('Błąd podczas pobierania konfiguracji wyświetlacza:', error);
    }
}

function toggleAutoBrightness() {
    const autoMode = document.getElementById('display-auto').value === 'true';
    const autoBrightnessSection = document.getElementById('auto-brightness-section');
    const normalBrightness = document.getElementById('brightness').parentElement.parentElement;
    
    if (autoMode) {
        autoBrightnessSection.style.display = 'block';
        normalBrightness.style.display = 'none';
    } else {
        autoBrightnessSection.style.display = 'none';
        normalBrightness.style.display = 'flex';
    }
}

// Funkcja zapisująca konfigurację wyświetlacza
async function saveDisplayConfig() {
    try {
        const data = {
            dayBrightness: parseInt(document.getElementById('day-brightness').value),
            nightBrightness: parseInt(document.getElementById('night-brightness').value),
            autoMode: document.getElementById('auto-mode').checked
        };

        // Walidacja wartości
        if (data.dayBrightness < 0 || data.dayBrightness > 100 ||
            data.nightBrightness < 0 || data.nightBrightness > 100) {
            throw new Error('Wartości podświetlenia muszą być między 0 a 100%');
        }

        const response = await fetch('/api/display/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });

        const result = await response.json();
        if (result.status === 'ok') {
            alert('Zapisano ustawienia wyświetlacza');
            fetchDisplayConfig();
        } else {
            throw new Error('Błąd odpowiedzi serwera');
        }
    } catch (error) {
        console.error('Błąd podczas zapisywania konfiguracji wyświetlacza:', error);
        alert('Błąd podczas zapisywania ustawień: ' + error.message);
    }
}

// Funkcja do formatowania liczby do dwóch cyfr
function padZero(num) {
    return String(num).padStart(2, '0');
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
    const s866Params = document.getElementById('s866-params');

    if (controllerType === 'kt-lcd') {
        ktLcdParams.style.display = 'block';
        s866Params.style.display = 'none';
    } else {
        ktLcdParams.style.display = 'none';
        s866Params.style.display = 'block';
    }
}

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
                    document.getElementById(`s866-p${i}`).value = data.controller[`p${i}`] || '';
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
    'brightness-info': {
        title: 'Podświetlenie wyświetlacza',
        description: 'Ustaw jasność podświetlenia wyświetlacza w zakresie od 0% do 100%. ' +
            'Wyższa wartość oznacza jaśniejszy wyświetlacz, ale również większe zużycie energii. ' +
            'Zalecane ustawienie to 50-70% dla optymalnej widoczności i czasu pracy baterii.'
    },
    // Dodaj więcej opisów dla innych parametrów
};

// Obsługa modala
document.addEventListener('DOMContentLoaded', function() {
    const modal = document.getElementById('info-modal');
    const modalTitle = document.getElementById('modal-title');
    const modalDescription = document.getElementById('modal-description');
    const modalTimestamp = document.getElementById('modal-timestamp');
    const modalUser = document.getElementById('modal-user');
    
    // Obiekt z informacjami dla każdego parametru
    const infoContent = {
        // Sekcja zegara
        'clock-config-info': {
            title: 'Konfiguracja zegara',
            description: 'Ustawienia daty i czasu dla systemu. Zegar jest wykorzystywany do sterowania automatycznym włączaniem świateł oraz podświetleniem wyświetlacza.'
        },

        // Sekcja świateł
        'light-config-info': {
            title: 'Konfiguracja świateł',
            description: 'Ustawienia dotyczące świateł rowerowych. Możesz skonfigurować różne tryby świateł dla jazdy dziennej i nocnej.'
        },
        'day-lights-info': {
            title: 'Światła dzienne',
            description: 'Wybór konfiguracji świateł dla jazdy w dzień:\n- Wyłączone: wszystkie światła wyłączone\n- Przód dzień: przednie światło w trybie dziennym (maksymalna jasność)\n- Przód zwykłe: przednie światło w trybie normalnym\n- Tył: tylko tylne światło\n- Przód dzień + tył: przednie światło dzienne i tylne\n- Przód zwykłe + tył: przednie światło normalne i tylne'
        },
        'day-blink-info': {
            title: 'Mruganie tylnego światła (dzień)',
            description: 'Włącza lub wyłącza funkcję mrugania tylnego światła podczas jazdy w dzień. Mrugające światło może być bardziej widoczne dla innych uczestników ruchu.'
        },
        'night-lights-info': {
            title: 'Światła nocne',
            description: 'Konfiguracja świateł dla jazdy w nocy. Podobnie jak w przypadku świateł dziennych, możesz wybrać różne kombinacje świateł przednich i tylnych, dostosowane do warunków nocnych.'
        },
        'night-blink-info': {
            title: 'Mruganie tylnego światła (noc)',
            description: 'Włącza lub wyłącza funkcję mrugania tylnego światła podczas jazdy w nocy. Należy rozważnie używać tej funkcji, gdyż w niektórych warunkach migające światło może być bardziej dezorientujące niż pomocne.'
        },
        'blink-frequency-info': {
            title: 'Częstotliwość mrugania',
            description: 'Określa częstotliwość mrugania tylnego światła w milisekundach (ms). Mniejsza wartość oznacza szybsze mruganie, większa - wolniejsze. Zakres: 100-2000ms.'
        },

        // Sekcja wyświetlacza
        'display-config-info': {
            title: 'Konfiguracja wyświetlacza',
            description: 'Ustawienia związane z wyświetlaczem LCD, w tym jasność podświetlenia i tryb automatyczny.'
        },
        'brightness-info': {
            title: 'Podświetlenie wyświetlacza',
            description: 'Ustaw jasność podświetlenia wyświetlacza w zakresie od 0% do 100%. Wyższa wartość oznacza jaśniejszy wyświetlacz, ale również większe zużycie energii. Zalecane ustawienie to 50-70% dla optymalnej widoczności i czasu pracy baterii.'
        },
        'auto-mode-info': {
            title: 'Tryb automatyczny',
            description: 'Automatycznie dostosowuje jasność wyświetlacza w zależności od pory dnia. W trybie dziennym używa jaśniejszego podświetlenia, a w nocnym - przyciemnionego.'
        },
        'day-brightness-info': {
            title: 'Jasność dzienna',
            description: 'Poziom jasności wyświetlacza używany w ciągu dnia (0-100%). Zalecana wyższa wartość dla lepszej widoczności w świetle słonecznym.'
        },
        'night-brightness-info': {
            title: 'Jasność nocna',
            description: 'Poziom jasności wyświetlacza używany w nocy (0-100%). Zalecana niższa wartość dla komfortowego użytkowania w ciemności.'
        },

        // Parametry sterownika KT-LCD
        'kt-p1-info': {
            title: 'P1 - Ilość magnesów w silniku',
            description: 'Określa ilość magnesów w silniku elektrycznym. Typowe wartości: 1-255. Parametr kluczowy dla prawidłowego obliczania prędkości.'
        },
        'kt-p2-info': {
            title: 'P2 - Rozmiar koła',
            description: 'Określa rozmiar koła w calach. Wartość wpływa na dokładność pomiaru prędkości i dystansu.'
        },
        'kt-p3-info': {
            title: 'P3 - Sposób pomiaru prędkości',
            description: 'Wybór metody pomiaru prędkości: zewnętrzny czujnik lub sygnały z silnika.'
        },
        'kt-p4-info': {
            title: 'P4 - Jednostki prędkości',
            description: 'Wybór jednostek wyświetlania prędkości: km/h lub mph.'
        },
        'kt-p5-info': {
            title: 'P5 - Ograniczenie prędkości',
            description: 'Maksymalna prędkość, przy której silnik będzie wspomagał. Zakres: 0-100 km/h.'
        },

        // Parametry C sterownika
        'kt-c1-info': {
            title: 'C1 - Czujnik PAS',
            description: 'Konfiguracja czułości czujnika asysty pedałowania (PAS). Wpływa na to, jak szybko system reaguje na pedałowanie.'
        },
        'kt-c2-info': {
            title: 'C2 - Typ silnika',
            description: 'Ustawienia charakterystyki silnika i jego podstawowych parametrów pracy.'
        },
        'kt-c3-info': {
            title: 'C3 - Tryb wspomagania',
            description: 'Konfiguracja poziomów wspomagania i ich charakterystyki (eco, normal, power).'
        },
        'kt-c4-info': {
            title: 'C4 - Manetka i PAS',
            description: 'Określa sposób współdziałania manetki z czujnikiem PAS i priorytety sterowania.'
        },
        'kt-c5-info': {
            title: 'C5 - Moc silnika',
            description: 'Ustawienia związane z maksymalną mocą silnika i charakterystyką podczas ruszania.'
        },
        'kt-c6-info': {
            title: 'C6 - Jasność wyświetlacza',
            description: 'Ustawienie domyślnej jasności podświetlenia wyświetlacza LCD.'
        },
        'kt-c7-info': {
            title: 'C7 - Tempomat',
            description: 'Konfiguracja tempomatu - utrzymywania stałej prędkości.'
        },
        'kt-c8-info': {
            title: 'C8 - Silnik',
            description: 'Dodatkowe parametry silnika, w tym temperatura i zabezpieczenia.'
        },
        'kt-c9-info': {
            title: 'C9 - Zabezpieczenia',
            description: 'Ustawienia kodów PIN i innych zabezpieczeń systemowych.'
        },
        'kt-c10-info': {
            title: 'C10 - Ustawienia fabryczne',
            description: 'Opcje przywracania ustawień fabrycznych i kalibracji systemu.'
        },
        'kt-c11-info': {
            title: 'C11 - Komunikacja',
            description: 'Parametry komunikacji między kontrolerem a wyświetlaczem.'
        },
        'kt-c12-info': {
            title: 'C12 - Napięcie odcięcia',
            description: 'Ustawienie minimalnego napięcia baterii, przy którym system się wyłączy.'
        },
        'kt-c13-info': {
            title: 'C13 - Regeneracja',
            description: 'Konfiguracja hamowania regeneracyjnego i odzyskiwania energii podczas hamowania.'
        },
        'kt-c14-info': {
            title: 'C14 - Poziomy PAS',
            description: 'Konfiguracja poziomów wspomagania i ich charakterystyk.'
        },
        'kt-c15-info': {
            title: 'C15 - Prowadzenie',
            description: 'Ustawienia trybu prowadzenia roweru (walk assist).'
        },

        // Parametry L sterownika
        'kt-l1-info': {
            title: 'L1 - Napięcie pracy',
            description: 'Konfiguracja napięcia pracy sterownika i zabezpieczeń napięciowych.'
        },
        'kt-l2-info': {
            title: 'L2 - Silniki wysoko-obrotowe',
            description: 'Specjalne ustawienia dla silników o wysokich obrotach.'
        },
        'kt-l3-info': {
            title: 'L3 - Czujniki Halla',
            description: 'Konfiguracja czujników Halla w silniku - tryb dual.'
        },

        // Parametry sterownika S866
        's866-p1-info': {
            title: 'P1 - Ograniczenie prądu',
            description: 'Ustawienie maksymalnego prądu sterownika S866.'
        },
        's866-p2-info': {
            title: 'P2 - Ograniczenie prędkości',
            description: 'Maksymalna prędkość dla sterownika S866.'
        }
    };
    
    // Otwieranie modala
    document.querySelectorAll('.info-icon').forEach(button => {
        button.addEventListener('click', function() {
            const infoId = this.dataset.info;
            const info = infoContent[infoId];
            
            if (info) {
                modalTitle.textContent = info.title;
                modalDescription.textContent = info.description;
                modalTimestamp.textContent = new Date().toLocaleString('pl-PL', {
                    year: 'numeric',
                    month: '2-digit',
                    day: '2-digit',
                    hour: '2-digit',
                    minute: '2-digit',
                    second: '2-digit'
                });
                modalUser.textContent = 'Autor: pimowo';
                
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
});
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

// Wywołaj przy załadowaniu strony
document.addEventListener('DOMContentLoaded', function() {
    fetchSystemVersion();
});
