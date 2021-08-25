
TARGET=sprinter
TARGETOLD=sprinter-old
OBJS=sprinter.o
OBJSOLD=sprinter-old.o
LIBS=-lcurl
CFLAGS=-O3

all: $(TARGET) $(TARGETOLD)

$(TARGET): $(OBJS)
	$(CC) -o $@ $< $(LIBS)

$(TARGETOLD): $(OBJSOLD)
	$(CC) -o $@ $< $(LIBS)

sprinter.o: sprinter.c
	$(CC) $(CFLAGS) -c $<

sprinter-old.o: sprinter.c
	$(CC) -DFOR_OLDER=1 $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(TARGET) $(OBJS) $(TARGETOLD) $(OBJSOLD)
