int trig = 5;
int echo = 4;
int pinValues[14];
void setup() {
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(11, OUTPUT);
    pinMode(13, OUTPUT);
    Serial.begin(9600);
}

void loop() {
  if (Serial.available()) {

    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("0")) {
      
      int firstComma = line.indexOf(',');
      int secondComma = line.indexOf(',', firstComma + 1);

      int pin = line.substring(firstComma + 1, secondComma).toInt();
      int valor = line.substring(secondComma + 1).toInt();

      if (pin == 9 || pin == 10 || pin == 11) {
        analogWrite(pin, valor);
        pinValues[pin] = valor;
      }

      if (pin == 13) {
        digitalWrite(pin, valor == 1 ? HIGH : LOW);
        pinValues[pin] = valor;
      }
    }

    else if (line == "1") {
      Serial.print(pinValues[9]);
      Serial.print(",");
      Serial.print(pinValues[10]);
      Serial.print(",");
      Serial.print(pinValues[11]);
      Serial.print(",");
      Serial.print(pinValues[13]);
      Serial.print(",");
      Serial.println(analogRead(A3));
    }
  }
}