#include <stdio.h>
#include <stdint.h>

#define MSG_FILE "msg.txt"
#define MAX_MSG_SIZE 500

#define SHM_NAME "/my_shared_memory"
#define SHM_SIZE 8388608

#define INDEX 1568
#define INDEX2 14

#define THRESHOLD_LOW 150
#define THRESHOLD_HIGH 250
//262144   256kiB
//8388608  8MiB
double check_accuracy(char*, int);
uint64_t rdtsc();