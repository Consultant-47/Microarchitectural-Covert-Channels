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
#define SHM_SIZE 8388608

void send_flush_reload(int* shared_array, unsigned char* msg, int msg_size, int* sent){
    long global_ctr = 0;
    // Send 1s to signal the start of the message
    clock_t start2;

    // Send the message
    int bit = 0;
    for (int i = 0; i < msg_size; i++) {
        for(int j = 7; j >= 0; j--){
            bit = (msg[i] >> j) & 1;
            if(bit){
                //Bit is 1 -> flush INDEX then dont do anything, i.e. receiver will read spike at INDEX
                sent[global_ctr++] = 1;
                start2 = clock();
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){//4
                    clflush((void *) (shared_array + INDEX_CLOCK));
                    clflush((void *) (shared_array + INDEX_ONE));
                }
                start2 = clock();
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){//4,6
                    
                }
            }
            else{
                sent[global_ctr++] = 0;
                start2 = clock();
                //Bit is 0 -> flush INDEX2 then dont do anything, i.e. receiver will read spike at INDEX2
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){//4
                    clflush((void *) (shared_array + INDEX_CLOCK));
                    clflush((void *) (shared_array + INDEX_ZERO));
                }
                start2 = clock();
                while(((double)clock() - start2) / CLOCKS_PER_SEC < 0.00004){//4,6
                    
                }
            }
            start2 = clock();
        }
    }
}

char* add_crc(const char *data) {
    // Allocate memory for 257 bytes.
    char *result = (char *)malloc(257 * sizeof(char));
    if (result == NULL) {
        return NULL;  // Memory allocation failed.
    }

    // Copy the first 256 bytes from the input data.
    memcpy(result, data, 256);

    // Compute the CRC8 for the first 256 bytes.
    uint8_t crc = 0x00;  // initial value
    for (int i = 0; i < 256; i++) {
        crc ^= (uint8_t)result[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ 0x07);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }

    // Set the 257th byte to the computed CRC.
    result[256] = (char)crc;
    return result;
}

void send_occupancy(uint8_t* buffer, char* shm_name, int len){
    // Initialize the buffer with some data.
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = (uint8_t) (i & 0xFF);
    }

    volatile uint64_t sum = 0;
    long sq_wv;

    int bit = 0;
    for (int i = 0; i < len; i++){
        for(int j = 7; j >= 0; j--){
            bit = (shm_name[i] >> j) & 1;
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
}

int get_ack(int* shared_array){
    // printf("Listening for ack...\n");
    volatile int read = 0;
    long t_1, t_2, delta; // Variables to store the time-stamp counter values
    double average_1 = 0.0; // Variable to store the average time-stamp counter difference
    double average_0 = 0.0; // Variable to store the average time-stamp counter difference
    double average_clk = 0.0; // Variable to store the average time-stamp counter difference
    long ctr = 0; // Variable to store the number of iterations in the inner loop

    clock_t start2;

    clock_t start = clock();

    int zero_ctr = 0;
    int one_ctr = 0;

    //Start receiving the message till we receive 500 empty windows
    while(((double)clock() - start) / CLOCKS_PER_SEC < 0.0008)
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

        if(average_1 - average_0 > 80){
            one_ctr++;
        }
        else if(average_0 - average_1 > 80){
            zero_ctr++;
        }
    }
    if(one_ctr > 10){
        return 1;
    }
    else{
        return 0;
    }
}

unsigned char crc8(const unsigned char *data, int len) {
    unsigned char crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];  // XOR with the current byte
        for (int j = 0; j < 8; j++) {  // Process 8 bits
            if (crc & 0x80)
                crc = (unsigned char)(((crc << 1) ^ 0x07) & 0xFF);
            else
                crc = (unsigned char)((crc << 1) & 0xFF);
        }
    }
    return crc;
}

void compute_and_append_crc16(unsigned char *data) {
    uint16_t crc = 0xFFFF;  // Initial value for CRC-16-CCITT

    // Process the first 256 bytes
    for (int i = 0; i < 256; i++) {
        // Cast to unsigned char to avoid sign extension issues
        crc ^= ((uint16_t)(unsigned char)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
            crc &= 0xFFFF;
        }
    }

    // Store the computed CRC in big-endian order:
    // - data[256] gets the high byte
    // - data[257] gets the low byte
    data[256] = (unsigned char)((crc >> 8) & 0xFF);
    data[257] = (unsigned char)(crc & 0xFF);
}

int verify_crc16(const unsigned char *data) {
    uint16_t crc = 0xFFFF;  // Initial value for CRC-16-CCITT

    // Process the first 256 bytes
    for (int i = 0; i < 256; i++) {
        // Cast to unsigned char to avoid sign extension issues
        crc ^= ((uint16_t)(unsigned char)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
            crc &= 0xFFFF;
        }
    }

    // Retrieve the stored CRC from index 256 and 257 (big-endian order)
    uint16_t stored_crc = (((unsigned char)data[256]) << 8) | ((unsigned char)data[257]);

    // Return 1 if the CRCs match, 0 otherwise.
    return (crc == stored_crc);
}

