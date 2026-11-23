# 1 · Problema y objetivos

## El fenómeno

Durante una ola de calor el consumo eléctrico no crece de forma suave: se
dispara, porque miles de hogares prenden el aire acondicionado casi al mismo
tiempo. La red de distribución está dimensionada para un pico estadístico, no
para esa simultaneidad, y el resultado son apagones focalizados que se repiten
cada verano.

Tres piezas:

- **Olas de calor.** Aumento extremo de la temperatura durante el verano.
- **Electrificación.** Adopción masiva de equipos de climatización.
- **Saturación.** Colapso recurrente de la infraestructura por sobrecarga.

## Los tres objetivos

1. **Simular.** Un modelo dinámico e interactivo que reproduzca el
   comportamiento de la red bajo una ola de calor, nodo por nodo.
2. **Medir.** Alimentarlo con temperatura y demanda reales en lugar de
   parámetros inventados.
3. **Proyectar.** Estimar qué ocurre si la temperatura media sube y la
   infraestructura no se renueva.

Objetivos secundarios: simular una renovación de infraestructura, y simular
el efecto de un uso más medido de los equipos de aire.

## La abstracción

El sistema se modela con tres componentes lógicos:

| Componente | Qué representa |
|---|---|
| **Hogares** | Demanda ligada a la temperatura ambiental |
| **Nodos** | Transformadores con un límite de carga físico |
| **SADI** | Suministro macro del sistema nacional |

Y un ciclo de cuatro pasos por unidad de tiempo:

```
1. Entrada      lectura de datos climáticos
2. Reacción     ajuste de la demanda residencial
3. Evaluación   medición del estrés en los nodos
4. Falla        desconexión por sobrecarga térmica
```

Ese ciclo es el esqueleto del modelo y se implementa en el procedimiento `go`
de NetLogo. Ver [5 · El modelo en NetLogo](5-El-modelo-en-NetLogo).

## Por qué esta escala

El modelo trabaja hoy sobre las **6 subestaciones AT/MT** que publica la
Secretaría de Energía para la concesión de EDELAP. Es el nivel más fino del
que existe dato público con clientes y potencia instalada.

Los apagones reales de La Plata ocurren un escalón más abajo, en los
transformadores de barrio (MT/BT), de los que no hay dato abierto. El módulo
de reglas está escrito en unidades **por cliente y por capacidad instalada**
justamente para que bajar ese escalón no obligue a reescribirlo.
