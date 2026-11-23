# Fuente de la wiki

Las páginas de <https://github.com/MatiasLopezING/Tiempo-Real/wiki> se editan
acá y se publican desde acá. La wiki de GitHub es un repositorio git aparte,
así que los archivos viven en este repo y se copian al otro.

## Publicar los cambios

La primera vez, clonar el repo de la wiki **afuera** de este proyecto:

```bash
cd ..
git clone https://github.com/MatiasLopezING/Tiempo-Real.wiki.git
```

> Si tira `repository not found`, la wiki todavía no existe: hay que crear la
> primera página a mano desde la pestaña *Wiki* en GitHub. Después del primer
> guardado el repo queda disponible.

Cada vez que se cambie algo acá:

```bash
cp wiki/*.md ../Tiempo-Real.wiki/
cd ../Tiempo-Real.wiki
git add -A
git commit -m "Actualiza la wiki"
git push
```

## Cómo funcionan los nombres

- El nombre del archivo es el nombre de la página y su URL.
  `2-Datos-y-limpieza.md` → `/wiki/2-Datos-y-limpieza`.
- `Home.md` es la página de inicio.
- `_Sidebar.md` es la barra lateral que aparece en todas las páginas.
- Los enlaces internos van sin extensión: `[texto](2-Datos-y-limpieza)`.

## Las páginas

| Archivo | Página |
|---|---|
| `Home.md` | Inicio, resumen y estado |
| `_Sidebar.md` | Navegación |
| `1-Problema-y-objetivos.md` | Qué se simula y por qué |
| `2-Datos-y-limpieza.md` | Fuentes, criterios de limpieza, la falla de telemetría |
| `3-Regla-temperatura-demanda.md` | El modelo de demanda y su validación |
| `4-La-red-y-el-grafo.md` | Shapefiles, nodos, jerarquía AT/AT |
| `5-El-modelo-en-NetLogo.md` | Arquitectura, reglas físicas, parámetros |
| `6-Supuestos-y-limites.md` | Hipótesis declaradas y lo que falta |
| `7-Como-correrlo.md` | Reproducir todo desde cero |

Al agregar una página nueva, sumarla a `_Sidebar.md` y a la tabla de `Home.md`.
