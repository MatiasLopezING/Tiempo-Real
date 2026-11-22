# Modelo de Aires Acondicionados, Consumo Eléctrico y Cortes de Suministro

Este documento especifica las ecuaciones, parámetros, fuentes de datos y reglas de decisión implementadas en el módulo reutilizable [reglas_energia.nls](../Archivo%20Principal/reglas_energia.nls) para la simulación en NetLogo de la red eléctrica de La Plata (EDELAP).

El diseño es **modular y agnóstico de escala**: todas las expresiones se formulan por cliente y por capacidad instalada (`kVA`), de modo que las mismas reglas aplican tanto sobre las **6 subestaciones AT/MT** actuales como sobre una futura expansión con **transformadores barriales MT/BT** dentro del casco urbano.

---

## 1. Fuentes de datos: qué sale del repositorio vs. qué es parámetro de referencia

Para mantener total trazabilidad en el análisis y la defensa del modelo, se distinguen explícitamente los datos empíricos del proyecto de los parámetros físicos/referenciales asumidos.

### 1.1 Datos empíricos (provenientes del repositorio)

| Dato | Valor / Rango | Origen en el repositorio |
|---|---|---|
| Subestaciones AT/MT de la red modelada | 6 nodos (`MT095`, `MT093`, `MT099`, `MT092`, `MT194`, `MT195`) | `TrabajoFinal/Archivo Principal/nodes_netlogo.csv` (extraído del SIG de la Secretaría de Energía vía `build_graph1.py`) |
| Clientes totales cubiertos por las 6 AT/MT | **196.972 clientes** | `nodes_netlogo.csv` (`sum [clientes] of mt-subs`) |
| Potencia instalada total en las 6 AT/MT | **402.660 kVA** (~382,5 MW con $\cos\varphi = 0{,}95$) | `nodes_netlogo.csv` (`pot_instalada` por subestación) |
| Perfil base horario (sin efecto térmico) | Hábil: **287–434 MW** (pico base a las 20 h)<br>Finde: **258–394 MW** (pico base a las 20 h) | `TrabajoFinal/analisis/data/perfil_base_horario.csv` (CAMMESA región EDELAP, feb–may 2026) |
| Curva de sensibilidad térmica $k(T)$ | Quiebres por BIC en **22 °C** (+1,14 %/°C) y **30,5 °C** (+4,43 %/°C) | `TrabajoFinal/analisis/analisis_demanda.ipynb` y `data/curva_temperatura.csv` |
| Pico histórico observado en la concesión | **625 MW** (03/02/2026, 14:00, $T = 37{,}4\ ^\circ\text{C}$) | `TrabajoFinal/analisis/data/demanda_la_plata_2026_con_temperatura.csv` |

#### Capacidad específica por cliente en cada subestación AT/MT

Los datos del SIG revelan de forma natural las diferencias de holgura entre el casco urbano y la periferia, sin necesidad de introducir vulnerabilidades artificiales:

| Subestación | Ubicación | Clientes | Potencia instalada (kVA) | Capacidad activa (MW, $\cos\varphi=0{,}95$) | kVA / cliente |
|---|---|---:|---:|---:|---:|
| `MT095` | 58 e/ 21 y 22 | 73.863 | 117.114 | 111,26 | **1,59** *(más exigida)* |
| `MT093` | 4 y 44 | 55.018 | 101.829 | 96,74 | **1,85** |
| `MT099` | 9 y Cno. Belgrano | 38.441 | 85.910 | 81,61 | 2,23 |
| `MT195` | Rta. 36 y Acc. Vern | 4.772 | 14.020 | 13,32 | 2,94 |
| `MT194` | De La Paz y Formigo | 7.390 | 22.718 | 21,58 | 3,07 |
| `MT092` | Rta. 2 y 514 | 17.488 | 61.069 | 58,02 | 3,49 |
| **Total** | — | **196.972** | **402.660** | **382,53** | **2,04** |

### 1.2 Parámetros de referencia y calibración (configurables en `.nls`)

