from fastapi import FastAPI, HTTPException, Depends, status
import firebase_admin
from firebase_admin import credentials, db
import os
from pydantic import BaseModel
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import APIKeyHeader
import logging

class UpdateRequest(BaseModel):
    path: str  
    value: int  

app = FastAPI()
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  
    allow_credentials=True,
    allow_methods=["*"],  
    allow_headers=["*"],  
)


CRED_PATH = "passkey.json"



DB_URL = "https://iot-hub-project-27607-default-rtdb.firebaseio.com/"


API_KEY_NAME = "X-API-KEY"
API_KEY = os.getenv("API_KEY")
api_key_header = APIKeyHeader(name=API_KEY_NAME, auto_error=False)

async def verify_api_key(api_key: str = Depends(api_key_header)):
    logging.info(f"[validation] Verifying API key")
    if api_key != API_KEY:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid API Key",
        )
    return api_key


if not firebase_admin._apps:
    if os.path.exists(CRED_PATH):
        try:
            cred = credentials.Certificate(CRED_PATH)
            firebase_admin.initialize_app(cred, {
                'databaseURL': DB_URL
            })
            print("Firebase inicializado.")
        except Exception as e:
            print(f" Error al inicializar Firebase: {e}")
            raise
    else:
        raise FileNotFoundError(f" Archivo de credenciales no encontrado: {CRED_PATH}")

@app.get("/data-state")
def read_root():
    try:
        ref = db.reference("/")
        data = ref.get()
        if data is None:
            return {"message": "La base de datos está vacía."}
        return {"stateDoor": data.get("Device1").get("stateDoor")}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error al leer desde Firebase: {e}")

@app.post("/firebase/update",dependencies=[Depends(verify_api_key)])
def update_value(req: UpdateRequest):
    try:
        req.path = "Device1/stateDoor"
        ref = db.reference(req.path)
        ref.set(req.value)
        return {
            "message": f"Se actualizó '{req.path}' con el valor: {req.value}"
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error al actualizar Firebase: {e}")