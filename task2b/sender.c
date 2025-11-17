#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h> // For mode constants
#include <unistd.h>
#include <string.h>
#include "utils.h"

#define BUFFER_SIZE (12UL * 1024 * 1024) // 256 MB
#define CACHE_LINE_SIZE 64                // typical cache line size in bytes


int main(){

    // ********** DO NOT MODIFY THIS SECTION **********
    FILE *fp = fopen(MSG_FILE, "r");
    if(fp == NULL){
        printf("Error opening file\n");
        return 1;
    }

    char msg[MAX_MSG_SIZE];
    int msg_size = 0;
    char c;
    while((c = fgetc(fp)) != EOF){
        msg[msg_size++] = c;
    }
    fclose(fp);

    clock_t start = clock();
    // **********************************************
    // ********** YOUR CODE STARTS HERE **********
    uint8_t *buffer = (uint8_t *) malloc(BUFFER_SIZE);

    // Initialize the buffer with some data.
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = (uint8_t) (i & 0xFF);
    }

    volatile uint64_t sum = 0;
    long sq_wv;

    int bit = 0;
    for (int i = 0; i < msg_size; i++){
        for(int j = 7; j >= 0; j--){
            bit = (msg[i] >> j) & 1;
            //transmission is done using Manchester encoding
            if(bit){//rising edge
                sq_wv = rdtsc();
                while((rdtsc() - sq_wv) < 0.004 * 2e9){
                    for (size_t i = 0; i < BUFFER_SIZE; i += CACHE_LINE_SIZE) {
                        sum += buffer[i];
                    }
                }
                sq_wv = rdtsc();
                while((rdtsc() - sq_wv) < 0.004 * 2e9){
                    
                }
            }
            else{//falling edge
                sq_wv = rdtsc();
                while((rdtsc() - sq_wv) < 0.004 * 2e9){
                    
                }
                sq_wv = rdtsc();
                while((rdtsc() - sq_wv) < 0.004 * 2e9){
                    for (size_t i = 0; i < BUFFER_SIZE; i += CACHE_LINE_SIZE) {
                        sum += buffer[i];
                    }
                }
            }
        }
    }


    free(buffer);

    // ********** YOUR CODE ENDS HERE **********
    // ********** DO NOT MODIFY THIS SECTION **********
    clock_t end = clock();
    double time_taken = ((double)end - start) / CLOCKS_PER_SEC;
    printf("Message sent successfully\n");
    printf("Time taken to send the message: %f\n", time_taken);
    printf("Message size: %d\n", msg_size);
    printf("Bits per second: %f\n", msg_size * 8 / time_taken);
    // **********************************************
}
