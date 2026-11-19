/*
 * populate_waste.c - Reserva 100 MB con mmap y toca solo 10 MB.
 * Compara la RAM residente con demand paging y con MAP_POPULATE.
 * Compilar: gcc -o populate_waste populate_waste.c
 * Uso: ./populate_waste            (demand paging)
 *      ./populate_waste populate   (MAP_POPULATE)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>

#define RESERVA (100UL * 1024 * 1024)  /* 100 MB reservados */
#define USO     (10UL * 1024 * 1024)   /* 10 MB realmente usados */

static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (argc > 1) flags |= MAP_POPULATE;

    double t0 = now_ms();
    char *p = mmap(NULL, RESERVA, PROT_READ | PROT_WRITE, flags, -1, 0);
    double t1 = now_ms();
    if (p == MAP_FAILED) { perror("mmap"); return 1; }

    memset(p, 0x55, USO);                 /* solo se usan 10 MB */
    double t2 = now_ms();

    printf("Modo %-14s mmap: %7.2f ms  uso de 10 MB: %6.2f ms\n",
           (flags & MAP_POPULATE) ? "MAP_POPULATE" : "demand paging", t1 - t0, t2 - t1);
    fflush(stdout);
    char cmd[64];
    snprintf(cmd, sizeof cmd, "grep VmRSS /proc/%d/status", (int)getpid());
    system(cmd);
    munmap(p, RESERVA);
    return 0;
}
