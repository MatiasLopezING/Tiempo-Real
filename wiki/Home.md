# Simulación de apagones eléctricos en el AMBA y La Plata

Modelo de simulación basada en agentes, en NetLogo, sobre la red de distribución
eléctrica real de la concesión de EDELAP. Responde una pregunta concreta:

> ¿Cuánto margen le queda a la red de La Plata frente a una ola de calor,
> y qué pasa con ese margen si la temperatura sube?

**Sistemas de Tiempo Real** — Ingeniería en Computación, UNLP, 2026.
Álvarez David · Báez Ezequiel · Giacoia Pedro · López Matías.

---

## El resultado, en una línea

Corriendo el 3 de febrero de 2026, el día más caluroso del período medido
(37,8 °C a las 15), la subestación más exigida llega al **92,7 %** de su
capacidad y no corta. El mismo día con **+3 °C** se va a **124 %** y corta
tres veces.

---

## Por dónde empezar

| Página | De qué trata |
|---|---|
| [1 · Problema y objetivos](1-Problema-y-objetivos) | Qué se quiere simular y por qué |
| [2 · Datos y limpieza](2-Datos-y-limpieza) | De dónde salen los datos y qué hubo que descartar |
| [3 · Regla temperatura–demanda](3-Regla-temperatura-demanda) | El núcleo del trabajo: cómo se mide el efecto del calor |
| [4 · La red y el grafo](4-La-red-y-el-grafo) | Las subestaciones reales y cómo se conectan |
| [5 · El modelo en NetLogo](5-El-modelo-en-NetLogo) | Arquitectura, reglas físicas y máquina de estados |
| [6 · Supuestos y límites](6-Supuestos-y-limites) | Lo que es hipótesis nuestra y no dato |
| [7 · Cómo correrlo](7-Como-correrlo) | Pasos para reproducir todo desde cero |

Si venís a entender el trabajo, leé 1, 3 y 5 en ese orden: son los que
explican la idea. Si venís a reproducirlo, andá directo a la 7.

---

## Estado

El ciclo de simulación tiene cuatro pasos. Los cuatro funcionan:

| | Paso | Estado |
|---|---|---|
| 1 | Entrada: leer la temperatura del momento | funciona |
| 2 | Reacción: ajustar la demanda de cada nodo | funciona |
| 3 | Evaluación: medir el estrés térmico | funciona |
| 4 | Falla: desconectar por sobrecarga y reponer | funciona |

Falta: correr los noventa días completos, modelar los hogares como agentes
individuales, y producir las tres salidas prometidas (frecuencia de falla por
nodo, fechas críticas y mapa de vulnerabilidad). Ver
[6 · Supuestos y límites](6-Supuestos-y-limites).

---

## Fuentes de datos

| Qué | Dónde |
|---|---|
| Demanda eléctrica, cada 5 min | API de CAMMESA, `id_region` 1943 (EDELAP) |
| Temperatura del aire, horaria | NSRDB (NREL), derivada del reanálisis MERRA-2 |
| Subestaciones y líneas | [sig.energia.gob.ar](https://sig.energia.gob.ar/v/) — Secretaría de Energía |
