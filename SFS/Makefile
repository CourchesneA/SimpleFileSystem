# To compile with fuse, make or make fuse both works
# To compile with test1, make test1
# To compile with test2, make test2

CC = clang -g -Wall
LDFLAGS = `pkg-config fuse --cflags --libs`
EXECUTABLE=sfs

SOURCES= disk_emu.c sfs_api.c fuse_wrappers.c
SOURCES_TEST1= disk_emu.c sfs_api.c sfs_test1.c tests.c
SOURCES_TEST2= disk_emu.c sfs_api.c sfs_test2.c tests.c
SOURCES_CUSTOM= disk_emu.c sfs_api.c tester.c

all: $(SOURCES)
	$(CC) $(LDFLAGS) -o $(EXECUTABLE) $(SOURCES)

test1: $(SOURCES_TEST1) 
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST1)

test2: $(SOURCES_TEST2)
	$(CC) -o $(EXECUTABLE) $(SOURCES_TEST2)

fuse:  $(SOURCES) $(LDFLAGS) 
	$(CC) $(LDFLAGS) -o $(EXECUTABLE)$(SOURCES)

custom: $(SOURCES_CUSTOM)
	$(CC) -o tester $(SOURCES_CUSTOM)

clean:
	rm $(EXECUTABLE)
