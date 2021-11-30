MKDIR_P = mkdir -p
DIR = bin
objects = handshake disconnect

all: createdir $(objects)

createdir:
	${MKDIR_P} ${DIR}

$(objects): %:%.c
	gcc -o ${DIR}/$@ $<

clean:
	rm -rf ${DIR}
