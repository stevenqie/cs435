CC=g++
CC_OPTS=
CC_LIBS=
CC_DEFINES=
CC_INCLUDES=
CC_ARGS=${CC_OPTS} ${CC_LIBS} ${CC_DEFINES} ${CC_INCLUDES}

# clean is not a file
.PHONY=clean

#target "all" depends on all others
all: client

client: client.cpp
	@${CC} ${CC_ARGS} -o client client.cpp

clean:
	@rm -f server client talker listener *.o
