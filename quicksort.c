/*
to run
 gcc -O2 -Wall -pthread a2.c -o quicksort
./quicksort
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define LIMIT 20

#define MESSAGES 50

#define WORK 0
#define FINISH 1
#define SHUTDOWN 2

#define THREADS 4
#define N 1000


struct message {
	int type;
	int start_pos;
	int end_pos;
};

struct message mqueue[MESSAGES];

//queue in and out pos counters
int qin = 0, qout = 0;

int message_count = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t msg_in = PTHREAD_COND_INITIALIZER;
pthread_cond_t msg_out = PTHREAD_COND_INITIALIZER;


float random_float( float min, float max){
    return ((((float) rand()) / (float) RAND_MAX)*(max - min)) + min;
}

void swap(double *a, double *b) {
	double t = *a;
	*a = *b;
	*b = t;
}

void inssort(double *a,int n) {
	int i,j;
	for (i=1;i<n;i++) {
		j = i;
		while ((j>0) && (a[j-1]>a[j])) {
			swap(&a[j-1], &a[j]);
			j--;
		}
	}
}

int partition(double *a,int n) {
	int first,last,middle,i,j;
	double p;

	// take first, last and middle positions
	first = 0;
	middle = n/2;
	last = n-1;

	// put median-of-3 in the middle
	if (a[middle]<a[first]) {
		swap(&a[first], &a[middle]);
	}
	if (a[last]<a[middle]) {
		swap(&a[middle], &a[last]);
	}
	if (a[middle]<a[first]) {
		swap(&a[first], &a[middle]);
	}

	// partition (first and last are already in correct half)
	p = a[middle]; // pivot
	for (i=1,j=n-2;;i++,j--) {
    	while (a[i]<p) i++;
		while (p<a[j]) j--;
    	if (i>=j) break;

    	swap(&a[i], &a[j]);
	}

	// return position of pivot
	return i;
}

void send(int type, int start_pos, int end_pos) {

	pthread_mutex_lock(&mutex);

	while(message_count >= MESSAGES) {
		pthread_cond_wait(&msg_out, &mutex);
	}
	//insert message in queue
	mqueue[qin].type = type;
	mqueue[qin].start_pos = start_pos;
	mqueue[qin].end_pos = end_pos;

	qin++;
	//if queue counter >= queue size
	//then return to the start
	if(qin >= MESSAGES) {
		qin = 0;
	}
	//add message to message counter
	message_count++;

	pthread_cond_signal(&msg_in);
	pthread_mutex_unlock(&mutex);
}

void receive(int *type, int *start_pos, int *end_pos) {
	pthread_mutex_lock(&mutex);

	while(message_count < 1) {
		pthread_cond_wait(&msg_out, &mutex);
	}
	//take message
	*type = mqueue[qout].type;
	*start_pos = mqueue[qout].start_pos;
	*end_pos = mqueue[qout].end_pos;

	qout++;
	//if queue counter >= queue size
	//then return to the start
	if(qout >= MESSAGES) {
		qout = 0;
	}
	//remove a message from message counter
	message_count--;

	pthread_cond_signal(&msg_out);
	pthread_mutex_unlock(&mutex);
}

void *thread_func(void *params) {
	double *a = (double*)params;
	int type, start_pos, end_pos;
    while(1) {
		receive(&type, &start_pos, &end_pos);
		if(type == SHUTDOWN) {
			send(SHUTDOWN, 0, 0);
			break;
        }
		else if(type == FINISH) {
			send(FINISH, start_pos, end_pos);
		}
		else if (type == WORK) {
			//work load
			int n = end_pos - start_pos;
			// check if below limit
			// if it is sort it
			if(n <= LIMIT) {
				inssort(a+start_pos, n);
				send(FINISH, start_pos, end_pos);
			}
			// if it's not partition into two halves
			// create packages to sort halves
			else {
				int pivot = partition(a+start_pos, n);
				int middle = start_pos+pivot;
				send(WORK, start_pos, middle);
				send(WORK, middle, end_pos);
			}
        }
    }
    pthread_exit(NULL);
}

int main() {
    double *a;
    int type, start_pos, end_pos;
    int completed = 0;

	a = (double*)malloc(N*sizeof(double));
	if(a == NULL) {
		return 1;
	}
	for(int i=0; i<N; i++) {
		a[i] = random_float(0.01,2.5);
	}
	//create threads
	pthread_t threads[THREADS];
	for(int i=0; i<THREADS; i++) {
		if(pthread_create(&threads[i], NULL, thread_func, a) != 0) {
			printf("error: thread creation");
			free(a);
			return 1;
		}
	}

	send(WORK, 0 , N);


	while(completed < MESSAGES) {
		receive(&type, &start_pos, &end_pos);

		if(type == FINISH) {
            completed += end_pos - start_pos;
        }
		else {
            send(type, start_pos, end_pos);
        }
    }

	send(SHUTDOWN, 0, 0);

	for(int i=0; i<THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	for(int i=0; i<N; i++) {
		if (i < N-1 && a[i] > a[i+1]) {
			printf("error: a[%d]=%f < a[%d]=%f !!!\n", i, a[i], i+1, a[i+1]);
			break;
		}
	}

	free(a);

	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&msg_in);
	pthread_cond_destroy(&msg_out);

	return 0;
}
