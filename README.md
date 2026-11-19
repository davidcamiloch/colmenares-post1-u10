# colmenares-post1-u10

Laboratorio del Post-contenido de la Unidad 10 (Memorias del Computador) de Arquitectura de Computadores, UFPS. Autor: David Camilo Colmenares Hernández.

La Parte 1 mide en C la latencia de acceso a memoria para arrays de 4 KB a 64 MB y relaciona cada salto con un nivel de la jerarquía de caché; incluye acceso secuencial, aleatorio y con stride fijo para exponer conflictos de asociatividad. La Parte 2 examina la memoria virtual de un proceso Linux con `/proc/PID/maps`, `/proc/PID/smaps`, `pmap -x`, `/proc/self/status` y `/usr/bin/time -v`, y mide el costo de un minor page fault individual con `mmap`.

## Entorno

| Elemento | Valor |
|----------|-------|
| Sistema | Ubuntu 22.04.5 LTS, kernel 6.8.0 x86_64 (máquina virtual Hyper-V, 2 CPU lógicas, 3,8 GiB) |
| Procesador | Intel Core i5-12450H (Alder Lake), núcleo P |
| Compilador | gcc 11.4.0 |
| Herramientas | `pmap` (procps-ng 3.3.17), `vmstat`, `free`, `/usr/bin/time`, `strace`, `getconf` |

Los programas de la Parte 1 se compilaron y ejecutaron en `/tmp/u10post1` y los de la Parte 2 en `/tmp/u10post2`. `perf` no está disponible dentro de la máquina virtual: el hipervisor no expone los contadores de rendimiento del procesador. Por eso todas las mediciones usan `clock_gettime(CLOCK_MONOTONIC)`, que es lo que exige la guía. La carpeta `capturas/` contiene las imágenes de la terminal con la salida de cada comando.

```
gcc -O0 -Wall -o cache_bench cache_bench.c && ./cache_bench
gcc -o mymaps mymaps.c && (sleep 3 | ./mymaps) &
gcc -o pagefault_demo pagefault_demo.c && ./pagefault_demo
gcc -o mmap_fault_cost mmap_fault_cost.c && ./mmap_fault_cost [populate]
gcc -o populate_waste populate_waste.c && ./populate_waste [populate]
```

## Parte 1: Benchmark de jerarquía de caché

### Checkpoint 1: niveles de caché detectados

`/sys/devices/system/cpu/cpu0/cache/` y `lscpu` reportan la geometría completa del núcleo.

| Índice | Nivel | Tipo | Tamaño | Vías | Sets | Línea |
|--------|-------|------|--------|------|------|-------|
| index0 | L1 | Datos | 48 KB | 12 | 64 | 64 B |
| index1 | L1 | Instrucciones | 32 KB | 8 | 64 | 64 B |
| index2 | L2 | Unificada | 1 280 KB | 10 | 2 048 | 64 B |
| index3 | L3 | Unificada | 12 288 KB | 8 | 24 576 | 64 B |

La L1 de datos de un núcleo P de Alder Lake mide 48 KB y no 32 KB como el ejemplo de la guía. Los puntos de inflexión esperados quedan entonces en 48 KB, 1 280 KB y 12 MB.

### Checkpoint 2: saltos de latencia por nivel

`cache_bench.c` conserva las tres funciones de la guía (`bench_seq`, `bench_rand`, `bench_stride`) y agrega `bench_chase`, una persecución de punteros: cada nodo ocupa una línea de 64 bytes y guarda la dirección del siguiente, de modo que cada carga depende de la anterior y el tiempo por carga es la latencia del nivel que la sirve. `chase_min` repite cada medición 5 veces y reporta el mínimo para descartar interrupciones del host.

| Array (KB) | seq ns/byte | rand ns/acceso | chase-seq ns | chase-rand ns | Nivel que sirve chase-rand |
|-----------:|------------:|---------------:|-------------:|--------------:|----------------------------|
| 4 | 0,491 | 1,411 | 1,164 | 1,208 | L1 |
| 32 | 0,470 | 0,815 | 1,194 | 1,162 | L1 |
| 64 | 0,502 | 1,053 | 1,227 | **3,515** ← | L2 (supera los 48 KB de L1) |
| 256 | 0,531 | 1,067 | 1,252 | 3,548 | L2 |
| 512 | 0,507 | 1,100 | 1,232 | 4,065 | L2 con fallos de DTLB |
| 1 024 | 0,507 | 1,407 | 1,240 | 7,541 | L2 al límite y L3 |
| 4 096 | 0,520 | 2,793 | 1,526 | **26,478** ← | L3 (supera los 1 280 KB de L2) |
| 8 192 | 0,530 | 6,096 | 3,958 | **92,755** ← | L3 al límite y STLB |
| 16 384 | 0,532 | 8,507 | 5,110 | 110,187 | RAM |
| 65 536 | 0,529 | 10,189 | 5,317 | 116,860 | RAM |

