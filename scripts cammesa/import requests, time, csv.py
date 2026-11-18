import json, time, csv, requests
from datetime import date, timedelta
from pathlib import Path

ID_REGION = 1943  # EDELAP / La Plata
DESDE = date(2026, 2, 1)
HASTA = date(2026, 5, 1)
CACHE_DIR = Path("data/laplata_raw")
SALIDA = Path("laplata_demanda_feb_may_2026.csv")

URL = "https://api.cammesa.com/demanda-svc/demanda/ObtieneDemandaYTemperaturaRegionByFecha"


def descargar_dia(fecha: date, intentos=3):
    for intento in range(1, intentos + 1):
        try:
            r = requests.get(URL, params={"fecha": fecha.isoformat(), "id_region": ID_REGION}, timeout=15)
            r.raise_for_status()
            return r.json()
        except requests.RequestException as e:
            print(f"  {fecha} intento {intento}/{intentos} falló: {e}")
            time.sleep(2 * intento)
    raise RuntimeError(f"no se pudo bajar {fecha} tras {intentos} intentos")


def bajar_rango(desde: date, hasta: date):
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    dia = desde
    while dia <= hasta:
        cache = CACHE_DIR / f"{dia.isoformat()}.json"
        if not cache.exists():
            data = descargar_dia(dia)
            cache.write_text(json.dumps(data), encoding="utf-8")
            print(dia, len(data), "puntos")
            time.sleep(0.3)
        dia += timedelta(days=1)


def consolidar(desde: date, hasta: date, salida: Path):
    vistos = set()
    filas = []
    dia = desde
    while dia <= hasta:
        cache = CACHE_DIR / f"{dia.isoformat()}.json"
        data = json.loads(cache.read_text(encoding="utf-8"))
        for punto in data:
            f = punto["fecha"]
            if f in vistos:          # cada día trae el 00:00:00 del día siguiente,
                continue              # que se repite como primer punto del próximo día
            vistos.add(f)
            filas.append((f, punto["dem"]))
        dia += timedelta(days=1)
    filas.sort(key=lambda x: x[0])
    with open(salida, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["fecha", "dem_mw", "id_region"])
        w.writerows((f, dem, ID_REGION) for f, dem in filas)
    print(f"consolidado: {len(filas)} filas -> {salida}")


if __name__ == "__main__":
    bajar_rango(DESDE, HASTA)
    consolidar(DESDE, HASTA, SALIDA)