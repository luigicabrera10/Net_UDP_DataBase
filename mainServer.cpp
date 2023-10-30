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
#include <math.h>
#include <fstream>

using namespace std;

const int serversNum = 4;
const int packSize = 1024;
const int timeout = 100;
int sequence_number = 0;
bool onlineServers[serversNum];


int sock;
socklen_t addr_len;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
struct sockaddr_in servers[serversNum];

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



// Envia paquetes individuales
bool sendPackageRDT3(struct sockaddr_in &objSocket, string msg){

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
    cout << "Hacia: " <<  inet_ntoa(objSocket.sin_addr) << " - " << ntohs(objSocket.sin_port) << endl; // IP y puerto

    bzero(send_data, packSize);
    strncpy(send_data, msg.c_str(), min(packSize, (int) msg.size())); // Copiar msg a send_data

    while (trys--){ // Try 3 times

        cout << "\nIntentos Restantes: " << trys +1 << endl;

        // SEND MSG
        sendto(sock, send_data, strlen(send_data), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));

        // Wait
        this_thread::sleep_for(std::chrono::milliseconds(timeout));

        // Try recive ACK 1
        do { // Descartar mesajes q no sean ACKS
            bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addr_len);

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
            bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addr_len);

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
bool sendMsg(struct sockaddr_in &objSocket, string msg){   
    // El msg se dividira en paquetes. Cada paquete desperdiciara:
    // 10 bytes en checksum
    // 10 bytes en seqNumber
    // 3  bytes en indicar numero de paquetes restantes
    // 1 byte en indicar protocolo

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

      send = sendPackageRDT3(objSocket, pack);
      if (!send) break;
      
    //   this_thread::sleep_for(chrono::seconds(timeout)); // ESPERA A QUE PROCESEN COSAS :c

      if (msg.size() > realSize) msg.substr(realSize, msg.size() - realSize); 

   }

   return send;

}


