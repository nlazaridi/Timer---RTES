#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>


#define PRODUCER_COUNT 3
#define CONSUMER_COUNT 10
#define THREAD_COUNT (PRODUCER_COUNT + CONSUMER_COUNT)
#define LOOP 100
#define QUEUE_SIZE 64

//declare functions that producers will insert in fifo queue
void *func_1(void* x);
void *func_2(void* x);
void *func_3(void* x);
void *func_4(void* x);
void *func_5(void* x);

// declaration of array of functions and inserting the pointers of the above functions
static void * (*funcs_array[5])(void *) = {&func_1,&func_2,&func_3,&func_4,&func_5}; //changed

//declaration of array of arguments and inserting numbers from 0 to 4
static int rand_args[5] = {0,1,2,3,4};


//my function structure
typedef struct{
  void * (*work)(void *);
  void * arg;
  struct timeval start,end;
  double duration;
} workFunction;


void *producer (void *args);
void *consumer (void *args);


// the structure of my fifo queue
typedef struct {
  int counter;
  workFunction buf[QUEUE_SIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut, *queue_tail_lock;
  pthread_cond_t *notFull, *notEmpty;
} queue;

//my timer struct
typedef struct timer{
    void* (*TmrFcn)(void *);
    void* (*StartFcn)(void *);
    void* (*StopFcn)(void *);
    void* (*ErrorFcn)(void *);
    void* my_arg;
    queue* fifo;
    double period;
    int tasksToExecute, startDelay;
    int timCounter;
} timer;

// the functions I will use
queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction in); //queueAdd now adds workFunction variable
void queueDel (queue *q, workFunction *out); //queueDel now deletes workFunction variable
void *StartFunction(void *arg);
void *StopFunction(void *arg);
void *ErrorFunction(void *arg);
void timerInit(timer *t, double period, int tasksToExecute, int startDelay, void *(*StartFcn)(void *), void *(*StopFcn)(void *), void *(*TmrFcn)(void *), void *(*ErrorFcn)(void *), void *my_arg, queue* fifo);
void start(timer *t);


struct timeval func_start, func_end;
int numProd = 0;
int producers_ended = 0;
pthread_t prod[PRODUCER_COUNT], cons[CONSUMER_COUNT];
timer* tmr[3];
queue *q;

void *func_1(void *x);
void *func_2(void *x);
void *func_3(void *x);
void *func_4(void *x);
void *func_5(void *x);

struct timeval my_start, my_end;
useconds_t my_time_taken;

FILE *con_file;
FILE *prod_file;


int main(){

  con_file = fopen("cons_time_3.txt", "w+");
  prod_file = fopen("prod_time_3.txt", "w+");

  q = queueInit ();
  if (q ==  NULL) {
    fprintf (stderr, "Queue Initialization failed.\n");
    exit (1);
  }

  tmr[0] = (timer *)malloc(sizeof(timer));
  tmr[1] = (timer *)malloc(sizeof(timer));
  tmr[2] = (timer *)malloc(sizeof(timer));


  //timer initialization
  timerInit(tmr[0], 0.01, 360000, 0, StartFunction, StopFunction, funcs_array[0], ErrorFunction, &rand_args[0],q);
  timerInit(tmr[1], 0.1, 36000, 0, StartFunction, StopFunction, funcs_array[0], ErrorFunction, &rand_args[0],q);
  timerInit(tmr[2], 1.0, 3600, 0, StartFunction, StopFunction, funcs_array[0], ErrorFunction, &rand_args[0],q);



  gettimeofday(&my_start, NULL);

  //starting timers
  start(tmr[0]);
  
  start(tmr[1]);
  
  start(tmr[2]);

  //creating consumers
  for (int i = 0; i < CONSUMER_COUNT; i++) {
    pthread_create (&cons[i], NULL, consumer, tmr[0]);
    
  }
  //joining threads
  for (int i = 0; i < PRODUCER_COUNT; i++) {
   
    pthread_join (prod[i], NULL);
     printf("JOIN PRODUCER %d =========================\n",i);
  }
 
  producers_ended = 1;
  printf("producers_ended %d !!!!!!!!!!!\n",producers_ended);
  

  for (int i = 0; i < CONSUMER_COUNT; i++) {
    pthread_join (cons[i], NULL);
  }

  queueDelete (q);
  gettimeofday(&my_end, NULL);
  my_time_taken = (my_end.tv_usec + my_end.tv_sec*1000000) - (my_start.tv_usec + my_start.tv_sec*1000000);
  printf("\nExecution Time: %f seconds\n", (float)my_time_taken/1000000);
  fclose(prod_file);
  return 0;

}

void timerInit(timer *t, double Period, int TasksToExecute, int StartDelay, void *(*StartFcn)(void *), void *(*StopFcn)(void *), void *(*TmrFcn)(void *), void *(*ErrorFcn)(void *), void *UserData, queue* Fifo){
  
  
  //printf("Timer Initialization: Period: %d ms,  Tasks: %d\n", TasksToExecute, Period);
  t->fifo = Fifo;
  t->period = Period;
  t->tasksToExecute = TasksToExecute;
  t->startDelay = StartDelay;
  t->StartFcn = StartFcn;
  t->TmrFcn = TmrFcn;
  t->ErrorFcn = ErrorFcn;
  t->StopFcn = StopFcn;
  t->timCounter = 0;

  return;
}

