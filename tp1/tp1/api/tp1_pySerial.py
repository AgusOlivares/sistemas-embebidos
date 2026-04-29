from fastapi import FastAPI, Form, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse
from fastapi.middleware.cors import CORSMiddleware
import serial
import time
import uvicorn
from pathlib import Path

# app
app = FastAPI()

# CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# dir
BASE_DIR = Path(__file__).parent
STATIC_DIR = BASE_DIR.parent / "static"

# init
try:
    # port
    ser = serial.Serial(port='COM5',
                        baudrate=9600,
                        bytesize=serial.EIGHTBITS,
                        parity=serial.PARITY_NONE,
                        stopbits=serial.STOPBITS_ONE,
                        timeout=1,
                        xonxoff=False,
                        rtscts=False,
                        dsrdtr=False,
                        inter_byte_timeout=None,
                        exclusive=None)
    # wait
    time.sleep(2)
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    ser = None

# mount
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

# root
@app.get("/", response_class=HTMLResponse)
async def read_root():
    # lee
    index_file = STATIC_DIR / "index.html"
    with open(index_file, "r") as f:
        return f.read()

# led
@app.post("/changeLedValue")
def change_led_value(pin: str = Form(...), valor: str = Form(...)):
    if not ser or not ser.is_open:
        raise HTTPException(status_code=500, detail="Serial port not open")
    try:
        # check
        if pin in ("9", "10", "11"):
            # scale
            value = (int(valor) * 255) // 100
        else:
            value = valor
        # send
        command = f"0,{pin},{value}\n"
        ser.write(command.encode("utf-8"))
        ser.flush()
        return {"status": "OK"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# read
@app.get("/getLedsValue")
def get_led_values():
    if not ser or not ser.is_open:
        raise HTTPException(status_code=500, detail="Serial port not open")
    try:
        # ask
        ser.write(b"1\n")
        ser.flush()
        # get
        line = ser.readline().decode("utf-8").strip()
        if not line or "," not in line:
            raise HTTPException(status_code=500, detail="Invalid response from Arduino")
        # split
        led9, led10, led11, led13, ldr = line.split(",")
        return {
            "brilloLed9": led9,
            "brilloLed10": led10,
            "brilloLed11": led11,
            "led13": led13,
            "valorLDR": ldr
        }
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# run
if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=3000, reload=False)
