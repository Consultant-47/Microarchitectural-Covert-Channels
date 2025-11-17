CC = gcc
CFLAGS = -Wall

# Targets
all: sender receiver

# Build sender executable
sender: sender.o utils.o
	$(CC) $(CFLAGS) -fno-prefetch-loop-arrays -o sender sender.o utils.o

# Build receiver executable
receiver: receiver.o utils.o
	$(CC) $(CFLAGS) -fno-prefetch-loop-arrays -o receiver receiver.o utils.o

# Compile utils.c to utils.o
utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -fno-prefetch-loop-arrays -c utils.c -o utils.o

# Compile sender.c to sender.o
sender.o: sender.c utils.h
	$(CC) $(CFLAGS) -fno-prefetch-loop-arrays -c sender.c -o sender.o

# Compile receiver.c to receiver.o
receiver.o: receiver.c utils.h
	$(CC) $(CFLAGS) -fno-prefetch-loop-arrays -c receiver.c -o receiver.o

# Clean up build artifacts
clean:
	rm -f sender receiver receiver2 *.o