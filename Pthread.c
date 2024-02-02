#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MAX_WORDS 250000
#define MAX_WORD_LENGTH 100
#define MAXWORKERS 10

pthread_mutex_t barrier_mutex, file_write_mutex;
pthread_cond_t go;
int numWorkers;
int numArrived = 0;
char words[MAX_WORDS][MAX_WORD_LENGTH];
int word_count = 0;
int palindromicCountPerWorker[MAXWORKERS] = {0};

void Barrier() {
    pthread_mutex_lock(&barrier_mutex);
    numArrived++;
    if (numArrived == numWorkers) {
        numArrived = 0;
        pthread_cond_broadcast(&go);
    } else {
        pthread_cond_wait(&go, &barrier_mutex);
    }
    pthread_mutex_unlock(&barrier_mutex);
}

double read_timer() {
    static struct timeval start;
    struct timeval end;
    gettimeofday(&end, NULL);
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

void loadDictionary(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening dictionary file");
        exit(1);
    }
    while (fgets(words[word_count], MAX_WORD_LENGTH, file) && word_count < MAX_WORDS) {
        words[word_count][strcspn(words[word_count], "\n")] = '\0';
        word_count++;
    }
    fclose(file);
}

int isPalindromic(const char* word) {
    int len = strlen(word);
    for (int i = 0; i < len / 2; i++) {
        if (word[i] != word[len - i - 1]) {
            return 0; // Not a palindrome
        }
    }
    return 1; // Is a palindrome
}

void *Worker(void *arg) {
    long myid = (long) arg;
    int first = myid * (word_count / numWorkers);
    int last = (myid == numWorkers - 1) ? (word_count - 1) : (first + (word_count / numWorkers) - 1);

    FILE *outFile = fopen("palindromic_words.txt", "a");
    if (outFile == NULL) {
        perror("Error opening output file");
        exit(1);
    }

    palindromicCountPerWorker[myid] = 0;
    for (int i = first; i <= last; i++) {
        if (isPalindromic(words[i])) {
            pthread_mutex_lock(&file_write_mutex);
            fprintf(outFile, "%s\n", words[i]);
            pthread_mutex_unlock(&file_write_mutex);
            palindromicCountPerWorker[myid]++;
        }
    }

    fclose(outFile);
    Barrier();
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_attr_t attr;
    pthread_t workerid[MAXWORKERS];

    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    pthread_mutex_init(&barrier_mutex, NULL);
    pthread_mutex_init(&file_write_mutex, NULL);
    pthread_cond_init(&go, NULL);

    if (argc > 2) {
        numWorkers = atoi(argv[1]);
        loadDictionary(argv[2]);
    } else {
        printf("Usage: %s <number_of_workers> <dictionary_file_path>\n", argv[0]);
        return 1;
    }

    if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;

    double start_time = read_timer();
    for (long l = 0; l < numWorkers; l++) {
        pthread_create(&workerid[l], &attr, Worker, (void *)l);
    }

    for (int i = 0; i < numWorkers; i++) {
        pthread_join(workerid[i], NULL);
    }

    double end_time = read_timer();
    printf("Execution time: %f sec\n", end_time - start_time);

    printf("Palindromic words count by each worker:\n");
    int totalPalindromic = 0;
    for (int i = 0; i < numWorkers; i++) {
        printf("Worker %d found %d palindromic words\n", i, palindromicCountPerWorker[i]);
        totalPalindromic += palindromicCountPerWorker[i];
    }
    printf("Total palindromic words: %d\n", totalPalindromic);

    pthread_mutex_destroy(&barrier_mutex);
    pthread_mutex_destroy(&file_write_mutex);
    pthread_cond_destroy(&go);
    pthread_attr_destroy(&attr);

    return 0;
}
