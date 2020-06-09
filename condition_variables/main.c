#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include "../functions.h"
#include <unistd.h>

pthread_cond_t wolne_miejsca; //miejsca dostepne w poczekalni
pthread_cond_t wolny_fryzjer; //fryzjer nie scina w danym momencie wlosow
pthread_cond_t obudz_fryzjera; //wysylany zeby obudzic fryzjera
pthread_cond_t skonczylem_scinac; //mowi klientowi

pthread_mutex_t fotel; //fotel u fryzjera
pthread_mutex_t poczekalnia; //mutex do ochrony miejsc w poczekalni
pthread_mutex_t koniec_klient; //klient po zakonczeniu scinania
pthread_mutex_t stan_fryzjera; //chroni stan pracy/spania fryzjera
bool czy_fotel_zajety = false; //czy ktos siedzi na fotelu
bool czy_fryzjer_spi; //czy fryzjer ucina sobie drzemke w oczekiwaniu na klienta

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
struct Node *rejectedClients = NULL; //lista tych, ktorzy sie nie dostali do poczekalni
struct Node *waitingClients = NULL; //lista/kolejka osob czekajacych w poczekalni

void Wypisz_odrzuceni()
{
    struct Node *temp = rejectedClients;
    printf("Nie weszli: ");
    while(temp!=NULL)
    {
        printf("%d ",temp->id);
        temp = temp->next;
    }
    printf("\n");
}
void Wypisz_czekajacy()
{
    struct Node *temp = waitingClients;
    printf("Czekaja : ");
    while(temp!=NULL)
    {
        printf("%d " ,temp->id);
        temp = temp->next;
    }
    printf("\n");
}
void Umiesc_odrzuceni(int x)
{
    struct Node *temp = (struct Node*)malloc(sizeof(struct Node));
    temp->id = x;
    temp->next = rejectedClients;
    rejectedClients = temp;
    Wypisz_odrzuceni();
}
void Umiesc_czekajacy(int x)
{
    struct Node *temp = (struct Node*)malloc(sizeof(struct Node));
    temp->id = x;
    temp->next = waitingClients;
    waitingClients = temp;

    Wypisz_czekajacy();
}
int pierwszy_kolejce()
{
    struct Node *temp = waitingClients;
    while(temp->next!=NULL)
    {
        temp=temp->next;
    }
    return temp->id;
}
void usun_ostatni()
{
    struct Node *temp = waitingClients;
    while(temp->next->next!=NULL)
    {
        temp=temp->next;
    }
    free(temp->next);
    temp->next=NULL;
}
void usun_klienta(int x)
{
    struct Node *temp = waitingClients;
    struct Node *pop = waitingClients;
    while (temp!=NULL)
    {
        if(temp->id==x)
        {
            if(temp->id == waitingClients->id)
            {
                waitingClients=waitingClients->next;
                free(temp);
            }
            else
            {
                pop->next=temp->next;
                free(temp);
            }
            break;
        }
        pop=temp;
        temp=temp->next;
    }
    Wypisz_czekajacy();

}

