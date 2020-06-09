#include "functions.h"
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct Node *clients;
struct Node *rejectedClients;
struct Node *waitingClients;

sem_t clientsSem; // to jest liczba klientow gotowych do strzyzenia; 0 gdy nie ma nikogo w poczekalni
sem_t barberSem; // to jest znacznik czy fryzjer jest zajetty czy nie; 0 gdy scina kogos, 1 gdy nie
pthread_mutex_t barberSeat; // blokuje fotel gdy siada na niego klient a zwalnia go fryzjer gdy skonczy
pthread_mutex_t waitingRoom; //blokuje poczekalnie tak aby zapobiedz wyscigowi sprawdzania ilosci osob w poczekalni

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

void printWaitingList() {
    printf("Waiting: ");
    printList(waitingClients);
    printf("\n");
}

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
    if (isDebug == 1) printf("Adding client id: %d time: %d\n", clientId, clientTime);
    struct Node *new_node = (struct Node *) malloc(sizeof(struct Node));
    struct Node *last = *head_ref; /* used in step 5*/
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