[![GitHub](https://img.shields.io/badge/GitHub-Julia--HiGHS-181717?style=for-the-badge&logo=github)](https://github.com/jonyblan/Julia-HiGHS)
[![CI](https://github.com/jonyblan/Julia-HiGHS/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/jonyblan/Julia-HiGHS/actions/workflows/pipeline.yaml)

# Julia-HiGHS

Proyecto de Autómatas, Teoría de Lenguajes y Compiladores. Es un DSL para escribir problemas de optimización usando una sintaxis más corta que la de JuMP. El compilador está escrito en C con Flex y Bison. No resuelve el modelo por sí mismo: genera un programa Julia que usa JuMP + HiGHS.

## Cómo correrlo

Con Docker abierto, desde la raíz del repo:

```bash
docker compose run --rm compiler
bash src/main/bash/build.sh
bash src/main/bash/test.sh
```

Para probar un archivo de entrada:

```bash
bash src/main/bash/run.sh src/test/c/accept/01-basic.txt
cat output/program.jl
```

El archivo entra por stdin (lo hace `run.sh`). Si todo salió bien, en `output/` quedan `program.txt`, `program.jl` y `program.ipynb`. Los primeros dos tienen el mismo código Julia; el tercero es un notebook con una celda para ejecutarlo en Jupyter. Antes de cada ejecución se limpian los resultados anteriores.

Para ejecutar el código generado necesitás tener Julia con JuMP y HiGHS instalados. No vienen instalados en el contenedor de compilación.

```bash
julia -e 'using Pkg; Pkg.add(["JuMP", "HiGHS"])' # la primera vez
julia output/program.jl
```

Para abrir el notebook en Jupyter también necesitás IJulia.

## Ejemplo mínimo

Archivo de entrada:

```text
a<=5
max a
print(a)
```

Se genera, entre otras cosas:

```julia
using JuMP
using HiGHS
using LinearAlgebra
model = Model(HiGHS.Optimizer)
@variable(model, a <= 5)
@objective(model, Max, a)
optimize!(model)
if has_values(model)
    println(value(a))
else
    println("No primal solution; status = ", termination_status(model))
end
```

## Qué acepta por ahora

- Variables con cotas: `x <= 10`, `float x >= -3.5`, `int n >= 0`, `binary activo`.
- Vectores y matrices: `vector v[3] >= 0`, `matrix m[2,2] >= 0`.
- Constantes y restricciones: `const LIM = 10`, `x + y <= LIM`, `x == y`.
- Objetivo: `max 2*x + y`, `min x`, `Rta = max x`.
- Salida: `print(x)`, `print(Rta)`, `print("hola")`, `silent`, `!print(x)`.
- Comentarios con `#`, `//` o `/* ... */`.
- Operaciones `+`, `-`, `*`, `/`, índices `v[1]` y `m[1,2]`, y funciones `sum`, `dot`, `cross`, `trace`, `matmul`.

Hay un solo modelo y un solo objetivo por archivo. Los nombres de variables distinguen mayúsculas y minúsculas.

Una comparación como `x <= 5` declara `x` si todavía no existe; una comparación posterior sobre `x` es una restricción. En una declaración la cota tiene que ser constante (las binarias son la excepción). Las restricciones deben ser lineales y la función objetivo puede ser lineal o cuadrática. La división sólo admite denominadores constantes distintos de cero.

Ojo con los modelos cuadráticos: que se genere Julia válido no significa que HiGHS pueda resolver cualquier objetivo, especialmente los no convexos (`max x*y`, por ejemplo). Si el solver no devuelve solución, el código evita llamar a `value` sobre una solución inexistente.

## Archivos

- `src/main/c/frontend/`: Flex, Bison y el AST.
- `src/main/c/backend/`: chequeo semántico y generador de Julia.
- `src/main/c/support/`: tipos, logs y utilidades del proyecto base.
- `src/test/c/accept/` y `src/test/c/reject/`: tests de entrada.
- `doc/Especificacion.pdf`: especificación.

Los archivos de la calculadora vieja (`backend/domain-specific`) siguen ahí para consultar el ejemplo, pero ya no se compilan.