// Recibe paquetes individuales
string recivePackageRDT3(struct sockaddr_in &objSocket){

    cout << "\nEsperando recibir paquete... " << endl;

    int bytes_read;
    string seqNum, pack = "";
    char buffData[packSize];   

    while (1){
        bytes_read = recvfrom(sock, buffData, packSize, 0, (struct sockaddr *)&objSocket, &addr_len);
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

    cout << "Paquete Recepcionado: " << endl << pack << endl;
    cout << "Desde: " << inet_ntoa(objSocket.sin_addr) << " - " << ntohs(objSocket.sin_port) << endl;


    return pack;

}

// Covierte un numero de paquetes en un mensaje
string reciveMsg(struct sockaddr_in &objSocket){

   int packNum;
   string pack;
   string totalMsg = "";

   // Recibimos el primer paquete
   pack = recivePackageRDT3(objSocket);

   // Le quitamos el numero de paquetes, el seqNumber y el checkSum
   totalMsg += pack[0] + pack.substr(4, pack.size()-24);

   packNum = stoi(pack.substr(1, 3));

   while (--packNum){
      pack = recivePackageRDT3(objSocket);
      totalMsg += pack.substr(4, pack.size()-24);
   }
   
   return totalMsg;

}




// Funcion que redirige el mensaje a dos servidores
void simpleRedirect(string msg){
    int mod = msg[3] % serversNum; // Seleccionar el subserver
    sendMsg(servers[mod], msg); // Enviar el mensaje al subserver
    sendMsg(servers[(mod+1) % serversNum], msg); // Enviar el mensaje al backup del subserver
}

// Funcion que lee el mensaje del subserver
string readSubServer(int mod){

    int size;
    string subMsg;
    string totalMsg;
    subMsg = reciveMsg(servers[mod]);
    totalMsg = subMsg;

    size = stoi(subMsg.substr(1, 7) ) - (subMsg.size() - 1 - 7 - 10); // Tamaño del mensaje - (header + checksum)

    while (size > 0){
        subMsg = reciveMsg(servers[mod]);
        totalMsg += subMsg;
        size -= (subMsg.size() - 10);
    }
    return totalMsg; // Regresa el mensaje completo
}


vector<string> parseRead(string answer){
    vector<string> parsedAnswers;
    return parsedAnswers;
}

string recursiveRead(string base, string query, int maxRecursive, int deep = 0){

    // Max recursion
    if (deep > maxRecursive) return base;

    // Check for online servers
    int mod = query[0] % serversNum;
    if (!onlineServers[mod]) {
        mod = (mod+1)%serversNum;
        if (!onlineServers[mod]) return base; // No server online for that query
    }

    // Set tabs
    string tabs = "";
    for (int i = 0; i < deep+1; ++i) tabs += "\t";

    // Send Query
    string querySubServer = "R" + to_string_parse(query.size(), 2) + query;
    cout << "Sending SubServerQuery: " << querySubServer << endl;
    sendMsg(servers[mod], querySubServer);

    // Recive the answer (all relations of query)
    // all results separated by ";"
    string rawAnsw = reciveMsg(servers[mod]); 
    cout << "RAW ANSW: " << rawAnsw << endl;

    // Parse protocols
    int size = stoi(rawAnsw.substr(1, 4));
    string answ = rawAnsw.substr(5, size);
    cout << "SemiParsed ANSW: " << answ << endl;

    // Parse answ
    string aux = "";
    vector <string> parsedAns;
    for (int i = 0; i < answ.size(); ++i){
        if (answ[i] == ';'){
            parsedAns.push_back(aux);
            aux = "";
        }
        else aux += answ[i];
    }

    // No relations:
    if (parsedAns[0] == "") return base;

    // Print parsed answers
    cout << "Parsed Answers: " << endl;
    for (int i = 0; i < parsedAns.size(); ++i) cout << parsedAns[i] << endl; 

    // Add parsed answers to base (and follow recursivity)
    string newQuery;
    for (int i = 0; i < parsedAns.size(); ++i){
        // Add relation
        base += tabs + parsedAns[i] + "\n";

        // Get Name2 (next Query)
        newQuery = "";
        for (int j = 0; j < parsedAns[i].size(); ++j){
            if (parsedAns[i][j] == ' ') break;
            newQuery += parsedAns[i][j];
        }

        base = recursiveRead(base, newQuery, maxRecursive, deep+1);
    }

    return base;

}

// Funcion que lee el mensaje del cliente
string readRedirect(string msg){ 

    // RDT3 (server turned off)
    string answ;
    int mod = msg[3] % serversNum;

    // Parsear la query
    int size1 = stoi(msg.substr(1, 2));
    string query = msg.substr(3, size1);
    int size2 = stoi(msg.substr(3 + size1, 2));
    int maxRecursive = stoi(msg.substr(3 + size1 + 2, size2));

    // Check SubServers online
    for (int i = 0; i < serversNum; ++i){
        onlineServers[i] = sendMsg(servers[i], "T00");
        cout << "Server " << i << " online? : " << onlineServers[i] <<endl;
    }

    // Print the parsed query
    cout << "\nREAD QUERY: " << query << endl;
    cout << "MaxRecursion: " << maxRecursive << endl;

    answ = recursiveRead("\n", query, maxRecursive);
    
    return query + answ; 

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
            onlineServers[i] = 1;
        }

    }


}

// Funcion que escucha al cliente
void listenClient(){
    int bytes_read;
    string recived_data;
    string query_answ;
    char recv_data[packSize];
    while(1){

        recived_data = reciveMsg(client_addr);
        if (recived_data[0] != 'R') simpleRedirect(recived_data); // Si no es read se usa simpleRedirect
        else{

            query_answ = readRedirect(recived_data); // Si es read se usa readRedirect

            cout << "Query answ: " << query_answ << endl;
            sendMsg(client_addr, "A" + to_string_parse(query_answ.size(), 4) + query_answ);

        }
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