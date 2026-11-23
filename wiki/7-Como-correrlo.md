# 7 · Cómo correrlo

## El análisis de datos

```bash
cd TrabajoFinal/analisis
pip install -r requirements.txt     # pandas, numpy, matplotlib
jupyter notebook analisis_demanda.ipynb
```

Correr todas las celdas. Son 59 y tardan **unos 30 segundos**, casi todo en la
celda del BIC, que prueba combinaciones de quiebres sobre 25.625 muestras. No
está colgado.

Al terminar deja en `data/`:

- `demanda_limpio.csv` — la serie sin las 295 muestras descartadas
- `perfil_base_horario.csv`, `curva_temperatura.csv`,
  `participacion_subestaciones.csv` — los insumos del modelo

y en `figuras/` los gráficos de control.

> `demanda_limpio.csv` y `figuras/` están en el `.gitignore` de esa carpeta:
> son salidas, se regeneran.

## El modelo

1. Abrir **`TrabajoFinal/Archivo Principal/laplata_at.nlogox`** con NetLogo 7.
   El `.nls` y los CSV tienen que estar en la misma carpeta; el modelo lee el
   perfil horario desde `../analisis/data/`.
2. **`setup-ciudad`** — carga la red. En el Command Center aparece el resumen
   de nodos, enlaces y de qué AT/AT cuelga cada AT/MT.
3. **`usar-dia-real`** — carga las 288 temperaturas del 03/02/2026, el día más
   caluroso del período.
4. **`go`** — corre. Un tick son 5 minutos; un día son 288 ticks.

### El experimento que vale la pena

```
escala = 0,50   ·   delta-temperatura = 0    →  MT095 llega a 92,7 %, no corta
escala = 0,50   ·   delta-temperatura = +3   →  MT095 pasa de 124 %, corta 3 veces
```

Esa es la conclusión del trabajo: hoy la red aguanta el peor día del verano,
con tres grados más deja de aguantar.

### Modo manual

`usar-temperatura-manual` vuelve al deslizador, para explorar a mano. Útil para
buscar el umbral exacto en que cada nodo entra en alerta.

## Reconstruir el grafo desde los shapefiles

Solo hace falta si se cambian las capas de origen.

```bash
cd TrabajoFinal/test
python build_graph.py        # -> nodes_netlogo.csv, edges_netlogo.csv
python make_basemap.py       # -> basemap_ciudad.png, basemap_completa.png
```

Después copiar los cuatro archivos a `TrabajoFinal/Archivo Principal/`.

## Volver a bajar los datos de CAMMESA

```bash
cd TrabajoFinal/Datasets
python "script La Plata.py"
```

Cachea un JSON por día en `data/laplata_raw/`, así que una segunda corrida no
vuelve a pegarle a la API.

> La columna de temperatura **no la produce ese script**. Ver
> [2 · Datos y limpieza](2-Datos-y-limpieza).

---

## Trabajar en equipo sobre el modelo

El `.nlogox` guarda todo su código dentro de una sola línea de XML. **Git no lo
puede mergear**: si dos personas lo editan a la vez, el merge es un conflicto
de 50 KB en una línea y alguien pierde todo.

Reglas prácticas:

- El `.nlogox` lo toca **una persona por vez**. Avisar antes.
- Todo lo que se pueda escribir en `reglas_energia.nls` va ahí: es texto plano
  y sí mergea.
- Si aparece un conflicto en el `.nlogox`, **no** se resuelve a mano: hay que
  quedarse con una de las dos versiones enteras. Un merge mal resuelto deja
  marcadores `<<<<<<<` adentro del código y NetLogo no abre el archivo.
