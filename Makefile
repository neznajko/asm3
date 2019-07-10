TARGET = asm3
CC = g++
DBFLAG = -ggdb
CFLAGS = $(DBFLAG) -m32
LDFLAGS = -march=i386	

$(TARGET): $(TARGET).o
	$(CC) $(DBFLAG) $^ -o $@ $(LDFLAGS)

$(TARGET).o: $(TARGET).C
	$(CC) $(CFLAGS) $< -c -o $@

.PHONY: clean debug
clean:
	$(RM) $(TARGET) $(TARGET).o

debug: CFLAGS += -DDEBUG
debug: $(TARGET)
