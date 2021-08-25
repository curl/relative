
TARGET=sprinter
OBJS=sprinter.o
LIBS=-lcurl
CFLAGS=-O3

sprinter: sprinter.o
	$(CC) -o $@ $< $(LIBS)

sprinter.o: sprinter.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS)
