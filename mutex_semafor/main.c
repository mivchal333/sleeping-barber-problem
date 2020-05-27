#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>

struct Lista
{
    int nr_klienta;
    struct Lista *next;
};

sem_t klienci; // to jest liczba klientow gotowych do strzyzenia; 0 gdy nie ma nikogo w poczekalni
sem_t fryzjer; // to jest znacznik czy fryzjer jest zajetty czy nie; 0 gdy scina kogos, 1 gdy nie
pthread_mutex_t fotel; // blokuje fotel gdy siada na niego klient a zwalnia go fryzjer gdy skonczy
pthread_mutex_t poczekalnia; //blokuje poczekalnie tak aby zapobiedz wyscigowi sprawdzania ilosci osob w poczekalni

int liczbaMiejsc=10;  // liczba wolnych miejsc
int liczbaKrzesel=10;   //dostepana liczba miejsc w poczekalni
int nie_weszli=0;   // liczba osob, ktora nie znalazla miejsc w poczekalni
int czas_fryzjera=2;    // tempo w jakim fryzjer scina klienta od 1 sec do czas_fryzjera
int czas_klienta=8;     // czas w jakim klient przychodzi do salonu od rozpoczecia programu od 1 do czas_klienta
bool debug=false;       // opcja pozwalajaca na wypisanie list
bool skonczone=false; // zmienna pozwalajaca na sprawdzenie czy fryzjer skonczyl prace
int akt_klient=-1; // zmienna przechowujaca aktualnego klienta na fotelu; -1 gdy nie ma klienta

void czekaj(int sec)
{
    int x = (rand()%sec) * (rand()%1000000) + 1000000;
    usleep(x);
}

struct Lista *odrzuceni = NULL;
struct Lista *czekajacy = NULL;

