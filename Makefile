MKDIR_P = mkdir -p
DIR = bin
objects = handshake disconnect sendmessage fastretransmit server tlp

all: createdir $(objects)

createdir:
	${MKDIR_P} ${DIR}

$(objects): %:%.c
	gcc -o ${DIR}/$@ $<

clean:
	rm -rf ${DIR}
