# 6 · Supuestos y límites

Lo que es hipótesis nuestra y no dato. Están todos acá a propósito: en una
defensa conviene decirlos antes de que los pregunten.

## 1 · El reparto de la demanda (`escala` = 0,50)

La demanda que publica CAMMESA es de **toda la concesión de EDELAP**. El
modelo solo representa 6 subestaciones. `escala` es la fracción de esa demanda
regional que se le asigna a la red modelada.

Hay dos anclas y miden cosas distintas:

| Ancla | Cuenta | Valor | Qué es |
|---|---|---|---|
| Clientes | 196.972 / ~400.000 | **0,49** | **estimación** |
| Capacidad | 382,5 MW / 625 MW | 0,61 | **techo**, no estimación |

La primera estima qué porción de demanda corresponde a esos clientes. EDELAP
declara unos 400.000 usuarios en toda la concesión (El Día, agosto de 2026).

La segunda no estima nada: dice que por encima de 0,61 las 6 subestaciones
quedan sobrecargadas sí o sí en el pico, porque su potencia instalada
(402,7 MVA, o 382,5 MW a factor de potencia 0,95) no alcanza para más.

**Se adopta 0,50**, el ancla de clientes redondeada al paso del deslizador. Es
el único valor atado a una cantidad medida. El rango **0,50 – 0,61** queda como
banda de sensibilidad y los resultados conviene presentarlos como banda.

Qué da, corriendo el 03/02/2026:

| escala | ΔT | MT095 máx. | Cortes |
|---|---|---|---|
| **0,50** | 0 | **92,7 %** | ninguno |
| **0,50** | +3 °C | **124,0 %** | 3 de MT095 |
| 0,55 | 0 | 119,7 % | 1 de MT095 |
| 0,61 | 0 | 124,0 % | 5 de MT095 |
| 0,61 | +3 °C | 124,0 % | 6 de MT095, 3 de MT093 |

## 2 · La capacidad de los nodos de alta tensión

La capa oficial de AT/AT publica ubicación, no potencia. Se estima como la
**suma de la de sus hijos**. Es una hipótesis.

## 3 · El orden de caída es fijo

La demanda se reparte proporcional a `clientes`, así que el factor de carga de
cada nodo es `clientes_i / pot_i` multiplicado por un mismo número. Ese
cociente no cambia con la temperatura, ni con la hora, ni con `escala`:

| | clientes/kVA | % del estrés de MT095 |
|---|---|---|
| MT095 | 0,631 | 100 % |
| MT093 | 0,540 | 86 % |
| MT099 | 0,447 | 71 % |
| MT195 | 0,340 | 54 % |
| MT194 | 0,325 | 52 % |
| MT092 | 0,286 | 45 % |

MT092 necesitaría `escala` 1,19 o 51,7 °C para llegar al 100 %. Es decir que
con este reparto **tres de los seis nodos no pueden caerse**.

No es un error: refleja que MT095 tiene 1,59 kVA por cliente (zona residencial
densa) y MT092 tiene 3,49 (más industria), y el golpe de los aires pega donde
hay hogares. Pero hay que presentarlo como un **resultado** y no como una
simulación cuyo ranking se decide antes de apretar `go`.

**Cómo se arregla:** diferenciando el perfil de cada zona, separando consumo
residencial de no residencial, en lugar de aplicarle la misma curva a todos.

## 4 · El período no cubre el invierno

Los datos van de febrero a mayo, con un mínimo de 7,3 °C. El efecto de
calefacción existe —el BIC lo detecta en 13 y 17 °C— pero se deja afuera a
propósito: el proyecto estudia olas de calor.

## 5 · La rama caliente se apoya en seis días

Arriba de 34 °C hay 6 días, todos de febrero. La pendiente caliente vale entre
**4,19 y 4,71 %/°C** según cuál se saque. Arriba de 37,8 °C no hay datos y la
tabla extrapola con esa pendiente.

## 6 · La temperatura es horaria

Se repite en las 12 muestras de 5 minutos. Si el modelo corre con paso de 5
minutos, la temperatura es la misma durante 12 ticks. No es un problema —la
demanda térmica no cambia en 5 minutos— pero conviene saberlo al leer los
gráficos.

## 7 · La temperatura es un dato modelado, no medido

Viene del NSRDB, y ahí la temperatura se toma del reanálisis MERRA-2 de la
NASA: un modelo que reconstruye el estado de la atmósfera sobre una grilla. Es
consistente y sin huecos, pero no es la medición de una estación local.

Falta registrar qué producto del NSRDB se usó y con qué coordenadas, y corregir
la presentación inicial, que atribuye la temperatura al Servicio Meteorológico
Nacional. Ver [2 · Datos y limpieza](2-Datos-y-limpieza).

## 8 · La demanda incluye todo

No solo residencial: hay comercio e industria. Atribuir el crecimiento térmico
enteramente a los aires domésticos es una simplificación.

## 9 · Hoy no es un modelo de agentes

El consumo sale de una regla agregada aplicada a cada nodo. No hay un hogar que
decida prender el aire. Convertirlo es el punto que sigue, y la curva medida
pasa entonces a ser el **criterio de validación**: si la suma de los hogares
reproduce la curva de CAMMESA, el modelo está bien calibrado.

---

## Lo que falta, en orden

1. **Correr los noventa días.** Hoy se corre un día. Para decir cuántos
   apagones tiene un verano hay que correr el período entero bajo cada
   escenario.
2. **Los hogares como agentes.** Dar vuelta la curva para que diga *cuántos
   hogares prenden el aire* en lugar de *cuánto consume la región*. La forma de
   esa probabilidad ya está medida: es `k(T) − 1`. Lo único que falta suponer
   es un escalar, qué fracción tiene el aire prendido el día más caluroso, y
   ese escalar puede ser el deslizador.
3. **Las tres salidas:** frecuencia de falla por nodo, fechas críticas de
   saturación y mapa de vulnerabilidad barrial.
4. **Los escenarios a futuro:** los mismos noventa días con +1,5 °C y +3 °C.
5. **Bajar un nivel**, de las seis subestaciones a los transformadores de
   barrio, que es donde ocurren los apagones reales.
