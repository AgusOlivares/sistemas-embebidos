# Monitor de Intensidad Luminosa con FreeRTOS

Sistema de monitoreo de luz ambiental con alarma, compuesto por tres capas que se comunican en cadena:

```
Arduino (FreeRTOS) ──USB Serial──→ Python (FastAPI) ──HTTP──→ Browser (HTML/JS)
```

---

## Arquitectura general

```
                     ┌──────────────────────────────────────────────────┐
                     │                   NAVEGADOR                      │
                     │              index.html + script.js              │
                     │                                                  │
                     │   setInterval cada 3s ──GET /valor──→ JSON       │
                     │   Botones            ──GET /start──→ escribe "R" │
                     │                      ──GET /stop───→ escribe "S" │
                     └───────────────────────┬──────────────────────────┘
                                             │ HTTP (localhost:3000)
                     ┌───────────────────────▼──────────────────────────┐
                     │              PYTHON (FastAPI)                    │
                     │              tp2_FreeRTOS.py                      │
                     │                                                  │
                     │  Servidor: uvicorn sirve endpoints HTTP          │
                     │  Hilo lector: lee Serial, actualiza estado       │
                     │                                                  │
                     │  /valor  → { alarm, ldrValue, readActivated }    │
                     │  /start  → envía "R\n" por Serial                │
                     │  /stop   → envía "S\n" por Serial                │
                     └───────────────────────┬──────────────────────────┘
                                             │ USB Serial (9600 baud)
                     ┌───────────────────────▼──────────────────────────┐
                     │           ARDUINO (FreeRTOS)                     │
                     │              tp2.ino                             │
                     │                                                  │
                     │  ┌─ TaskControlRead  (prioridad 2)               │
                     │  ├─ TaskAnalogRead   (prioridad 1)               │
                     │  ├─ TaskAnalogWrite  (prioridad 1)               │
                     │  └─ BlinkFunction    (prioridad 1)               │
                     │                                                  │
                     │  ISR pin2 → activateSemaphore                    │
                     │  ISR pin3 → deactivateSemaphore                  │
                     └──────────────────────────────────────────────────┘
```

---

## Capa 1: Arduino — FreeRTOS (`tp2.ino`)

### Pines

| Pin | Conexión | Constante |
|-----|----------|-----------|
| 2 | Botón activar lectura | `BOTON_ACTIVAR` |
| 3 | Botón desactivar lectura | `BOTON_DESACTIVAR` |
| 11 | LED indicador de actividad | `LED_INDICADOR` |
| 12 | LED de alarma | `LED_ALARMA` |
| A3 | LDR (fotorresistencia) | `PIN_LDR` |

### Variables globales

| Variable | Tipo | Propósito |
|---|---|---|
| `mutex` | `SemaphoreHandle_t` | Protege `a3Value` entre tareas |
| `activateSemaphore` | `SemaphoreHandle_t` | Notifica "activar" desde ISR pin 2 a `TaskControlRead` |
| `deactivateSemaphore` | `SemaphoreHandle_t` | Notifica "desactivar" desde ISR pin 3 a `TaskControlRead` |
| `readActivated` | `volatile bool` | Flag compartido: `true` = leyendo, `false` = detenido |
| `a3Value` | `int` | Último valor leído del LDR (protegido por mutex) |

### Tareas

#### `TaskControlRead` — prioridad 2 (más alta)
- Espera los semáforos binarios dados por las ISRs
- Si recibe `activateSemaphore` → `readActivated = true`, envía `"R"` por serial
- Si recibe `deactivateSemaphore` → `readActivated = false`, envía `"S"` por serial
- Ciclo cada 20 ms

#### `TaskAnalogRead` — prioridad 1
- Si `readActivated == true`:
  1. Toma el mutex, lee `PIN_LDR`, suelta el mutex
  2. Si `a3Value > UMBRAL_ALARMA` (800) → activa alarma, envía `"A"` por serial
  3. Si hay alarma → parpadea LED en pin 12
- Si `readActivated == false` y había alarma → la desactiva, envía `"a"` por serial
- También acepta comandos seriales `'R'` / `'S'` como respaldo a botones
- Ciclo cada 10 ms

#### `TaskAnalogWrite` — prioridad 1
- Si `readActivated == true`, toma el mutex e imprime `a3Value` (0–1023) por serial
- Cada 3 segundos

#### `BlinkFunction` — prioridad 1
- Si `readActivated == true`, parpadea LED en pin 11 (100 ms on)
- Cada 1 segundo

