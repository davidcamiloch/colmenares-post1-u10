/*
 * mymaps.c - Proceso minimo que se pausa para inspeccionar /proc/PID/maps
 * Compilar: gcc -o mymaps mymaps.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int global_data = 42;          /* segmento .data   */
const char *msg = "hola";      /* cadena en .rodata */

int main(void)
{
    void *heap = malloc(1024 * 1024);   /* 1 MB en el heap */
    printf("PID: %d\n", getpid());
    printf("stack var addr: %p\n", (void *)&heap);
    printf("heap addr:      %p\n", heap);
    printf("global addr:    %p\n", (void *)&global_data);
    printf("rodata addr:    %p\n", (void *)msg);
    printf("main addr:      %p\n", (void *)main);
    printf("Presione Enter para continuar...\n");
    fflush(stdout);
    getchar();                          /* mantiene vivo el proceso */
    free(heap);
    return 0;
}
