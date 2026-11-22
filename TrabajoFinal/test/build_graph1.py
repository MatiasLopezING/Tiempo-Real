#!/usr/bin/env python3
"""
Shapefile de líneas AT (Edelap) -> grafo (nodes.csv / edges.csv) para NetLogo.

Uso (automático, desde la carpeta con los 3 shapefiles):
    python build_graph.py

Uso (manual):
    python build_graph.py LINEAS.shp [--out DIR] [--snap 2]
                          [--subs-at-at AT_AT.shp] [--subs-at-mt AT_MT.shp]
                          [--sub-radius 60] [--sub-leaf-radius 250]
                          [--leaf-radius 100]   # sólo se usa si NO hay subestaciones

Pasos:
 1. Limpieza: descarta tramos con TENSION==0, sin MATRICULA o de longitud < 1 m.
 2. Snap de extremos a --snap metros (conectividad por extremos coincidentes).
 3. Subestaciones (--subs-at-at / --subs-at-mt): cada nodo de línea se asigna a la
    subestación más cercana si dista <= --sub-radius m (cualquier grado: cubre
    subestaciones "en línea", con entrada y salida) o, si es extremo libre (grado 1),
    <= --sub-leaf-radius m. Sin subestaciones: se agrupan extremos libres a
    <= --leaf-radius m entre sí (HIPÓTESIS).
 4. Contracción de nodos de grado 2 (torres/postes) que no cambian tensión.
 5. Exporta nodes.csv, edges.csv y grafo.png (control visual).

Dependencias: geopandas, pyogrio, networkx, scipy, numpy, pandas, matplotlib
"""
import argparse
import glob
import os
from collections import Counter

import geopandas as gpd
import matplotlib
import networkx as nx
import numpy as np
import pandas as pd
from scipy.spatial import cKDTree

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

CRS_M = 32721  # UTM 21S; cualquier CRS métrico sirve para La Plata


def find_one(pattern, what):
    """Busca exactamente un .shp en la carpeta actual que cumpla el patrón."""
    hits = sorted(h for h in glob.glob(pattern) if h.lower().endswith(".shp"))
    if len(hits) != 1:
        raise SystemExit(f"No pude identificar de forma única la capa {what} "
                         f"(patrón '{pattern}', encontré: {hits}). "
                         f"Indicá el archivo a mano.")
    print(f"capa {what}: {hits[0]}")
    return hits[0]


def resolve(path):
    """Si el archivo no existe (p. ej. tilde corrupta en el nombre), busca por patrón:
    cada carácter no ASCII/raro se reemplaza por '?'."""
    if os.path.exists(path):
        return path
    pat = "".join(ch if (ch.isascii() and (ch.isalnum() or ch in "._-/\\")) else "?"
                  for ch in path)
    hits = glob.glob(pat)
    if len(hits) == 1:
        print(f"'{path}' no existe; uso '{hits[0]}'")
        return hits[0]
    raise SystemExit(f"No encuentro '{path}' (ni por patrón '{pat}'). "
                     f"Corré 'dir *.shp' y verificá el nombre y la carpeta actual.")


