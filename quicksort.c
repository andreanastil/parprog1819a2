#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
//gcc -pthread -O2 quicksort.c -o qsort 

#define N 1000000 
#define THREADS 4 

#define ARRAY_SIZE 100 
#define CUTOFF 10	

#define WORK 0
#define FINISH 1
#define SHUTDOWN 2

struct message {
	int type;
	int start;
	int end;
};

struct message mqueue[N]; // global queue

int qin = 0;
int qout = 0; 
int message_count = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //protecting common resources
pthread_cond_t msg_in = PTHREAD_COND_INITIALIZER; // condition variable, signals a put operation (receiver waits on this)
pthread_cond_t msg_out = PTHREAD_COND_INITIALIZER; // condition variable, signals a get operation (sender waits on this)

void swap (double *a, int i, double *b, int j){
	double tmp = a[i];
	a[i] = b[j];
	b[j] = tmp;
}
int partition (double *a, int n) {
	int first = 0;
	int middle = n/2;
	int last = n-1;

	if (a[first] > a[middle]) {
		swap(a,first,a,middle);
	}
	if (a[middle] > a[last]) {
		swap(a, middle, a, last);
	}
	if (a[first] > a[middle]) {
		swap(a, first, a, middle);
	}

	int i,j;
	double pivot = a[middle];
	for (i=1, j=n-2 ;; i++, j--) {

		//find element that needs sorting	
		while(a[i]<pivot) {
			i++;
		}
		while(a[j]>pivot) {
			j--;
		}
		if (i >=j) {
			break;
		}

		swap(a,i,a,j);
	}
	return i;
}

void inssort(double *a, int n) {
	int j;
	for (int i=1; i<n; i++) {
		j=i;
		while ((j>0) && (a[j-1] > a[j])) {
			swap(a, j-1, a, j);
			j--;
		}
	}
} 
void send(int type, int start, int end) {

	pthread_mutex_lock(&mutex);
	while (message_count>=N) {
		printf("producer locked\n");
		pthread_cond_wait (&msg_out, &mutex);

	}//when the while is finished we are ready to send

	mqueue[qin].type = type; //copy message's contents 
	mqueue[qin].start = start;
	mqueue[qin].end = end;
	qin++;
	message_count++;
	if (qin >= N){ //wrap around
		qin = 0;
	}

	pthread_cond_signal(&msg_in);
	pthread_mutex_unlock(&mutex);
}

void recv (int *type, int *start, int *end) {
	pthread_mutex_lock(&mutex);
	while (message_count<1) {
		printf("\nconsumer locked\n");
		pthread_cond_wait (&msg_in, &mutex);	
	}
	*type = mqueue[qout].type;
	*start = mqueue[qout].start;
	*end = mqueue[qout].end;
	qout++;
	if (qout >= N){
		qout = 0;
	}

	message_count--;
	pthread_cond_signal(&msg_out);
	pthread_mutex_unlock(&mutex);

} 

void * thread_func(void *params) {
	double *a = (double*)params;
	int type, start, end;
	recv(&type, &start, &end);
	while(1) {
		
		if (type == WORK) { 
			int size = end-start;
			//if array is less than cutoff call insertion 
			if (size <= CUTOFF) {
            
                inssort(a+start, end-start);
                //completion msg
                send(FINISH, start, end);
			}
			else{ 
				//partition around pivot
				int pivot = partition(a+start, end-start);
				//send two new packets to queue
				send(WORK, start, start+pivot); 
				send(WORK, start+pivot, end);
			} 
		}	
		else if (type ==FINISH){
			send(FINISH,start, end); //send to queue
		}
		else if (type == SHUTDOWN){
			send(SHUTDOWN, 0, 0); //sent to queue and stop
			break;
		}

	recv(&type, &start, &end); //update packet	
	}

	pthread_exit(NULL);
}


int main () {
	double * a = (double*) malloc(N *  sizeof(double));
	if (a == NULL) {
        printf("error creating array\n");
        exit(1);
    }
	srand(time(NULL)); // randomize array's seed

	//Initialization
	for (int i=0; i<N; i++) {
		a[i] = (double) rand()/RAND_MAX;
	}

	pthread_t mythread[THREADS];

	for (int i = 0; i<THREADS; i++){

	if (pthread_create(&mythread[i], NULL, thread_func, a)!=0){
		printf("error creating thread\n");
		exit(1);
	}

	}

	
	send(WORK,0, ARRAY_SIZE); //start quicksort
	

	int completed = 0;

	int type, start, end;
	recv(&type, &start, &end);

	while(completed>=ARRAY_SIZE) { //while number of sorted elements are less than ARRAY_SIZE 
		if (type == FINISH){ //if finished count the elements
			completed += end- start; // number of sorted array elements
		}
		else {
			send(type, start, end); // else send to queue 
		}

	recv(&type, &start, &end);	//update packet  
	}

	send(SHUTDOWN, 0, 0); //shut down the process


	for (int i = 0; i<THREADS; i++){
	pthread_join(mythread[i], NULL);
	}

	// check if array is actually sorted
	int i;
	for (i=0; i<(ARRAY_SIZE-1); i++) {
		if (a[i] > a[i+1]) {
			printf("%lf, %lf\n", a[i], a[i+1]);
			printf("sorting error\n");
			break;
		}
	}
	if (i ==  ARRAY_SIZE-1 ) {
		printf("array is sorted\n");
	}


	//deallocate memory
	free(a);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&msg_in);
	pthread_cond_destroy(&msg_out);
	return 0;
}

