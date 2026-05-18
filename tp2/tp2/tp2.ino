#include <Arduino_FreeRTOS.h>
#include <semphr.h>

// Configuracion de pines
constexpr int BOTON_ACTIVAR     = 2;
constexpr int BOTON_DESACTIVAR  = 3;
constexpr int LED_ALARMA        = 12;
constexpr int LED_INDICADOR     = 11;
constexpr int PIN_LDR           = A3;
constexpr int UMBRAL_ALARMA     = 800;

SemaphoreHandle_t mutex;
SemaphoreHandle_t activateSemaphore;   // notifica desde ISR BOTON_ACTIVAR a TaskControlRead
SemaphoreHandle_t deactivateSemaphore; // notifica desde ISR BOTON_DESACTIVAR a TaskControlRead

volatile bool readActivated = true; // volatile porque la modifican ISRs y tareas
int a3Value;                        // protegido con mutex

void TaskAnalogRead(void *pvParameters);
void TaskAnalogWrite(void *pvParameters);
void TaskControlRead(void *pvParameters); 
void BlinkFunction(void *pvParameters);


void setup() {
  Serial.begin(9600);

  pinMode(PIN_LDR, INPUT);
  pinMode(LED_ALARMA, OUTPUT);
  pinMode(LED_INDICADOR, OUTPUT);

  mutex = xSemaphoreCreateMutex();
  activateSemaphore   = xSemaphoreCreateBinary();
  deactivateSemaphore = xSemaphoreCreateBinary();

  xTaskCreate(TaskAnalogRead,   "AnalogRead",   128, NULL, 1, NULL);
  xTaskCreate(TaskAnalogWrite,  "AnalogWrite",  128, NULL, 1, NULL);
  xTaskCreate(TaskControlRead,  "ControlRead",  128, NULL, 2, NULL); // prioridad alta
  xTaskCreate(BlinkFunction,  "Blink",  128, NULL, 1, NULL); 
  attachInterrupt(digitalPinToInterrupt(BOTON_ACTIVAR),   interruptHandlerActivate,   RISING);
  attachInterrupt(digitalPinToInterrupt(BOTON_DESACTIVAR), interruptHandlerDeactivate, RISING);
}

void loop() {}

// ISR da semaforo a TaskControlRead; si desbloquea tarea de mayor prioridad, yield
void interruptHandlerActivate() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(activateSemaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR(); 
  }
}

void interruptHandlerDeactivate() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(deactivateSemaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR(); 
  }
}

// Tarea de control: recibe notificaciones de las ISRs y cambia readActivated
void TaskControlRead(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (xSemaphoreTake(activateSemaphore, 30) == pdPASS) {
      readActivated = true;
      Serial.println("R"); // notifica al exterior: Reading ON
    }

    if (xSemaphoreTake(deactivateSemaphore, 30) == pdPASS) {
      readActivated = false;
      Serial.println("S"); // notifica al exterior: Stop reading
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Tarea de lectura: lee PIN_LDR, controla alarma, y acepta comandos seriales
void TaskAnalogRead(void *pvParameters) {
  (void) pvParameters;
  bool alarmActivated = false;
  for (;;) {
    if (Serial.available()) {
      char c = Serial.read(); // lee un solo byte, sin String
      if (c == 'R') {
        readActivated = true;
      }
      else if (c == 'S') {
        readActivated = false;
      }
    }

    if (readActivated) {
      if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) { // espera si otra tarea usa a3Value
        a3Value = analogRead(PIN_LDR);
        if (a3Value > UMBRAL_ALARMA) {
          alarmActivated = true;
          Serial.println("A"); // alarma activada
        } else if (a3Value <= UMBRAL_ALARMA and (alarmActivated == true)) {
          alarmActivated = false; 
          Serial.println("a"); // alarma desactivada
        }
        
        
        xSemaphoreGive(mutex);
      }
    }
    else {
      if (alarmActivated == true) {
        alarmActivated = false; 
        Serial.println("a"); // alarma desactivada
      }
    }
    if(alarmActivated) {
      digitalWrite(LED_ALARMA, HIGH);
      vTaskDelay(pdMS_TO_TICKS(100));
      digitalWrite(LED_ALARMA, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Tarea de escritura: imprime a3Value cada 3s si el sistema esta activo
void TaskAnalogWrite(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (readActivated) {
      if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
        Serial.println(a3Value); // valor positivo (0-1023)
        xSemaphoreGive(mutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}


// Tarea LED: parpadea LED_INDICADOR cada 1s indicando que el sistema esta activo
void BlinkFunction(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (readActivated) {
      digitalWrite(LED_INDICADOR, HIGH);
      vTaskDelay(pdMS_TO_TICKS(100));
      digitalWrite(LED_INDICADOR, LOW);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
