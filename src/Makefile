TARGET=og_monitor

CC=gcc -Wall

all:$(TARGET)

og_monitor:og_ctl.o og_write.o og_init.o og_watch.o og_record.o obuf.o og_tool.o og_tree.o og_stack.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -pthread -o $@
og_init.o:og_init.c og_init.h
	$(CC) $(CFLAGS) -c $<
og_record.o:og_record.c og_record.h
	$(CC) $(CFLAGS) -c $<
og_watch.o:og_watch.c og_watch.h
	$(CC) $(CFLAGS) -c $<
og_ctl.o:og_ctl.c
	$(CC) $(CFLAGS) -c $<

obuf.o:obuf.c obuf.h
	$(CC) $(CFLAGS) -c $<

og_stack.o:og_stack.c og_stack.h
	$(CC) $(CFLAGS) -c $<

og_tree.o:og_tree.c og_tree.h
	$(CC) $(CFLAGS) -c $<

og_tool.o:og_tool.c og_tool.h
	$(CC) $(CFLAGS) -c $<

og_write:og_write.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -pthread -o $@
og_write.o:og_write.c og_write.h
	$(CC) $(CFLAGS) -c $<

.PHONY:clean
clean:
	-rm -f $(TARGET) *.o
