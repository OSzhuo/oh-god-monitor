TARGET=og_monitor

CC=gcc -Wall

all:$(TARGET)

og_monitor:og_stack.o og_driver.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

og_stack.o:og_stack.c og_stack.h
	$(CC) $(CFLAGS) -c $<

og_tree.o:og_tree.c og_tree.h
	$(CC) $(CFLAGS) -c $<

.PHONY:clean
clean:
	rm $(TARGET) *.o
