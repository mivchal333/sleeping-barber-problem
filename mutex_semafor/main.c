#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include "../functions.h"
#include <time.h>

struct Node {
    int client_id;
    struct Node *next;
};


sem_t klienci; // to jest liczba klientow gotowych do strzyzenia; 0 gdy nie ma nikogo w poczekalni
sem_t fryzjer; // to jest znacznik czy fryzjer jest zajetty czy nie; 0 gdy scina kogos, 1 gdy nie
pthread_mutex_t fotel; // blokuje fotel gdy siada na niego klient a zwalnia go fryzjer gdy skonczy
pthread_mutex_t poczekalnia; //blokuje poczekalnie tak aby zapobiedz wyscigowi sprawdzania ilosci osob w poczekalni

int liczbaMiejsc = 10;  // liczba wolnych miejsc
int liczbaKrzesel = 10;   //dostepana liczba miejsc w poczekalni
int nie_weszli = 0;   // liczba osob, ktora nie znalazla miejsc w poczekalni
int czas_fryzjera = 2;    // tempo w jakim fryzjer scina klienta od 1 sec do czas_fryzjera
int czas_klienta = 8;     // czas w jakim klient przychodzi do salonu od rozpoczecia programu od 1 do czas_klienta

int debug = 0;       // opcja pozwalajaca na wypisanie list
int skonczone = 0; // zmienna pozwalajaca na sprawdzenie czy fryzjer skonczyl prace


struct Node *odrzuceni = NULL;
struct Node *czekajacy = NULL;


int akt_klient = -1; // zmienna przechowujaca aktualnego klienta na fotelu; -1 gdy nie ma klienta

void Umiesc_czekajacy(int x);

void *Fryzjer();

void Wypisz_czekajacy();

void czekaj(int sec);


void czekaj(int sec) {
    int x = (rand() % sec) * (rand() % 1000000) + 1000000;
    usleep(x);
}

void *Klient(void *nr_klienta);

void Umiesc_odrzuceni(int clientId);

/* Given a reference (pointer to pointer) to the head
of a list and an int, appends a new node at the end */
void append(struct Node **head_ref, int new_data) {
    /* 1. allocate node */
    struct Node *new_node = (struct Node *) malloc(sizeof(struct Node));

    struct Node *last = *head_ref; /* used in step 5*/

    /* 2. put in the data */
    new_node->client_id = new_data;

    /* 3. This new node is going to be the last node, so make next
        of it as NULL*/
    new_node->next = NULL;

    /* 4. If the Linked List is empty, then make the new node as head */
    if (*head_ref == NULL) {
        *head_ref = new_node;
        return;
    }

    /* 5. Else traverse till the last node */
    while (last->next != NULL)
        last = last->next;

    /* 6. Change the next of last node */
    last->next = new_node;
}

void printList(struct Node *node) {
    while (node != NULL) {
        printf(" %d ", node->client_id);
        node = node->next;
    }
}

void Umiesc_czekajacy(int clientId) {
    append(&czekajacy, clientId);
    Wypisz_czekajacy();
}

void deleteNode(struct Node **head_ref, int key) {
    // Store head node
    struct Node *temp = *head_ref, *prev;

    // If head node itself holds the key to be deleted
    if (temp != NULL && temp->client_id == key) {
        *head_ref = temp->next;   // Changed head
        free(temp);               // free old head
        return;
    }

    // Search for the key to be deleted, keep track of the
    // previous node as we need to change 'prev->next'
    while (temp != NULL && temp->client_id != key) {
        prev = temp;
        temp = temp->next;
    }

    // If key was not present in linked list
    if (temp == NULL) return;

    // Unlink the node from linked list
    prev->next = temp->next;

    free(temp);  // Free memory
    Wypisz_czekajacy();
}

void *Klient(void *nr_klienta) {
    czekaj(czas_klienta);
    int nr = *(int *) nr_klienta;
    pthread_mutex_lock(&poczekalnia);  // blokujemy poczekalnie
    if (liczbaMiejsc > 0) {
        liczbaMiejsc--;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  zajeto miejsce w poczekalni\n", nie_weszli,
               liczbaKrzesel - liczbaMiejsc, liczbaKrzesel, akt_klient);
        if (debug == 1) {
            Umiesc_czekajacy(nr);
        }
        sem_post(&klienci); // dajemy sygnal dla fryzjera ze klient jest w poczekalni
        pthread_mutex_unlock(&poczekalnia); // odblokowanie poczekalni
        sem_wait(&fryzjer);// czekamy az fryzjer bedzie gotowy(skonczy scinac kogos innego)
        pthread_mutex_lock(&fotel);  // siadamy na fotelu czyli blokujemy fotel
        akt_klient = nr;  // ustalamy numer aktualnego klienta na fotelu
        printf("Res:%d WRomm: %d/%d [in: %d]  -  zaczeto scinac\n", nie_weszli, liczbaKrzesel - liczbaMiejsc,
               liczbaKrzesel, akt_klient);
        if (debug == 1) {
            deleteNode(&czekajacy, nr);
        }
    } else {
        pthread_mutex_unlock(&poczekalnia); // odblokowanie poczekalni
        nie_weszli++;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient nie wszedl\n", nie_weszli, liczbaKrzesel - liczbaMiejsc,
               liczbaKrzesel, akt_klient);
        if (debug == 1) {
            Umiesc_odrzuceni(nr);
        }
    }
    return NULL;
}