La columna `chase-rand` muestra los tres saltos con claridad. La latencia de L1 es 1,16 ns, unos 5 ciclos a 4,4 GHz. Al pasar de 32 a 64 KB salta a 3,5 ns, la latencia de L2. Entre 1 y 4 MB salta a 26 ns, la latencia de L3. Desde 8 MB llega a 93 y 117 ns, la latencia de la RAM más el recorrido de la tabla de páginas. El salto a RAM aparece antes de los 12 MB porque la L3 es compartida con el resto de núcleos del host y porque desde 8 MB (2 048 páginas) se agota la STLB.

La columna `seq` de la guía se mantiene plana en 0,5 ns/byte para todos los tamaños. Con `-O0` cada byte cuesta varias instrucciones de bucle, y el recorrido secuencial lo detecta el prefetcher de hardware, que trae las líneas antes de que el bucle las pida. El acceso secuencial byte a byte mide el bucle y no la memoria; por eso se agregó `bench_chase`.

### Checkpoint 3: acceso secuencial frente a aleatorio

| Array (KB) | chase-rand / chase-seq | rand / seq |
|-----------:|-----------------------:|-----------:|
| 4 a 32 | 1,0 | 1,7 a 3,3 |
| 64 a 512 | 1,5 a 3,3 | 2,0 a 2,2 |
| 1 024 | 6,1 | 2,8 |
| 4 096 | 17,4 | 5,4 |
| 8 192 a 65 536 | 21,6 a 23,4 | 11,5 a 19,3 |

Mientras el array cabe en L1 el orden no importa: ambos recorridos se sirven desde L1 y la razón es 1,0. Al superar L1 el recorrido aleatorio pierde la localidad espacial y cada carga es un fallo de L1; el secuencial sigue en 1,2 ns porque el prefetcher de hardware trae la línea siguiente a tiempo. Desde 1 MB el recorrido aleatorio también falla en el TLB: la DTLB de primer nivel cubre unos pocos cientos de KB y la STLB 2 048 páginas (8 MB). Por eso la razón crece de 6,1 a 1 MB a 17,4 a 4 MB (fallo de L2 más fallo de DTLB) y a 23,4 a 8 MB (fallo de L3 más fallo de STLB con recorrido de la tabla de páginas). El `bench_rand` de la guía muestra la misma tendencia (de 1,1 a 10,2 ns) pero atenuada, porque sus cargas son independientes y el procesador fuera de orden superpone varios fallos a la vez.

### Checkpoint 4: conflictos de asociatividad con stride

| Stride sobre 1 MB | `bench_stride` ns/acceso | `bench_chase` ns/carga | Sets de L1d que toca |
|-------------------|-------------------------:|-----------------------:|---------------------:|
| 64 B | 2,618 | 2,049 | 64 |
| 4 096 B | 2,467 | **5,532** | 1 |
| 4 160 B (control) | — | 2,449 | 64 |

El array de 1 MB cabe en la L2 de 1 280 KB en los tres casos. Con cargas dependientes, el stride de 4 096 bytes cuesta 5,5 ns por carga frente a 2,0 ns del stride de 64 bytes. La diferencia no viene del tamaño de trabajo, que es el mismo, sino de la asociatividad: las 256 direcciones separadas por 4 096 bytes comparten los bits 6 a 11, caen todas en el mismo set de la L1d y solo 12 caben a la vez. Cada carga es un fallo de conflicto y se sirve desde L2. El stride de 4 160 bytes (4 096 + 64) es el control: toca las mismas 256 páginas, así que tiene el mismo patrón de TLB, pero cada acceso cae en un set distinto y vuelve a 2,4 ns.

`bench_stride` de la guía no muestra la diferencia (2,6 frente a 2,5 ns). Cada iteración calcula `(i * stride) % n_bytes` con una división de 64 bits, que a `-O0` cuesta más que el fallo de L1, y las cargas son independientes: el procesador lanza la siguiente sin esperar la anterior y oculta los 15 ciclos de L2. La persecución de punteros elimina las dos cosas.

### Desviaciones respecto a la guía (Parte 1)

