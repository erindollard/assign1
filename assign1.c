#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


int M, K, N, T1, T2, T3, T4;

int n_leaving = 0; //updated by customer
int n_waiting = 0; // updated by customer
int barber_busy = 0; // updated by barber
int call_num = 0; // updated by assistant
int ticket_num = 0; // updated and used by customer
int ready = 0;
int next_free = 0;
int last_full = 0;
int bounded_buff[10000];
int assigned = 1;


void * customer_routine(void *);
void * barber_routine(void *);
void * assistant_routine(void *arg);

//declare global mutex and condition variables
pthread_mutex_t seats_mutex = PTHREAD_MUTEX_INITIALIZER; //static initialization
pthread_mutex_t barber_chair_mutex = PTHREAD_MUTEX_INITIALIZER; //static initilization
pthread_mutex_t barber_mutex = PTHREAD_MUTEX_INITIALIZER; //static initilization

pthread_cond_t customer_cond;
pthread_cond_t barber_cond;
pthread_cond_t assistant_cond;
pthread_cond_t cut_done;
pthread_cond_t customer_ready_cond;
pthread_cond_t barber_free_cond;
pthread_cond_t customer_called;
pthread_cond_t both_ready;
pthread_cond_t called = PTHREAD_COND_INITIALIZER;


