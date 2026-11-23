# 5 · El modelo en NetLogo

Carpeta: `TrabajoFinal/Archivo Principal/`. NetLogo 7.

| Archivo | Qué es |
|---|---|
| `laplata_at.nlogox` | El modelo: interfaz, carga de la red, ciclo de tiempo |
| `reglas_energia.nls` | Módulo de reglas físicas, reutilizable |
| `nodes_netlogo.csv`, `edges_netlogo.csv` | El grafo |
| `basemap_ciudad.png`, `basemap_completa.png` | Mapas de fondo |

El modelo trae el módulo con `__includes [ "reglas_energia.nls" ]`, que
funciona como un `import`: todo lo que está en el `.nls` queda disponible como
si estuviera escrito en el modelo.

> **Por qué dos archivos.** El `.nlogox` guarda todo su código dentro de una
> sola línea de XML, así que git nunca lo puede mergear bien: siempre es «o el
> tuyo o el mío». La regla práctica es que **lo toca una persona por vez**, y
> el resto trabaja en el `.nls`, que es texto plano y sí mergea.

---

## Qué pasa en `setup-ciudad`

1. **Lee los CSV** y crea una tortuga por nodo: cuadrado si es AT/AT, triángulo
   si es AT/MT, círculo si es empalme, equis si es extremo libre.
2. **Los ubica.** Los CSV traen coordenadas UTM en metros; se escalan al mundo
   de NetLogo y se encuadran sobre el basemap. Lo que cae fuera se esconde
   (por eso MT194 y MT195 no se ven en modo «ciudad»).
3. **Conecta las líneas**, fusionando las paralelas.
4. **`asignar-padres`** — la búsqueda en anchura descrita en
   [4 · La red y el grafo](4-La-red-y-el-grafo).
5. **`init-reglas-energia`** — lee `perfil_base_horario.csv`, fija todos los
   parámetros físicos y deja cada nodo operativo con el estrés en cero.
6. **`preparar-at-subs`** — calcula clientes y capacidad de los cuadrados.

---

## Qué pasa en cada tick

Un tick son **5 minutos**, el mismo paso con que CAMMESA publica la demanda.

```netlogo
to go
  sincronizar-sliders
  let hora ((floor (ticks * cfg-minutos-por-tick / 60)) mod 24)
  let T temperatura-ahora
  step-red-simple mt-subs hora dia-finde? T          ; los triángulos
  ask at-subs with [ any? nodos-hijos ]              ; los cuadrados agregan
    [ actualizar-subestacion-agregada nodos-hijos ]
  etiquetar
  tick
end
```

Para cada triángulo, el módulo encadena:

```
1. demanda base      cuánto consume normalmente a esta hora
2. multiplicador     cuánto la sube el calor  →  k(T)
3. aires prendidos   de dónde sale ese exceso
4. carga total       base + aires
5. factor de carga   carga / capacidad del transformador
```

### Ejemplo numérico

MT095 a las 15 h, 37,8 °C, `escala` 0,50:

```
base de las 15 h (región)  = 364 MW
le toca el 50 %            = 182 MW / 196.972 clientes = 0,924 kW por cliente
MT095 (73.863 clientes)    = 68,3 MW de base
k(37,8) = 1,511            → + 34,8 MW de aires
carga total                = 103,1 MW
capacidad = 117.114 kVA × 0,95 = 111,3 MW
factor de carga            = 92,7 %
```

---

## Las reglas físicas

### De la curva a los equipos

La curva dice cuánto sube el consumo; el modelo necesita saber cuántos aires
hay prendidos. Se invierte:

```
f_on = p_base_cliente · (k(T) − 1) / (equipos_por_cliente · potencia_unitaria)
```

Acotado por un tope físico de simultaneidad del **85 %**. Con los parámetros
por defecto (0,90 equipos por cliente, 1,15 kW cada uno) la cadena reproduce
exactamente la curva medida, así que mover esos valores es un escenario
deliberado y no un desajuste.

