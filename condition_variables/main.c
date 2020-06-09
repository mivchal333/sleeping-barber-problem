#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include "../functions.h"

pthread_cond_t freeSeatsCond; //miejsca dostepne w poczekalni
pthread_cond_t isBarberAvailableCond; //fryzjer nie scina w danym momencie wlosow
pthread_cond_t wakeupBarberCond; //wysylany zeby obudzic fryzjera
pthread_cond_t shearEndCond; //mowi klientowi

pthread_mutex_t barberSeatMutex; //fotel u fryzjera
pthread_mutex_t waitingRoom; //mutex do ochrony miejsc w poczekalni
pthread_mutex_t clientFinished; //klient po zakonczeniu scinania
pthread_mutex_t barberMutex; //chroni stan pracy/spania fryzjera
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
int isSeatBusy = 0;

struct Node *clients = NULL;
struct Node *rejectedClients = NULL; //lista tych, ktorzy sie nie dostali do poczekalni
struct Node *waitingClients = NULL; //lista/kolejka osob czekajacych w poczekalni

void *ClientThread(void *client) {
    struct Node *actualClient = (struct Node *) client;
    int clientId = (*actualClient).id;
    int clientTime = (*actualClient).time;


    travelToBarbershop(clientTime);

    pthread_mutex_lock(&waitingRoom); // blokujemy poczekalnie
    if (freeSeatsAmount > 0) //jesli sa wolne miejsca, wejdz
    {
        freeSeatsAmount--;
        if (isDebug == 1) addToWaitingList(clientId, clientTime);
        pthread_mutex_unlock(&waitingRoom); //odbezpieczamy poczekalnie

        printf("Res:%d WRomm: %d/%d [in: %d]  -  nowy klient w poczekalni\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);

        //czekamy az fotel sie zwolni
        pthread_mutex_lock(&barberSeatMutex);
        if (isSeatBusy == 1) pthread_cond_wait(&isBarberAvailableCond, &barberSeatMutex);
        isSeatBusy = 1;
        pthread_mutex_unlock(&barberSeatMutex);

        //klient wychodzi z poczekalni i idzie do fryzjera
        pthread_mutex_lock(&waitingRoom);
        freeSeatsAmount++;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient poszedl do fryzjera\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        clientOnSeatId = clientId;
        if (isDebug == 1)
            deleteNode(&waitingClients, clientId);
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient jest scinany\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        pthread_mutex_unlock(&waitingRoom);
        //pthread_cond_signal(&wolne_miejsca); //wysylamy sygnal ze jest wolne miejsce

        //obudz fryzjera
        pthread_cond_signal(&wakeupBarberCond);

        //czekaj az scinanie sie skonczy
        pthread_mutex_lock(&clientFinished);
        pthread_cond_wait(&shearEndCond, &clientFinished);
        isEnd = false; //resetowanie zeby nastepny mogl przyjsc
        pthread_mutex_unlock(&clientFinished);

        //odblokowanie fotelu i danie znac ze fryzjer jest wolny
        pthread_mutex_lock(&barberSeatMutex);
        isSeatBusy = 0;
        pthread_mutex_unlock(&barberSeatMutex);
        pthread_cond_signal(&isBarberAvailableCond);

    } else {
        rejectedClientsCounter++; //zwiekszamy licznik osob ktore sie nie dostaly
        if (isDebug == 1) addToRejectedList(clientId, clientTime);
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient nie wszedl\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        pthread_mutex_unlock(&waitingRoom); //odblokuj poczekalnie
    }

}

void *BarberThread() {
    pthread_mutex_lock(&barberMutex); //lock na stanie fryzjera zeby zapewnic bezpieczenstwo
    while (!isEnd) //dopoki sa klienci
    {
        //czekaj az fryzjer zostanie wybudzony
        pthread_cond_wait(&wakeupBarberCond, &barberMutex);
        pthread_mutex_unlock(&barberMutex);
        if (!isEnd) // //jesli jest klient
        {
            doBarberWork();  // scinamy klienta
            pthread_cond_signal(&shearEndCond); //koniec
        } else //jesli nie, to idz do domu...
            printf("Nikogo juz nie ma, nikt nie przyjdzie, pora na mnie...\n");
    }

}

int main(int argc, char *argv[]) {
    static char usage[] = "Usage: %s -k value -r value [-c value] [-f value] [-d]\n";
    if (argc < 5) {
        fprintf(stderr, "%s: too few command-line arguments\n", argv[0]);
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }
    if (argc > 10) {
        fprintf(stderr, "%s: too many command-line arguments\n", argv[0]);
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }
    srand(time(NULL));

    int clientCount = 20; //liczba klientow ktorzy sie pojawia
    int wybor;
    int kFlag = 0;
    int rFlag = 0;
    while ((wybor = getopt(argc, argv, "k:r:c:f:d")) != -1) {
        switch (wybor) {
            case 'k': //max liczba klientow pobrana jako argument
                clientCount = atoi(optarg);
                kFlag = 1;
                break;
            case 'r': // liczba krzesel w poczekalni
                freeSeatsAmount = atoi(optarg);
                seatsAmount = atoi(optarg);
                rFlag = 1;
                break;
            case 'c': // co ile czasu maja pojawiac sie klienci w salonie
                maxClientArriveTime = atoi(optarg);
                break;
            case 'f':  // jak szybko ma scinac fryzjer
                maxShearTime = atoi(optarg);
                break;
            case 'd':
                isDebug = true;
                break;
        }
    }
    if (kFlag == 0) {
        fprintf(stderr, "%s: missing -k option\n", argv[0]);
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }
    if (rFlag == 0) {
        fprintf(stderr, "%s: missing -r option\n", argv[0]);
        fprintf(stderr, usage, argv[0]);
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

    //inicjalizacja zmiennych warunkowych
    pthread_cond_init(&freeSeatsCond, NULL);
    pthread_cond_init(&isBarberAvailableCond, NULL);
    pthread_cond_init(&wakeupBarberCond, NULL);
    pthread_cond_init(&shearEndCond, NULL);

    //inicjalizacja mutexow
    pthread_mutex_init(&barberSeatMutex, NULL);
    pthread_mutex_init(&waitingRoom, NULL);
    pthread_mutex_init(&clientFinished, NULL);
    pthread_mutex_init(&barberMutex, NULL);

    //tworzymy watek fryzjera
    pthread_create(&barberThread, 0, BarberThread, 0);

    //czekamy az zostana obsluzeni
    for (i = 0; i < clientCount; i++) {
        pthread_join(clientThreads[i], 0);
    }
    //ustawiamy flage ze to koniec roboty na dzisiaj
    isEnd = 1;
    //budzimy fryzjera zeby poszedl do domu
    pthread_cond_signal(&wakeupBarberCond);
    pthread_join(barberThread, NULL);

    free(clients);
    free(waitingClients);
    free(rejectedClients);

    pthread_mutex_destroy(&barberMutex);
    pthread_mutex_destroy(&barberSeatMutex);
    pthread_mutex_destroy(&clientFinished);
    pthread_mutex_destroy(&waitingRoom);

    pthread_cond_destroy(&freeSeatsCond);
    pthread_cond_destroy(&isBarberAvailableCond);
    pthread_cond_destroy(&wakeupBarberCond);
    pthread_cond_destroy(&shearEndCond);

    return 0;
}