void Wypisz_czekajacy() {
    printf("Waiting: ");
    printList(czekajacy);
    printf("\n");
}

void *Fryzjer() {
    while (skonczone != 1) {
        sem_wait(&klienci); // fryzjer spi, czyli czaka az pojawi sie jakis klient i go obudzi
        // zablokowanie obiektu poczekalni przez wątek
        pthread_mutex_lock(&poczekalnia);
        liczbaMiejsc++;
        // odblokowanie obiektu poczekalni przez wątek
        pthread_mutex_unlock(&poczekalnia);
        // operacja signal
        sem_post(&fryzjer);

        czekaj(czas_fryzjera);
        printf("Res:%d WRomm: %d/%d [in: %d]  -  skonczono scinac\n", nie_weszli, liczbaKrzesel - liczbaMiejsc,
               liczbaKrzesel, akt_klient);
        // odblokowanie mutexa
        pthread_mutex_unlock(&fotel);
    }
    printf("Barber: End of Work\n");
    return NULL;
}

void Umiesc_odrzuceni(int clientId) {
    append(&odrzuceni, clientId);
    printf("Rejected: \n");
    printList(odrzuceni);
    printf("\n");
}

int main(int argc, char *argv[]) {
    // inicjalizacja
    srand(time(NULL));

    int liczbaKlientow = 5;
    int wybor;
    while ((wybor = getopt(argc, argv, "k:r:c:f:d")) != -1) {
        switch (wybor) {
            case 'k': //max liczba klientow
                liczbaKlientow = atoi(optarg);
                break;
            case 'r': // liczba krzesel w poczekalni
                liczbaMiejsc = atoi(optarg);
                liczbaKrzesel = atoi(optarg);
                break;
            case 'c': // co ile czasu maja pojawiac sie klienci w salonie
                czas_klienta = atoi(optarg);
                break;
            case 'f':  // jak szybko ma scinac fryzjer
                czas_fryzjera = atoi(optarg);
                break;
            case 'd':
                debug = 1;
                break;
        }
    }

    pthread_t *klienci_watki = malloc(sizeof(pthread_t) * liczbaKlientow);
    pthread_t fryzjer_watek;
    int *tablica = malloc(sizeof(int) * liczbaKlientow);

    int i;
    for (i = 0; i < liczbaKlientow; i++) {
        tablica[i] = i;
    }

    sem_init(&klienci, 0, 0);
    sem_init(&fryzjer, 0, 0);

    // inicjalizacja zamka dla zamka fotel
    if (pthread_mutex_init(&fotel, NULL) != 0) printf("Fotel mutex init error");
    if (pthread_mutex_init(&poczekalnia, NULL) != 0) printf("Poczekalnia mutex init error");
    // tworzymy wątek fryzjera. wskaźnik do wątku, parametry wątku, wskaźnik do funkcji wykonywanej przez wątek, argumenty do funkcji wykonywanej
    pthread_create(&fryzjer_watek, NULL, Fryzjer, NULL);

    // iteracyjnie tworzymy wątki klientów
    for (i = 0; i < liczbaKlientow; i++) {
        printf("\n Tworzymy klienta nr: %d", i);

        // tworzymy wątek klienta
        pthread_create(&klienci_watki[i], NULL, Klient, (void *) &tablica[i]);
    }
    // czekamy na kazdego klienta
    for (i = 0; i < liczbaKlientow; i++) {
        // dodajemy watki klinetow do listy oczekiwanych
        printf("\n Dodajemy klienta nr: %d", i);
        pthread_join(klienci_watki[i], NULL);
    }
    skonczone = 1;
    //sem_post(&klienci);
    pthread_join(fryzjer_watek, NULL);

    pthread_mutex_destroy(&fotel);
    pthread_mutex_destroy(&poczekalnia);
    sem_destroy(&klienci);
    sem_destroy(&fryzjer);
    free(czekajacy);
    free(odrzuceni);
    return 0;
}
