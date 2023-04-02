#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

typedef struct lcgParams {
    int x0;
    int a;
    int c;
    int m;
    size_t size;
} lcgParams;

typedef struct otpParams {
    char *x;
    char *key;
    size_t size;
    char *output;
    pthread_barrier_t *bar;
} otpParams;

void StopAndCheckTime(struct timeval start, struct timeval end) {
    gettimeofday(&end, NULL);
    long int diff = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("%lo ms \n", diff);
    gettimeofday(&start, NULL);
}

//Создаем шифр лкг
char *MakeLcg(const int *const x0, const int *const a, const int *const c, const int *const m, const size_t *const size) {
  char *ptr = (char *)malloc(*size + 1);
  ptr[0] = (char)*x0;

  for (int i = 1; i < *size; i++) {
    ptr[i] = (char)((*a * ptr[i-1] + *c) % *m);
  }

  return ptr;
}

//Шифр вернама
void MakeOpt(const char *const x, const char *const key, const size_t *const size, char *ptr) {
  for (int i = 0; i < *size; i++) {
    ptr[i] = x[i] ^ key[i];
  }
}

void *OtpThread(void *params) {
    MakeOpt(((otpParams *) params)->x, ((otpParams *) params)->key,
            &((otpParams *) params)->size, ((otpParams *) params)->output);
  pthread_barrier_wait(((otpParams *)params)->bar);
  pthread_exit(0);
}

void *LcgThread(void *params) {
  char *ptr = MakeLcg(&((lcgParams *) params)->x0, &((lcgParams *) params)->a,
                      &((lcgParams *) params)->c, &((lcgParams *) params)->m,
                      &((lcgParams *) params)->size);
  pthread_exit(ptr);
}

int main(int argc, char *argv[]) {
  struct timeval start, end;
  gettimeofday(&start, NULL);

  char *inputFile = NULL;
  char *outputFile = NULL;

  lcgParams lcgStruct;

  int opt;
  while ((opt = getopt(argc, argv, "i:o:x:a:c:m:")) != -1) {
    switch (opt) {
        case 'i':
            inputFile = optarg;
            break;
        case 'o':
            outputFile = optarg;
            break;
        case 'x': {
            lcgStruct.x0 = atoi(optarg);
            break;
        }
        case 'a':
            lcgStruct.a = atoi(optarg);
            break;
        case 'c':
            lcgStruct.c = atoi(optarg);
            break;
        case 'm':
            lcgStruct.m = atoi(optarg);
            break;
        case '?':
            if (optopt == 'i' || optopt == 'o' || optopt == 'x' ||
                optopt == 'a' || optopt == 'c' || optopt == 'm')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            exit(EXIT_FAILURE);
        default:
            abort();
    }
  }
    StopAndCheckTime(start, end);

    FILE *fdInput;
    fdInput = fopen(inputFile, "rb");
    if (fdInput == NULL) {
        fprintf(stderr, "Error opening file.\n");
        return 1;
    }

    // Получаем размер файла, для этого перемещаем указатель на конец файла и фиксируем размер
    fseek(fdInput, 0L, SEEK_END);
    lcgStruct.size = ftell(fdInput);
    fseek(fdInput, 0L, SEEK_SET);

    char* fileData = (char *)malloc(lcgStruct.size);
    if (fileData == NULL) {
        fprintf(stderr, "Error allocating memory.\n");
        return 1;
    }

    size_t file_size = fread(fileData, sizeof(char), lcgStruct.size, fdInput);
    if (file_size != lcgStruct.size) {
        fprintf(stderr, "Error reading file.\n");
        return 1;
    }

    StopAndCheckTime(start, end);

    //Количество ядер процессора
    long threadsNum = sysconf(_SC_NPROCESSORS_ONLN);

    char *outputBuffer = (char *)malloc(lcgStruct.size + 1);
    outputBuffer[lcgStruct.size] = '\0';

    pthread_t lcgThread, ptpThreads[threadsNum];
    otpParams otpStruct[threadsNum];

    pthread_create(&lcgThread, NULL, &LcgThread, (void *)&lcgStruct);

    StopAndCheckTime(start, end);

    char *key;
    pthread_join(lcgThread, (void **)&key);

    StopAndCheckTime(start, end);

    pthread_barrier_t bar;
    pthread_barrier_init(&bar, NULL, threadsNum + 1);

    StopAndCheckTime(start, end);

    size_t blockSize = lcgStruct.size / threadsNum;
    size_t additionToLastBlock = lcgStruct.size - (threadsNum * blockSize);
    for (size_t i = 0; i < threadsNum; i++) {
        otpStruct[i].output = outputBuffer + blockSize * i;
        otpStruct[i].key = key + blockSize * i;
        otpStruct[i].size = blockSize;
        otpStruct[i].x = fileData + blockSize * i;
        otpStruct[i].bar = &bar;
        if (i == (threadsNum - 1))
            otpStruct[i].size += additionToLastBlock;
    }

    StopAndCheckTime(start, end);

    for (size_t i = 0; i < threadsNum; i++)
        pthread_create(&ptpThreads[i], NULL, &OtpThread, &otpStruct[i]);

    StopAndCheckTime(start, end);

    pthread_barrier_wait(&bar);

    StopAndCheckTime(start, end);

    fclose(fdInput);
    free(fileData);

    FILE *fdOutput;
    fdOutput = fopen(outputFile, "w");
    if (fdOutput == NULL) {
        fprintf(stderr, "File descriptor open error.\n");
        return 1;
    }

    fwrite(outputBuffer, sizeof(char), lcgStruct.size, fdOutput);

    StopAndCheckTime(start, end);

    pthread_barrier_destroy(&bar);

    fclose(fdOutput);
    free(outputBuffer);
    free(key);

    return 0;
}

