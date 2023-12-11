#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <math.h>
#include <algorithm>
#include <map>
#include <fstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>

using namespace std;

// Globals ------------------------------------------------------------------------------------------
int basePort = 5000;
const int serversNum = 4;
const int packSize = 1024;
const int timeout = 50;

// Keep Alive data
const int keepAliveSecs = 2;
string keepAliveData = "KAre You Alive?";
string subServerWave = "WI'm a SubServer";


// AUX Functions ------------------------------------------------------------------------------------

// Funcion que convierte una string a un numero
int checksum(const string input) {
   int checksum = 0;
   for (int i = 0; i < input.size(); ++i) checksum += (int) input[i];
   return checksum;
}

// Funcion que convierte un numero a una string completando 0's
string to_string_parse(int num, int size = 2){
	string s = to_string(num);
	while(s.size() < size) s = "0" + s;
	return s;	
}

// Funcion que valida el checksum de un mensaje (Confirma si esta correcto)
bool validateChecksum(string msg){        
    int check = stoi(msg.substr(msg.size() - 10, 10)); // 10 ultimos caracteres son checksum
    return check == checksum(msg.substr(0, msg.size()-10));
}



// SEND ---------------------------------------------------------------------------------------------

// Envia paquetes individuales
bool sendPackageRDT3(struct sockaddr_in &objSocket, socklen_t &addrLen, string msg, int sock, int &seqNumb){

    char send_data[packSize];
    char ack_data[packSize];
    string data_str = "";

    int bytes_read;
    int trys = 2;
    bool messageSent = 0; // Resultado de la funcion
    bool flag; // Indica si llego ACK o no

    // Add SeqNumber y CheckSum
    msg = msg + to_string_parse(seqNumb++, 10);
    msg = msg + to_string_parse(checksum(msg), 10);

    cout << "\nEnviando paquete: " << msg<< endl;
    cout << "Hacia: " <<  inet_ntoa(objSocket.sin_addr) << " - " << ntohs(objSocket.sin_port) << endl;

    // Se limpia el buffer y se convierte string a char[]
    bzero(send_data, packSize);
    strncpy(send_data, msg.c_str(), min(packSize, (int) msg.size()));

    for (int i = 0; i < trys; ++i){

        cout << "\nIntentos Restantes: " << trys - i << endl;

        // SEND MSG
        sendto(sock, send_data, strlen(send_data), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));

        // Wait
        this_thread::sleep_for(std::chrono::milliseconds(timeout));

        // Check if ACK1 arrived 
        while (1) { 

            bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addrLen);

            if (bytes_read == -1){ // NO ACK 
                flag = 0;
                break; 
            }

            data_str.assign(ack_data, bytes_read);
            if (data_str == "V" + to_string_parse(seqNumb -1, 10)){ // Verificar que sea nuestro ACK, sino descartar
                flag = 1;
                cout << "Validate ACK SeqNum: " << flag << endl;
                break;
            }
            
        }


        if (!flag){ // NO ACK (bytes_read == -1)
            cout << "Ningun ACK llego -> Reenviando" << endl;
            continue;
        }


        // Check if ACK2 arrived 
        while (1) {
        
            bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addrLen);

            if (bytes_read == -1){ // NO ACK 
                flag = 0;
                break; 
            }

            data_str.assign(ack_data, bytes_read);
            if (data_str == "V" + to_string_parse(seqNumb -1, 10)){ // Verificar que sea nuestro ACK, sino descartar
                flag = 1;
                cout << "Validate ACK SeqNum: " << flag << endl;
                break;
            }
            
        }


        if (!flag){ // NO ACK 2 (bytes_read == -1)
            cout << "Solo 1 ACK -> Enviado con exito!" << endl;
            messageSent = 1;
            break;
        }

        // Second ACK
        cout << "Dos ACK's llegaron -> Error de envio" << endl;
    }
    
    return messageSent;

}

// Calcula numero de paquetes y divide mensaje en varios
bool sendMsg(struct sockaddr_in &objSocket, socklen_t &addrLen, string msg, int sock, int &seqNumb){

    // El msg se dividira en paquetes. Cada paquete desperdiciara:
    
    // 1 byte en indicar protocolo
    // 3  bytes en indicar numero de paquetes restantes
    // 10 bytes en seqNumber
    // 10 bytes en checksum

    // Se le resta 24 al packsize pq cada uno gastara 10 + 10 + 3 + 1 bytes en informacion de paquetes
    int realSize = packSize - 24;

    // Se guarda el protocolo
    char protocol = msg[0];

    // Se le resta el primer caracter a msg, pq protocolo (CRUDA) va a ser incluido repetitivamente en cada paquete
    // Ademas esto nos ayuda a calcular el numero de paquetes requeridos
    msg = msg.substr(1, msg.size() - 1);

    // calculamos el numero de paquetes requeridos
    int numPacks = ceil( (float) msg.size() / realSize );

    bool send;
    string pack;

    for (int i = 0; i < numPacks; ++i){
        
        // Se crea un paquete: A 003 msg
        pack = protocol + to_string_parse(numPacks-i, 3) + msg.substr(i*realSize, realSize);

        // Cada paquete se envia por RDT3
        send = sendPackageRDT3(objSocket, addrLen, pack, sock, seqNumb);
        if (!send) break;

        // if (msg.size() > realSize) msg.substr(realSize, msg.size() - realSize); 

    }

    return send;

}