### ISRs (interrupciones)

Ambas siguen el mismo patrón:

```c
void interruptHandlerX() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

- Pin 2 (RISING) → da `activateSemaphore`
- Pin 3 (RISING) → da `deactivateSemaphore`
- Si la tarea desbloqueada tiene mayor prioridad que la interrumpida, se fuerza un cambio de contexto inmediato

---

## Capa 2: Python — FastAPI (`api/tp2_FreeRTOS.py`)

### Endpoints HTTP

| Ruta | Acción |
|---|---|
| `/` | Sirve `index.html` |
| `/valor` | Devuelve JSON con `alarm`, `ldrValue`, `readActivated` |
| `/start` | Envía `"R\n"` por serial al Arduino |
| `/stop` | Envía `"S\n"` por serial al Arduino |

### Hilo lector serial

Un hilo daemon corre en segundo plano leyendo constantemente el puerto serial. Por cada línea recibida:

| Línea recibida | Acción |
|---|---|
| `"A"` | `alarm = True` |
| `"a"` | `alarm = False` |
| `"R"` | `readActivated = True` |
| `"S"` | `readActivated = False` |
| Número (ej. `"512"`) | `ldrValue = int(line)` |

---

## Capa 3: Frontend — HTML/JS (`static/index.html`)

Interfaz visual que muestra:

- **Valor del LDR** (número grande, 0–1023)
- **Estado de lectura** (ACTIVA / DETENIDA) con un punto verde o gris
- **Alarma** (Normal / ALARMA) con animación roja cuando se supera el umbral
- **Botones** Iniciar / Detener que llaman a `/start` y `/stop`

Cada 3 segundos, el navegador consulta `/valor` y actualiza la UI.

---

## Protocolo serial (Arduino ↔ Python)

Cada mensaje es una línea terminada en `\n` (un solo carácter + newline).

| Dirección | Carácter | Significado |
|---|---|---|
| Arduino → Python | `A` | Alarma activada (LDR > 800) |
| Arduino → Python | `a` | Alarma desactivada |
| Bidireccional | `R` | Lectura activada (comando o notificación) |
| Bidireccional | `S` | Lectura desactivada (comando o notificación) |
| Arduino → Python | `0`–`1023` | Valor actual del LDR |

---

## Flujo de datos — ejemplo completo

```
1. Usuario hace clic en "Iniciar lectura"
2. fetch(/start) → Python recibe GET /start
3. Python escribe "R\n" en el puerto serial
4. Arduino TaskAnalogRead lee 'R' del serial
5. readActivated = true
6. TaskAnalogRead lee LDR → a3Value = 750 (< 800, sin alarma)
7. TaskAnalogWrite envía "750\n" por serial cada 3s
8. Python hilo lector recibe "750" → ldrValue = 750
9. Browser consulta GET /valor cada 3s → recibe {"ldrValue": 750, ...}
10. UI muestra "750"

--- Si LDR supera 800 ---

11. TaskAnalogRead detecta a3Value > 800
12. alarmActivated = true, envía "A\n" por serial
13. LED alarma (pin 12) comienza a parpadear
14. Python recibe "A" → alarm = True
15. Browser recibe {"alarm": true} → muestra ALARMA en rojo

--- Si se presiona botón físico DESACTIVAR (pin 3) ---

16. ISR pin 3 → deactivateSemaphore
17. TaskControlRead toma el semáforo
18. readActivated = false, envía "S\n" por serial
19. TaskAnalogRead apaga alarma, envía "a\n" por serial
20. Python recibe "S" y "a" → readActivated = False, alarm = False
21. Browser actualiza: DETENIDA, Normal
```

---

## Requisitos

- **Hardware**: Arduino compatible (Uno, Nano, Mega) con:
  - LDR en A3
  - 2 pulsadores (pull-down o pull-up externo) en pines 2 y 3
  - 2 LEDs (con resistencias) en pines 11 y 12
- **Software**:
  - Python 3 con `fastapi`, `uvicorn` y `pyserial`
  - Arduino IDE (para compilar y subir `tp2.ino`)

## Cómo ejecutar

```bash
# 1. Subir tp2.ino al Arduino desde Arduino IDE

# 2. Instalar dependencias Python
cd tp2/api
pip install -r requirements.txt

# 3. Verificar puerto COM en tp2_FreeRTOS.py (línea 20)
#    Ajustar 'COM5' al puerto correspondiente

# 4. Ejecutar servidor
python tp2_FreeRTOS.py

# 5. Abrir navegador en http://localhost:3000
```
