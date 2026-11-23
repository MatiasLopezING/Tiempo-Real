# 2 · Datos y limpieza

Todo el análisis está en `TrabajoFinal/analisis/analisis_demanda.ipynb`.

## De dónde sale la demanda

Script: `TrabajoFinal/Datasets/script La Plata.py`.

```
https://api.cammesa.com/demanda-svc/demanda/ObtieneDemandaYTemperaturaRegionByFecha
  ?fecha=YYYY-MM-DD&id_region=1943
```

`id_region` 1943 es **EDELAP**: La Plata, Berisso, Ensenada, Magdalena y Punta
Indio. Período 1/2/2026 – 1/5/2026, una muestra cada 5 minutos.

**25.920 filas**, sin nulos, sin fechas duplicadas, con el paso de 5 minutos
respetado en las 25.919 diferencias.

## De dónde sale la temperatura

La API de CAMMESA devuelve demanda, no temperatura. La columna `temperatura_c`
se agregó cruzando esa serie con datos del **NSRDB** (*National Solar Radiation
Database*, del NREL), que publica variables meteorológicas derivadas de
satélite para las Américas.

El cruce no tocó la demanda: `demanda_la_plata_2026_con_temperatura.csv` es
idéntico fila por fila a `laplata_demanda_feb_may_2026.csv` en `fecha` y
`dem_mw`. El merge solo agregó una columna.

### Las dos series no tienen el mismo paso

| Serie | Fuente | Paso |
|---|---|---|
| Demanda | CAMMESA | **5 minutos** |
| Temperatura | NSRDB | **1 hora** |

En el archivo cruzado la temperatura aparece una vez por hora, repetida en las
12 muestras de esa hora. Verificado: hay exactamente **un valor distinto por
hora de reloj** en las 2.161 horas del período, y el valor cambia siempre en el
minuto 0.

Importa al leer los resultados: la resolución real de la temperatura es
**horaria**, aunque el archivo tenga una fila cada 5 minutos. Si el modelo
corre con tick de 5 minutos, la temperatura es la misma durante 12 ticks.

Rango: 7,3 – 37,8 °C.

### Por qué la serie no tiene ni un hueco

2.161 horas sin un solo faltante, lo que no se parece a una estación
meteorológica. La explicación está en el origen: la temperatura del NSRDB no la
mide el satélite, se toma del reanálisis **MERRA-2** de la NASA. Un reanálisis
es un modelo que reconstruye el estado de la atmósfera, así que entrega una
grilla completa por construcción.

Es un dato consistente y sin huecos, pero **es una estimación modelada, no una
medición local**. No equivale a la temperatura registrada en el Observatorio de
La Plata.

> ⚠️ **Dos cosas pendientes de documentar.** El script que bajó y cruzó los
> datos no está: `Datasets/archivo.py` existe pero está vacío. Y no quedó
> registrado qué producto del NSRDB se usó ni con qué coordenadas; el NSRDB
> tiene productos de distinta resolución (el v3 de las Américas es de 4 km y
> 30 min; hay uno del hemisferio occidental de 2 km y 10 min). Conviene anotar
> cuál fue antes de la entrega final.
>
> Hay además un error a corregir en la presentación inicial, que atribuye la
> temperatura al **Servicio Meteorológico Nacional**.

## El dato crudo no se puede usar

| | Valor |
|---|---|
| Mínimo | **−1456 MW** |
| Máximo | **1547 MW** |
| Mediana | 352 MW |
| Desvío | 107 MW (65 MW sacando los imposibles) |

## Los criterios de limpieza

Ningún umbral en MW está escrito a mano. Los tres se calculan de la serie.

### a) Atípicos extremos (Tukey)

El criterio de caja y bigotes:

```
RIC = Q3 − Q1
anómalo si   x < Q1 − 3·RIC   o   x > Q3 + 3·RIC
```

Con Q1 = 306, Q3 = 397, RIC = 91, los límites quedan en **[33, 670] MW** y
marcan **180 muestras**.

Dos observaciones:

- Los cuartiles toleran hasta un 25 % de contaminación y acá hay un 1,1 %, así
  que se calculan sobre la serie sucia sin que los imposibles los muevan.
- El criterio de atípicos *leves* (1,5·RIC) cortaría en 534 MW y se llevaría
  **224 muestras legítimas**. El pico real del período es 625 MW. Por eso
  corresponde el extremo, no el leve.

