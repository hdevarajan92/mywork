/*
 * Compile using g++ -pthread pthread.cpp -o pthread
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#define _REENTRANT
/*used to caculate minimum during elimination*/
#define min(a, b)	((a) < (b)) ? (a) : (b)

/*Program Parameters.*/

/*Maximum value of N (dimension size of matrix).*/
#define MAXN 2000

/*                          Matrix size.                              */
int N;

/* Matrices and vector. */
volatile float A[MAXN][MAXN], B[MAXN], X[MAXN];

/*Number of threads to use. */
int numThreads;

/* For storing the identity of each thread.*/
pthread_t Threads[_POSIX_THREAD_THREADS_MAX];
/* For Synchronization locks*/
pthread_mutex_t Mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t CountLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t NextIter = PTHREAD_COND_INITIALIZER;

int Norm,
CurrentRow, Count;

/*This function spawns off <NumThreads>.*/
void createThreads(void);

/*This function runs concurrently in <NumThreads> threads.*/
void *gaussPT(void *);

/*This function determines the chunk size of rows.*/
int find_next_row_chunk(int *, int);

/*This function implements a barrier synchronisation.*/
void barrier(int *);

/*This function awaits the termination of all threads.*/
void wait_for_threads(void);

/*Returns a seed for <srand()> based on the time.*/
unsigned int time_seed(void) {
	struct timeval t;
	struct timezone tzdummy;

	gettimeofday(&t, &tzdummy);
	return (unsigned int) (t.tv_usec);
}

/* Set the program parameters from the command-line arguments */
void parameters(int argc, char **argv) {
	int seed = 0; /* Random seed */
	char uid[32]; /*User name */

	/* Read command-line arguments */
	srand(time_seed()); /* Randomize */
	bool isSet = false;
	/*inputs are mandatory*/
	if (argc >= 2) {
		N = atoi(argv[1]);
		if (N < 1 || N > MAXN) {
			printf("N = %i is out of range.\n", N);
			exit(0);
		}
		isSet = true;
	}
	if (argc >= 3) {
		seed = atoi(argv[2]);
		srand(seed);
		isSet = true;

	} else {
		isSet = false;
	}
	if (argc >= 4) {
		freopen(argv[3], "w", stdout);

	} else {
		isSet = false;
	}
	if (argc >= 5) {
		numThreads = atoi(argv[4]);

	}
	if (!isSet) {
		printf(
				"Usage: %s <matrix_dimension> [random seed] [log file] [Number of threads]<>\n",
				argv[0]);
		exit(0);
	}

	/* Print parameters */
	printf("\nMatrix dimension N = %i.\n", N);
	printf("Random seed = %i\n", seed);
}
/*Initialise Inputs A and B to random number and X to 0*/
void initialise_inputs(void) {
	int row, col;

	printf("\nInitialising ...\n");
	for (col = 0; col < N; col++) {
		/*
		 * The addition of 1 is to ensure non-zero entries in the
		 * coefficient matrix A.
		 */
		for (row = 0; row < N; row++)
			A[row][col] = (float) rand() / RAND_MAX + 1;
		B[col] = (float) rand() / RAND_MAX + 1;
		X[col] = 0.0;
	}
}
/*Print inputs if N is less than 10*/
void print_inputs(void) {
	int row, col;

	if (N < 10) {
		printf("\nA =\n\t");
		for (row = 0; row < N; row++) {
			for (col = 0; col < N; col++)
				printf("%6.3f  %s", A[row][col],
						(col < N - 1) ? ", " : ";\n\t");
		}
		printf("\nB = [");
		for (col = 0; col < N; col++)
			printf("%6.3f  %s", B[col], (col < N - 1) ? "; " : "]\n");
	}
}
/*Print the ourput matrix X*/
void print_X(void) {
	int row;
	printf("\nX = [");
	for (row = 0; row < N; row++)
		printf("%6.3f  %s", X[row], (row < N - 1) ? "; " : "]\n");
}

