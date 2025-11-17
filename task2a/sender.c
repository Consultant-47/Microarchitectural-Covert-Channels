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


//Assembly code for clflush
void clflush(void* ptr) {
    __asm__ volatile("clflush (%0)" : : "r"(ptr) : "memory");
}


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

    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    // Map shared memory
    void *shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // treat shm_ptr as an array of integers
    int* shared_array = (int *)shm_ptr;

    // temporary variable to read from shared memory
    int read = 0;

    // Send 1s to signal the start of the message
    clock_t start2;
    while(((double)clock() - start) / CLOCKS_PER_SEC < 0.02)
    {
        start2 = clock();
        while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){
            clflush((void *) (shared_array + INDEX));
        }
        start2 = clock();
        while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){
            
        }
    }

    // Send the message
    int bit = 0;
    for (int i = 0; i < msg_size; i++) {
        for(int j = 7; j >= 0; j--){
            bit = (msg[i] >> j) & 1;
            if(bit){
                //Bit is 1 -> flush INDEX then dont do anything, i.e. receiver will read spike at INDEX
                start2 = clock();
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){
                    clflush((void *) (shared_array + INDEX));
                }
                start2 = clock();
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){
                    
                }
            }
            else{
                start2 = clock();
                //Bit is 0 -> flush INDEX2 then dont do anything, i.e. receiver will read spike at INDEX2
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){
                    clflush((void *) (shared_array + INDEX2));
                }
                start2 = clock();
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){
                    
                }
            }
            start2 = clock();
        }
    }

    // Clean up shared memory
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    shm_unlink(SHM_NAME);

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
