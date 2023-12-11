final: mainServer.cpp subServer.cpp client.cpp
	g++ -o mainServer mainServer.cpp
	g++ -o subServer subServer.cpp
	g++ -o client client.cpp
	g++ -o prg.exe prg.cpp

mainServer:
	./mainServer

subServer:
	./subServer

client:
	./client

prg:
	./prg.exe

mainServer: mainServer.cpp
	g++ -o mainServer mainServer.cpp

subServer: subServer.cpp
	g++ -o subServer subServer.cpp

client: client.cpp
	g++ -o client client.cpp

client: prg.cpp
	g++ -o prg.exe prg.cpp

clean:
	rm mainServer
	rm subServer
	rm client
	rm prg.exe