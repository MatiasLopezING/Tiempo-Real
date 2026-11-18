#!/usr/bin/env python3
"""
Genera el mapa de fondo (PNG cuadrado) alineado con el mundo de NetLogo.

NetLogo ubica los nodos con la misma cuenta que este script: toma un recuadro
(bounding box) de nodes_netlogo.csv, le agrega 10 % de margen, lo vuelve CUADRADO y lo
mapea a todo el mundo. Este script descarga el mapa de exactamente ese recuadro
(en UTM 21S, EPSG:32721) y lo guarda como imagen cuadrada.

Uso (desde la carpeta de nodes_netlogo.csv):
    python make_basemap.py                      # ambos modos, calles Esri (por defecto)
    python make_basemap.py --source esri-gray   # fondo gris claro (mejor contraste)
    python make_basemap.py --source esri        # satelital
    python make_basemap.py --source osm --user-agent "tpfinal-redAT/1.0 (tu@mail.com)"
    python make_basemap.py --modes ciudad --px 1204

OSM bloquea (403) a las aplicaciones que no se identifican: por eso OSM exige
--user-agent propio. Esri no lo necesita.

Salida: basemap_ciudad.png y basemap_completa.png (copiarlos junto al .nlogox).

Dependencias: pip install contextily matplotlib pandas numpy
Requiere internet (descarga teselas). Respetá los términos de uso del proveedor y
conservá la atribución que contextily dibuja en la esquina de la imagen.
"""
import argparse
import os

import matplotlib
import pandas as pd

matplotlib.use("Agg")
import contextily as cx  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402

CRS = "EPSG:32721"  # el mismo CRS métrico en el que se calcularon x, y


def bbox_cuadrado(nodes, modo):
    """Réplica exacta de `ubicar-nodos` de NetLogo."""
    ref = nodes[nodes.kind == "AT_AT"] if modo == "ciudad" else nodes
    xmin, xmax = ref.x.min(), ref.x.max()
    ymin, ymax = ref.y.min(), ref.y.max()
    margen = 0.1 * max(xmax - xmin, ymax - ymin)
    xmin, xmax, ymin, ymax = xmin - margen, xmax + margen, ymin - margen, ymax + margen
    lado = max(xmax - xmin, ymax - ymin)
    xc, yc = (xmin + xmax) / 2, (ymin + ymax) / 2
    return xc - lado / 2, xc + lado / 2, yc - lado / 2, yc + lado / 2


def get_source(name):
    n = name.lower()
    if n == "osm":
        return cx.providers.OpenStreetMap.Mapnik
    if n == "esri-street":
        return cx.providers.Esri.WorldStreetMap
    if n == "esri":
        return cx.providers.Esri.WorldImagery
    if n == "esri-gray":
        return cx.providers.Esri.WorldGrayCanvas
    if os.path.exists(name):
        return name  # raster local georreferenciado
    raise SystemExit(f"Fuente desconocida: {name} (usar esri-street, esri-gray, esri, osm o un archivo)")


def render(nodes, modo, out, source, px, zoom, headers=None):
    x0, x1, y0, y1 = bbox_cuadrado(nodes, modo)
    dpi = 100
    fig = plt.figure(figsize=(px / dpi, px / dpi), dpi=dpi)
    ax = fig.add_axes([0, 0, 1, 1])  # sin márgenes: la imagen ES el recuadro
    ax.set_axis_off()
    ax.set_xlim(x0, x1)
    ax.set_ylim(y0, y1)
    ax.set_aspect("equal", adjustable="box")
    cx.add_basemap(ax, crs=CRS, source=source, headers=headers,
                   zoom="auto" if zoom is None else zoom, attribution_size=7)
    ax.set_xlim(x0, x1)  # por si add_basemap tocó los límites
    ax.set_ylim(y0, y1)
    fig.savefig(out, dpi=dpi)  # sin bbox_inches: tamaño exacto px x px
    plt.close(fig)
    print(f"{modo}: {out}  ({px}x{px} px, lado = {(x1 - x0) / 1000:.1f} km, "
          f"{(x1 - x0) / px:.1f} m/px)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nodes", default="nodes_netlogo.csv")
    ap.add_argument("--modes", nargs="+", default=["ciudad", "completa"],
                    choices=["ciudad", "completa"])
    ap.add_argument("--source", default="esri-street")
    ap.add_argument("--user-agent", default=None,
                    help="obligatorio con --source osm: nombre de la app y un contacto")
    ap.add_argument("--px", type=int, default=1204, help="lado de la imagen en píxeles")
    ap.add_argument("--zoom", type=int, default=None, help="zoom de teselas (por defecto auto)")
    ap.add_argument("--outdir", default=".")
    a = ap.parse_args()

    nodes = pd.read_csv(a.nodes)
    src = get_source(a.source)
    headers = None
    if a.source.lower() == "osm":
        if not a.user_agent:
            raise SystemExit("OSM bloquea a las apps sin identificar: usá --user-agent "
                             '"nombre/1.0 (tu@mail.com)", o elegí --source esri-street.')
        headers = {"User-Agent": a.user_agent}
    os.makedirs(a.outdir, exist_ok=True)
    for modo in a.modes:
        render(nodes, modo, os.path.join(a.outdir, f"basemap_{modo}.png"), src, a.px, a.zoom, headers)


if __name__ == "__main__":
    main()
