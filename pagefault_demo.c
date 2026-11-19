/*
 * pagefault_demo.c - VmRSS antes y despues de tocar 100 MB reservados con malloc
 * Compilar: gcc -o pagefault_demo pagefault_demo.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_BYTES (100UL * 1024 * 1024)   /* 100 MB = 25 600 paginas de 4 KB */

/* Imprime VmPeak, VmSize y VmRSS del propio proceso */
static void show_mem(const char *etiqueta)
{
    printf("--- %s ---\n", etiqueta);
    fflush(stdout);                     /* evita que system() adelante su salida */
    system("grep -E \"VmPeak|VmSize|VmRSS\" /proc/$PPID/status");
}

/* Lee minflt (campo 10) y majflt (campo 12) de /proc/self/stat */
static void show_faults(const char *etiqueta)
{
    unsigned long minflt = 0, majflt = 0;
    FILE *f = fopen("/proc/self/stat", "r");
    if (f) {
        fscanf(f, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %lu %*u %lu", &minflt, &majflt);
        fclose(f);
    }
    printf("%-28s minflt=%lu majflt=%lu\n", etiqueta, minflt, majflt);
}

int main(void)
{
    char *buf = (char *)malloc(N_BYTES);
    if (!buf) { perror("malloc"); return 1; }

    show_mem("Antes del memset");
    show_faults("Faults antes del memset:");

    memset(buf, 0xAA, N_BYTES);         /* toca cada pagina: demand paging */

    show_mem("Despues del memset");
    show_faults("Faults despues del memset:");

    free(buf);
    return 0;
}
