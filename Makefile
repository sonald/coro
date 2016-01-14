TARGETS = libcoro.so demo
CFLAGS = -Wall -std=gnu99

all: $(TARGETS)

libcoro.so: coro.c coro.h
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^

demo: main.c libcoro.so
	$(CC) $(CFLAGS) -L./ -o $@ $^ -lcoro

test: $(TARGETS)
	LD_LIBRARY_PATH=. ./demo 
	LD_LIBRARY_PATH=. ./demo calc
	LD_LIBRARY_PATH=. ./demo calc "12 21*100-"

.PHONY: clean

clean:
	-rm $(TARGETS)