int main(int argc, char **argv) {
	/* Timing variables */
	struct timeval etstart, etstop; /* Elapsed times using gettimeofday() */
	struct timezone tzdummy;
	clock_t etstart2, etstop2; /* Elapsed times using times() */
	unsigned long long usecstart, usecstop;
	struct tms cputstart, cputstop; /* CPU times for my processes */

	int row, col;

	parameters(argc, argv);
	freopen(argv[3], "w", stdout);
	initialise_inputs();
	print_inputs();

	CurrentRow = Norm + 1;
	Count = numThreads - 1;

	/* Start Clock */
	printf("\nStarting clock.\n");
	gettimeofday(&etstart, &tzdummy);
	etstart2 = times(&cputstart);
	/*create Threads for each thread will solve one aprt of gauss*/
	createThreads();
	/*wait for all threads to complete*/
	wait_for_threads();

	/*
	 * Diagonal elements are not normalised to 1.
	 * This is treated in back substitution.
	 */

	/*Back substitution.*/
	for (row = N - 1; row >= 0; row--) {
		X[row] = B[row];
		for (col = N - 1; col > row; col--)
			X[row] -= A[row][col] * X[col];
		X[row] /= A[row][row];
	}

	/* Stop Clock */
	gettimeofday(&etstop, &tzdummy);
	etstop2 = times(&cputstop);
	printf("Stopped clock.\n");
	usecstart = (unsigned long long) etstart.tv_sec * 1000000 + etstart.tv_usec;
	usecstop = (unsigned long long) etstop.tv_sec * 1000000 + etstop.tv_usec;

	print_X();

	/* Display timing results */
	printf("\nElapsed time = %g ms.\n",
			(float) (usecstop - usecstart) / (float) 1000);

	printf("(CPU times are accurate to the nearest %g ms)\n",
			1.0 / (float) CLOCKS_PER_SEC * 1000.0);
	printf("My total CPU time for parent = %g ms.\n",
			(float) ((cputstop.tms_utime + cputstop.tms_stime)
					- (cputstart.tms_utime + cputstart.tms_stime))
					/ (float) CLOCKS_PER_SEC * 1000);
	printf("My system CPU time for parent = %g ms.\n",
			(float) (cputstop.tms_stime - cputstart.tms_stime)
					/ (float) CLOCKS_PER_SEC * 1000);
	printf("My total CPU time for child processes = %g ms.\n",
			(float) ((cputstop.tms_cutime + cputstop.tms_cstime)
					- (cputstart.tms_cutime + cputstart.tms_cstime))
					/ (float) CLOCKS_PER_SEC * 1000);
	/* Contrary to the man pages, this appears not to include the parent */
	printf("--------------------------------------------\n");
	return 0;

}

void *gaussPT(void *dummy) {
	/*  <myRow> denotes the first row of the chunk assigned to a thread.  */
	int myRow = 0, row, col;

	/*Normalisation row.*/
	int myNorm = 0;

	float multiplier;
	int chunkSize;

	/*Actual Gaussian elimination begins here.*/

	while (myNorm < N - 1) {
		/*Ascertain the row chunk to be assigned to this thread.*/
		while (chunkSize = find_next_row_chunk(&myRow, myNorm)) {

			/*We perform the eliminations across these rows concurrently.*/
			for (row = myRow; row < (min(N, myRow + chunkSize)); row++) {
				multiplier = A[row][myNorm] / A[myNorm][myNorm];
				for (col = myNorm; col < N; col++)
					A[row][col] -= A[myNorm][col] * multiplier;
				B[row] -= B[myNorm] * multiplier;
			}
		}

		/*We wait until all threads are done with this stage.*/
		/*We then proceed to handle the next normalisation row.*/
		barrier(&myNorm);
	}
}
/*makes thread wait till all threads dont complete.*/
void barrier(int *myNorm) {

	/*We implement synchronisation using condition variables.*/
	pthread_mutex_lock(&CountLock);

	if (Count == 0) {
		/*Only the last thread for each value of <Norm> reaches this point.*/
		Norm++;
		Count = numThreads - 1;
		CurrentRow = Norm + 1;
		pthread_cond_broadcast(&NextIter);
	} else {
		Count--;
		pthread_cond_wait(&NextIter, &CountLock);
	}

	/*<*myNorm> is each thread's view of the global variable <Norm>.*/
	*myNorm = Norm;

	pthread_mutex_unlock(&CountLock);
}
/*synchronises between threads and get the row chunk not taken by any thread.*/
int find_next_row_chunk(int *myRow, int myNorm) {
	int chunkSize;

	pthread_mutex_lock(&Mutex);

	*myRow = CurrentRow;

	/*For guided-self scheduling, we determine the chunk size here.*/
	chunkSize = (*myRow < N) ? (N - myNorm - 1) / (2 * numThreads) + 1 : 0;
	CurrentRow += chunkSize;

	pthread_mutex_unlock(&Mutex);

	return chunkSize;
}
/*Creates threads*/
void createThreads(void) {
	for (int i = 0; i < numThreads; i++) {
		pthread_create(&Threads[i], NULL, gaussPT, NULL);
	}
}
/*Waits for all threads to complete*/
void wait_for_threads(void) {
	int i;

	for (i = 0; i < numThreads; i++) {
		pthread_join(Threads[i], NULL);
	}
}