void *ClientThread(void *client)
{
    struct Node *actualClient = (struct Node *) client;
    int clientId = (*actualClient).id;
    int clientTime = (*actualClient).time;


    travelToBarbershop(clientTime);

    pthread_mutex_lock(&poczekalnia); // blokujemy poczekalnie
    if(freeSeatsAmount > 0) //jesli sa wolne miejsca, wejdz
    {
        freeSeatsAmount--;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  nowy klient w poczekalni\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        if(isDebug == true) Umiesc_czekajacy(clientId); //umiesc klienta w kolejnce poczekalni
        pthread_mutex_unlock(&poczekalnia); //odbezpieczamy poczekalnie

        //czekamy az fotel sie zwolni
        pthread_mutex_lock(&fotel);
        if(czy_fotel_zajety)
            pthread_cond_wait(&wolny_fryzjer, &fotel);
        czy_fotel_zajety = true;
        pthread_mutex_unlock(&fotel);

        //klient wychodzi z poczekalni i idzie do fryzjera
        pthread_mutex_lock(&poczekalnia);
        freeSeatsAmount++;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient poszedl do fryzjera\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        clientOnSeatId=clientId;
        if(isDebug == true) usun_klienta(clientId); //usuwamy klienta z kolejki
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient jest scinany\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        pthread_mutex_unlock(&poczekalnia);
        //pthread_cond_signal(&wolne_miejsca); //wysylamy sygnal ze jest wolne miejsce

        //obudz fryzjera
        pthread_cond_signal(&obudz_fryzjera);

        //czekaj az scinanie sie skonczy
        pthread_mutex_lock(&koniec_klient);
        pthread_cond_wait(&skonczylem_scinac, &koniec_klient);
        isEnd=false; //resetowanie zeby nastepny mogl przyjsc
        pthread_mutex_unlock(&koniec_klient);

        //odblokowanie fotelu i danie znac ze fryzjer jest wolny
        pthread_mutex_lock(&fotel);
        czy_fotel_zajety = false;
        pthread_mutex_unlock(&fotel);
        pthread_cond_signal(&wolny_fryzjer);

    }
    else
    {
        rejectedClientsCounter++; //zwiekszamy licznik osob ktore sie nie dostaly
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient nie wszedl\n", rejectedClientsCounter, seatsAmount - freeSeatsAmount, seatsAmount, clientOnSeatId);
        if(isDebug == true) Umiesc_odrzuceni(clientId); //umieszczamy na liste osob ktore sie nie dostaly
        pthread_mutex_unlock(&poczekalnia); //odblokuj poczekalnie
    }

}
void *BarberThread()
{
    pthread_mutex_lock(&stan_fryzjera); //lock na stanie fryzjera zeby zapewnic bezpieczenstwo
    while(!isEnd) //dopoki sa klienci
    {
        //czekaj az fryzjer zostanie wybudzony
        pthread_cond_wait(&obudz_fryzjera, &stan_fryzjera);
        pthread_mutex_unlock(&stan_fryzjera);
        if(!isEnd) // //jesli jest klient
        {
            doBarberWork();  // scinamy klienta
            pthread_cond_signal(&skonczylem_scinac); //koniec
        }
        else //jesli nie, to idz do domu...
            printf("Nikogo juz nie ma, nikt nie przyjdzie, pora na mnie...\n");
    }

}

int main(int argc, char *argv[])
{
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

    int clientCount =20; //liczba klientow ktorzy sie pojawia
    int wybor;
    int kFlag=0;
    int rFlag=0;
    while ((wybor = getopt(argc, argv, "k:r:c:f:d")) != -1 )
    {
        switch(wybor)
        {
        case 'k': //max liczba klientow pobrana jako argument
            clientCount=atoi(optarg);
            kFlag = 1;
            break;
        case 'r': // liczba krzesel w poczekalni
            freeSeatsAmount=atoi(optarg);
                seatsAmount=atoi(optarg);
            rFlag = 1;
            break;
        case 'c': // co ile czasu maja pojawiac sie klienci w salonie
            maxClientArriveTime=atoi(optarg);
            break;
        case 'f':  // jak szybko ma scinac fryzjer
            maxShearTime=atoi(optarg);
            break;
        case 'd':
            isDebug=true;
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

    //inicjalizacja zmiennych warunkowych
    pthread_cond_init(&wolne_miejsca, NULL);
    pthread_cond_init(&wolny_fryzjer, NULL);
    pthread_cond_init(&obudz_fryzjera, NULL);
    pthread_cond_init(&skonczylem_scinac, NULL);

    //inicjalizacja mutexow
    pthread_mutex_init(&fotel, NULL);
    pthread_mutex_init(&poczekalnia, NULL);
    pthread_mutex_init(&koniec_klient, NULL);
    pthread_mutex_init(&stan_fryzjera, NULL);

    //tworzymy watek fryzjera
    pthread_create(&barberThread, 0, BarberThread, 0);

    //czekamy az zostana obsluzeni
    for (i=0; i < clientCount; i++)
    {
        pthread_join(clientThreads[i], 0);
    }
    //ustawiamy flage ze to koniec roboty na dzisiaj
    isEnd=1;
    //budzimy fryzjera zeby poszedl do domu
    pthread_cond_signal(&obudz_fryzjera);
    pthread_join(barberThread, NULL);

    //sprzatanie...
    pthread_cond_destroy(&wolne_miejsca);
    pthread_cond_destroy(&wolny_fryzjer);
    pthread_cond_destroy(&obudz_fryzjera);
    pthread_cond_destroy(&skonczylem_scinac);

    pthread_mutex_destroy(&fotel);
    pthread_mutex_destroy(&poczekalnia);
    pthread_mutex_destroy(&koniec_klient);
    pthread_mutex_destroy(&stan_fryzjera);

    free(waitingClients);
    free(rejectedClients);
    free(clients);
    return 0;
}
