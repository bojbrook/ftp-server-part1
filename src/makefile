all: client server

client:
	mkdir -p ../bin
	gcc -o ../bin/myclient myclient.c -lpthread
server: 
	mkdir -p ../bin
	gcc myserver.c -o ../bin/myserver -lpthread
clean:
	rm -f ../bin/myserver ../bin/myclient