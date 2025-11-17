#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"
#include <math.h>

#define SHM_SIZE 8388608

// Cache parameters
#define CACHE_SETS    8192   // total number of cache sets assumed
#define CACHE_WAYS    8     // number of ways (associativity) 8
#define SETS_PER_PAGE 64     // eviction buffer is organized into 64 sets per page
#define SET_SKIPPING_STEP 200  // skip sets in probing (probe every 2nd set)

// Memory allocation for the eviction array.
#define BYTES_PER_MB  (1024 * 1024)
#define EVICTION_ARRAY_SIZE (256 * BYTES_PER_MB / sizeof(uint32_t))

uint32_t *eviction_array = NULL;      // The big eviction array
unsigned int setHeads[SETS_PER_PAGE];   // Heads (starting indices) of the circular linked lists

// We will use a “shuffled” array for index ordering; its length is:
#define SHUFFLE_LEN  (CACHE_SETS / SETS_PER_PAGE)  // for 8192/64 = 128

//shuffle the array
void shuffle_array(unsigned int *array, size_t n) {
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        unsigned int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

//Build pointer-chasing lists in eviction_array
void create_set_heads() {
    unsigned int shuffle_arr[SHUFFLE_LEN];
    for (unsigned int i = 0; i < SHUFFLE_LEN; i++) {
        shuffle_arr[i] = i;
    }
    shuffle_array(shuffle_arr, SHUFFLE_LEN);

    // Compute allSetOffset = log2(CACHE_SETS) + 4.
    // We assume CACHE_SETS is a power of 2.
    unsigned int allSetOffset = (unsigned int)(log2(CACHE_SETS)) + 4; // e.g. 17

    // For each of the SETS_PER_PAGE sets, create the pointer chain.
    for (unsigned int set_index = 0; set_index < SETS_PER_PAGE; set_index++) {
        // Compute the starting index for this set.
        unsigned int current = (shuffle_arr[0] << 10) + (set_index << 4);
        setHeads[set_index] = current;
        // For each cache line ("way")
        for (unsigned int line_index = 0; line_index < CACHE_WAYS; line_index++) {
            // For each element in the list for this way
            for (unsigned int element_index = 0; element_index < SHUFFLE_LEN - 1; element_index++) {
                // Compute next index:
                unsigned int next = (line_index << allSetOffset) + (shuffle_arr[element_index + 1] << 10) + (set_index << 4);
                // Link current to next.
                eviction_array[current] = next;
                current = next;
            }
            // After the inner loop, finish the current line.
            unsigned int next;
            if (line_index == CACHE_WAYS - 1) {
                // In the last line, the last pointer goes to the head of the entire set.
                next = setHeads[set_index];
            } else {
                // Otherwise, the last pointer goes to the head of the next line.
                next = ((line_index + 1) << allSetOffset) + (shuffle_arr[0] << 10) + (set_index << 4);
            }
            eviction_array[current] = next;
            current = next;
        }
    }
}

// Probe one circular list given its head index.
void probe_set(unsigned int head) {
    unsigned int pointer = head;
    do {
        pointer = eviction_array[pointer];
    } while(pointer != head);
    // The result of this pointer chasing is discarded.
}

// Probe all sets (using a skipping step) like the JS version.
void probe_all_sets() {
    for (unsigned int set = 0; set < SETS_PER_PAGE; set += SET_SKIPPING_STEP) {
        probe_set(setHeads[set]);
    }
}

// Check edge quality
int edgeQ(const int window[]) {
    int sum1 = 0, sum2 = 0;
    // Note: Python window[3:8] covers indices 3,4,5,6,7.
    for (int i = 3; i < 8; i++) {
        sum1 += window[i];
    }
    // Python window[8:13] covers indices 8,9,10,11,12.
    for (int i = 8; i < 13; i++) {
        sum2 += window[i];
    }
    return abs(sum1 - sum2);
}

// Return bit from edge 0/1
int bit_from_edge(const int window[]) {
    int sum1 = 0, sum2 = 0;
    for (int i = 3; i < 8; i++) {
        sum1 += window[i];
    }
    for (int i = 8; i < 13; i++) {
        sum2 += window[i];
    }
    return (sum1 - sum2) > 0 ? 0 : 1;
}

// Find the edge position
int edgePos(const int window[]) {
    int max_delta = 0;
    int max_delta_pos = 0;
    // Loop from i=4 to 12 (Python: range(4, 13))
    for (int i = 4; i < 13; i++) {
        int delta = abs(window[i-2] + window[i-1] - window[i] - window[i+1]);
        if (delta > max_delta) {
            max_delta = delta;
            max_delta_pos = i;
        }
    }
    return max_delta_pos;
}

// Perform the measurement using rdtsc. for 25 seconds
// Decipher the message
// This block basically uses a Delay Locked Loop to keep track
// of the bits and then deciphers the message
// The DLL works by using Early/In-Phase and Late windows to
// keep the edge position in the center of the window
int __attribute__((optimize("O0"))) perform_measurement_rdtsc(char* received_msg) {
    unsigned int sample = 0;
    
    // Let’s set a reference for the first period.
    long period_start = rdtsc();
    long period_end = period_start + (500* 2e9)/1000000 ;//every 500us

    // Variables for the message deciphering
    int total_bytes = 0;
    int latch = 0;
    int no_added = 0;
    char ch = 0;
    int bits_done = 0;
    int curr_len = 0;   
    int curr_window[17];

    while(rdtsc() < period_end + + (50000* 2e9)/1000000) {
        probe_all_sets();
    }
    period_start = rdtsc();
    period_end = period_start + (500* 2e9)/1000000 ;//every 500us
    while (1) {
        unsigned int sweeps = 0;
        // Count how many sweeps we can complete in the SAMPLING_PERIOD_MS window.
        while (rdtsc() < period_end) {
            probe_all_sets();
            sweeps++;
        }
        //results[sample] = sweeps;
        //Start deciphering message

        // Initialize the current window with the first sample.
        if(sample==0) {
            for (int i = 0; i < 17; i++) {
                curr_window[i] = sweeps;
            }
        }

        // Save the current window as early_window.
        int early_window[17];
        memcpy(early_window, curr_window, sizeof(curr_window));

        // Shift curr_window left by one element and append sweeps
        for (int i = 0; i < 17 - 1; i++) {
            curr_window[i] = curr_window[i+1];
        }
        curr_window[17 - 1] = sweeps;
        no_added++;

        // If latch is active and we haven’t added a full window, skip the rest.
        if (latch && no_added != 17) {
            sample++;
            period_start = period_end;
            period_end = period_start + (500* 2e9)/1000000;
            continue;
        } else if (!latch && edgeQ(curr_window) > 5*DIFFERENCE_THRESHOLD) {
            // When latch is not set and the edge quality exceeds threshold:
            ch = (ch << 1) | 0;
            bits_done++;
            latch = 1;
            no_added = 0;
            sample++;
            period_start = period_end;
            period_end = period_start + (500* 2e9)/1000000;
            continue;
        } else if (!latch) {
            sample++;
            period_start = period_end;
            period_end = period_start + (500* 2e9)/1000000;
            continue;
        }

        // Check if the range in curr_window is very small (i.e. signal flat); if so, break.
        int max_val = curr_window[0], min_val = curr_window[0];
        for (int i = 1; i < 17; i++) {
            if (curr_window[i] > max_val) max_val = curr_window[i];
            if (curr_window[i] < min_val) min_val = curr_window[i];
        }
        if (max_val - min_val <= 3) {
            break;
        }

        // Compute edge position.
        int edgepos = edgePos(curr_window);

        // Decide which window (early, curr, or late) to use based on edge position.
        if (edgepos == 8) {
            ch = (ch << 1) | bit_from_edge(curr_window);
            bits_done++;
            no_added = 0;
        } else if (edgepos > 8) {
            no_added -= 1;
            sample++;
            period_start = period_end;
            period_end = period_start + (500* 2e9)/1000000;
            continue;
        } else if (edgepos < 8) {
            ch = (ch << 1) | bit_from_edge(early_window);
            bits_done++;
            no_added = 1;
        }

        // 8 bits received
        if(bits_done == 8){
            received_msg[curr_len++] = ch;
            bits_done = 0;
            ch = 0;
            total_bytes++;
        }
        //End deciphering message
        sample++;

        // Update the period: shift the window by SAMPLING_PERIOD_MS.
        period_start = period_end;
        period_end = period_start + (500* 2e9)/1000000;
    }
    return sample;
}

// Check edge quality
int edgeQ_FR(const int window[]) {
    int copy[8];
    for (int i = 0; i < 8; i++) {
        if(i>=1 && i<=6 && window[i] > 400 && window[i-1] < 150 && window[i+1] < 150){
            copy[i] = (window[i-1] + window[i+1])/2;
        }
        else {
            copy[i] = window[i];
        }
    }
    return abs(copy[3] + copy[4] - copy[1] - copy[6]);
}

// Return bit from edge 0/1
int bit_from_edge_FR(const int zero[], const int one[]) {
    int sum1 = 0, sum2 = 0;
    for (int i = 2; i < 6; i++) {
        sum1 += zero[i];
    }
    for (int i = 2; i < 6; i++) {
        sum2 += one[i];
    }
    return (sum1 - sum2) > 0 ? 0 : 1;
}

// Find the edge position
int edgePos_FR(const int window[]) {
    int sum_in = 0, sum_early = 0, sum_late = 0;
    for (int i = 2; i < 6; i++) {
        sum_in += window[i];
    }
    for (int i = 1; i < 5; i++) {
        sum_early += window[i];
    }
    for (int i = 3; i < 7; i++) {
        sum_late += window[i];
    }
    if(sum_in > sum_early && sum_in > sum_late){
        return 0;
    }
    else if(sum_early > sum_in && sum_early > sum_late){
        return 1;
    }
    else{
        return -1;
    }
}

int write_to_file_flush_reload(int* shared_array, int* CLK, int* ONE, int* ZERO, char* received_msg) {
    // printf("Listening...\n");
    volatile int read = 0;
    long t_1, t_2, delta; // Variables to store the time-stamp counter values
    double average_1 = 0.0; // Variable to store the average time-stamp counter difference
    double average_0 = 0.0; // Variable to store the average time-stamp counter difference
    double average_clk = 0.0; // Variable to store the average time-stamp counter difference
    long ctr = 0; // Variable to store the number of iterations in the inner loop

    clock_t start2;

    int global_ctr = 0;
    clock_t start = clock();

    // Variables for the message deciphering
    int total_bytes = 0;
    int latch = 0;
    int no_added = 0;
    char ch = 0;
    int bits_done = 0;
    int curr_len = 0;   
    int curr_window[8];
    int curr_one[8];
    int curr_zero[8];

    //Start receiving the message till we receive 500 empty windows
    while(((double)clock() - start) / CLOCKS_PER_SEC < 3)
    {
        ctr = 0;
        start2 = clock();
        while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00001){
            ctr++;

            t_1 = rdtsc();
            read = shared_array[INDEX_CLOCK];
            t_2 = rdtsc();
            delta=t_2 - t_1;
            average_clk += (delta-average_clk)/ctr; // Calculate the average time-stamp counter difference for CLK

            t_1 = rdtsc();
            read = shared_array[INDEX_ONE];
            t_2 = rdtsc();
            delta=t_2 - t_1;
            average_1 += (delta-average_1)/ctr; // Calculate the average time-stamp counter difference for ONE

            t_1 = rdtsc();
            read = shared_array[INDEX_ZERO];
            t_2 = rdtsc();
            delta=t_2 - t_1;
            average_0 += (delta-average_0)/ctr; // Calculate the average time-stamp counter difference for ZERO
        }

        CLK[global_ctr] = average_clk;
        ONE[global_ctr] = average_1;
        ZERO[global_ctr] = average_0;
        //Begin the decoding process

        // Initialize the current window with the first sample.
        if(global_ctr==0) {
            for (int i = 0; i < 8; i++) {
                curr_window[i] = 100;
                curr_one[i] = 100;
                curr_zero[i] = 100;
            }
        }

        // Save the current window as early_window.
        int early_window[8];
        int early_one[8];
        int early_zero[8];
        memcpy(early_window, curr_window, sizeof(curr_window));
        memcpy(early_one, curr_one, sizeof(curr_one));
        memcpy(early_zero, curr_zero, sizeof(curr_zero));

        // Shift curr_window left by one element and append sweeps
        for (int i = 0; i < 8 - 1; i++) {
            curr_window[i] = curr_window[i+1];
            curr_one[i] = curr_one[i+1];
            curr_zero[i] = curr_zero[i+1];
        }
        curr_window[8 - 1] = average_clk;
        curr_one[8 - 1] = average_1;
        curr_zero[8 - 1] = average_0;
        no_added++;

        // If latch is active and we haven’t added a full window, skip the rest.
        if (latch && no_added != 8) {
            global_ctr++;
            continue;
        } else if (!latch && edgeQ_FR(curr_window) > 350) {
            // When latch is not set and the edge quality exceeds threshold:
            ch = (ch << 1) | bit_from_edge_FR(curr_zero, curr_one);
            bits_done++;
            latch = 1;
            no_added = 0;
            global_ctr++;
            continue;
        } else if (!latch) {
            global_ctr++;
            continue;
        }

        

        // Compute edge position.
        int edgepos = edgePos_FR(curr_window);

        // Decide which window (early, curr, or late) to use based on edge position.
        if (edgepos == 0) {
            ch = (ch << 1) | bit_from_edge_FR(curr_zero, curr_one);
            bits_done++;
            no_added = 0;
        } else if (edgepos == -1) {
            no_added -= 1;
            global_ctr++;
            continue;
        } else {
            ch = (ch << 1) | bit_from_edge_FR(early_zero, early_one);
            bits_done++;
            no_added = 1;
        }

        // 8 bits received
        if(bits_done == 8){
            received_msg[curr_len++] = ch;
            bits_done = 0;
            ch = 0;
            total_bytes++;
        }

        if (total_bytes == 258) {
            break;
        }
        //End deciphering message
        global_ctr++;
        
        
    }
    // printf("Bytes written: %d\n", bytes_written);
    return global_ctr;
}

