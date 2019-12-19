
.PHONY: all clean

BIN = osmthread
OBJ = schedule.o dispatch.o osmthread.o

CFLAGS = -Wall -Wextra -g
LDLIBS = -lrt -lpthread

all: $(BIN)

$(BIN): $(OBJ)

clean:
	$(RM) $(BIN) $(OBJ)