// Recive -------------------------------------------------------------------------------------------

// Recibe paquetes individuales
string recivePackageRDT3(struct sockaddr_in &objSocket, socklen_t &addrLen, int sock){

    cout << "\nEsperando recibir paquete... " << endl;

    int bytes_read;
    string seqNum, pack = "";
    char buffData[packSize];   

    while (1){

        // Leer hasta que se reciva un paquete
        bytes_read = recvfrom(sock, buffData, packSize, 0, (struct sockaddr *)&objSocket, &addrLen);
        pack.assign(buffData, bytes_read);
        seqNum = "V" + pack.substr(pack.size()-20, 10);

        if (validateChecksum(pack)){ // Send 1 ACK and exit
            sendto(sock, seqNum.c_str(), strlen(seqNum.c_str()), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
            break;
        }
        else{ // Send 2 ACK (Corrupted Data)
            sendto(sock, seqNum.c_str(), strlen(seqNum.c_str()), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
            sendto(sock, seqNum.c_str(), strlen(seqNum.c_str()), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
        }
    }

    cout << "Paquete Recepcionado: " << pack << endl;
    cout << "Desde: " << inet_ntoa(objSocket.sin_addr) << " - " << ntohs(objSocket.sin_port) << endl;


    return pack;

}

// Covierte varios paquetes a un solo mensaje
string reciveMsg(struct sockaddr_in &objSocket, socklen_t &addrLen, int sock){

    int packNum;
    string pack;
    string totalMsg = "";

    // Recibimos el primer paquete
    pack = recivePackageRDT3(objSocket, addrLen, sock);

    // Le quitamos el numero de paquetes, el seqNumber y el checkSum
    totalMsg += pack[0] + pack.substr(4, pack.size()-24);

    packNum = stoi(pack.substr(1, 3));

    // Recivimos tantos paquetes como falten
    while (--packNum){
        pack = recivePackageRDT3(objSocket, addrLen, sock);
        totalMsg += pack.substr(4, pack.size()-24);
    }
   
   return totalMsg;

}



class Server{
private:

    socklen_t addr_len;
    struct sockaddr_in client_addr; // last client
    int port, sock, seqNumber = 0;
    
public:

    Server(){}

    Server(int sockPort){

        struct sockaddr_in initSock;
        port = sockPort;

        if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
            perror("Socket err");
            exit(1);
        }

        initSock.sin_family = AF_INET;
        initSock.sin_port = htons(port);
        initSock.sin_addr.s_addr = INADDR_ANY;
        bzero(&(initSock.sin_zero),8);

        if (bind(sock,(struct sockaddr *)&initSock, sizeof(struct sockaddr)) == -1)  {
            perror("Bind");
            exit(1);
        }

        addr_len = sizeof(struct sockaddr);
    }


    // Guarda en "client_addr" el ultimo cliente que envio mensaje
    // (Para potencialmente responderle despues)
    string serverRecive(){
        string answ = reciveMsg(client_addr, addr_len, sock); 
        return answ;
    }

    // Le envia msg a alguien en especifico
    bool serverSend(struct sockaddr_in &addr, string msg){
        return sendMsg(addr, addr_len, msg, sock, seqNumber);
    }

    // Le envia msg al ultimo cliente que se leyo
    bool serverSendLast(string msg){
        return sendMsg(client_addr, addr_len, msg, sock, seqNumber);
    }

    struct sockaddr_in getLastReadClient(){
        return client_addr;
    }

};



class Client{

private:

    socklen_t addr_len;
    struct sockaddr_in server_addr; // last client
    int port, sock, seqNumber = 0;

public:

    Client(){}

    Client(int sockPort){

        port = sockPort;

        struct hostent *host = (struct hostent *) gethostbyname((char *)"127.0.0.1");

        if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
            perror("socket");
            exit(1);
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr = *((struct in_addr *)host->h_addr);
        bzero(&(server_addr.sin_zero),8);

    }


    // Recive un mensaje del servidor
    string clientRecive(){
        string answ = reciveMsg(server_addr, addr_len, sock); 
        return answ;
    }


    // Le envia msg al servidor
    bool clientSend(string msg){
        return sendMsg(server_addr, addr_len, msg, sock, seqNumber);
    }


    
};