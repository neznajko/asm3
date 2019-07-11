TARGET = asm3
CC = g++
GDB = -ggdb

$(TARGET): $(TARGET).o
	$(CC) $(GDB) $^ -o $@ $(LDFLAGS)

$(TARGET).o: $(TARGET).C
	$(CC) $(GDB) $< -c -o $@

.PHONY: clean debug
clean:
	$(RM) $(TARGET) $(TARGET).o

debug: CFLAGS += -DDEBUG
debug: $(TARGET)
