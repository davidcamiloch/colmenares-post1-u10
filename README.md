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

## Referencias

- Drepper, U. (2007). *What every programmer should know about memory*. Red Hat. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- GCC Team. (s.f.). *Optimize Options*. GNU Compiler Collection. https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- Kerrisk, M. (s.f.). *proc_pid_smaps(5)*. Linux manual pages. https://man7.org/linux/man-pages/man5/proc_pid_smaps.5.html
- Rey Castillo, J. R. (s.f.). *Memorias del computador: benchmark de jerarquía de caché y análisis de memoria virtual en C sobre Linux*. Material del curso Arquitectura de Computadores, UFPS.
