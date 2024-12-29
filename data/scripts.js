document.addEventListener('DOMContentLoaded', function() {
    // Pobierz aktualny czas z RTC przy starcie
    fetchRTCTime();

    // Obsługa WebSocket dla danych w czasie rzeczywistym
    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    
    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        updateDashboard(data);
    };

    // Aktualizacja danych na dashboardzie
    function updateDashboard(data) {
        // Aktualizacja temperatury
        if(data.temperature !== undefined) {
            document.querySelector('.temperature .value').textContent = data.temperature.toFixed(1);
        }
        // Aktualizacja baterii
        if(data.battery !== undefined) {
            document.querySelector('.battery-status .value').textContent = data.battery;
        }
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
            // Ustawienie czasu
            const date = new Date(
                data.time.year, 
                data.time.month - 1, 
                data.time.day, 
                data.time.hours, 
                data.time.minutes, 
                data.time.seconds
            );
            
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
            // Pokaż komunikat o sukcesie
            alert('Zapisano ustawienia zegara');
            // Odśwież wyświetlany czas
            fetchRTCTime();
        } else {
            alert('Błąd podczas zapisywania ustawień');
        }
    } catch (error) {
        console.error('Błąd podczas zapisywania konfiguracji RTC:', error);
        alert('Błąd podczas zapisywania ustawień');
    }
}

// Funkcja do formatowania liczby do dwóch cyfr
function padZero(num) {
    return String(num).padStart(2, '0');
}
