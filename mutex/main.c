#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <getopt.h>
#include "../functions.h"

// to jest liczba klientow gotowych do strzyzenia; 0 gdy nie ma nikogo w poczekalni
sem_t clientsSem;
// Znacznik wskazujący zajętość fryzjera
sem_t barberSem;
// Blokuje fotel barbera w momencie gdy fryzjez zaczyna ścinać i zwalnia gdy klient zostanie obcięty
pthread_mutex_t barberSeat;
// Blokuje poczekalnie w celu likwidacji wyścigu
pthread_mutex_t waitingRoom;

// Domyslna liczba miejsc w poczekalni
int seatsAmount = 10;
// Liczba wolnych miejsc w poczekalni
int freeSeatsAmount = 10;
// Liczba osób które zrezygnowały
int rejectedClientsCounter = 0;
// Domyślny maksymalny czas strzyżenia
int maxShearTime = 2;
// Domyślny maksymalny czas przychodzenia klientów
int maxClientArriveTime = 8;
// Informacja o wolnym fotelu (-1 wskazuje na pusty fotel)
int clientOnSeatId = -1;

// Zmiena wskazująca czy mają być wyświetlane rozszerzone komunikaty (0 - wyłączone, 1 - włączone)
int isDebug = 0;
// Zmiena informująca o tym, że fryzjer ukończył strzyżenie( 0 - nieukończone, 1 - ukończone)
int isEnd = 0;

// Inicjalizacja list - klientów, klientów którzy zrezygnowali, oczekujacych klientów
struct Node *clients = NULL;
struct Node *rejectedClients = NULL;
struct Node *waitingClients = NULL;

void *BarberThred() {
    while (isEnd != 1) {
        // Oczekiwanie fryzjera na pojawienie się klienta
        sem_wait(&clientsSem);
        // Blokada poczekalni
        pthread_mutex_lock(&waitingRoom);
        freeSeatsAmount++;
        // Wysłanie sygnału
        sem_post(&barberSem);
        // Odblokowanie poczekalni
        pthread_mutex_unlock(&waitingRoom);


        doBarberWork();

        printf("Res:%d WRomm: %d/%d [in: %d] - Client has new haircut!\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        // Odblokowanie fotela przez klienta
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

    // Blokada poczekalni z racji na przybycie nowego klienta i zmiane stanu wolnych miejsc w poczekalni
    pthread_mutex_lock(&waitingRoom);
    // Dodanie klienta do listy oczekujących w poczekalni, jeżeli w tej znajdują się wolne miejsca
    if (freeSeatsAmount > 0) {
        freeSeatsAmount--;
        if (isDebug == 1)
            addToWaitingList(clientId, clientTime);

        printf("Res:%d WRomm: %d/%d [in: %d] - New client in waiting room!\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);

        // Zasygnalizowanie, że klient znajduje się w poczekalni
        sem_post(&clientsSem);
        // Odblokowanie poczekalni, z racji na to że klient został już dodany do oczekujących
        pthread_mutex_unlock(&waitingRoom);
        // Oczekiwanie na gotowość fryzjera - zacznie pracę lub skończy ścinać inną osobę
        sem_wait(&barberSem);
        // Blokada fotela, rozpoczęcie strzyżenia
        pthread_mutex_lock(&barberSeat);
        clientOnSeatId = clientId;
        printf("Res:%d WRomm: %d/%d [in: %d] - The client's hair was cut!\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount,
               seatsAmount, clientOnSeatId);
        // Usunięcie klienta z miejsca w poczekalni
        if (isDebug == 1)
            deleteNode(&waitingClients, clientId);

    }
    // Dodanie klienta do listy rezygnujących, z racji na brak miejsc w poczekalni
    else {
        // Odblokowanie poczekalni z racji na brak miejsca w niej
        pthread_mutex_unlock(&waitingRoom);
        // Zwiększenie liczby osób, które zrezygnowały
        rejectedClientsCounter++;
        printf("Res:%d WRomm: %d/%d [in: %d] - Client rejected!\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount,
               seatsAmount, clientOnSeatId);
        if (isDebug == 1) addToRejectedList(clientId, clientTime);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Sprawdzenie poprawoności wywołania (Ilość parametrów)
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

    // Domyślna liczna klientów
    int clientCount = 5;
    int option;
    int kFlag=0;
    int rFlag=0;
    while ((option = getopt(argc, argv, "k:r:c:f:d")) != -1) {
        switch (option) {
            // Liczba Klientów
            case 'k':
                clientCount = atoi(optarg);
                kFlag = 1;
                break;
            // Liczba miejsc w poczekalni
            case 'r':
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
                isDebug = 1;
                break;
        }
    }
    // Sprawdzenie wymaganych parametrów parametrów
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

    // Inicjacja obiektu, ustawienie wartości
    sem_init(&clientsSem, 0, 0);
    sem_init(&barberSem, 0, 0);

    // Inicjalizacja zamka dla fotela fryzjera
    if (pthread_mutex_init(&barberSeat, NULL) != 0) {
        fprintf(stderr, "barebrSeat mutex init error");
        exit(1);
    }
    // Inicjalizacja zamka dla poczekalni
    if (pthread_mutex_init(&waitingRoom, NULL) != 0) {
        fprintf(stderr, "barebrSeat mutex init error");
        exit(1);
    }
    // Utworzenie wątku fryzjera
    pthread_create(&barberThread, NULL, BarberThred, NULL);

    // Dodanie wątków klientów do listy oczekujących
    for (int i = 0; i < clientCount; i++) {
        pthread_join(clientThreads[i], NULL);
    }

    isEnd = 1;
    // Dodanie wątku fryzjera
    pthread_join(barberThread, NULL);

    // Zwonienie pamięci
    pthread_mutex_destroy(&barberSeat);
    pthread_mutex_destroy(&waitingRoom);
    sem_destroy(&clientsSem);
    sem_destroy(&barberSem);
    free(clients);
    free(waitingClients);
    free(rejectedClients);
    return 0;
}
