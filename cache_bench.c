/*
 * cache_bench.c - Benchmark de la jerarquia de cache (Parte 1, Unidad 10)
 * Compilar: gcc -O0 -o cache_bench cache_bench.c
 *
 * Mide la latencia media de acceso a memoria con tres patrones:
 *   bench_seq    acceso secuencial byte a byte (ns/byte)
 *   bench_rand   acceso aleatorio con indices mezclados por Fisher-Yates (ns/acceso)
 *   bench_stride acceso con salto fijo para exponer conflictos de asociatividad
 *   bench_chase  persecucion de punteros: cada carga depende de la anterior, asi
 *                que el tiempo por carga es la latencia real del nivel que la sirve
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
#define STRIDE_ARR  (1UL * MB)  /* 1 MB: cabe completo en la L2 de 1 280 KB         */
#define CHASE_STEPS 4000000L    /* cargas dependientes por medicion                 */
#define CHASE_RUNS  5           /* se reporta el minimo de 5 corridas (menos ruido) */

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

/* Acceso aleatorio usando indices pre-mezclados (Fisher-Yates): ns/acceso */
double bench_rand(size_t n_bytes)
{
    size_t n = n_bytes / sizeof(int);
    int *arr = (int *)malloc(n * sizeof(int));
    size_t *idx = (size_t *)malloc(n * sizeof(size_t));
    if (!arr || !idx) { free(arr); free(idx); return -1.0; }

    for (size_t i = 0; i < n; i++) { arr[i] = (int)i; idx[i] = i; }
    for (size_t i = n - 1; i > 0; i--) {    /* mezcla Fisher-Yates */
        size_t j = (size_t)rand() % (i + 1);
        size_t tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
    }

    struct timespec t0, t1;
    volatile long sum = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < REPEAT; r++)
        for (size_t i = 0; i < n; i++)
            sum += arr[idx[i]];
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free(arr); free(idx);
    return elapsed_ns(t0, t1) / (REPEAT * (double)n);
}

/* Acceso con stride fijo sobre n_bytes: expone conflictos de asociatividad */
double bench_stride(size_t n_bytes, size_t stride)
{
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void *)arr, 1, n_bytes);

    size_t steps = n_bytes / stride;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < REPEAT; r++)
        for (size_t i = 0; i < steps; i++)
            (void)arr[(i * stride) % n_bytes];
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free((void *)arr);
    return elapsed_ns(t0, t1) / (REPEAT * (double)steps);
}

/*
 * Persecucion de punteros sobre n_bytes: un nodo cada `stride` bytes, enlazados
 * en un solo ciclo. Con shuffle = 1 el orden es aleatorio (algoritmo de Sattolo);
 * con shuffle = 0 el orden es creciente. Devuelve ns por carga dependiente.
 */
double bench_chase(size_t n_bytes, size_t stride, int shuffle)
{
    size_t nodes = n_bytes / stride;
    char *arr = (char *)malloc(n_bytes);
    size_t *order = (size_t *)malloc(nodes * sizeof(size_t));
    if (!arr || !order || nodes < 2) { free(arr); free(order); return -1.0; }
    memset(arr, 0, n_bytes);

    for (size_t i = 0; i < nodes; i++) order[i] = i;
    if (shuffle)
        for (size_t i = nodes - 1; i > 0; i--) {     /* Sattolo: un solo ciclo */
            size_t j = (size_t)rand() % i;
            size_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
        }
    for (size_t i = 0; i < nodes; i++)               /* nodo i apunta al i+1 */
        *(char **)(arr + order[i] * stride) = arr + order[(i + 1) % nodes] * stride;

    char *p = arr + order[0] * stride;
    for (size_t i = 0; i < nodes; i++) p = *(char * volatile *)p;   /* calentamiento */

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long i = 0; i < CHASE_STEPS; i++)
        p = *(char * volatile *)p;                   /* carga dependiente */
    clock_gettime(CLOCK_MONOTONIC, &t1);

    volatile char *sink = p; (void)sink;
    free(arr); free(order);
    return elapsed_ns(t0, t1) / (double)CHASE_STEPS;
}

/* Minimo de CHASE_RUNS mediciones: descarta interrupciones del host y de la VM */
double chase_min(size_t n_bytes, size_t stride, int shuffle)
{
    double best = bench_chase(n_bytes, stride, shuffle);
    for (int k = 1; k < CHASE_RUNS; k++) {
        double t = bench_chase(n_bytes, stride, shuffle);
        if (t < best) best = t;
    }
    return best;
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

    srand(12345);
    printf("%-10s %11s %14s %13s %13s\n", "Array(KB)", "seq ns/byte",
           "rand ns/acceso", "chase-seq ns", "chase-rand ns");
    for (int i = 0; i < n; i++) {
        double seq = bench_seq(sizes[i]);
        double rnd = bench_rand(sizes[i]);
        double cseq = chase_min(sizes[i], LINE_SIZE, 0);
        double crnd = chase_min(sizes[i], LINE_SIZE, 1);
        printf("%-10zu %11.3f %14.3f %13.3f %13.3f\n",
               sizes[i] / KB, seq, rnd, cseq, crnd);
    }

    printf("\n%-12s  %10s   (array de %lu KB)\n", "Stride", "ns/acceso", STRIDE_ARR / KB);
    double t_line = bench_stride(STRIDE_ARR, LINE_SIZE);
    double t_page = bench_stride(STRIDE_ARR, PAGE_SIZE);
    printf("%-12s  %10.3f\n", "64 B", t_line);
    printf("%-12s  %10.3f\n", "4096 B", t_page);

    /* Misma prueba con cargas dependientes; 4160 B = 4096 + 64 es el control:
       toca las mismas 256 paginas pero cada acceso cae en un set distinto */
    printf("\n%-12s  %14s   (persecucion de punteros, minimo de 5, %lu KB)\n",
           "Stride", "ns/carga", STRIDE_ARR / KB);
    printf("%-12s  %14.3f\n", "64 B", chase_min(STRIDE_ARR, LINE_SIZE, 0));
    printf("%-12s  %14.3f\n", "4096 B", chase_min(STRIDE_ARR, PAGE_SIZE, 0));
    printf("%-12s  %14.3f\n", "4160 B", chase_min(STRIDE_ARR, PAGE_SIZE + LINE_SIZE, 0));
    return 0;
}
