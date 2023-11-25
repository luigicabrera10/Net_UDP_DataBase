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
int sequence_number = 0;

// int sock; // No more need
// struct sockaddr_in server_addr; // IDK IF THIS IS OKAY

// Keep Alive data
const int keepAliveSecs = 2;
string keepAliveStr = "RUAlive?";
string keepAliveStrAnsw = "OkiDoki";


// SOCKETS Functions --------------------------------------------------------------------------------

// init Socket Function
void initSocket(int &sockRef, struct sockaddr_in &objSocket, socklen_t &addr_len, int port){
    if ((sockRef = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket err");
        exit(1);
    }

    objSocket.sin_family = AF_INET;
    objSocket.sin_port = htons(port);
    objSocket.sin_addr.s_addr = INADDR_ANY;
    bzero(&(objSocket.sin_zero),8);

    if (bind(sockRef,(struct sockaddr *)&objSocket, sizeof(struct sockaddr)) == -1)  {
        perror("Bind");
        exit(1);
    }

    addr_len = sizeof(struct sockaddr);
}

void connectToSocket(int &sock, struct sockaddr_in &objSocket, int port){
    struct hostent *host = (struct hostent *) gethostbyname((char *)"127.0.0.1");

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("socket");
        exit(1);
    }

    objSocket.sin_family = AF_INET;
    objSocket.sin_port = htons(port);
    objSocket.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(objSocket.sin_zero),8);
}



// AUX Functions ------------------------------------------------------------------------------------

// Funcion que convierte una string con un numero
int checksum(const string input) {
   int checksum = 0;
   for (int i = 0; i < input.size(); ++i) checksum += (int) input[i];
   return checksum;
}

string to_string_parse(int num, int size = 2){
	string s = to_string(num);
	while(s.size() < size) s = "0" + s;
	return s;	
}

// Funcion que valida el checksum de un mensaje (Confirma si esta correcto)
bool validateChecksum(string msg){        
    int check = stoi(msg.substr(msg.size() - 10, 10)); // 10 ultimos caracteres son checksum
    return check ==  checksum(msg.substr(0, msg.size()-10));
}



// SEND ---------------------------------------------------------------------------------------------

// Envia paquetes individuales
bool sendPackageRDT3(struct sockaddr_in &objSocket, socklen_t &addrLen, string msg, int sock){

    char send_data[packSize];
    char ack_data[packSize];
    string data_str = "";

    int bytes_read;
    int trys = 2;
    bool messageSent = 0;
    bool flag; // Indica si llego ACK o no

    // Add SeqNumber y CheckSum
    msg = msg + to_string_parse(sequence_number++, 10);
    msg = msg + to_string_parse(checksum(msg), 10);

    cout << "\nEnviando paquete: " << msg<< endl;
    cout << "Hacia: " <<  inet_ntoa(objSocket.sin_addr) << " - " << ntohs(objSocket.sin_port) << endl;

    bzero(send_data, packSize);
    strncpy(send_data, msg.c_str(), min(packSize, (int) msg.size()));

    while (trys--){

        cout << "\nIntentos Restantes: " << trys +1 << endl;

        // SEND MSG
        sendto(sock, send_data, strlen(send_data), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));

        // Wait
        this_thread::sleep_for(std::chrono::milliseconds(timeout));

        // Try recive ACK 1
        do { // Descartar mesajes q no sean ACKS
            bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addrLen);

            if (bytes_read == -1){ // NO ACK 
                flag = 0;
                break; 
            }

            data_str.assign(ack_data, bytes_read);
            cout << "Read ACK str (" << data_str.size() << "): " << data_str << endl;
            flag = data_str == to_string_parse(sequence_number -1, 10);
            cout << "Validate SeqNum: " << flag << endl;
            
        } while (!flag);


        if (!flag){ // NO ACK (bytes_read == -1)
            cout << "Ningun ACK llego... Reenviando" << endl;
            continue;
        }


        // Try recive ACK 2
        do { // Descartar mesajes q no sean ACKS
            bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addrLen);

            if (bytes_read == -1){ // NO ACK 
                flag = 0;
                break; 
            }

            data_str.assign(ack_data, bytes_read);
            cout << "Read ACK str (" << data_str.size() << "): " << data_str << endl;
            flag = data_str == to_string_parse(sequence_number -1, 10);
            cout << "Validate SeqNum: " << flag << endl;
            
        } while (!flag);


        if (!flag){ // NO ACK 2 (bytes_read == -1)
            cout << "Solo 1 ACK... Enviado con exito!" << endl;
            messageSent = 1;
            break;
        }

        // Second ACK
        cout << "Dos ACK's llegaron... Reenviando" << endl;
        data_str.assign(ack_data, bytes_read);
        cout << "Read str (" << data_str.size() << "): " << data_str << endl;
        cout << "Validation: " << (bool) (data_str == to_string_parse(sequence_number -1, 10)) << endl;

    }
    
    return messageSent;

}

// Calcula numero de paquetes y divide mensaje en varios
bool sendMsg(struct sockaddr_in &objSocket, socklen_t &addrLen, string msg, int sock){

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

      pack = protocol + to_string_parse(numPacks-i, 3) + msg.substr(0, realSize);

      send = sendPackageRDT3(objSocket, addrLen, pack, sock);
      if (!send) break;
      
    //   this_thread::sleep_for(chrono::seconds(timeout)); // ESPERA A QUE PROCESEN COSAS :c

      if (msg.size() > realSize) msg.substr(realSize, msg.size() - realSize); 

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
        bytes_read = recvfrom(sock, buffData, packSize, 0, (struct sockaddr *)&objSocket, &addrLen);
        pack.assign(buffData, bytes_read);
        seqNum = pack.substr(pack.size()-20, 10);

        if (validateChecksum(pack)){ // Send 1 ACK and exit
            sendto(sock, seqNum.c_str(), strlen(seqNum.c_str()), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
            this_thread::sleep_for(std::chrono::milliseconds(timeout)); // PARA QUE NO SE ENVIE OTRO PAQUETE Y SE CONFUNDA CON 2do ACK
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

// Covierte un numero de paquetes en un mensaje
string reciveMsg(struct sockaddr_in &objSocket, socklen_t &addrLen, int sock){

   int packNum;
   string pack;
   string totalMsg = "";

   // Recibimos el primer paquete
   pack = recivePackageRDT3(objSocket, addrLen, sock);

   // Le quitamos el numero de paquetes, el seqNumber y el checkSum
   totalMsg += pack[0] + pack.substr(4, pack.size()-24);

   packNum = stoi(pack.substr(1, 3));

   while (--packNum){
      pack = recivePackageRDT3(objSocket, addrLen, sock);
      totalMsg += pack.substr(4, pack.size()-24);
   }
   
   return totalMsg;

}