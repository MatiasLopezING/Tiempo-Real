# 3 · Regla temperatura–demanda

Es el núcleo del trabajo: la función que convierte una temperatura en una
demanda, medida sobre datos reales en vez de inventada.

## El problema: hora y temperatura están confundidas

Las temperaturas bajas se miden de madrugada, cuando la demanda es baja de
todos modos, y las altas a la tarde, cuando es alta por el ritmo de actividad.
Un gráfico directo de demanda contra temperatura **mide el reloj, no el
termómetro**.

## La solución: estimar los dos efectos a la vez

Una regresión con variables indicadoras por cada combinación de hora y tipo de
día (48 en total), más dos tramos lineales de temperatura:

```
log xᵢ = α_{h(i), d(i)} + a₁·clip(Tᵢ − b₁, 0, b₂ − b₁) + a₂·max(0, Tᵢ − b₂) + εᵢ
         └── base horaria ──┘  └── tramo tibio ──┘        └─ tramo caliente ─┘
```

Parte por parte:

1. **Base horaria.** Cuánto se consume normalmente a esa hora y ese tipo de
   día. Son 48 números y los estima el propio ajuste.
2. **Tramo tibio.** Entre `b₁` y `b₂` la demanda sube `a₁` por grado. El `clip`
   hace que valga 0 por debajo de `b₁` y que deje de crecer en `b₂`.
3. **Tramo caliente.** Por encima de `b₂` se suma `a₂` por grado. Es la
   pendiente que importa. El `max` lo apaga por debajo.
4. **Error.** Lo que no explican ni la hora ni la temperatura.

Se trabaja sobre **log x** porque el efecto es multiplicativo: el calor sube la
demanda un porcentaje, no una cantidad fija de MW. Tomando logaritmo ese
porcentaje pasa a ser una suma y el ajuste vuelve a ser lineal.

> **Por qué este método y no otro.** La primera versión calculaba un perfil
> base promediando solo las muestras de una franja templada elegida a mano
> (17–22 °C) y después dividía. Eso introduce dos parámetros arbitrarios —la
> franja y un corte mínimo de muestras por grado— y da un R² peor. La
> regresión conjunta no necesita ninguno de los dos.

## Cuántos tramos

Tampoco se decide mirando el gráfico: se comparan por **BIC**, que premia el
ajuste y penaliza cada parámetro de más.

| Modelo | Quiebres | BIC |
|---|---|---|
| 1 tramo | 24,0 | −115.184 |
| **2 tramos** | **22,0 y 30,5** | **−115.707** |
| 3 tramos | 13,0 · 17,0 · 30,0 | −115.866 |

El BIC sigue bajando con tres quiebres, pero los dos que agrega caen en 13 y
17 °C: es el efecto de **calefacción**, que el proyecto deja fuera de alcance
porque estudia olas de calor y porque el período no cubre el invierno. Se
queda con dos tramos, que es el mejor modelo dentro de la rama de
refrigeración.

## El resultado

```
quiebres   : 22,0 °C y 30,5 °C
pendientes : +1,14 %/°C  y  +4,43 %/°C
```

| Zona | Comportamiento |
|---|---|
| Hasta 22 °C | La demanda no reacciona al termómetro |
| 22 a 30,5 °C | Sube 1,14 % por grado. Adopción gradual |
| **Más de 30,5 °C** | **Sube 4,43 % por grado.** Casi cuatro veces más rápido |

Ese salto de pendiente es el hallazgo: es el momento en que la simultaneidad de
los aires acondicionados se dispara.

El multiplicador, listo para usar:

```
k(T) = exp( 0,01132 · clip(T − 22 , 0 , 8,5) + 0,04338 · max(0 , T − 30,5) )
```

| T | k(T) |
|---|---|
| 22 °C | 1,000 |
| 25 °C | 1,035 |
| 30 °C | 1,095 |
| 35 °C | 1,338 |
| 37,8 °C | 1,511 |
| 40 °C | 1,663 |

## Validación

```
R² del modelo completo : 0,650
R² solo con la hora    : 0,510
error medio absoluto   : 28,6 MW
```

La hora sola explica el 51 % y el modelo completo el 65 %: **la temperatura
aporta 14 puntos**. El resto queda en manos del día de la semana, la actividad
industrial y la inercia térmica, que el modelo no mira.

Es el número que hay que declarar. Sobre la *curva agregada por grado* el
ajuste es de 0,99, pero ese no es el número honesto para una medición suelta.

### Cuántos días sostienen la rama caliente

El total de muestras engaña: la temperatura es horaria y las horas calurosas se
agrupan en pocos días de verano.

| T ≥ | muestras | horas | **días** |
|---|---|---|---|
| 30 °C | 1.728 | 144 | 20 |
| 32 °C | 840 | 70 | 12 |
| 34 °C | 336 | 28 | **6** |
| 35 °C | 204 | 17 | **3** |
| 37 °C | 60 | 5 | **1** |

El punto de 37 °C son cinco horas de un solo día, y los seis días arriba de
34 °C son todos de febrero.

Se probó sacando cada uno de esos días y reajustando: la pendiente caliente se
mueve entre **4,19 y 4,71 %/°C**. Ningún día la sostiene solo, así que el
resultado aguanta, pero el número honesto es ese rango y no un valor exacto.

## Las salidas

En `TrabajoFinal/analisis/data/`:

| Archivo | Contenido |
|---|---|
| `perfil_base_horario.csv` | 24 filas: hora, base en día hábil, base en finde (MW) |
| `curva_temperatura.csv` | 38 filas, de 8 a 45 °C: temperatura, multiplicador |
| `participacion_subestaciones.csv` | Las 6 AT/MT con su fracción de clientes |

Y la regla en NetLogo:

```netlogo
to-report multiplicador-temp [ T ]
  let c1 max (list 0 (min (list (T - 22) 8.5)))   ; tramo 22 - 30.5 C
  let c2 max (list 0 (T - 30.5))                  ; tramo sobre 30.5 C
  report exp (0.01132 * c1 + 0.04338 * c2)
end
```
