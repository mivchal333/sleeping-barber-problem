#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <getopt.h>
#include "../functions.h"

sem_t clientsSem; // to jest liczba klientow gotowych do strzyzenia; 0 gdy nie ma nikogo w poczekalni
sem_t barberSem; // to jest znacznik czy fryzjer jest zajetty czy nie; 0 gdy scina kogos, 1 gdy nie
pthread_mutex_t barberSeat; // blokuje fotel gdy siada na niego klient a zwalnia go fryzjer gdy skonczy
pthread_mutex_t waitingRoom; //blokuje poczekalnie tak aby zapobiedz wyscigowi sprawdzania ilosci osob w poczekalni

int freeSeatsAmount = 10;
int seatsAmount = 10;
int rejectedClientsCounter = 0;
int maxShearTime = 2;
int maxClientArriveTime = 8;
int isDebug = 0;
int isEnd = 0;

// if minus value seat is empty
int clientOnSeatId = -1;

struct Node *clients = NULL;
struct Node *rejectedClients = NULL;
struct Node *waitingClients = NULL;

/*

void *BarberThred();

void printWaitingList();

void doBarberWork() {
    int randomShearTime = rand() % maxShearTime + 1;
    sleep(randomShearTime);
}

void travelToBarbershop(int clientTime) {
    sleep(clientTime);
}

void push(struct Node **head_ref, int clientId, int clientTime) {
    struct Node *new_node = (struct Node *) malloc(sizeof(struct Node));
    new_node->id = clientId;
    new_node->time = clientTime;
    new_node->next = (*head_ref);
    (*head_ref) = new_node;
}


void append(struct Node **head_ref, int clientId, int clientTime) {
    if (isDebug == 1)
        printf("Adding client id: %d time: %d\n", clientId, clientTime);
    struct Node *new_node = (struct Node *) malloc(sizeof(struct Node));
    struct Node *last = *head_ref;  used in step 5
    new_node->id = clientId;
    new_node->time = clientId;
    new_node->next = NULL;
    if (*head_ref == NULL) {
        *head_ref = new_node;
        return;
    }
    while (last->next != NULL)
        last = last->next;
    last->next = new_node;
}

void printList(struct Node *node) {
    while (node != NULL) {
        printf(" %d ", node->id);
        node = node->next;
    }
}

void addToWaitingList(int clientId, int clientTime) {
    append(&waitingClients, clientId, clientTime);
    printWaitingList();
}

void deleteNode(struct Node **head_ref, int key) {
    struct Node *temp = *head_ref, *prev;
    if (temp != NULL && temp->id == key) {
        *head_ref = temp->next;   // Changed head
        free(temp);               // free old head
        return;
    }
    while (temp != NULL && temp->id != key) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL) return;
    prev->next = temp->next;
    free(temp);
    printWaitingList();
}

void addToRejectedList(int clientId, int clientTime) {
    append(&rejectedClients, clientId, clientTime);
    printf("Rejected: \n");
    printList(rejectedClients);
    printf("\n");
}


void *ClientThread(void *client) {
    struct Node *actualClient = (struct Node *) client;
    int clientId = (*actualClient).id;
    int clientTime = (*actualClient).time;

    travelToBarbershop(clientTime);

    pthread_mutex_lock(&waitingRoom);  // blokujemy poczekalnie
    if (freeSeatsAmount > 0) {
        freeSeatsAmount--;
        if (isDebug == 1)
            addToWaitingList(clientId, clientTime);
        printf("Res:%d WRomm: %d/%d [in: %d] - %d sat in the waiting room\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId, clientId);

        sem_post(&clientsSem); // dajemy sygnal dla fryzjera ze klient jest w poczekalni
        pthread_mutex_unlock(&waitingRoom); // odblokowanie poczekalni
        sem_wait(&barberSem);// czekamy az fryzjer bedzie gotowy(skonczy scinac kogos innego)
        pthread_mutex_lock(&barberSeat);  // siadamy na fotelu czyli blokujemy fotel
        clientOnSeatId = clientId;
        printf("Res:%d WRomm: %d/%d [in: %d]  Client accepted to shear\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount,
               seatsAmount, clientOnSeatId);
        // klient wchodzi na fotel
        if (isDebug == 1) deleteNode(&waitingClients, clientId);

    } else {
        // jeżeli nie ma miejsca
        // odblokowujemy poczekalnie bo zostalismy odrzuceni
        pthread_mutex_unlock(&waitingRoom);
        // zwiększamy licznik odrzucen
        rejectedClientsCounter++;
        printf("Res:%d WRomm: %d/%d [in: %d]  Rejected\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount,
               seatsAmount, clientOnSeatId);
        if (isDebug == 1) addToRejectedList(clientId, clientTime);

    }
    return NULL;
}

void printWaitingList() {
    printf("Waiting: ");
    printList(waitingClients);
    printf("\n");
}

void *BarberThred() {
    while (isEnd != 1) {
        sem_wait(&clientsSem); // fryzjer spi, czyli czaka az pojawi sie jakis klient i go obudzi
        // zablokowanie obiektu poczekalni przez wątek
        pthread_mutex_lock(&waitingRoom);
        freeSeatsAmount++;
        // odblokowanie obiektu poczekalni przez wątek
        pthread_mutex_unlock(&waitingRoom);
        // operacja signal
        sem_post(&barberSem);

        doBarberWork();

        printf("Res:%d WRomm: %d/%d [in: %d]  Shearing end\n", rejectedClientsCounter,
               seatsAmount - freeSeatsAmount,
               seatsAmount, clientOnSeatId);
        // odblokowanie mutexa
        pthread_mutex_unlock(&barberSeat);
    }
    if (isDebug == 1) printf("Barber: End of Work\n");
    return NULL;
}

*/

int main(int argc, char *argv[]) {
    // inicjalizacja
    static char usage[] = "Usage: %s -k value -r value [-c value] [-f value] [-d]\n";
    if(argc<5){
        fprintf(stderr, "%s: too few command-line arguments\n",argv[0]);
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
            case 'k': //max liczba klientow
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
        // losujemy czas po jakim klient przyjdzie do salonu
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
        //if (isDebug == 1) printf("Dodajemy klienta nr: %d\n", i+1);
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