int verify_crc8(const char data[257]) {
    uint8_t crc = 0x00;  // initial CRC value
    for (int i = 0; i < 256; i++) {
        crc ^= (uint8_t)data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ 0x07);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return (crc == (uint8_t)data[256]);
}

int verify_crc16(const char *data) {
    uint16_t crc = 0xFFFF;  // Initial value for CRC-16-CCITT

    // Compute CRC over the first 256 bytes
    for (int i = 0; i < 256; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
            crc &= 0xFFFF;  // Ensure CRC remains 16-bit
        }
    }

    // Retrieve the stored CRC from index 256 and 257 (big-endian order)
    uint16_t stored_crc = (((unsigned char)data[256]) << 8) | ((unsigned char)data[257]);

    // Return 1 if the CRCs match, 0 otherwise.
    return (crc == stored_crc);
}

void send_status(int* shared_array, int status){
    int index_to_flush = status == 1 ? INDEX_ONE : INDEX_ZERO;
    // Send 1s to signal the start of the message
    // printf("Sending status\n");
    int read = 0;
    clock_t start = clock();
    while(((double)clock() - start) / CLOCKS_PER_SEC < 0.0002)
    {
        clflush((void *) (shared_array + index_to_flush));
        read++;
    }
    // printf("Status sent %d\n", read);
}

