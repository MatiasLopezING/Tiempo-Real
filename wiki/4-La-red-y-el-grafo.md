# 4 · La red y el grafo

La red del modelo no se inventó: se bajó de las capas oficiales del
[visor geográfico de la Secretaría de Energía](https://sig.energia.gob.ar/v/).

## De shapefile a grafo

Capas usadas, todas de la concesión de **EDELAP**:

- `at-at-subestaciones-transformadoras-shp` — 9 registros
- `at-mt-subestaciones-transformadoras-shp` — 6 registros
- `at-líneas-shp` — líneas de transmisión

Los shapefiles originales están en `TrabajoFinal/test/`, junto con los scripts
que los convierten: `build_graph.py` produce `nodes_netlogo.csv` y
`edges_netlogo.csv`, y `make_basemap.py` los dos mapas de fondo.

| En el modelo | Cantidad |
|---|---|
| Nodos | 50 |
| Líneas de alta tensión | 67 |
| Subestaciones AT/AT | 9 |
| Subestaciones AT/MT | 6 |
| Clientes | 196.972 |
| Potencia instalada | 402,7 MVA |

Los 50 nodos no son todos subestaciones: los demás son empalmes y derivaciones
de las líneas, y hacen falta para que el grafo quede conectado.

NetLogo no admite dos enlaces no dirigidos iguales entre el mismo par de
nodos, así que las líneas paralelas se fusionan y se cuentan en la variable
`circuits`.

## Las 6 subestaciones AT/MT

Son las únicas con clientes y potencia instalada, y por eso las únicas que
pueden tener estado en el modelo.

| | Clientes | Potencia (kVA) | kVA/cliente | Ubicación |
|---|---|---|---|---|
| MT095 | 73.863 | 117.114 | 1,59 | 58 e/ 21 y 22 |
| MT093 | 55.018 | 101.829 | 1,85 | 4 y 44 |
| MT099 | 38.441 | 85.910 | 2,24 | 9 y Camino Belgrano |
| MT092 | 17.488 | 61.069 | 3,49 | Ruta 2 y 514 |
| MT194 | 7.390 | 22.718 | 3,07 | Brandsen / Magdalena |
| MT195 | 4.772 | 14.020 | 2,94 | Ruta 36 y Acc. Verónica |

La columna **kVA por cliente** es la que determina todo: MT095 es zona
residencial densa (1,59) y MT092 tiene más industria (3,49). El golpe de los
aires pega donde hay hogares. Ver
[6 · Supuestos y límites](6-Supuestos-y-limites).

## Las 9 subestaciones AT/AT

La capa oficial de AT/AT trae **solo dos campos**: un identificador y la
coordenada. Sin clientes y sin potencia instalada.

Por eso en el modelo no son nodos de carga propios sino **agregadoras**: su
carga es la suma de la de los nodos que alimentan, y su capacidad se estima
como la suma de la de sus hijos. Es una hipótesis declarada.

## La jerarquía: quién alimenta a quién

No está escrita a mano. El modelo la calcula en cada `setup` con una **búsqueda
en anchura** sobre las líneas reales: cada AT/MT salta de vecino en vecino
hasta toparse con la primera AT/AT, y esa es su alimentadora.

```netlogo
to asignar-padres
  ask mt-subs [
    let vistos (turtle-set self)
    let frente (turtle-set self)
    let encontrado nobody
    let saltos 0
    while [ (encontrado = nobody) and (any? frente) and (saltos < 60) ] [
      let vecinos ((turtle-set [link-neighbors] of frente) with [ not member? self vistos ])
      set vistos (turtle-set vistos vecinos)
      let candidatos (vecinos with [ breed = at-subs ])
      ifelse any? candidatos
        [ set encontrado min-one-of candidatos [ distance myself ] ]
        [ set frente vecinos ]
      set saltos saltos + 1
    ]
    set padre encontrado
  ]
  ask at-subs [ set nodos-hijos (mt-subs with [ padre = myself ]) ]
end
```

Resultado:

| AT/AT | Alimenta a | Clientes |
|---|---|---|
| AT 28000007 | MT095 · MT194 · MT195 | 86.025 |
| AT 28000004 | MT093 | 55.018 |
| AT 28000016 | MT099 | 38.441 |
| AT 28000010 | MT092 | 17.488 |
| Otras cinco | ninguna | 0 |

Esto habilita la **cascada**: si un nodo de alta corta, se lleva puestos a
todos los que cuelgan de él, estén como estén.

> **Consecuencia a tener presente.** Con la capacidad del padre igual a la suma
> de la de sus hijos, el padre siempre está menos cargado que su hijo más
> exigido, así que el hijo se corta primero y lo alivia. La cascada está
> implementada pero en los escenarios probados no se dispara. Donde el padre
> tiene un solo hijo, padre e hijo son equivalentes y caen juntos.

## Nota de conectividad

El grafo tiene dos componentes conexas: una de 47 nodos y otra de 2. La chica
corresponde a un par de nodos de la capa de líneas que no se conectan con el
resto del dibujo de EDELAP.
