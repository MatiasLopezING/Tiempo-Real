# Sistemas de Tiempo Real

Trabajos prácticos y trabajo final de la materia **Sistemas de Tiempo Real**
(I115) — Ingeniería en Computación, UNLP, 2026.

Álvarez David · Báez Ezequiel · Giacoia Pedro · López Matías.

---

## Trabajo final — Simulación de apagones eléctricos

Modelo de simulación basada en agentes, en NetLogo, sobre la red de
distribución eléctrica real de la concesión de EDELAP (La Plata, Berisso,
Ensenada, Magdalena y Punta Indio). Responde una pregunta concreta:

> ¿Cuánto margen le queda a la red frente a una ola de calor, y qué pasa con
> ese margen si la temperatura sube?

Corriendo el día más caluroso del período medido (3/2/2026, 37,8 °C a las 15),
la subestación más exigida llega al **92,7 %** de su capacidad y no corta. El
mismo día con **+3 °C** se va a **124 %** y corta tres veces.

**📖 La documentación completa está en la [wiki](https://github.com/MatiasLopezING/Tiempo-Real/wiki).**
Ahí está explicado de dónde salen los datos, cómo se mide la relación entre
temperatura y demanda, cómo funciona el modelo y qué supuestos tiene.

### Cómo correrlo rápido

```bash
# el análisis de datos
cd TrabajoFinal/analisis && pip install -r requirements.txt
jupyter notebook analisis_demanda.ipynb

# el modelo: abrir con NetLogo 7
TrabajoFinal/Archivo Principal/laplata_at.nlogox
#   1. setup-ciudad      carga la red real
#   2. usar-dia-real     carga el 03/02/2026
#   3. go
```

### Qué hay en `TrabajoFinal/`

| Carpeta | Contenido |
|---|---|
| `Archivo Principal/` | El modelo de NetLogo, el módulo de reglas y el grafo |
| `analisis/` | Notebook del análisis de demanda y los CSV que alimentan el modelo |
| `Datasets/` | Scripts de descarga de CAMMESA y los datos crudos |
| `test/` | Shapefiles del SIG y los scripts que generan el grafo |
| `presentacion-avance/` | Presentación de avance y su guion |

### Fuentes de datos

| Qué | Dónde |
|---|---|
| Demanda eléctrica, cada 5 min | API de CAMMESA, `id_region` 1943 (EDELAP) |
| Temperatura del aire, horaria | NSRDB (NREL), derivada del reanálisis MERRA-2 |
| Subestaciones y líneas de alta tensión | [sig.energia.gob.ar](https://sig.energia.gob.ar/v/) |

---

## Trabajos prácticos

### TP1 — Interrupciones

Programación de microcontroladores: manejo de interrupciones sobre AVR.
Enunciado en `TP1-Interrupciones/Practica-1-Interrupciones.pdf`.

### TP2 — FreeRTOS

Introducción a FreeRTOS: tareas, sincronización y comunicación entre tareas.
Enunciado en `TP2-FreeRTOS/Practica-2.pdf`.

### TP3 — Planificación

Planificación de tareas sobre FreeRTOS en ATmega328P, simulado en Proteus:
Round Robin con reparto por cuantos (Ejercicio 4) contra FIFO no expropiativo
(Ejercicio 5). Cada ejercicio tiene su informe y su guía en PDF.

Los pasos para reproducir los ejercicios 4 y 5 —qué archivos van dónde, qué
hay que cambiar en la configuración de la biblioteca y cómo armar el
analizador lógico en Proteus— están en
[`TP3-Planificacion/COMO-REPRODUCIR.txt`](TP3-Planificacion/COMO-REPRODUCIR.txt).

Entorno: VS Code + PlatformIO, placa `uno`, biblioteca `feilipu/FreeRTOS`,
simulación en Proteus 8.

---

## Estructura

```
TP1-Interrupciones/      práctica 1
TP2-FreeRTOS/            práctica 2
TP3-Planificacion/       práctica 3, con informes y guías por ejercicio
TrabajoFinal/            el proyecto de simulación
```

## Editar la wiki

Las páginas de la [wiki](https://github.com/MatiasLopezING/Tiempo-Real/wiki)
viven en un repositorio aparte de GitHub. Para editarlas hay que clonarlo:

```bash
git clone https://github.com/MatiasLopezING/Tiempo-Real.wiki.git
```

Se puede clonar dentro de esta misma carpeta: `Tiempo-Real.wiki/` está en el
`.gitignore`, así que el repo del proyecto lo ignora. Después se edita, se
commitea y se pushea ahí como en cualquier repo; su rama se llama `master`.

| Archivo | Página |
|---|---|
| `Home.md` | Inicio, resumen y estado |
| `_Sidebar.md` | Navegación lateral |
| `1-Problema-y-objetivos.md` … `7-Como-correrlo.md` | Las siete páginas de contenido |

El nombre del archivo es el nombre de la página y su URL, y los enlaces
internos van sin extensión: `[texto](2-Datos-y-limpieza)`.

## Trabajar sobre el modelo de NetLogo

El archivo `.nlogox` guarda todo su código dentro de una sola línea de XML, así
que **git no lo puede mergear**: si dos personas lo editan a la vez, alguien
pierde su trabajo. La regla es que lo toca una persona por vez, y todo lo que
se pueda escribir en `reglas_energia.nls` va ahí, que es texto plano.