### b) Apartamiento de la mediana móvil

El rango no ve un valor imposible que caiga entre los dos límites. Para eso se
compara cada muestra con la mediana de una ventana centrada:

```
x̃ᵢ = mediana{ xⱼ : i−k ≤ j ≤ i+k }     w = 13, k = 6   (±30 min)
anómalo si   |xᵢ − x̃ᵢ| > δ
```

- **Por qué la mediana y no la media.** La media se deja arrastrar por los
  mismos valores que se quieren detectar: un pico de 1500 MW mueve la media de
  su ventana unos 115 MW y la mediana, nada.
- **Por qué 13.** La temperatura es horaria, o sea 12 muestras, más una para
  que la ventana sea impar y la mediana caiga sobre un dato real. Verificado:
  de 5 a 37 muestras se marca exactamente lo mismo. Con 145 (12 h) la mediana
  se atrasa respecto del ciclo diario y sesga 51 MW en el pico de las 20,
  contra 1,3 MW con 13.
- **De dónde sale δ.** No se elige: los residuos ordenados de mayor a menor dan
  295, 69, 66, 51… Hay un hueco de ×4,3 entre el primero y el segundo. El
  umbral se pone en la media geométrica de ese hueco: **143 MW**. Marca
  **1 muestra** que el rango no veía.

### c) Tipificación dentro de la hora del día

El z global no alcanza porque la demanda tiene un ciclo diario: 250 MW es
normal a las 4 de la mañana e imposible a las 20. Hay que tipificar contra el
grupo al que la muestra pertenece:

```
z⁽ʰ⁾ᵢ = (xᵢ − μ_h(i)) / σ_h(i)      h(i) ∈ {0 … 23}
```

**Por qué 4 sigmas.** Con n = 25.626 muestras, bajo normalidad se esperarían
por puro azar:

| Umbral | Probabilidad | Esperadas |
|---|---|---|
| 3σ | 2,7 × 10⁻³ | 69 |
| 4σ | 6,3 × 10⁻⁵ | **1,6** |
| 5σ | 5,7 × 10⁻⁷ | 0,01 |

Con 3σ habría 69 falsas alarmas y no se podría mirar ninguna. Con 5σ el
criterio se vuelve ciego.

## Lo que encontró cada criterio

| Criterio | Muestras |
|---|---|
| Atípicos extremos | 180 |
| Mediana móvil | 1 |
| Falla de telemetría (hallada con el z por hora) | 114 |
| **Total descartado** | **295** (1,14 %) |

Quedan **25.625 mediciones**, con demanda entre 214 y 625 MW.

## La falla de telemetría del 19–20 de abril

Es el hallazgo metodológico del análisis. Veinticuatro horas seguidas de
valores entre −1456 y +1547 MW. **Ningún filtro automático la agarró**, porque
cuando la basura dura un día entero contamina su propia mediana móvil.

Apareció al tipificar dentro de la hora del día: el 20 de abril daba **7,1
desvíos estándar con 13,8 °C de temperatura**. El contraste con el 3 de
febrero, que daba 4,5 σ con 37,4 °C, es lo que los separa: uno es una ola de
calor real y el otro no puede serlo.

Se descarta con una ventana explícita: `19/04 09:45 – 20/04 10:00`.

## Lo que no se pudo verificar

La mediana móvil es **estructuralmente ciega a la temperatura**. Si una serie
es constante en bloques de L = 12 muestras, una ventana centrada de w = 2k+1
contiene al menos k+1 valores del bloque propio y a lo sumo k de los vecinos,
así que el bloque propio tiene mayoría estricta y el residuo da exactamente
cero. La condición es w ≤ 2L−1 = 23: con 13, y con cualquier ventana de hasta
dos horas, el filtro no puede detectar nada.

Por eso la temperatura se revisa a su granularidad real, un valor por hora:

| Criterio | Resultado |
|---|---|
| Tukey extremos | \[−5,0 ; 47,5\] °C → 0 marcados |
| Mediana móvil de 5 h | apartamiento máximo 1,6 °C, hueco mayor ×1,23 |
| z dentro de la hora del día | máximo 2,55 σ |
| Salto entre horas consecutivas | máximo 4,4 °C |

**La temperatura no tiene anomalías.** La ceguera no tapó nada, pero eso se
sabe recién después de mirarlo así.
