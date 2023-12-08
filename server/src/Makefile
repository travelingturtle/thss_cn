CC = gcc
CFLAGS = -Wall -g


SRCS = FTPServer.c main.c
TARGET = server


OBJS = $(SRCS:.c=.o)


$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(OBJS) $(TARGET)