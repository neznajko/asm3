TARGET = asm3
CC = g++
GDB = -ggdb
CFLAGS = $(GDB) -std=c++11

$(TARGET): $(TARGET).o
	$(CC) $(GDB) $^ -o $@ $(LDFLAGS)

$(TARGET).o: $(TARGET).C
	$(CC) $(CFLAGS) $< -c -o $@

.PHONY: clean debug
clean:
	$(RM) $(TARGET) $(TARGET).o

debug: CFLAGS += -DDEBUG
debug: $(TARGET)
