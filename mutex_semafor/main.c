#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <getopt.h>
#include "../functions.h"

// to jest liczba klientow gotowych do strzyzenia; 0 gdy nie ma nikogo w poczekalni
sem_t clientsSem;
// to jest znacznik czy fryzjer jest zajetty czy nie; 0 gdy scina kogos, 1 gdy nie
sem_t barberSem;
// blokuje fotel gdy siada na niego klient a zwalnia go fryzjer gdy skonczy
pthread_mutex_t barberSeat;
//blokuje poczekalnie tak aby zapobiedz wyscigowi sprawdzania ilosci osob w poczekalni
pthread_mutex_t waitingRoom;

//Liczba wolnych miejsc w poczekalni
int freeSeatsAmount = 10;
//Domyslna liczba miejsc w poczekalni
int seatsAmount = 10;
//Liczba osób które zrezygnowały
int rejectedClientsCounter = 0;
//Domyślny maksymalny czas strzyżenia
int maxShearTime = 2;
//Domyślny maksymalny czas przychodzenia klientów
int maxClientArriveTime = 8;
//Informacja o wolnym fotelu (-1 wskazuje na pusty fotel)
int clientOnSeatId = -1;

int isDebug = 0;
int isEnd = 0;


struct Node *clients = NULL;
struct Node *rejectedClients = NULL;
struct Node *waitingClients = NULL;

void *BarberThred() {
    while (isEnd != 1) {
        // fryzjer spi, czyli czaka az pojawi sie jakis klient i go obudzi
        sem_wait(&clientsSem);
        // zablokowanie obiektu poczekalni przez wątek
        pthread_mutex_lock(&waitingRoom);
        freeSeatsAmount++;
        // odblokowanie obiektu poczekalni przez wątek
        pthread_mutex_unlock(&waitingRoom);
        // operacja signal
        sem_post(&barberSem);

        doBarberWork();

        printf("Res:%d WRomm: %d/%d [in: %d] - It has new haircut!\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        // odblokowanie mutexa
        pthread_mutex_unlock(&barberSeat);
    }
    if (isDebug == 1) printf("Barber finished work!\n");
    return NULL;
}

void *ClientThread(void *client) {
    struct Node *actualClient = (struct Node *) client;
    int clientId = (*actualClient).id;
    int clientTime = (*actualClient).time;

    travelToBarbershop(clientTime);

    // blokujemy poczekalnie
    pthread_mutex_lock(&waitingRoom);
    if (freeSeatsAmount > 0) {
        freeSeatsAmount--;
        if (isDebug == 1)
            addToWaitingList(clientId, clientTime);

        printf("Res:%d WRomm: %d/%d [in: %d] - Client sat in the waiting room!\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);

        // dajemy sygnal dla fryzjera ze klient jest w poczekalni
        sem_post(&clientsSem);
        // odblokowanie poczekalni
        pthread_mutex_unlock(&waitingRoom);
        // czekamy az fryzjer bedzie gotowy(skonczy scinac kogos innego)
        sem_wait(&barberSem);
        // siadamy na fotelu czyli blokujemy fotel
        pthread_mutex_lock(&barberSeat);
        clientOnSeatId = clientId;
        printf("Res:%d WRomm: %d/%d [in: %d] - Barber starts cutting its hair!\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount,
               seatsAmount, clientOnSeatId);
        // klient usuwany z miejsca z poczekalni
        if (isDebug == 1)
            deleteNode(&waitingClients, clientId);

    }
    else {
        // jeżeli nie ma miejsca
        // odblokowujemy poczekalnie bo zostalismy odrzuceni
        pthread_mutex_unlock(&waitingRoom);
        // zwiększamy licznik odrzucen
        rejectedClientsCounter++;
        printf("Res:%d WRomm: %d/%d [in: %d] - Client rejected!\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount,
               seatsAmount, clientOnSeatId);
        if (isDebug == 1) addToRejectedList(clientId, clientTime);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    static char usage[] = "Usage: %s -k value -r value [-c value] [-f value] [-d]\n";
    if(argc<5){
        fprintf(stderr, "%s: too few command-line arguments\n",argv[0]);
        fprintf(stderr,usage, argv[0]);
        exit(1);
    }
    if(argc>10){
        fprintf(stderr, "%s: too many command-line arguments\n",argv[0]);
        fprintf(stderr,usage, argv[0]);
        exit(1);
    }

    srand(time(NULL));

    int clientCount = 5;
    int option;
    int kFlag=0;
    int rFlag=0;
    while ((option = getopt(argc, argv, "k:r:c:f:d")) != -1) {
        switch (option) {
            //Liczba Klientów
            case 'k':
                clientCount = atoi(optarg);
                kFlag = 1;
                break;
            //Liczba miejsc w poczekalni
            case 'r':
                freeSeatsAmount = atoi(optarg);
                seatsAmount = atoi(optarg);
                rFlag = 1;
                break;
            //Maksymalny czas pojawienia się klientów
            case 'c':
                maxClientArriveTime = atoi(optarg);
                break;
            //Maksymalna czas strzyżenia
            case 'f':
                maxShearTime = atoi(optarg);
                break;
            //Parametr Debug
            case 'd':
                isDebug = 1;
                break;
        }
    }

    if(kFlag == 0){
        fprintf(stderr, "%s: missing -k option\n",argv[0]);
        fprintf(stderr, usage,argv[0]);
        exit(1);
    }
    if(rFlag == 0){
        fprintf(stderr, "%s: missing -r option\n",argv[0]);
        fprintf(stderr, usage,argv[0]);
        exit(1);
    }

    pthread_t *clientThreads = malloc(sizeof(pthread_t) * clientCount);
    pthread_t barberThread;

    int i;
    if (isDebug == 1) printf("Number of Clients: %d\n", clientCount);

    for (i = 0; i < clientCount; i++) {
        // Wylosowanie czasu, po jakim klient pojawi się u fryzjera
        int randomTime = rand() % maxClientArriveTime + 1;
        // dodajemy klienta do listy
        push(&clients, i, randomTime);
        // watek dla klienta, przekazujemy wskaznik na danego klienta
        pthread_create(&clientThreads[i], NULL, ClientThread, (void *) clients);
    }

    sem_init(&clientsSem, 0, 0);
    sem_init(&barberSem, 0, 0);

    // inicjalizacja zamka dla zamka fotel
    if (pthread_mutex_init(&barberSeat, NULL) != 0) {
        fprintf(stderr, "barebrSeat mutex init error");
        exit(1);
    }
    if (pthread_mutex_init(&waitingRoom, NULL) != 0) {
        fprintf(stderr, "barebrSeat mutex init error");
        exit(1);
    }
    // tworzymy wątek fryzjera. wskaźnik do wątku, parametry wątku, wskaźnik do funkcji wykonywanej przez wątek, argumenty do funkcji wykonywanej
    pthread_create(&barberThread, NULL, BarberThred, NULL);

    // czekamy na kazdego klienta
    for (i = 0; i < clientCount; i++) {
        // dodajemy watki klinetow do listy oczekiwanych
        pthread_join(clientThreads[i], NULL);
    }

    isEnd = 1;
    pthread_join(barberThread, NULL);

    pthread_mutex_destroy(&barberSeat);
    pthread_mutex_destroy(&waitingRoom);
    sem_destroy(&clientsSem);
    sem_destroy(&barberSem);
    free(clients);
    free(waitingClients);
    free(rejectedClients);
    return 0;
}