def cluster_points(xy, radius):
    """Etiqueta de componente conexa del grafo de proximidad (distancia <= radius)."""
    n = len(xy)
    if n == 0:
        return np.array([], dtype=int)
    G = nx.Graph()
    G.add_nodes_from(range(n))
    G.add_edges_from(cKDTree(xy).query_pairs(radius))
    lab = np.empty(n, dtype=int)
    for k, comp in enumerate(nx.connected_components(G)):
        for i in comp:
            lab[i] = k
    return lab


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lines", nargs="?", default=None,
                    help="shapefile de líneas AT; si se omite, se autodetectan los 3 en la carpeta actual")
    ap.add_argument("--out", default="salida")
    ap.add_argument("--snap", type=float, default=2.0)
    ap.add_argument("--leaf-radius", type=float, default=100.0)
    ap.add_argument("--subs-at-at", default=None)
    ap.add_argument("--subs-at-mt", default=None)
    ap.add_argument("--sub-radius", type=float, default=60.0)
    ap.add_argument("--sub-leaf-radius", type=float, default=250.0)
    a = ap.parse_args()
    if a.lines is None:  # modo automático: busca las 3 capas en la carpeta actual
        a.lines = find_one("at-l*neas*.shp", "de líneas AT")
        a.subs_at_at = a.subs_at_at or find_one("at-at-subestaciones*.shp", "AT/AT")
        a.subs_at_mt = a.subs_at_mt or find_one("at-mt-subestaciones*.shp", "AT/MT")
    else:
        a.lines = resolve(a.lines)
        if a.subs_at_at:
            a.subs_at_at = resolve(a.subs_at_at)
        if a.subs_at_mt:
            a.subs_at_mt = resolve(a.subs_at_mt)
    os.makedirs(a.out, exist_ok=True)

    # ---- 1. carga y limpieza
    g = gpd.read_file(a.lines).to_crs(CRS_M).reset_index(drop=True)
    n0 = len(g)
    g["len_m"] = g.length
    bad = g.TENSION.fillna(0).eq(0) | g.MATRICULA.isna() | (g.len_m < 1.0)
    print(f"tramos: {n0}, descartados por limpieza: {int(bad.sum())}")
    g = g[~bad].reset_index(drop=True)

    # ---- 2. extremos y snap
    ends = []
    for geom in g.geometry:
        c = list(geom.coords)
        ends += [c[0], c[-1]]
    xy = np.array(ends)
    lab = cluster_points(xy, a.snap)
    node_xy = pd.DataFrame(xy, columns=["x", "y"]).assign(n=lab).groupby("n").mean()

    H = nx.MultiGraph()
    for i, r in g.iterrows():
        H.add_edge(lab[2 * i], lab[2 * i + 1], seg=i)
    H.remove_edges_from([(u, v, k) for u, v, k in list(H.edges(keys=True)) if u == v])

    role = {n: "tower" for n in H}
    name = {}

    # ---- 3. subestaciones / terminales
    leaves = [n for n in H if H.degree(n) == 1]
    sub_attrs = {}
    if a.subs_at_at or a.subs_at_mt:
        parts = []
        if a.subs_at_at:
            t = gpd.read_file(a.subs_at_at).to_crs(CRS_M)
            parts.append(pd.DataFrame({
                "sub_type": "AT/AT",
                "sub_name": t["CLASS_MEMB"].astype("Int64").astype(str),
                "clientes": np.nan, "pot_instalada": np.nan, "acceso": "",
                "x": t.geometry.x, "y": t.geometry.y}))
        if a.subs_at_mt:
            t = gpd.read_file(a.subs_at_mt).to_crs(CRS_M)
            parts.append(pd.DataFrame({
                "sub_type": "AT/MT",
                "sub_name": t["MATRICULA"].astype(str),
                "clientes": t["CLIENTES"], "pot_instalada": t["POTINSTALA"],
                "acceso": t["ACCESO"].fillna(""),
                "x": t.geometry.x, "y": t.geometry.y}))
        subs = pd.concat(parts, ignore_index=True)
        tree = cKDTree(subs[["x", "y"]].values)
        ids = list(H.nodes)
        d, j = tree.query(node_xy.loc[ids].values)
        remap = {}
        for n, dd, jj in zip(ids, d, j):
            lim = a.sub_leaf_radius if H.degree(n) == 1 else a.sub_radius
            if dd <= lim:
                remap[n] = f"SUB{jj}"
        for sid in sorted(set(remap.values())):
            k = int(sid[3:])
            H.add_node(sid)
            role[sid] = "substation"
            node_xy.loc[sid] = subs.loc[k, ["x", "y"]].values.astype(float)
            sub_attrs[sid] = subs.loc[k, ["sub_type", "sub_name", "clientes",
                                         "pot_instalada", "acceso"]].to_dict()
        sin = [f"{r.sub_type} {r.sub_name}" for k, r in subs.iterrows()
               if f"SUB{k}" not in set(remap.values())]
        if sin:
            print("AVISO: subestaciones sin línea asociada:", sin)
        n_leaf_un = sum(1 for n in leaves if n not in remap)
        print(f"nodos de línea asignados a subestaciones: {len(remap)}; "
              f"extremos libres sin subestación (dead_end): {n_leaf_un}")
    else:
        remap = {}
        if leaves:
            lp = node_xy.loc[leaves].values
            ll = cluster_points(lp, a.leaf_radius)
            cnt = Counter(ll)
            for n, k in zip(leaves, ll):
                if cnt[k] > 1:  # sólo se fusionan grupos de >= 2 extremos
                    remap[n] = f"T{k}"
            for t in set(remap.values()):
                members = [n for n, v in remap.items() if v == t]
                node_xy.loc[t] = node_xy.loc[members].mean()
                role[t] = "terminal_cluster"
                H.add_node(t)
        print(f"extremos libres: {len(leaves)}, agrupados: {len(remap)}")

    # reconstruir multigrafo con remapeo (guardando atributos de tramo en cada arista)
    H2 = nx.MultiGraph()
    dropped_loops = []
    for u, v, d in H.edges(data=True):
        uu, vv = remap.get(u, u), remap.get(v, v)
        if uu != vv:  # se descartan lazos creados por el agrupamiento
            H2.add_edge(uu, vv, seg=d["seg"])
        else:
            dropped_loops.append(d["seg"])
    print(f"tramos internos a una misma subestación (descartados): {len(dropped_loops)}")
    for n in H2:
        role.setdefault(n, "tower")
    for n in H2:
        if role[n] == "tower" and H2.degree(n) >= 3:
            role[n] = "junction"
        elif role[n] == "tower" and H2.degree(n) == 1:
            role[n] = "dead_end"

    # ---- 4. contracción de cadenas de grado 2
    def volt(seg):
        return g.TENSION[seg]

    keep = set()
    for n in H2:
        if role[n] in ("substation", "terminal_cluster"):
            keep.add(n)
        elif H2.degree(n) != 2:
            keep.add(n)
        else:
            (e1, e2) = [d["seg"] for _, _, d in H2.edges(n, data=True)]
            if volt(e1) != volt(e2):  # cambio de tensión => transformador, se conserva
                keep.add(n)
                role[n] = "transformer_or_change"
    # componentes sin ningún nodo "keep" (anillos puros): se conserva un nodo
    for comp in nx.connected_components(H2):
        if not (comp & keep):
            keep.add(next(iter(comp)))

    edges_out = []
    visited = set()
    for s in keep:
        for _, nb, k, d in H2.edges(s, keys=True, data=True):
            if (s, nb, k, d["seg"]) in visited:
                continue
            chain = [d["seg"]]
            prev, cur = s, nb
            ek = (s, nb, k)
            while cur not in keep:
                nxt = [(x, y, kk, dd) for x, y, kk, dd in H2.edges(cur, keys=True, data=True)
                       if dd["seg"] != chain[-1]]
                _, y, kk, dd = nxt[0]
                chain.append(dd["seg"])
                prev, cur = cur, y
            for sg in chain:
                visited.add((s, cur, 0, sg))
            visited.add((s, nb, k, d["seg"]))
            edges_out.append((s, cur, chain))
    # eliminar duplicados (cada cadena se recorre desde ambos extremos)
    uniq = {}
    for s, e, ch in edges_out:
        uniq[frozenset(ch)] = (s, e, ch)

    rows = []
    for ch_set, (s, e, ch) in uniq.items():
        sub = g.loc[ch]
        tipos = sorted(sub.TIPOTRAMO.dropna().unique())
        rows.append(dict(
            from_id=s, to_id=e,
            length_m=round(float(sub.len_m.sum()), 1),
            n_spans=len(ch),
            voltage_kv=float(sub.TENSION.max()) / 1000,
            tipo="+".join(tipos),
            seccion=sub.SECCION.mode().iat[0] if sub.SECCION.notna().any() else "",
            propiedad="+".join(sorted(sub.PROPIEDAD.dropna().unique())),
            matriculas=";".join(sub.MATRICULA.astype(str)),
        ))
    E = pd.DataFrame(rows)

    used = set(E.from_id) | set(E.to_id)
    N = node_xy.loc[sorted(used, key=str)].copy()
    N.index.name = "id"
    N = N.reset_index()
    N["role"] = N["id"].map(role)
    ptsll = gpd.GeoSeries(gpd.points_from_xy(N.x, N.y), crs=CRS_M).to_crs(4326)
    N["lon"], N["lat"] = ptsll.x.round(6), ptsll.y.round(6)
    deg = Counter(list(E.from_id) + list(E.to_id))
    N["degree"] = N["id"].map(deg)
    for col in ["sub_type", "sub_name", "clientes", "pot_instalada", "acceso"]:
        N[col] = N["id"].map(lambda k, c=col: sub_attrs.get(k, {}).get(c, np.nan))
    # IDs enteros consecutivos (más cómodo en NetLogo)
    idmap = {k: i for i, k in enumerate(N["id"])}
    N.insert(0, "nid", N["id"].map(idmap))
    E["from_nid"] = E.from_id.map(idmap)
    E["to_nid"] = E.to_id.map(idmap)
    # salida determinista: sentido from < to (se invierte el orden de matrículas) y orden fijo
    sw = E.from_nid > E.to_nid
    E.loc[sw, ["from_nid", "to_nid"]] = E.loc[sw, ["to_nid", "from_nid"]].values
    E.loc[sw, "matriculas"] = E.loc[sw, "matriculas"].map(lambda m: ";".join(m.split(";")[::-1]))
    E = E.sort_values(["from_nid", "to_nid", "length_m"]).reset_index(drop=True)
    N = N.drop(columns=["id"])
    E = E[["from_nid", "to_nid", "length_m", "n_spans", "voltage_kv", "tipo",
           "seccion", "propiedad", "matriculas"]]
    N.to_csv(os.path.join(a.out, "nodes.csv"), index=False)
    E.to_csv(os.path.join(a.out, "edges.csv"), index=False)

    # ---- resumen
    G = nx.Graph()
    G.add_nodes_from(N.nid)
    G.add_edges_from(zip(E.from_nid, E.to_nid))
    comps = sorted((len(c) for c in nx.connected_components(G)), reverse=True)
    print(f"nodos: {len(N)}  aristas: {len(E)}  componentes: {len(comps)}  top: {comps[:8]}")
    print(N.role.value_counts().to_string())
    print(f"longitud total: {E.length_m.sum()/1000:.1f} km")

    # ---- plot de control
    fig, ax = plt.subplots(figsize=(11, 11))
    for gm_row in g.itertuples():
        x, y = gm_row.geometry.xy
        ax.plot(x, y, lw=0.8, color="#d62728" if gm_row.TENSION >= 220000 else "#1f77b4")
    colors = {"substation": "k", "terminal_cluster": "m", "junction": "g",
              "dead_end": "orange", "transformer_or_change": "c", "tower": "gray"}
    for r, c in colors.items():
        s = N[N.role == r]
        ax.scatter(s.x, s.y, s=22 if r != "tower" else 3, c=c, label=r, zorder=3)
    ax.set_aspect("equal")
    ax.legend(loc="upper left", fontsize=8)
    ax.set_title("Líneas AT: azul 132 kV, rojo 220 kV")
    fig.savefig(os.path.join(a.out, "grafo.png"), dpi=110, bbox_inches="tight")


if __name__ == "__main__":
    main()
