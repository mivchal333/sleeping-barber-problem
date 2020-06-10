#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include "../functions.h"

// Zmienna warunkowa wskazujuąca na dostępność fryzjea
pthread_cond_t isBarberAvailableCond;
// Zmienna warunkowa informująca o wybudzeniu fryzjera
pthread_cond_t wakeupBarberCond;
// Zmienna warunkowa informująca o ukończeniu ścinania
pthread_cond_t shearEndCond;

// Blokuje fotel barbera w momencie gdy fryzjez zaczyna ścinać i zwalnia gdy klient zostanie obcięty
pthread_mutex_t barberSeatMutex;
// Blokuje poczekalnię w momencie gdy dokonywane są w niej zmiany i zwalnia gdy te zostaną ukończone
pthread_mutex_t waitingRoom;
// Blokuje klienta w momencie gdy ten jest strzyżony i zwalnia gdy te strzyżenie zostanie ukończone
pthread_mutex_t clientFinished;
// Blokuje fryzjera gdy ten wykonuje swoją pracę i odblokowuje gdy już okończy strzyc
pthread_mutex_t barberMutex;


//Domyslna liczba miejsc w poczekalni
int seatsAmount = 10;
// Liczba wolnych miejsc w poczekalni
int freeSeatsAmount = 10;
//Liczba osób które zrezygnowały
int rejectedClientsCounter = 0;
//Domyślny maksymalny czas strzyżenia
int maxShearTime = 2;
//Domyślny maksymalny czas przychodzenia klientów
int maxClientArriveTime = 8;
//Informacja o wolnym fotelu (-1 wskazuje na pusty fotel)
int clientOnSeatId = -1;

// Zmiena wskazująca czy mają być wyświetlane rozszerzone komunikaty (0 - wyłączone, 1 - włączone)
int isDebug = 0;
// Zmiena informująca o tym, że fryzjer ukończył strzyżenie( 0 - nieukończone, 1 - ukończone)
int isEnd = 0;
// Zmiena informująca o dostępności fotela fryzjera (0 - dostępny, 1 - niedostępny)
int isSeatBusy = 0;

// Inicjalizacja list - klientów, klientów którzy zrezygnowali, oczekujacych klientów
struct Node *clients = NULL;
struct Node *rejectedClients = NULL;
struct Node *waitingClients = NULL;