void start(timer *t){

  tmr[numProd] = t;
  usleep(tmr[numProd]->startDelay);
  pthread_create (&prod[numProd], NULL, producer, tmr[numProd]);
  numProd++;
  return;
}




void *producer(void *t){
  timer* my_tmr;
  int i,sleep,time_drift;
  my_tmr = (timer *)t;
  int tasksToExecute = my_tmr->tasksToExecute;
  float period = my_tmr->period;
  int startDelay = my_tmr->startDelay;
  void *argm;
  struct timeval time;
  useconds_t start, end, time_taken;
  gettimeofday(&time, NULL);
  sleep = 1000000*period;
  start = (time.tv_sec*1000000 + time.tv_usec) - sleep;

  clock_t beg,fin;
  double time_used;
  beg = clock();


  for(i = 0; i < (my_tmr->tasksToExecute); i++){
 
    pthread_mutex_lock (my_tmr->fifo->mut);

    while (my_tmr->fifo->full) {

      printf ("producer: queue FULL.\n");
      pthread_cond_wait (my_tmr->fifo->notFull, my_tmr->fifo->mut);
    }

    workFunction job;

    void * argm;
    argm = &rand_args[rand()%5];

        //We define arguments to the structure
    job.arg = argm;
    job.work = funcs_array[rand()%5];

    gettimeofday(&time, NULL);
    queueAdd(my_tmr->fifo, job);
    fin = clock();
    time_used = ((double) (fin - beg)) / CLOCKS_PER_SEC;
    beg = fin;
   
    printf("QUEUE ADD TASK\n");
    pthread_mutex_lock (my_tmr->fifo->queue_tail_lock);
    my_tmr->fifo->counter++;
    my_tmr->timCounter++;
    //printf("Task number %d added.",my_tmr->fifo->counter);
    pthread_mutex_unlock (my_tmr->fifo->queue_tail_lock);
    pthread_mutex_unlock (my_tmr->fifo->mut);
    pthread_cond_signal (my_tmr->fifo->notEmpty);


    end = (time.tv_sec*1000000 + time.tv_usec);
    time_taken = end - start;
    start = end;
    time_drift = time_taken - period*1000000;
    fprintf(prod_file, "%f\n",time_drift);
    sleep = sleep - time_drift;
    usleep(sleep);
    printf("I IS: %d and TASKSTOEXECUTE: %d\n",i,my_tmr->tasksToExecute);
  }
  printf("============ PRODUCER FINISHED ===========\n");
  //fclose(prod_file);
  return NULL;
}

void *consumer(void *t){
  timer* my_tmr;
  my_tmr = (timer*)t;
  workFunction job;
  clock_t start,end;
  double time_used;
  start = clock();
  while (1){
    pthread_mutex_lock (my_tmr->fifo->mut);
    while(my_tmr->fifo->empty){

      printf ("consumer: queue EMPTY. AND COUNTER = %d\n",my_tmr->fifo->counter);
      
      if(my_tmr->fifo->counter == 399600){
      	printf("\n============ CONSUMER FINISHED ===========\n");
      	pthread_mutex_unlock (my_tmr->fifo->mut);
  		pthread_cond_signal (my_tmr->fifo->notEmpty);

        return NULL;
      }
      pthread_cond_wait (my_tmr->fifo->notEmpty, my_tmr->fifo->mut);

      if (producers_ended == 1){
      	return NULL;
      }
    }

    queueDel(my_tmr->fifo, &job);
    end = clock();
    time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    start = end;
    fprintf(con_file, "%f\n",time_used);
    pthread_mutex_unlock (my_tmr->fifo->mut);
    pthread_cond_signal (my_tmr->fifo->notFull);
    (*job.work)(job.arg);
  }
  fclose(con_file);
  return(NULL);
}

void queueAdd (queue *q, workFunction in)
{
  q->buf[q->tail] = in;
  q->tail++;
  if (q->tail == QUEUE_SIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueDel (queue *q, workFunction *out)
{
  *out = q->buf[q->head];

  q->head++;
  if (q->head == QUEUE_SIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}

queue *queueInit (void){
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->counter = 0;
  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
  q->queue_tail_lock = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init (q->queue_tail_lock, NULL);

  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);
  pthread_mutex_destroy (q->queue_tail_lock);
  free (q->queue_tail_lock);
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}


void* func_1(void* arg){
  //prints a message
  printf("Hello this is random func_1\n");
  return (NULL);
}

void* func_2(void* arg){
  int i;
  for(i=0 ; i<10 ; i++){
    int x=rand()%360;
  }
  printf("Random Function is function 2 and random argument is %d\n",*((int *) arg));
  return (NULL);
}

void* func_3(void* arg){
  //prints a message
  printf("Random Function is function 3 and random argument is %d\n",*((int *) arg));
  return (NULL);
}

void* func_4(void* arg){
  //prints a message
  printf("Random Function is function 4 and random argument is %d\n",*((int *) arg));
  return (NULL);
}

void* func_5(void* arg){
  //prints a message
  printf("Random Function is function 5 and random argument is %d\n",*((int *) arg));
  return (NULL);
}




// Timer Functions
void *StartFunction(void *arg)
{
    printf("Start Function\n");
    return NULL;
}


void *StopFunction(void *arg)
{
    printf("Stop Function\n");
    return NULL;
}


void *ErrorFunction(void *arg)
{
    printf("Error Function\n");
    return NULL;
}
