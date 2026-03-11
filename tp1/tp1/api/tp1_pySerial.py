from fastapi import FastAPI
from fastapi import HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse
import serial


app = FastAPI()

try:
    ser = serial.Serial(port='/dev/ttyACM0',
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
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")



app.mount("/static", StaticFiles(directory="static"), name="static")
async def read_html():
    with open("static/index.html", "r") as f:
        html_content = f.read()
    return HTMLResponse(content=html_content, status_code=200)


@app.get("/read_serial")
def read_serial():

    if ser.is_open:
        try:
            data = ser.readline().decode('utf-8').rstrip()
            return {"data": data}
        except Exception as e:
            return {"error": str(e)}
    else:
        return {"error": "Serial port is not open"}
    
@app.post("/write_serial")
def write_serial(message: str):
    if ser.is_open:
        try:
            ser.write(message.encode('utf-8'))
            return {"status": "Message sent"}
        except Exception as e:
            return {"error": str(e)}
    else:
        return {"error": "Serial port is not open"}