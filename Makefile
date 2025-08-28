LIBS = -lncurses

all: taghost tagclient

taghost: taghost.c
	$(CC) taghost.c -o taghost $(LIBS)

tagclient: tagclient.c
	$(CC) tagclient.c -o tagclient $(LIBS)
