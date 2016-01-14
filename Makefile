TARGETS = $(patsubst %.c, %, $(wildcard *.c))

all: $(TARGETS)

%: %.c
	$(CC) -Wall -std=gnu99 -o $@ $^

test: $(TARGETS)
	./coro_context 
	./coro_context calc
	./coro_context calc "12 21*100-"

.PHONY: clean

clean:
	-rm $(TARGETS)
