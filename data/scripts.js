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
