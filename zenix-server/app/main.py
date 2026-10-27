"""Zenix AI Server: OTA + WebSocket cho thiet bi, REST API + trang quan tri."""
import logging
from pathlib import Path

from fastapi import FastAPI, Request
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from . import admin, db, devices, ws

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")

STATIC = Path(__file__).parent / "static"

db.init()
app = FastAPI(title="Zenix AI Server", docs_url=None, redoc_url=None)
app.include_router(devices.router)
app.include_router(ws.router)
app.include_router(admin.router)
app.mount("/static", StaticFiles(directory=STATIC), name="static")


@app.middleware("http")
async def revalidate_static(request: Request, call_next):
    # Trinh duyet luon hoi lai (ETag) de nhan ban moi ngay sau khi trien khai.
    response = await call_next(request)
    if request.url.path.startswith("/static/"):
        response.headers["Cache-Control"] = "no-cache"
    return response


@app.get("/healthz")
def healthz():
    return {"ok": True}


@app.get("/favicon.ico", include_in_schema=False)
def favicon():
    return FileResponse(STATIC / "favicon.ico")


@app.get("/")
def index():
    return FileResponse(STATIC / "index.html", headers={"Cache-Control": "no-store"})
