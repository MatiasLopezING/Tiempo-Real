"""
Descarga demanda y temperatura de CAMMESA (demanda-svc) para Capital Federal
(id_region=425) entre el 01/02/2026 y el 01/05/2026, y consolida todo en un
único CSV horario. Reintenta automáticamente, cachea cada día en disco, y
saltea/reintenta días con respuesta inválida sin abortar el resto.
"""

import json
import time
import csv
import requests
from datetime import date, timedelta
from pathlib import Path

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
BASE_DIR = Path(__file__).resolve().parent          # todo relativo al script, no al cwd
ID_REGION = 425                                      # Capital Federal
DESDE = date(2026, 2, 1)
HASTA = date(2026, 5, 1)
CACHE_DIR = BASE_DIR / "data" / "capital_raw"
SALIDA = BASE_DIR / "capital_demanda_feb_may_2026.csv"
URL = "https://api.cammesa.com/demanda-svc/demanda/ObtieneDemandaYTemperaturaRegionByFecha"


# ---------------------------------------------------------------------------
# Descarga
# ---------------------------------------------------------------------------
def punto_valido(punto: dict) -> bool:
    """Filtra objetos placeholder/malformados (fecha vacía, sin dem ni temp)."""
    f = punto.get("fecha")
    if not isinstance(f, str) or not f:
        return False
    return "dem" in punto or "temp" in punto


def descargar_dia(fecha: date, intentos=4):
    for intento in range(1, intentos + 1):
        try:
            r = requests.get(
                URL,
                params={"fecha": fecha.isoformat(), "id_region": ID_REGION},
                timeout=20,
            )
            r.raise_for_status()
            data = r.json()
            validos = [p for p in data if punto_valido(p)]
            if not validos:
                print(f"  {fecha}: respuesta sin puntos válidos (intento {intento}/{intentos})")
                time.sleep(2 * intento)
                continue
            return validos
        except requests.RequestException as e:
            print(f"  {fecha}: intento {intento}/{intentos} falló ({e})")
            time.sleep(2 * intento)
    print(f"  {fecha}: SIN DATOS tras {intentos} intentos, se deja constancia y se sigue")
    return []


def bajar_rango(desde: date, hasta: date):
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    dia = desde
    while dia <= hasta:
        cache = CACHE_DIR / f"{dia.isoformat()}.json"
        necesita_descarga = True
        if cache.exists():
            try:
                existente = json.loads(cache.read_text(encoding="utf-8"))
                if existente:  # cache no vacío -> ya está bien, no volver a pedir
                    necesita_descarga = False
            except json.JSONDecodeError:
                pass  # cache corrupto, se vuelve a bajar
        if necesita_descarga:
            data = descargar_dia(dia)
            cache.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
            print(dia, len(data), "puntos válidos")
            time.sleep(0.3)
        dia += timedelta(days=1)


# ---------------------------------------------------------------------------
# Consolidación
# ---------------------------------------------------------------------------
def consolidar(desde: date, hasta: date):
    valores = {}          # fecha_str -> {"dem": x, "temp": y}
    dias_vacios = []
    dia = desde
    while dia <= hasta:
        cache = CACHE_DIR / f"{dia.isoformat()}.json"
        data = json.loads(cache.read_text(encoding="utf-8")) if cache.exists() else []
        if not data:
            dias_vacios.append(dia)
        for punto in data:
            f = punto["fecha"]
            fila = valores.setdefault(f, {})
            if "dem" in punto:
                fila["dem"] = punto["dem"]
            if "temp" in punto:
                fila["temp"] = punto["temp"]
        dia += timedelta(days=1)

    filas = sorted(valores.items(), key=lambda kv: kv[0])
    with open(SALIDA, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["fecha", "dem_mw", "temp_c", "id_region"])
        for f, vals in filas:
            w.writerow([f, vals.get("dem", ""), vals.get("temp", ""), ID_REGION])

    print(f"\nconsolidado: {len(filas)} timestamps -> {SALIDA}")
    if dias_vacios:
        print(f"ATENCIÓN: {len(dias_vacios)} día(s) quedaron sin ningún dato:")
        for d in dias_vacios:
            print("  -", d)
    else:
        print("sin huecos: los", (hasta - desde).days + 1, "días tienen datos")


if __name__ == "__main__":
    print(f"bajando {ID_REGION} desde {DESDE} hasta {HASTA} ...")
    bajar_rango(DESDE, HASTA)
    consolidar(DESDE, HASTA)