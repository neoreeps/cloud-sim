binaries=csim

CC=g++
# Needed for linux to get rid of 'Value too large for defined data type' error.
CPPFLAGS += -D_FILE_OFFSET_BITS=64
# Not always needed
CPPFLAGS += -lrt

all: $(binaries)

debug:
	$(CC) $(CPPFLAGS) -DDEBUG csim.cc -o csim

clean:
	rm $(binaries)

