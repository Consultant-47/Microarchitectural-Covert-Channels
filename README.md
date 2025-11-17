# Microarchitectural Covert Channels

## Submission Details
Samar Perwez - 22B3913  

## Task 2A
### Approach
For this task we were expected to use shared memory for communication between the sender and receiver through flush and reload. Our approach was to maintain two locations in the shared memory, INDEX_ZERO and INDEX_ONE. When the sender wants to send a 0 it will flush the INDEX_ZERO location for 40us and then wait for 40us. Similarly, when the sender wants to send a 1 it will flush the INDEX_ONE location for 40us and then wait for 40us. The receiver constantly be reloading both locations and measuring the time taken to do so, this time taken is averaged over 0.01us. The time taken was measured using the rdtsc() function. 

The important thing to note is that we use two locations to send a single bit and that we wait for 40us after flushing the location. This is because it is far easier for the receiver to detect spikes at two different locations than to constantly time a single location. For example, let us consider the case wherein the sender wishes to transmit '1111'. If we use a single location, the sender will flush the location for 4T, now when the receiver tries to time the access, it becomes very difficult to differentiate between the 4T and 3T or 5T. However, in this case, the sender will send 4 'spikes' to the receiver through INDEX_ONE, which will be detected as '1111' reliably. Such an approach also necessitates the use of two locations to send a single bit.

### Results
- **Accuracy:** 100%
- **Bandwidth:** 10500bps

## Task 2B
### Approach
For this task we were expected to have no shared memory and communicate between processes using the cache occupancy. The two primitives used for this were to thrash the cache and check cache occupancy. The sender thrashes the cache by simply writing to a large array bigger than LLC. The receiver checks the cache occupancy by using pointer chasing and counts how many circular linked lists [which have been randomly initialized] it can traverse in some fixed time. The sender uses manchester encoding to send the data. It transmits a rising edge [thrash for T then do nothing for T] for 1 and a falling edge [do nothing for T then thrash for T] for 0. The receiver samples the cache occupancy every 500us and tries to detect the rising and falling edges.

To reliably detect manchester encoding, we need to see if in a given time period, the sender sent a falling edge or a rising edge. A naive approach to do this would be to keep a window, say of size 17, and look for an edge in such fixed windows throughout [0 to 16, 17 to 33, 34 to 50, ...]. However, this approach is not reliable because there is some jitter in the square wave received, so the edges will be slightly misplaced which will lead to such a fixed scheme breaking down very soon. The solution to this is to use a Delay-Locked-Loop (DLL). The DLL will keep track of the time between the last two edges and will adjust the window accordingly. Basically, we keep an early window, an in-phase window and a late window. The DLL will adjust the window such that the edges are always in the in-phase window. Such an approach ensures that the receiver can reliably detect the manchester encoding even for long transmissions.

### Results
- **Accuracy:** 100%
- **Bandwidth:** 118bps

## Task 3
### Approach
For task 3, we initially have to transmit the location of the shared memory using cache-occupancy based channel. For this part the code from task 2B can be reused, especially since the message size is small. However for transmitting the actual image and audio file, the naive approach in task 2B will not work. This is because for such longer time scales, the sender will sometimes 'skip' sending a bit, i.e. the resulting spike will not be read by the receiver, but all preceding and succeeding spikes will be correctly read. Such an event corrupts the entire bitstream as it is very difficult to detect such a skip. Also, suppose that the sender flushes the cache for 4T and the receiver is sampling every T, so it is expected that thereceiver will detect high time intervals 4 times. However, sometimes, rather than recording 4 high values, the receiver will record 1 super-high value. Furthermore, sometimes even when the sender is not flushing the cache, the receiver will record a very high value. For longer transmissions, it becomes very difficult to identify such anomalies and correct them especially since we are looking to transmit >1000000 bits reliably.

### Changes from Previous Tasks
The fundamental realization needed to solve such a problem is that we are dealing with a unreliable channel and we need to introduce redundancy to correct the errors. However, before we do that, let us first make the channel more reliable. The first step to do that is to add a CLOCK channel in addition to ONE and ZERO, now if if the receiver wants to send a 1, it will flush the ONE location and the CLOCK location for 40us and then wait for 40us. Similarly, if the receiver wants to send a 0, it will flush the ZERO location and the CLOCK location for 40us and then wait for 40us. Such a CLOCK helps us to synchronize the receiver because now the receiver knows when data is being sent and when it is not. Further, we can also reuse the concept of a DLL to remove the jitters that might be present in any of the channels.

Now that we have increased the reliability of the channel the next step is to add error-detection and packetization. The reason for this is that it is futile for us to accept that we will be able to send >140kB of data in one go reliably. The idea is to break the data into packets of size 256B and add a 16 bit CRC to each packet, thus each packet is of size 258B. The receiver will then check the CRC of each packet and if it is correct, it will send an ACK to the sender, else it will send a NACK. The sender will then retransmit the packet if it receives a NACK. This way we can ensure that the data is transmitted reliably. Initially, CRC-8 was used, but it was found that it has an error rate of 1 in 256 or 0.4%, so we switched to CRC-16 which has a much lower error rate of 1 in 65536 or 0.0015%.

### Results
#### Image Transmission
- **Sent Image:**  
![Sent Image](task3a/red_heart.jpg)
- **Received Image:**  
![Sent Image](task3a/rcvd_heart.jpg)
- **Time taken:** 30secs @ 8700bps


#### Audio Transmission
- **Sent Audio:**  
<audio controls>
  <source src="task3a/audio.mp3" type="audio/mpeg">
  Your browser does not support the audio element.
</audio>

- **Received Audio:**   
<audio controls>
  <source src="task3a/rcvd_song.mp3" type="audio/mpeg">
  Your browser does not support the audio element.
</audio>

- **Time taken:** 130.91secs @ 8900bps


## Plagiarism Checklist
Your answers should be written in a new line below each bullet.

1. Have you strictly adhered to the submission guidelines?  
Yes

2. Have you received or provided any assistance for this assignment beyond public discussions with peers/TAs?  
No

3. If you answered "yes" to the previous question, please specify what part of code you've taken and from whom you've taken it.  
N/A