int main(int argc, char ** argv){

    pthread_t *threads; //system thread id
    int *t_ids; //user-defined thread id
    int k, rc, t;

    // ask for a total number of customers.
    printf("Enter the total number of customers (int): \n");
    scanf("%d", &M);
    // ask for the total number of barbers.
    printf("Enter the total number of barbers (int): \n");
    scanf("%d", &K);
    // ask for number of chairs
    printf("Enter the total number of waiting chairs (int): \n");
    scanf("%d", &N);
    //ask for barber's min working pace
    printf("Enter barber's minimum service time (int): \n");
    scanf("%d", &T1);
    //ask for barber's max working pace
    printf("Enter barber's maximum service time (int): \n");
    scanf("%d", &T2);
    //ask for customers' minimum arrival rate
    printf("Enter customers minimum arrival rate (int): \n");
    scanf("%d", &T3);
    //ask for customers' maximum arrival rate
    printf("Enter customers maximum arrival rate (int): \n");
    scanf("%d", &T4);
   
    //Initialize condition variable objects
    rc = pthread_cond_init(&customer_cond, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    rc = pthread_cond_init(&barber_cond, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    rc = pthread_cond_init(&assistant_cond, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    rc = pthread_cond_init(&barber_free_cond, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    rc = pthread_cond_init(&customer_ready_cond, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    rc = pthread_cond_init(&cut_done, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    rc = pthread_cond_init(&customer_called, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    rc = pthread_cond_init(&both_ready, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", rc);
        exit(-1);
    }
    
    threads = malloc((M+K+1) * sizeof(pthread_t)); 
    if(threads == NULL){
        fprintf(stderr, "threads out of memory\n");
        exit(1);
    }
    t_ids = malloc((M+K+1) * sizeof(int)); 
    if(t_ids == NULL){
        fprintf(stderr, "t_ids out of memory\n");
        exit(1);
    }

    //create barber threads. 
    for (k = 0; k<K; k++){
        t_ids[k] = k;
        rc = pthread_create(&threads[k], NULL, barber_routine, (void *) &t_ids[k+1]);
        if (rc) {
            printf("ERROR; return code from pthread_create() (barber) is %d\n", rc);
            exit(-1);
        }
    }
    // create assistant thread 
    rc = pthread_create(&threads[K+M], NULL, assistant_routine, NULL);
    if (rc) {
        printf("ERROR; return code from pthread_create() (assistant) is %d\n", rc);
        exit(-1);
    }

    //create consumer threads
    for (k = K; k<M+K; k++){
        t_ids[k] = k;
        rc = pthread_create(&threads[k], NULL, customer_routine, (void *) &t_ids[k-K+1]);
        if (rc) {
            printf("ERROR; return code from pthread_create() (consumer) is %d\n", rc);
            exit(-1);
        }
    }

    //join barber threads.
    for (k = K; k<M+K; k++){
        pthread_join(threads[k], NULL);
    }
    //join consumer threads.
    for (k = 0; k<K; k++){
        pthread_join(threads[k], NULL);
    }
    //terminate the assistant thread using pthread_cancel().
    pthread_cancel(threads[K+M]);
    
    free(threads);
    free(t_ids);
    //destroy mutex and condition variable objects
    pthread_mutex_destroy(&seats_mutex);
    pthread_mutex_destroy(&barber_chair_mutex);
    pthread_cond_destroy(&customer_cond);
    pthread_cond_destroy(&barber_cond);
    pthread_cond_destroy(&assistant_cond);
    pthread_exit(NULL);
}

void *barber_routine(void *arg) {
    int barber_id = *((int *)arg);
    while (1) {
        printf("Barber %d: I'm now ready to accept a customer.\n", barber_id);
        pthread_mutex_lock(&barber_mutex);
        bounded_buff[next_free] = barber_id;
        next_free ++;
        last_full++;
        pthread_mutex_unlock(&barber_mutex);

        pthread_mutex_lock(&seats_mutex);


        // call assistant to tell them barber is free
        pthread_cond_signal(&barber_free_cond);
        

        // wait for assistant to assign customer
        pthread_cond_wait(&customer_called, &seats_mutex);
        if (call_num == -1) {
            printf("Barber %d: Thank you Assistant, see you tomorrow", barber_id);
            pthread_mutex_unlock(&seats_mutex);
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&seats_mutex);
        pthread_mutex_lock(&barber_chair_mutex);

        
        printf("Barber %d: Hello, Customer %d \n", barber_id, call_num);

        int working_time = T1 + (rand() % (T2 - T1 + 1));
        sleep(working_time);

        printf("Barber %d: Finished cutting, Goodbye Customer %d.\n", barber_id, call_num);
        call_num++;
        pthread_cond_signal(&cut_done);
        pthread_mutex_unlock(&barber_chair_mutex);
    }
}

void * customer_routine(void * arg){
    int customer_id = *((int *)arg);
    int arrival_time = T3 + (rand() % (T4 - T3 + 1));
    sleep(arrival_time);
    pthread_mutex_lock(&seats_mutex);
    printf("Customer %d: I have arrived at the barber shop.\n", customer_id);
    
    if (n_waiting == N ){
        printf("customer %d: oh no! all seats have been taken and I'll leave now!\n", customer_id);
        n_leaving++;
        pthread_exit(NULL);
    }
    n_waiting++;
    int ticket = ticket_num+1;
    ticket_num++;

    // call assistant
    pthread_cond_signal(&customer_ready_cond);
    printf("Customer %d: I'm lucky to get a free seat and a ticket numbered %d\n", customer_id, ticket);
    

   pthread_cond_wait(&customer_called, &seats_mutex);
    
    n_waiting--;
    ticket_num --;
    pthread_mutex_unlock(&seats_mutex);


    // get assigned to barber
    printf("Customer %d: My ticket number %d has been called. Hello, Barber.\n", customer_id, ticket);
    pthread_mutex_lock(&barber_chair_mutex);
    pthread_cond_wait(&cut_done, &barber_chair_mutex);
    printf("Customer %d: Well done. Thank you barber, bye!\n", customer_id);
    n_leaving++;

    if (n_leaving == M){
        n_leaving = -1;
        printf("Assistant: Hi Barber, we've finished the work for the day.\n");
        call_num = -1;
        pthread_cond_signal(&barber_cond);
        pthread_mutex_unlock(&barber_chair_mutex);
        pthread_exit(NULL);
    }
    
    pthread_mutex_unlock(&barber_chair_mutex);
    pthread_exit(NULL);
}

void *assistant_routine(void *arg) {
    while (1) {
        pthread_mutex_lock(&seats_mutex);
        printf("Assistant: I'm waiting for customers.\n");
        while (n_waiting == 0){
            pthread_cond_wait(&customer_ready_cond, &seats_mutex);
        }
        if (n_leaving == -1 && barber_busy == 0) {
            printf("Assistant: Hi Barbers, we've finished the work for the day.\n");
            call_num = -1;
            pthread_cond_signal(&barber_cond);
            pthread_exit(NULL);
        }
       
        printf("Assistant: I'm waiting for a barber to become available.\n");
       
        pthread_cond_wait(&barber_free_cond, &seats_mutex);
        
        
        
        printf("Assistant: Assign Customer %d to Barber %d.\n", call_num, bounded_buff[last_full]);
        last_full --;
        next_free--;
        call_num = ticket_num;
        pthread_cond_signal(&customer_called);
      
        pthread_mutex_unlock(&seats_mutex);
    }
    pthread_exit(NULL);
}