void *ClientThread(void *client) {
    struct Node *actualClient = (struct Node *) client;
    int clientId = (*actualClient).id;
    int clientTime = (*actualClient).time;

    travelToBarbershop(clientTime);
    // Blokada poczekalni z racji na przybycie nowego klienta i zmiane stanu wolnych miejsc w poczekalni
    pthread_mutex_lock(&waitingRoom);
    // Dodanie klienta do listy oczekujących w poczekalni, jeżeli w tej znajdują się wolne miejsca
    if (freeSeatsAmount > 0) {
        freeSeatsAmount--;
        if (isDebug == 1)
            addToWaitingList(clientId, clientTime);
        printf("Res:%d WRomm: %d/%d [in: %d]  -  nowy klient w poczekalni\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);

        // Odblokowanie poczekalni, z racji na to że klient został już dodany do oczekujących
        pthread_mutex_unlock(&waitingRoom);

        // Blokada fotela fryzjera z racji na zmianę statusu dostępności
        pthread_mutex_lock(&barberSeatMutex);

        if (isSeatBusy == 1)
            // Oczekiwanie na dostepność fryzjera
            pthread_cond_wait(&isBarberAvailableCond, &barberSeatMutex);

        isSeatBusy = 1;
        // Odblokowanie fotela fryzjera, z racji na dokonanie zmiany w statusie dostępności
        pthread_mutex_unlock(&barberSeatMutex);

        // Blokada poczekalni z racji na przejście klienta do fryzjera i zmiane stanu wolnych miejsc w poczekalni
        pthread_mutex_lock(&waitingRoom);
        freeSeatsAmount++;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient poszedl do fryzjera\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        clientOnSeatId = clientId;
        if (isDebug == 1)
            deleteNode(&waitingClients, clientId);
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient jest scinany\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        // Odblokowanie poczekalni, z racji na to że klient rozpoczął ścinanie
        pthread_mutex_unlock(&waitingRoom);


        // Zasygnalizowanei wybudzenia się fryzjera
        pthread_cond_signal(&wakeupBarberCond);

        // Zablokowanie osoby ścinanej
        pthread_mutex_lock(&clientFinished);
        // Oczekiwanie na Ukończenie ścinania
        pthread_cond_wait(&shearEndCond, &clientFinished);
        // Zmaiana statusu w celu wpuszczenia kolejnych osób na fotel
        isEnd = false;
        // Odblokowanie osoby ścinanej
        pthread_mutex_unlock(&clientFinished);

        // Zablokowanie fotela w celu zmiany jego statusu zajętości
        pthread_mutex_lock(&barberSeatMutex);
        isSeatBusy = 0;
        // Odblokowanie fotela, oraz zasyganlizowanie że jest już wolny
        pthread_mutex_unlock(&barberSeatMutex);
        pthread_cond_signal(&isBarberAvailableCond);

    }
    // Dodanie klienta do listy rezygnujących, z racji na brak miejsc w poczekalni
    else {
        // Odblokowanie poczekalni z racji na brak miejsca w niej
        pthread_mutex_unlock(&waitingRoom);
        // Zwiększenie liczby osób, które zrezygnowały
        rejectedClientsCounter++;
        if (isDebug == 1)
            addToRejectedList(clientId, clientTime);
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient nie wszedl\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
    }
    return NULL;
}

void *BarberThread() {
    // Blokada fryzjera gdy ten pracuje
    pthread_mutex_lock(&barberMutex);
    // Realizowane póki w są klienci
    while (!isEnd)
    {
        // Oczekiwanie na wybudzenie fryzjera
        pthread_cond_wait(&wakeupBarberCond, &barberMutex);
        // Odblokowanie fryzjera gdy ten ukończył strzyżenie
        pthread_mutex_unlock(&barberMutex);
        // Realizowane póki w są klienci
        if (!isEnd)
        {

            doBarberWork();
            printf("Res:%d WRomm: %d/%d [in: %d] - It has new haircut!\n", rejectedClientsCounter,
                   seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
            // Zasygnalizowanie ukończenia strzyżenia
            pthread_cond_signal(&shearEndCond);
        }
        // W przypadku braku klientów
        else {
            if (isDebug == 1) printf("Barber finished work!\n");
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Sprawdzenie poprawoności wywołania (Ilość parametrów)
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

    // Domyślna liczna klientów
    int clientCount = 5;
    int wybor;
    int kFlag = 0;
    int rFlag = 0;
    while ((wybor = getopt(argc, argv, "k:r:c:f:d")) != -1) {
        switch (wybor) {
            // Liczba Klientów
            case 'k':
                clientCount = atoi(optarg);
                kFlag = 1;
                break;
            // Liczba miejsc w poczekalni
            case 'r': // liczba krzesel w poczekalni
                freeSeatsAmount = atoi(optarg);
                seatsAmount = atoi(optarg);
                rFlag = 1;
                break;
            // Maksymalny czas pojawienia się klientów
            case 'c':
                maxClientArriveTime = atoi(optarg);
                break;
            // Maksymalna czas strzyżenia
            case 'f':
                maxShearTime = atoi(optarg);
                break;
            // Parametr Debug
            case 'd':
                isDebug = true;
                break;
        }
    }
    // Sprawdzenie wymaganych parametrów parametrów
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

    // Rezerwacja miejsca na wątki klientów
    pthread_t *clientThreads = malloc(sizeof(pthread_t) * clientCount);
    pthread_t barberThread;

    if (isDebug == 1) printf("Number of Clients: %d\n", clientCount);

    for (int i = 0; i < clientCount; i++) {
        // Wylosowanie czasu, po jakim klient pojawi się u fryzjera
        int randomTime = rand() % maxClientArriveTime + 1;
        // Dodanie klienta do listy klientów
        push(&clients, i, randomTime);
        // Utworzenie wątku dla klienta, wskazanie nie klienta
        pthread_create(&clientThreads[i], NULL, ClientThread, (void *) clients);
    }

    // Inicjalizacia zmiennych warunkowyc
    pthread_cond_init(&isBarberAvailableCond, NULL);
    pthread_cond_init(&wakeupBarberCond, NULL);
    pthread_cond_init(&shearEndCond, NULL);

    // Inicjalizacja mutexów
    pthread_mutex_init(&barberSeatMutex, NULL);
    pthread_mutex_init(&waitingRoom, NULL);
    pthread_mutex_init(&clientFinished, NULL);
    pthread_mutex_init(&barberMutex, NULL);

    // Utworzenie wątku fryzjera
    pthread_create(&barberThread, 0, BarberThread, 0);

    // Dodanie wątków klientów do listy oczekujących
    for (int i = 0; i < clientCount; i++) {
        pthread_join(clientThreads[i], 0);
    }
    isEnd = 1;
    // Sygnał wybudzający fryzjera po tym jak ukończy
    pthread_cond_signal(&wakeupBarberCond);
    // Dodanie wątku fryzjera
    pthread_join(barberThread, NULL);

    // Zwonienie pamięci
    free(clients);
    free(waitingClients);
    free(rejectedClients);

    pthread_mutex_destroy(&barberMutex);
    pthread_mutex_destroy(&barberSeatMutex);
    pthread_mutex_destroy(&clientFinished);
    pthread_mutex_destroy(&waitingRoom);

    pthread_cond_destroy(&isBarberAvailableCond);
    pthread_cond_destroy(&wakeupBarberCond);
    pthread_cond_destroy(&shearEndCond);
    return 0;
}
