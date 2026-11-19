/*
 * cache_bench.c - Benchmark de la jerarquia de cache (Parte 1, Unidad 10)
 * Compilar: gcc -O0 -o cache_bench cache_bench.c
 *
 * Mide la latencia media de acceso a memoria:
 *   bench_seq    acceso secuencial byte a byte (ns/byte)
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define REPEAT      100
#define KB          1024UL
#define MB          (1024UL * KB)
#define LINE_SIZE   64UL        /* tamano de linea de la cache (coherency_line_size) */
#define PAGE_SIZE   4096UL      /* tamano de pagina = 48 KB / 12 vias en la L1d     */

/* Tiempo transcurrido entre dos marcas, en nanosegundos */
static double elapsed_ns(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
}

/* Acceso secuencial de lectura a un array de n_bytes: devuelve ns/byte */
double bench_seq(size_t n_bytes)
{
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void *)arr, 1, n_bytes);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < REPEAT; r++)
        for (size_t i = 0; i < n_bytes; i++)
            (void)arr[i];                   /* acceso de lectura */
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free((void *)arr);
    return elapsed_ns(t0, t1) / (REPEAT * (double)n_bytes);
}

int main(void)
{
    size_t sizes[] = {
        4 * KB, 8 * KB, 16 * KB, 32 * KB,       /* region L1d (48 KB)    */
        64 * KB, 128 * KB, 256 * KB, 512 * KB,  /* region L2 (1 280 KB)  */
        1 * MB, 4 * MB, 8 * MB,                 /* region L3 (12 288 KB) */
        16 * MB, 64 * MB                        /* region RAM            */
    };
    int n = (int)(sizeof(sizes) / sizeof(sizes[0]));

    printf("%-12s  %12s\n", "Array (KB)", "seq ns/byte");
    for (int i = 0; i < n; i++)
        printf("%-12zu  %12.3f\n", sizes[i] / KB, bench_seq(sizes[i]));
    return 0;
}
