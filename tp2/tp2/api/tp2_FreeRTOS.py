import threading
import time
import serial
import os

from fastapi import FastAPI
from fastapi.responses import FileResponse
import uvicorn

STATIC_DIR = os.path.join(os.path.dirname(__file__), "..", "static")

app = FastAPI()

# Constantes del protocolo serial
CMD_ALARMA_ON  = "A"
CMD_ALARMA_OFF = "a"
CMD_ACTIVAR    = "R"
CMD_DESACTIVAR = "S"

alarm = False
ldrValue = 0
readActivated = True

ser = serial.Serial(
    port="COM5",
    baudrate=9600,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    timeout=1,
    xonxoff=False,
    rtscts=False,
    dsrdtr=False,
    inter_byte_timeout=None,
    exclusive=None,
)
time.sleep(2)


@app.get("/")
def index():
    return FileResponse(os.path.join(STATIC_DIR, "index.html"))


@app.get("/valor")
def getValue():
    return {
        "alarm": alarm,
        "ldrValue": ldrValue,
        "readActivated": readActivated,
    }


@app.get("/start")
def startReading():
    global readActivated
    readActivated = True
    ser.write((CMD_ACTIVAR + "\n").encode("utf-8"))
    return {"ok": True}


@app.get("/stop")
def stopReading():
    global readActivated
    readActivated = False
    ser.write((CMD_DESACTIVAR + "\n").encode("utf-8"))
    return {"ok": True}


def readSerial():
    global alarm
    global ldrValue
    global readActivated
    while True:
        raw = ser.readline().decode("utf-8").strip()
        if not raw:
            continue
        if raw == CMD_ALARMA_ON:
            alarm = True
        elif raw == CMD_ALARMA_OFF:
            alarm = False
        elif raw == CMD_ACTIVAR:
            readActivated = True
        elif raw == CMD_DESACTIVAR:
            readActivated = False
        else:
            try:
                ldrValue = int(raw)
            except ValueError:
                pass


if __name__ == "__main__":
    thread = threading.Thread(target=readSerial)
    thread.daemon = True
    thread.start()
    uvicorn.run(app, host="0.0.0.0", port=3000)