- Se agregó `bench_chase` con `chase_min`, porque `bench_seq` y `bench_stride` miden con `-O0` el costo del bucle y no la latencia de la memoria. La carga del puntero usa `*(char * volatile *)p` para que `-O2` no saque el bucle de entre las dos llamadas a `clock_gettime`.
- La L1d detectada mide 48 KB con 12 vías y no 32 KB con 8 vías; el número de sets sigue siendo 64, así que el stride de conflicto sigue siendo 4 096 bytes.
- `bench_rand` libera los dos bloques si uno de los `malloc` falla, en lugar de perder el otro.
- Los tamaños de array usan las constantes `KB` y `MB`, con un comentario que indica el nivel de caché de cada grupo.

### Decisión técnica 1: qué stride expone mejor los conflictos de asociatividad

El stride que lleva todos los accesos al mismo set es el tamaño de la caché dividido entre su número de vías, porque ese es el tamaño de una vía y dos direcciones separadas por una vía completa tienen el mismo índice de set. Para la L1d de 32 KB y 8 vías de la guía el resultado es 4 096 bytes; para la L1d de 48 KB y 12 vías de este procesador también es 4 096 bytes, porque ambas tienen 64 sets de líneas de 64 bytes. El índice de set lo forman los bits 6 a 11 de la dirección: los 6 bits bajos eligen el byte dentro de la línea y los 6 siguientes eligen uno de los 64 sets. Un stride de 4 096 (2^12) nunca cambia los bits 6 a 11. Un stride de 2 048 (2^11) sí cambia el bit 11 en cada paso y alterna entre dos sets; la captura `p1-cp5-geometria-stride.png` lo confirma con 512 accesos repartidos en 2 sets. Sobre 1 MB siguen siendo 256 líneas por set, así que 2 048 también provoca conflictos, pero reparte la presión entre dos sets y aprovecha 24 vías en lugar de 12. Con 4 096 bytes compiten 256 líneas por un set de 12. Desde el acceso número 13 dentro del set, que es el noveno en la L1 de 8 vías de la guía, cada línea nueva debe desalojar a otra según la política de reemplazo; como el recorrido es cíclico, la línea desalojada es justo la que se volverá a pedir. Cada carga baja entonces a la latencia de L2, 5,5 ns medidos frente a 1,2 ns del recorrido secuencial del Checkpoint 2 que se sirve desde L1. La regla es calcular el stride de conflicto como tamaño de caché entre vías y verificarlo con un stride de control que cambie el set.

### Decisión técnica 2: `-fno-prefetch-loop-arrays` y el prefetcher de hardware

`-fprefetch-loop-arrays` y `-fno-prefetch-loop-arrays` solo controlan si gcc inserta instrucciones `prefetcht0` en el código que genera; el prefetcher de hardware vive en el procesador, observa las direcciones en tiempo de ejecución y funciona igual con cualquiera de los dos binarios. La captura `p1-cp5-prefetch-asm.png` lo muestra: con `-O2 -fprefetch-loop-arrays` el ensamblador contiene 8 instrucciones `prefetcht0` y con `-fno-prefetch-loop-arrays` contiene 0; las 8 están en los bucles de inicialización y de recorrido de `bench_rand` y `bench_chase`, y ninguna en `bench_seq`, porque gcc no inserta prefetch para accesos `volatile`. El patrón secuencial es el que el compilador puede predecir y el más sensible a la diferencia; en `bench_rand` el compilador anticipa el arreglo `idx`, que se lee en orden, y a 1 MB la medición baja de 0,846 a 0,601 ns. El acceso a `arr[idx[i]]` sigue una mezcla Fisher-Yates y el recorrido de stride 4 096 depende de conflictos de asociatividad; ningún prefetch insertado en compilación puede anticiparlos, y por eso a 64 MB los dos binarios dan 9,1 ns y el stride de 4 096 da 3,5 y 3,7 ns. La columna `seq` es idéntica con ambos flags (0,13 a 0,17 ns/byte) porque el prefetcher de hardware ya cubre ese recorrido. El prefetcher de hardware solo se desactiva con registros MSR o desde la BIOS, y dentro de una máquina virtual ni siquiera eso está disponible. La conclusión válida es limitada: la comparación mide el aporte del prefetch por software de gcc sobre un procesador que ya hace prefetch por hardware, no el valor del prefetching en general. La regla es atribuir a `-fprefetch-loop-arrays` solo las instrucciones que aparecen en el ensamblador.

## Referencias

- Drepper, U. (2007). *What every programmer should know about memory*. Red Hat. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- GCC Team. (s.f.). *Optimize Options*. GNU Compiler Collection. https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- Kerrisk, M. (s.f.). *proc_pid_smaps(5)*. Linux manual pages. https://man7.org/linux/man-pages/man5/proc_pid_smaps.5.html
- Rey Castillo, J. R. (s.f.). *Memorias del computador: benchmark de jerarquía de caché y análisis de memoria virtual en C sobre Linux*. Material del curso Arquitectura de Computadores, UFPS.
