/*
 * mmap_fault_cost.c - Costo de un minor page fault individual con mmap
 * Compilar: gcc -o mmap_fault_cost mmap_fault_cost.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>

#define N_PAGES 200

static double elapsed_ns(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
}

int main(int argc, char **argv)
{
    long page_size = sysconf(_SC_PAGESIZE);
    size_t total = (size_t)N_PAGES * page_size;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (argc > 1) flags |= MAP_POPULATE;    /* ./mmap_fault_cost populate */

    /* mmap anonimo: reserva direcciones virtuales sin RAM fisica asociada */
    char *base = mmap(NULL, total, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (base == MAP_FAILED) { perror("mmap"); return 1; }

    struct timespec t0, t1;
    double sum_first = 0, sum_second = 0;

    for (int i = 0; i < N_PAGES; i++) {
        char *page = base + (size_t)i * page_size;

        /* Primer toque: la pagina aun no tiene marco fisico -> minor page fault */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        page[0] = 1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sum_first += elapsed_ns(t0, t1);

        /* Segundo toque: la pagina ya esta mapeada, no hay fault */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        page[0] = 2;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sum_second += elapsed_ns(t0, t1);
    }

    printf("Modo: %s, %d paginas de %ld bytes\n",
           (flags & MAP_POPULATE) ? "MAP_POPULATE" : "demand paging", N_PAGES, page_size);
    printf("Costo promedio primer toque (con fault):  %8.1f ns\n", sum_first / N_PAGES);
    printf("Costo promedio segundo toque (sin fault): %8.1f ns\n", sum_second / N_PAGES);
    printf("Overhead atribuible al page fault:        %8.1f ns\n",
           (sum_first - sum_second) / N_PAGES);

    munmap(base, total);
    return 0;
}
