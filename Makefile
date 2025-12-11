CC = gcc
CFLAGS = -Wall -Wextra -O2

download: src/download.c
	$(CC) $(CFLAGS) -o download src/download.c

clean:
	rm -f download

.PHONY: download clean