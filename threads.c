#include <stdio.h>
#include <pthread.h>

void* print_message(void* ptr) {
    char* message;
    message = (char*) ptr;
    printf("%s\n", message);
    return NULL;
}

int main() {
    pthread_t thread1, thread2;
    char* message1 = "Hilo 1";
    char* message2 = "Hilo 2";

    // Crear hilos
    pthread_create(&thread1, NULL, print_message, (void*) message1);
    pthread_create(&thread2, NULL, print_message, (void*) message2);

    // Esperar a que los hilos terminen
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("Los hilos han terminado. \n");
    return 0;
}