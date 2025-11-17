#include "utils.h"

// DO NOT MODIFY THIS FUNCTION
double check_accuracy(char* received_msg, int received_msg_size){
    FILE *fp = fopen(MSG_FILE, "r");
    if(fp == NULL){
        printf("Error opening file\n");
        return 1;
    }

    char original_msg[MAX_MSG_SIZE];
    int original_msg_size = 0;
    char c;
    while((c = fgetc(fp)) != EOF){
        original_msg[original_msg_size++] = c;
    }
    fclose(fp);

    int min_size = received_msg_size < original_msg_size ? received_msg_size : original_msg_size;

    int error_count = (original_msg_size - min_size) * 8;
    for(int i = 0; i < min_size; i++){
        char xor_result = received_msg[i] ^ original_msg[i];
        for(int j = 0; j < 8; j++){
            if((xor_result >> j) & 1){
                error_count++;
            }
        }
    }

    return 1-(double)error_count / (original_msg_size * 8);
}

uint64_t rdtsc() {
    uint64_t a, d;

    // Ensure memory operations are completed before reading the TSC
    asm volatile ("mfence");

    // The rdtsc instruction loads the low 32 bits into the 'a' register and 
    // the high 32 bits into the 'd' register.
    asm volatile ("rdtsc" : "=a" (a), "=d" (d));

    // Combine the high and low 32-bit values into a single 64-bit value
    a = (d << 32) | a;

    // Ensure memory operations are completed after reading the TSC
    asm volatile ("mfence");

    return a; // Return the 64-bit value containing the time-stamp counter
}