int process_file(const char *filename, unsigned char blocks[][258]) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening file");
        return -1;
    }
    
    // Determine the file size.
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);
    
    // Calculate how many 256-byte blocks are needed.
    int full_blocks = file_size / 256;
    int remainder    = file_size % 256;
    int file_blocks  = full_blocks + (remainder ? 1 : 0);
    int pad          = (remainder > 0) ? (256 - remainder) : 0;
    
    // --- Build the header block in blocks[0] ---
    // (Here we simply zero the header data; modify as needed.)
    memset(blocks[0], 0, 256);
    
    // Set the third-last byte (index 254) to the number of pad bytes.
    blocks[0][253] = (unsigned char)pad;
    
    // Set the second-last byte (index 255) to the number of file blocks.
    blocks[0][255] = (unsigned char)file_blocks;

    blocks[0][254] = (unsigned char)(file_blocks >> 8);
    
    // Compute the CRC8 over the header (first 256 bytes) and store it in the last byte.
    compute_and_append_crc16(blocks[0]);  
    // blocks[0][256] = crc8(blocks[0], 256);
    
    // --- Read the file in 256-byte blocks ---
    for (int i = 0; i < file_blocks; i++) {
        unsigned char *block = blocks[i + 1];  // file blocks start at row 1

        size_t bytes_read = fread(block, 1, 256, fp);;
        
        // If we read less than 256 bytes, pad the remainder with zeros.
        if (bytes_read < 256) {
            memset(block + bytes_read, 0, 256 - bytes_read);
        }
        
        // Compute and store the CRC8 in the extra (257th) byte.
        // block[256] = crc8(block, 256);
        compute_and_append_crc16(block);
    }
    
    fclose(fp); 
    
    // Return total number of blocks (header block + file blocks)
    return file_blocks + 1;
}

int main(){

    clock_t start = clock();

    //Name of Shared Memory Object
    char shm_name[] = "/this_is_raj";

    // Open shared memory
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    // Set the size of shared memory
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
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

    // buffer to thrash LLC
    uint8_t *buffer = (uint8_t *) malloc(BUFFER_SIZE);

    // Send the message
    start = clock();
    send_occupancy(buffer, shm_name, sizeof(shm_name)/sizeof(char));
    printf("mmap address of size %ld sent successfully in %.2f seconds at rate %.2fbps\n", sizeof(shm_name)/sizeof(char), ((double)clock() - start) / CLOCKS_PER_SEC, 8*sizeof(shm_name)/sizeof(char)/(((double)clock() - start) / CLOCKS_PER_SEC));
    
    // Sleep for 50ms to let the receiver read the message
    usleep(50000);

    unsigned char blocks[4800][258] = {0};
    int total_blocks = process_file("red_heart.jpg", (unsigned char (*)[258])blocks);

    int total_sent = 0;
    start = clock();
    int packet = 0, status = 0;
    int *sent = (int *)malloc(1500000 * sizeof(int));
    while(packet<total_blocks){
        send_flush_reload(shared_array, (unsigned char *)blocks[packet], 258, sent);
        // printf("Packet %d\n", packet);
        // printf("Data[256]: %x\n", (unsigned char)blocks[packet][256]);
        // printf("Data[257]: %x\n", (unsigned char)blocks[packet][257]);
        status = get_ack(shared_array);
        if(status) {
            // printf("CRC %d: %x\n", packet, blocks[packet][256]);
            packet++;
        }
        total_sent++;
    }

    printf("File with %d bytes sent successfully in %.2f seconds at rate %.2fbps\n", total_blocks*256, ((double)clock() - start) / CLOCKS_PER_SEC, 8*total_blocks*256/(((double)clock() - start) / CLOCKS_PER_SEC));
    printf("Total packets sent: %d\n", total_sent);
    

    // {
    //     FILE *dump_file = fopen("blocks_dump.txt", "w");
    //     if (!dump_file) {
    //         perror("fopen");
    //         exit(1);
    //     }
    //     for (int i = 0; i < total_blocks; i++) {
    //         for (int j = 0; j < 257; j++) {
    //             fprintf(dump_file, "%02x ", ((unsigned char (*)[257])blocks)[i][j]);
    //         }
    //         fprintf(dump_file, "\n");
    //     }
    //     fclose(dump_file);
    // }

    // int fd = open("tiny.txt", O_RDONLY);
    // int len;
    // char* arr = add_crc(read_from_file(fd, &len));
    // len++;

    // start = clock();
    // int *sent = (int *)malloc(1500000 * sizeof(int));

    // int sent_s = 0;
    // while(sent_s < 100){
    //     send_flush_reload(shared_array, arr, len, sent);
    //     int received = get_ack(shared_array);
    //     if(received){
    //         sent_s++;
    //     }
    // }
    // printf("Success!\n");

    //Open file descriptor for red_heart.jpg
    // int fd = open("tiny.txt", O_RDONLY);
    // int len;
    // char* arr = add_crc(read_from_file(fd, &len));
    // len++;

    // start = clock();
    // int *sent = (int *)malloc(1500000 * sizeof(int));

    // int recvd[500] = {0};
    // for(int i = 0 ; i < 500; i++){
    //     send_flush_reload(shared_array, arr, len, sent);
    //     int received = get_ack(shared_array);
    //     recvd[i] = received;
    // }
    // int success = 0;
    // for(int i = 0; i < 500; i++){
    //     // printf("Received correctly: %d\n", recvd[i]);
    //     success += recvd[i];
    // }
    // printf("Success rate: %d\n", success);

    // printf("Random text with %d bytes sent successfully in %.2f seconds at rate %.2fbps\n", len, ((double)clock() - start) / CLOCKS_PER_SEC, 8*len/(((double)clock() - start) / CLOCKS_PER_SEC));
    // printf("Received correctly: %hhu\n", received);

    // FILE *f = fopen("sent.txt", "w");
    // if (f == NULL)
    // {
    //     printf("Error opening file!\n");
    //     exit(1);
    // }
    // for(int i = 0; i < 1500000; i++){
    //     fprintf(f, "%d\n", sent[i]);
    // }

    // free(arr);
    free(buffer);
}
