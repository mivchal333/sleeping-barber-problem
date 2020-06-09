#ifndef SLEEPING_BARBER_PROBLEM_FUNCTIONS_H
#define SLEEPING_BARBER_PROBLEM_FUNCTIONS_H

extern struct Node {
    int id;
    int time;
    struct Node *next;
}Node;

extern int freeSeatsAmount;
extern int seatsAmount;
extern int rejectedClientsCounter;
extern int maxShearTime;
extern int maxClientArriveTime;
extern int isDebug;
extern int isEnd;
extern int clientOnSeatId;

void *BarberThred();
void printWaitingList();
void doBarberWork();
void travelToBarbershop(int clientTime);
void push(struct Node **head_ref, int clientId, int clientTime);
void append(struct Node **head_ref, int clientId, int clientTime);
void printList(struct Node *node);
void addToWaitingList(int clientId, int clientTime);
void deleteNode(struct Node **head_ref, int key);
void addToRejectedList(int clientId, int clientTime);
void *ClientThread(void *client);

#endif //SLEEPING_BARBER_PROBLEM_FUNCTIONS_H
