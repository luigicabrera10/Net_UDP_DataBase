#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <iostream>
#include <thread>
#include <vector>
#include <fstream>

using namespace std;

const int serversNum = 4;
const int packSize = 1024;

int sock;
socklen_t addr_len;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
struct sockaddr_in servers[serversNum];

int checksum(const string input) {
   int checksum = 0;
   for (int i = 0; i < input.size(); ++i) checksum += (int) input[i];
   return checksum;
}

bool validateChecksum(string msg){

    // cout << msg.size() << " - " << msg << endl;
    // cout << msg.substr(msg.size() - 10, 10) << endl;
    // cout << checksum(msg.substr(0, msg.size()-10)) << endl;
        
    int check = stoi(msg.substr(msg.size() - 10, 10));
    return check ==  checksum(msg.substr(0, msg.size()-10));

}

void sendMsg(struct sockaddr_in objSocket, string msg){

    int bytes_read;
    char data[packSize];
    char ack[packSize];
    string aux;

    bzero(data, packSize);
    for (int i = 0; i < msg.size(); ++i) data[i] = msg[i];

    while(1){
        sendto(sock, data, strlen(data), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));

        // Wait for ACK
        bytes_read = recvfrom(sock,ack,1024,MSG_WAITALL, (struct sockaddr *)&objSocket, &addr_len);
        aux.assign(ack, bytes_read);

        if (aux.substr(0,3) == "UWU") break;
    }

    cout << "MSG send: " << msg << endl;    

}

string reciveMsg(struct sockaddr_in objSocket){

    int bytes_read;
    char data[packSize];
    string aux;

    while(1){

        bytes_read = recvfrom(sock,data,1024,MSG_WAITALL, (struct sockaddr *)&objSocket, &addr_len);
        aux.assign(data, bytes_read);

        if (validateChecksum(aux)){ // SEND ACK
            sendto(sock, "UWU", strlen("UWU"), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
            break;
        }
        else{ // SEND NAK
            sendto(sock, "UNU", strlen("UNU"), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
        }  
    }

    cout << "MSG recived: " << aux << endl;    

    return aux;
   
}


void simpleRedirect(string msg){
    
    int mod = msg[3] % serversNum;
    cout << "MOD: " << msg[3] << " " << mod << endl;

    sendMsg(servers[mod], msg);
    sendMsg(servers[(mod+1) % serversNum], msg);

}

void threadReciveACK(int mod){
    char ack[packSize];
    cout << "Running thread" << endl;
    int bytes_read = recvfrom(sock,ack,1024,MSG_WAITALL, (struct sockaddr *)&(servers[mod]), &addr_len);
    cout << "Recived: " << ack << endl;
}

string readSubServer(int mod){
    int size;
    string subMsg;
    string totalMsg;


    subMsg = reciveMsg(servers[mod]);
    totalMsg = subMsg;

    size = stoi( subMsg.substr(1, 7) ) - (subMsg.size() - 1 - 7 - 10);

    while (size > 0){
        subMsg = reciveMsg(servers[mod]);
        totalMsg += subMsg;
        size -= (subMsg.size() - 10);
    }

    return totalMsg; 

}

// estamos F
void sendAnswer(string msg){

    string subMsg = msg.substr(0, 1014);
    //subMsg = subMsg + to_string_parse(checksum(subMsg), 10);
    sendMsg(client_addr, subMsg);

    msg = msg.substr(1024, msg.size()- 1024);
    
    while(msg.size()){

        subMsg = msg.substr(0, 1024);
        sendMsg(client_addr, subMsg);
        msg = msg.substr(1024, msg.size() - 1024);

    }

}

void readRedirect(string msg){

    // RDT3 (server turned off)
    int bytes_read;
    string recived_data;
    char recv_data[packSize];
    int mod = msg[3] % serversNum;

    // First server
    sendto(sock, msg.c_str(), strlen(msg.c_str()), 0, (struct sockaddr *)&(servers[mod]), sizeof(struct sockaddr));
    thread th = thread(threadReciveACK, mod);

    this_thread::sleep_for(chrono::seconds(2));

    if (!th.joinable()){

        cout << "SERVER " << mod << " not online" << endl;

        // Autosend ACK
        sendto(sock, "UWU", strlen("UWU"), 0, (struct sockaddr *)&(server_addr), sizeof(struct sockaddr));
        th.join();

        mod = (mod+1) % serversNum;
        sendto(sock, msg.c_str(), strlen(msg.c_str()), 0, (struct sockaddr *)&(servers[mod]), sizeof(struct sockaddr));
        thread th2 = thread(threadReciveACK, mod);

        this_thread::sleep_for(chrono::seconds(2));

        if (!th2.joinable()){
            // Autosend ACK
            sendto(sock, "UWU", strlen("UWU"), 0, (struct sockaddr *)&(server_addr), sizeof(struct sockaddr));
            th2.join();
            cout << "SERVER " << mod << " not online" << endl;
            return;
        }
        else th2.join();
    }
    else th.join();

    // El servidor si recibio el mensaje
    // LALO CODEA EL REGRESO
    cout << "LEER" << endl;
    string totalMsg = readSubServer(mod);
    cout << totalMsg << endl;




}

void initServer(){


    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_addr.sin_zero),8);

    if (bind(sock,(struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)  {
        perror("Bind");
        exit(1);
    }

    addr_len = sizeof(struct sockaddr);
		
	printf("\nUDPServer Waiting for client on port 5000: \n");
    fflush(stdout);

}

void waitSubServers(){

    int bytes_read;
    string recived_data;
    char recv_data[packSize];

    struct sockaddr_in client_addr;

    // IGNORAR SUB 0
    // sendto(sock, "OkiDoki", strlen("OkiDoki"), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
    // bytes_read = recvfrom(sock,recv_data,1024,MSG_WAITALL, (struct sockaddr *)&client_addr, &addr_len);

    for (int i = 0; i < serversNum; ++i){
        
        bytes_read = recvfrom(sock,recv_data,1024,MSG_WAITALL, (struct sockaddr *)&(servers[i]), &addr_len);
	    recived_data.assign(recv_data, bytes_read);
        recived_data += '\0';

        if (recived_data.substr(0, 7) != "OkiDoki") {
            --i;
            cout << "No es servidor (Mal Protocolo)" << endl;
        }
        else{
            cout << "SubServer " << i <<  ": " << recived_data << " " << inet_ntoa(servers[i].sin_addr) << " " << ntohs(servers[i].sin_port) << endl;
        }

    }

}

void listenClient(){

    int bytes_read;
    string recived_data;
    char recv_data[packSize];


    while(1){

        recived_data = reciveMsg(client_addr);

        if (recived_data[0] != 'R') simpleRedirect(recived_data);     
        else readRedirect(recived_data);

    }

}

int main(){

    // Inicializar el servidor
    initServer();

    // Esperar 4 servidores para insertar datos
    waitSubServers();

    cout << "ALL SUBSERVERS READY" << endl;

    // listen client
    listenClient();


    return 0;
}