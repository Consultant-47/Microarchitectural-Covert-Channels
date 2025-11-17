#include <stdio.h>
#include <stdint.h>

#define MSG_FILE "msg.txt"
#define MAX_MSG_SIZE 500

#define L3_SIZE 8888608
//8388608 - 8MiB

#define INDEX_ZERO 1568
#define INDEX_ONE 14
#define INDEX_CLOCK 15842
#define LENGTH 10 
#define INDICES_LENGTH 100
#define DIFFERENCE_THRESHOLD 6

#define THRESHOLD_LOW 150
#define THRESHOLD_HIGH 250

double check_accuracy(char*, int);
uint64_t rdtsc();
void clflush(void* ptr);