int __attribute__((optimize("O0")))main(){

    // Update these values accordingly
    char* mmap_addr = NULL;
    
    // Allocate memory for received message
    mmap_addr = (char *)malloc(MAX_MSG_SIZE * sizeof(char));
    
    srand((unsigned int) time(NULL));  // Seed the random number generator

    // Allocate the eviction array.
    eviction_array = (uint32_t*) malloc(EVICTION_ARRAY_SIZE * sizeof(uint32_t));
    if (eviction_array == NULL) {
        perror("malloc");
        return 1;
    }

    // Initialize the eviction array to 0 (not really needed)
    for (unsigned int i = 0; i < EVICTION_ARRAY_SIZE; i++) {
        eviction_array[i] = 0;
    }

    // Build the pointer–chasing lists.
    create_set_heads();

    // Do a few probes as a sanity check.
    for (unsigned int i = 0; i < 500; i++) {
        probe_all_sets();
    }

    fflush(stdout);

    // Perform the measurement.
    perform_measurement_rdtsc(mmap_addr);

    //print the received message
    printf("Received message: %s\n", mmap_addr);

    free(eviction_array);

    int shm_fd = shm_open(mmap_addr, O_RDWR, 0666);
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

    int *CLK = (int *)malloc(300000 * sizeof(int));
    int *ONE = (int *)malloc(300000 * sizeof(int));
    int *ZERO = (int *)malloc(300000 * sizeof(int));
    // char *received_msg = (char *)malloc(257 * sizeof(char));

    char *received_msg = (char *)malloc(1228000 * sizeof(char));
    char *start_msg = received_msg;

    int status = 0, blocks = 0, pad = 0, total_recvd = 0;
    while(!status){
        write_to_file_flush_reload(shared_array, CLK, ONE, ZERO, received_msg);
        status = verify_crc16(received_msg);
        send_status(shared_array, status);
        total_recvd += 1;
    }
    blocks = (int)(received_msg[255] + (received_msg[254] << 8));
    pad = (int)received_msg[253];
    for(int i = 0; i < blocks; i++){
        write_to_file_flush_reload(shared_array, CLK, ONE, ZERO, received_msg);
        status = verify_crc16(received_msg);
        send_status(shared_array, status);
        if(!status){
            i-=1;
        }
        else{
            received_msg+=256;
        }
        total_recvd += 1;
    }
    FILE *f = fopen("rcvd_heart.jpg", "w");
    if (f == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }
    for(int i = 0; i < blocks*256 - (int)((uint8_t)pad); i++){
        fprintf(f, "%c", start_msg[i]);
    }
    fclose(f);
    printf("Header information - Blocks: %d, Padding in last Block: %d\n", blocks, (int)((uint8_t)pad));
    printf("Total packets received %d with efficiency %f\n", total_recvd, (float)blocks/total_recvd);

    // for(int i = 0; i < blocks; i++){
    //     printf("CRC %d: %x\n", i, crcs[i]);
    // }

    // int N = 100;
    // while(N){
    //     int max = write_to_file_flush_reload(shared_array, CLK, ONE, ZERO, received_msg);
    //     int status = verify_crc8(received_msg);
    //     send_status(shared_array, status);
    //     if(status){
    //         N--;
    //     }
    // }
    // printf("Success!\n");


    // int success[500] = {0};
    // for(int i = 0; i < 500; i++){
    //     int max = write_to_file_flush_reload(shared_array, CLK, ONE, ZERO, received_msg);
    //     int status = verify_crc8(received_msg);
    //     success[i] = status;
    //     send_status(shared_array, status);
    // }
    // int s = 0;
    // for(int i = 0; i < 500; i++){
    //     // printf("Verification %d: %d\n", i, success[i]);
    //     s += success[i];
    // }
    // printf("Success rate: %d\n", s);

    

    // FILE *f = fopen("out.txt", "w");
    // if (f == NULL)
    // {
    //     printf("Error opening file!\n");
    //     exit(1);
    // }
    // for(int i = 0; i < max; i++){
    //     fprintf(f, "%d %d %d\n", CLK[i], ONE[i], ZERO[i]);
    // }
    // fclose(f);
}