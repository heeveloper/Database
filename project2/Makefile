.SUFFIXES: .c .o

CC=gcc

SRCDIR=src/
TESTDIR=test/
INC=include/
LIBS=lib/

# SRCS:=$(wildcard src/*.c)
# OBJS:=$(SRCS:.c=.o)

# main source file
TARGET_SRC:=$(SRCDIR)main.c
TARGET_OBJ:=$(SRCDIR)main.o

# Include more files if you write another source file.
SRCS_FOR_LIB:=$(SRCDIR)page.c $(SRCDIR)dbpt.c 
OBJS_FOR_LIB:=$(SRCS_FOR_LIB:.c=.o)


CFLAGS+= -g -fPIC -I $(INC)

TARGET=main

all: $(TARGET)

$(TARGET): $(TARGET_OBJ)
	$(foreach SRC,$(SRCS_FOR_LIB),\
		$(CC) $(CFLAGS) -o $(SRC:.c=.o) -c $(SRC) ; \
	)
	make static_library
	$(CC) $(CFLAGS) -o $@ $^ -L $(LIBS) -lbpt

clean:
	# rm $(TARGET) $(TARGET_OBJ) $(OBJS_FOR_LIB) $(LIBS)*
	-rm -f $(TARGET_OBJ) $(OBJS_FOR_LIB) $(LIBS)* *.db main

library:
	gcc -shared -Wl,-soname,libbpt.so -o $(LIBS)libbpt.so $(OBJS_FOR_LIB)

static_library:
	ar cr $(LIBS)libbpt.a $(OBJS_FOR_LIB)


at:
	make clean
	make main
	make build_test
	make describe
	test/tests