Dado que CAMMESA publica demanda eléctrica agregada y no cantidad de aires acondicionados encendidos, se introducen parámetros físicos de referencia para el ámbito urbano AMBA/La Plata. Todos se definen como variables configurables en `reglas_energia.nls`.

| Parámetro (`.nls`) | Valor base | Rango sugerido | Justificación y origen |
|---|---:|---:|---|
| `cfg-escala-red` (`ESCALA`) | **0,55** | 0,50 – 0,65 | Fracción de la demanda de toda la región CAMMESA-EDELAP que corresponde a los 196.972 clientes de las 6 AT/MT del SIG. Cota superior empírica del repositorio: $382{,}5\ \text{MW} / 625\ \text{MW} \approx 0{,}61$ (conclusión #6 de `analisis_demanda.ipynb`). Como referencia externa consistente, la concesión completa de EDELAP (La Plata, Berisso, Ensenada, Brandsen, Magdalena y Punta Indio) ronda ~360–400 mil clientes ($197\text{k} / 370\text{k} \approx 0{,}53$). |
| `cfg-eq-aa-por-cliente` | **0,90** eq/cli | 0,60 – 1,30 | Promedio de equipos instalados por cliente. Basado en penetración residencial urbana en AMBA/La Plata (~75% de hogares con al menos un equipo, según relevamientos del sector eléctrico/ENRE) y ~1,2 equipos promedio por hogar equipado ($0{,}75 \times 1{,}20 = 0{,}90$). Equivale a ~177.275 equipos instalados en el área de las 6 AT/MT. |
| `cfg-pot-unit-aa-kw` | **1,15** kW | 0,85 – 1,45 | Potencia eléctrica efectiva promedio de un split de 3000–3500 frigorías con compresor activo (etiquetado IRAM 62406 / Secretaría de Energía: ~0,85–1,0 kW en tecnología Inverter y ~1,25–1,45 kW en equipos on/off convencionales). |
| `cfg-f-sim-max` | **0,85** | 0,75 – 0,95 | Simultaneidad máxima física de equipos encendidos al mismo instante (contempla viviendas desocupadas y ciclado de termostatos). Aporta un techo físico realista frente a la extrapolación exponencial de $k(T)$ por encima de 37,8 °C (limitación declarada en la conclusión #7 de `analisis_demanda.ipynb`). |
| `cfg-cos-phi` | **0,95** | 0,90 – 0,98 | Factor de potencia estándar de redes de distribución para convertir potencia aparente instalada (`kVA`) a potencia activa útil (`MW`). |

---

## 2. Ecuaciones del modelo: Temperatura → Aires Acondicionados → Consumo

### 2.1 Multiplicador térmico (del análisis estadístico)

A partir de la regresión con efectos fijos de hora y tipo de día del análisis de demanda:

$$k(T) = \exp\!\Bigl(0{,}01132 \cdot \operatorname{clip}(T - 22,\; 0,\; 8{,}5) + 0{,}04338 \cdot \max(0,\; T - 30{,}5)\Bigr)$$

### 2.2 Demanda base por cliente

Se normaliza el perfil horario de CAMMESA (escalado a la porción modelada) sobre la cantidad total de clientes de la red (`clientes-totales-red`, calculada dinámicamente como la suma de `clientes` de los nodos cargados, sin hardcodear `196.972`):

$$p_{\text{base}}(h, d) = \frac{\text{base\_horaria}(h, d) \cdot \text{ESCALA} \cdot 1000}{N_{\text{clientes,red}}} \qquad [\text{kW/cliente}]$$

> **Nota sobre `ESCALA` y $N_{\text{clientes,red}}$:** modificar `ESCALA` **no** modifica $N_{\text{clientes,red}}$ (que es un dato fijo del mapa). Cambiar `ESCALA` varía únicamente qué porcentaje de los MW medidos por CAMMESA para toda la concesión se asigna a los clientes presentes en la red modelada.

### 2.3 Fracción de aires acondicionados encendidos (calibrada a $k(T)$)

El exceso de demanda unitaria atribuible al calor por encima de 22 °C según CAMMESA es:

$$\Delta p_{\text{calor}}(h, d, T) = p_{\text{base}}(h, d) \cdot \bigl(k(T) - 1\bigr) \qquad [\text{kW/cliente}]$$

Dividiendo ese excedente por la potencia instalada de AA por cliente del parque de referencia ($p_{\text{ref}} \cdot P_{\text{unit,ref}} = 0{,}90 \times 1{,}15 = 1{,}035\ \text{kW/cliente}$) y aplicando el tope físico de simultaneidad $F_{\text{sim,max}}$, se obtiene la fracción de equipos encendidos:

$$f_{\text{on}}(h, d, T) = \min\!\left(F_{\text{sim,max}},\; \max\!\left(0,\; \frac{\Delta p_{\text{calor}}(h, d, T)}{p_{\text{ref}} \cdot P_{\text{unit,ref}}}\right)\right)$$

### 2.4 Consumo en cualquier nodo de carga

Para **cualquier nodo** (una subestación `mt-sub` o un futuro transformador barrial `trafo-bt`) con $N$ clientes:

$$P_{\text{base,nodo}} = \frac{N \cdot p_{\text{base}}(h, d)}{1000} \qquad [\text{MW}]$$

$$N_{\text{AA\_on}} = \operatorname{round}\!\bigl(N \cdot \text{eq\_por\_cliente} \cdot f_{\text{on}}(h, d, T)\bigr)$$

$$P_{\text{AA,nodo}} = \frac{N_{\text{AA\_on}} \cdot P_{\text{unit\_AA}}}{1000} \qquad [\text{MW}]$$

$$P_{\text{nodo}} = P_{\text{base,nodo}} + P_{\text{AA,nodo}} \qquad [\text{MW}]$$

**Propiedades clave de esta formulación:**
1. **Consistencia exacta con los datos reales:** con los valores de referencia (`eq_por_cliente = 0,90` y `P_unit_AA = 1,15 kW`), los factores se cancelan y el consumo total reproduce exactamente la curva validada de CAMMESA en todo el rango medido (8–37,8 °C).
2. **Saturación física en olas extremas:** por encima de ~41–42 °C el techo `F_sim_max = 0,85` impide un crecimiento exponencial irrestricto.
3. **Capacidad de experimentar escenarios:** modificar `cfg-eq-aa-por-cliente` (p. ej., mayor penetración futura) o `cfg-pot-unit-aa-kw` (p. ej., recambio masivo a equipos Inverter de 0,85 kW) impacta directamente sobre $P_{\text{AA,nodo}}$ y sobre los cortes.

---

## 3. Reglas de sobrecarga térmica, corte y restablecimiento

La resolución temporal base del módulo es **1 tick = 5 minutos** (`cfg-minutos-por-tick = 5`), coincidente con el muestreo de CAMMESA (12 ticks = 1 hora; 288 ticks = 1 día). Todas las tasas térmicas y temporizadores se escalan automáticamente si se cambia `cfg-minutos-por-tick`.

### 3.1 Capacidad efectiva y factor de carga

$$P_{\max,\text{nodo}} = \frac{\text{pot\_instalada}}{1000} \cdot \cos\varphi \qquad [\text{MW}]$$

$$L = \frac{P_{\text{nodo}}}{P_{\max,\text{nodo}}}$$

### 3.2 Máquina de estados del nodo transformador

Cada nodo transita entre tres estados discretos (`operativo`, `sobrecargado`, `cortado`) con **inercia térmica** (`estres-termico`), reflejando que un transformador tolera sobrecargas moderadas durante un lapso acotado antes del disparo de protecciones (comportamiento tiempo-inverso simplificado):

| Estado actual | Condición en el tick | Transición y efectos |
|---|---|---|
| `operativo` o `sobrecargado` | $L < 0{,}85$ | Estado $\to$ **`operativo`** (verde). Enfría `estres-termico` a razón de `-0,05` por paso de 5 min (piso 0). |
| `operativo` o `sobrecargado` | $0{,}85 \le L \le 1{,}00$ | Estado $\to$ **`sobrecargado`** (naranja, alerta temprana / *fail-soft*). Enfriamiento lento (`-0,02` por paso de 5 min). |
| `operativo` o `sobrecargado` | $1{,}00 < L < 1{,}25$ | Estado $\to$ **`sobrecargado`** (naranja). Acumula estrés térmico: $\Delta E = (L - 1{,}00) \cdot (\Delta t / 5)$. Si $E \ge 0{,}60$, pasa a **`cortado`**. |
| `operativo` o `sobrecargado` | $L \ge 1{,}25$ | Disparo instantáneo de protección $\to$ **`cortado`** (rojo). |
| **`cortado`** | `timer-reposicion > 0` | $P_{\text{nodo}} = 0$, `aa-encendidos = 0`, los clientes del nodo quedan sin suministro. El equipo se enfría (`-0,08` por paso de 5 min) y decrementa el temporizador. |
| **`cortado`** | `timer-reposicion <= 0` | Restablecimiento $\to$ **`operativo`**, `estres-termico = 0` y activación de pico transitorio de reconexión (*cold load pickup*). |

#### Tiempos de disparo resultantes (con `cfg-limite-estres-termico = 0,60`)

- Sobrecarga del **105%** ($L = 1{,}05$): dispara en **60 minutos** (12 ticks).
- Sobrecarga del **110%** ($L = 1{,}10$): dispara en **30 minutos** (6 ticks).
- Sobrecarga del **115%** ($L = 1{,}15$): dispara en **20 minutos** (4 ticks).
- Sobrecarga del **120%** ($L = 1{,}20$): dispara en **15 minutos** (3 ticks).
- Sobrecarga $\ge$ **125%**: disparo inmediato (0 min).

### 3.3 Tiempo de reposición y efecto rebote (*Cold Load Pickup*)

- **Reposición:** al cortarse un nodo, se sortea una demora uniforme entre **60 y 180 minutos** (`cfg-reposicion-min-min` y `cfg-reposicion-max-min`), representando el tiempo de enfriamiento, diagnóstico y maniobra de cuadrilla/telecontrol.
- **Rebote de reconexión:** al volver el servicio, los compresores de aires acondicionados y heladeras arrancan en simultáneo generando un pico transitorio de **+20%** (`cfg-factor-rebote = 1,20`) durante **15 minutos** (`cfg-duracion-rebote-min = 15`). Si la temperatura sigue siendo extrema al reconectar, esto puede provocar **recortes repetidos**, reproduciendo un fenómeno real característico de las olas de calor.

---

## 4. Verificación numérica sobre un día real de ola de calor (03/02/2026)

Evaluando las ecuaciones con los parámetros base (`ESCALA = 0,55`, `eq_por_cliente = 0,90`, `P_unit_AA = 1,15 kW`) sobre las temperaturas horarias reales del **03/02/2026** (día hábil más caluroso del dataset), la demanda potencial y el factor de carga sin cortes se comportan así:

| Hora | $T$ real (°C) | $f_{\text{on}}$ | AA encendidos | $P_{\text{total}}$ (MW) | `MT095` | `MT093` | `MT099` | `MT092` | `MT194` | `MT195` |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 06:00 | 25,0 | 2,8% | 5.006 | 172,4 | 58,1% | 49,8% | 41,2% | 26,4% | 30,0% | 31,4% |
| 10:00 | 33,8 | 25,8% | 45.789 | 247,4 | 83,4% | 71,4% | 59,1% | 37,9% | 43,0% | 45,0% |
| 12:00 | 36,5 | 43,1% | 76.409 | 293,0 | **98,8%** | 84,6% | 70,1% | 44,8% | 50,9% | 53,3% |
| 14:00 | 37,4 | 48,0% | 85.161 | 299,8 | **101,0%** | **86,6%** | 71,7% | 45,9% | 52,1% | 54,5% |
| 15:00 | 37,8 | 50,2% | 88.991 | 302,5 | **102,0%** | **87,4%** | 72,3% | 46,3% | 52,6% | 55,0% |
| 16:00 | 37,8 | 50,1% | 88.746 | 301,7 | **101,7%** | **87,1%** | 72,1% | 46,2% | 52,4% | 54,9% |
| 18:00 | 35,8 | 39,2% | 69.526 | 287,3 | **96,8%** | 83,0% | 68,7% | 44,0% | 49,9% | 52,3% |
| 20:00 | 30,1 | 11,2% | 19.932 | 261,6 | **88,2%** | 75,5% | 62,6% | 40,0% | 45,5% | 47,6% |
| 22:00 | 27,8 | 7,4% | 13.144 | 237,9 | 80,2% | 68,7% | 56,9% | 36,4% | 41,4% | 43,3% |

**Lectura del comportamiento emergente:**
- Aunque la demanda *base* sin calor tiene su pico a las 20:00 (434 MW), durante una ola de calor real la temperatura máxima ocurre a la siesta/tarde (14–16 h) y desplaza el pico efectivo a esa franja (621 MW en CAMMESA a las 14:00).
- Con `ESCALA = 0,55`, `MT095` supera el 100% de capacidad entre las 14:00 y 16:00 (acumulando estrés térmico hasta disparar el corte), mientras `MT093` entra en zona de alerta temprana (86–87%) y las subestaciones periféricas permanecen operativas.
- Con `ESCALA = 0,60` o con temperaturas de ola extrema (40–41 °C), `MT095` alcanza >111% (corte en ~25–30 min) y `MT093` supera el 95–100%.

---

## 5. Cómo enchufar `reglas_energia.nls` en NetLogo

El archivo [reglas_energia.nls](../Archivo%20Principal/reglas_energia.nls) está listo para integrarse tanto sobre la topología actual de 6 AT/MT como sobre una futura red con transformadores barriales MT/BT.

### 5.1 Integración en el modelo actual (`laplata_at.nlogox`, nivel AT/MT)

1. Al principio del código del `.nlogox`, incluir el módulo:
   ```netlogo
   extensions [csv nw]
   __includes [ "reglas_energia.nls" ]
   ```

2. Agregar las variables de estado a `mt-subs-own`:
   ```netlogo
   mt-subs-own [
     clientes pot-instalada acceso
     estado demanda-base-mw demanda-aa-mw carga-actual-mw
     aa-encendidos factor-carga estres-termico
     timer-reposicion ticks-post-reconexion cant-cortes
   ]
   ```

3. Al final de `cargar-red` (antes de `reset-ticks`), inicializar el módulo:
   ```netlogo
   init-reglas-energia "../analisis/data/perfil_base_horario.csv" mt-subs
   ```

4. Agregar el procedimiento `go` (usando temperatura de slider, curva diaria sintética o un día real cargado con `cargar-temperaturas-dia`):
   ```netlogo
   to go
     let hora ((ticks * cfg-minutos-por-tick) / 60) mod 24
     let temp-actual temperatura-diaria-sintetica hora 24 39  ; o valor de slider / CSV
     step-red-simple mt-subs hora false temp-actual
     tick
   end
   ```

5. Conectar monitores y gráficos usando los reporters globales ya provistos:
   - `demanda-total-mw mt-subs`
   - `demanda-aa-total-mw mt-subs`
   - `aa-encendidos-total mt-subs`
   - `clientes-sin-luz mt-subs`
   - `cortes-acumulados mt-subs`

### 5.2 Extensión futura con transformadores barriales (nivel MT/BT)

Si posteriormente se agrega un breed `trafos-bt` dentro del casco urbano (cada uno con sus `clientes`, `pot-instalada`, una referencia a su `sub-padre` y las mismas variables de estado):

1. En `setup`, inicializar con `init-reglas-energia "..." trafos-bt` y además `ask mt-subs [ iniciar-nodo-carga ]`.
2. En `go`:
   ```netlogo
   ; 1) Cada transformador MT/BT calcula su demanda, AA y estado local
   step-red-simple trafos-bt hora finde? temp-actual

   ; 2) Cada subestacion AT/MT agrega la carga real de sus transformadores hijos
   ;    (si la AT/MT dispara, corta en cascada a todos sus hijos)
   ask mt-subs [
     actualizar-subestacion-agregada (trafos-bt with [ sub-padre = myself ])
   ]
   ```

De este modo, la lógica de temperatura, aires acondicionados, estrés térmico, cortes y restablecimiento se reutiliza sin duplicar ni reescribir código.
