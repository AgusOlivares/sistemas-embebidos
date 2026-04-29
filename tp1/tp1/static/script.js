document.addEventListener("DOMContentLoaded", async () => {
    const API_BASE = "";

    async function fetchLedValues() {
        try {
            const controller = new AbortController();
            const timeout = setTimeout(() => controller.abort(), 5000);
            const res = await fetch(`${API_BASE}/getLedsValue`, { signal: controller.signal });
            clearTimeout(timeout);
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            const data = await res.json();
            Object.entries(data).forEach(([key, value]) => {
                const elem = document.getElementById(key);
                if (!elem) return;
                if (key === "led13") {
                    elem.checked = value === "1";
                } else {
                    elem.innerText = value;
                }
            });
        } catch (err) {
            if (err.name !== "AbortError") {
                console.error("Error fetching LED values:", err);
            }
        }
    }

    async function updateLedValue(pin, value) {
        try {
            const controller = new AbortController();
            const timeout = setTimeout(() => controller.abort(), 5000);
            const res = await fetch(`${API_BASE}/changeLedValue`, {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: `pin=${pin}&valor=${value}`,
                signal: controller.signal
            });
            clearTimeout(timeout);
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
        } catch (err) {
            if (err.name !== "AbortError") {
                console.error("Error updating LED value:", err);
            }
        }
    }

    function setupSliderHandlers() {
        document.querySelectorAll(".slider").forEach(slider => {
            slider.addEventListener("change", function() {
                const pin = this.id.replace("led", "");
                const value = this.value;
                if (pin != 13) {
                    document.getElementById(`brilloLed${pin}`).innerText = Math.floor(value * 255 / 100);
                }
                updateLedValue(pin, value);
            });
        });
    }

    function setupCheckboxHandler() {
        const checkbox = document.getElementById("led13");
        checkbox.addEventListener("change", () => {
            updateLedValue(13, checkbox.checked ? 1 : 0);
        });
    }

    await fetchLedValues();
    setupSliderHandlers();
    setupCheckboxHandler();

    setInterval(fetchLedValues, 1000);
});