Todo el módulo está formulado **por cliente y por capacidad instalada**, nunca
en megawatts absolutos. Por eso las mismas reglas sirven hoy para las seis
subestaciones y mañana para los transformadores de barrio.

### Cuándo corta un transformador

Un transformador no estalla al cruzar el 100 %: se va calentando.

| Factor de carga | Estado |
|---|---|
| < 85 % | Operativo. Se enfría |
| 85 – 100 % | Alerta. No acumula daño |
| > 100 % | Acumula estrés térmico, proporcional al exceso |
| ≥ 125 % | Disparo instantáneo de la protección |

El acumulador suma `(factor − 1) · paso` mientras está sobrecargado y dispara
al llegar a 0,60. En la práctica:

- al **110 %** tarda **30 minutos** en cortar
- al **101 %** tarda **5 horas**
- si baja del 100 % antes, se enfría y el contador vuelve atrás

Cuando corta queda sin suministro entre **1 y 3 horas** (sorteadas). Al
reconectar tiene un pico del **20 % durante 15 minutos**: es el *cold load
pickup*, el arranque simultáneo de heladeras y aires que estuvieron apagados.

> Ese pico está **limitado por debajo del umbral de disparo instantáneo**. Sin
> ese tope, un nodo que vuelve con 120 % de carga pasaba a 144 %, superaba el
> 125 % y se cortaba otra vez en el mismo tick, quedando en bucle.

### Parámetros

Todos están en `init-reglas-energia`, en un solo lugar:

| Parámetro | Valor | Qué es |
|---|---|---|
| `cfg-escala-red` | 0,50 | Fracción de la demanda regional asignada |
| `cfg-eq-aa-por-cliente` | 0,90 | Equipos de aire instalados por cliente |
| `cfg-pot-unit-aa-kw` | 1,15 | kW por equipo encendido |
| `cfg-f-sim-max` | 0,85 | Tope de simultaneidad |
| `cfg-cos-phi` | 0,95 | Factor de potencia, kVA → MW |
| `cfg-minutos-por-tick` | 5 | Resolución temporal |
| `cfg-umbral-alerta` | 0,85 | Entra en alerta |
| `cfg-umbral-sobrecarga` | 1,00 | Empieza a acumular estrés |
| `cfg-umbral-corte-inmediato` | 1,25 | Disparo instantáneo |
| `cfg-limite-estres-termico` | 0,60 | Estrés acumulado que dispara |
| `cfg-reposicion-min-min` / `max` | 60 / 180 | Duración del corte, en minutos |
| `cfg-factor-rebote` | 1,20 | Pico de reconexión |

---

## La interfaz

| Control | Para qué |
|---|---|
| `setup-ciudad` | Carga la red y encuadra el casco urbano |
| `go` | Corre (botón continuo) |
| `usar-dia-real` | Carga las 288 temperaturas reales del 03/02/2026 |
| `usar-temperatura-manual` | Vuelve al deslizador |
| **escala** | Porción de la demanda regional asignada a la red modelada |
| **temperatura** | Temperatura fija, modo manual |
| **delta-temperatura** | Se suma a lo que haya. **Es el control del objetivo 3** |
| **aa-por-cliente** | Parque de equipos. Bajarlo simula uso medido |

Los nodos muestran su carga en la etiqueta (`MT095 92%`) y cambian de color:
verde operativo, naranja en alerta, rojo cortado. Cada hora simulada el modelo
imprime una línea:

```
h=15  T=37.8 C  demanda=277.4 MW  AA=61203  sin luz=0  cortes=0
```

---

## El perfil térmico sintético

`temperatura-diaria-sintetica` genera un día a partir de una mínima y una
máxima. No usa un coseno simétrico: en los 90 días de datos el mínimo cae a
las **6** (moda de 30 días) y el máximo a las **14** (moda de 47), separados
por 8 horas y no por 12, así que ningún coseno los puede representar. El
coseno dejaba la tarde caliente de más: 3,3 °C por encima a las 19 h, justo
donde la demanda base hace pico. Se usa la forma media medida, normalizada
entre 0 y 1.