void Wypisz_odrzuceni()
{
    struct Lista *temp = odrzuceni;
    printf("Nie weszli: ");
    while(temp!=NULL)
    {
        printf("%d ",temp->nr_klienta);
        temp = temp->next;
    }
    printf("\n");
}
void Wypisz_czekajacy()
{
    struct Lista *temp = czekajacy;
    printf("Czekaja : ");
    while(temp!=NULL)
    {
        printf("%d " ,temp->nr_klienta);
        temp = temp->next;
    }
    printf("\n");
}
void Umiesc_odrzuceni(int x)
{
    struct Lista *temp = (struct Lista*)malloc(sizeof(struct Lista));
    temp->nr_klienta = x;
    temp->next = odrzuceni;
    odrzuceni = temp;
    Wypisz_odrzuceni();
}
void Umiesc_czekajacy(int x)
{
    struct Lista *temp = (struct Lista*)malloc(sizeof(struct Lista));
    temp->nr_klienta = x;
    temp->next = czekajacy;
    czekajacy = temp;

    Wypisz_czekajacy();
}
void usun_klienta(int x)
{
    struct Lista *temp = czekajacy;
    struct Lista *pop = czekajacy;
    while (temp!=NULL)
    {
        if(temp->nr_klienta==x)
        {
            if(temp->nr_klienta==czekajacy->nr_klienta)
            {
                czekajacy=czekajacy->next;
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

void *Klient(void *nr_klienta)
{
    czekaj(czas_klienta);
    int nr = *(int *)nr_klienta;
    pthread_mutex_lock(&poczekalnia);  // blokujemy poczekalnie
    if(liczbaMiejsc>0)
    {
        liczbaMiejsc--;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  zajeto miejsce w poczekalni\n",nie_weszli, liczbaKrzesel-liczbaMiejsc, liczbaKrzesel, akt_klient);
        if(debug==true)
        {
            Umiesc_czekajacy(nr);
        }
        sem_post(&klienci); // dajemy sygnal dla fryzjera ze klient jest w poczekalni
        pthread_mutex_unlock(&poczekalnia); // odblokowanie poczekalni
        sem_wait(&fryzjer);// czekamy az fryzjer bedzie gotowy(skonczy scinac kogos innego)
        pthread_mutex_lock(&fotel);  // siadamy na fotelu czyli blokujemy fotel
        akt_klient=nr;  // ustalamy numer aktualnego klienta na fotelu
        printf("Res:%d WRomm: %d/%d [in: %d]  -  zaczeto scinac\n",nie_weszli, liczbaKrzesel-liczbaMiejsc, liczbaKrzesel, akt_klient);
        if(debug==true)
        {
            usun_klienta(nr);
        }
    }
    else
    {
        pthread_mutex_unlock(&poczekalnia); // odblokowanie poczekalni
        nie_weszli++;
        printf("Res:%d WRomm: %d/%d [in: %d]  -  klient nie wszedl\n",nie_weszli, liczbaKrzesel-liczbaMiejsc, liczbaKrzesel, akt_klient);
        if(debug==true)
        {
            Umiesc_odrzuceni(nr);
        }
    }
}
void *Fryzjer()
{
    while(!skonczone)
    {
    if(!skonczone){
        sem_wait(&klienci); // fryzjer spi, czyli czaka az pojawi sie jakis klient i go obudzi
        pthread_mutex_lock(&poczekalnia);
        liczbaMiejsc++;
        pthread_mutex_unlock(&poczekalnia);
        sem_post(&fryzjer);// fryzjer wysyla sygnal o tym ze jest gotowy do scinania
        czekaj(czas_fryzjera);  // scinamy klienta
        printf("Res:%d WRomm: %d/%d [in: %d]  -  skonczono scinac\n",nie_weszli, liczbaKrzesel-liczbaMiejsc, liczbaKrzesel, akt_klient);
        pthread_mutex_unlock(&fotel); // zwalniamy miejsce na fotelu dla nastepnego klienta
        }
    }
    printf("fryzjer idzie do domu\n");
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    static struct option parametry[] =
    {
        {"klient", required_argument, NULL, 'k'},
        {"krzesla", required_argument, NULL, 'r'},
        {"czas_k", required_argument, NULL, 'c'},
        {"czas_f", required_argument, NULL, 'f'},
        {"debug", no_argument, NULL, 'd'}

    };
    int liczbaKlientow =20;
    int wybor = 0;
    while((wybor = getopt_long(argc, argv, "k:r:c:f:d",parametry,NULL)) != -1)
    {
        switch(wybor)
        {
        case 'k': //max liczba klientow
            liczbaKlientow=atoi(optarg);
            break;
        case 'r': // liczba krzesel w poczekalni
            liczbaMiejsc=atoi(optarg);
            liczbaKrzesel=atoi(optarg);
            break;
        case 'c': // co ile czasu maja pojawiac sie klienci w salonie
            czas_klienta=atoi(optarg);
            break;
        case 'f':  // jak szybko ma scinac fryzjer
            czas_fryzjera=atoi(optarg);
            break;
        case 'd':
            debug=true;
            break;
        }
    }

    pthread_t *klienci_watki = malloc(sizeof(pthread_t)*liczbaKlientow);
    pthread_t fryzjer_watek;
    int *tablica=malloc(sizeof(int)*liczbaKlientow);

    int i;
    for (i=0; i<liczbaKlientow; i++)
    {
        tablica[i] = i;
    }

    sem_init(&klienci,0,0);
    sem_init(&fryzjer,0,0);

    pthread_mutex_init(&fotel, NULL);
    pthread_mutex_init(&poczekalnia, NULL);
    pthread_create(&fryzjer_watek, NULL, Fryzjer, NULL);

    for (i=0; i<liczbaKlientow; ++i)
    {
        pthread_create(&klienci_watki[i], NULL, Klient, (void *)&tablica[i]);
    }

    for (i=0; i<liczbaKlientow; i++)
    {
        pthread_join(klienci_watki[i],NULL);
    }
    skonczone=true;
    //sem_post(&klienci);
    pthread_join(fryzjer_watek,NULL);

    pthread_mutex_destroy(&fotel);
    pthread_mutex_destroy(&poczekalnia);
    sem_destroy(&klienci);
    sem_destroy(&fryzjer);
    free(czekajacy);
    free(odrzuceni);
    return 0;
}
