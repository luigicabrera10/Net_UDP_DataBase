#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <map>
#include <math.h>

using namespace std;

int sock;
socklen_t addr_len;
struct sockaddr_in server_addr;

int sequence_number = 0;
const int packSize = 1024;
const int timeout = 2;

map< string, vector< pair<string, string> > > Data;


string to_string_parse(int num, int size){
	string s = to_string(num);
	while(s.size() < size) s = "0" + s;
	return s;	
}

int checksum(const string input) {
   int checksum = 0;
   for (int i = 0; i < input.size(); ++i) checksum += (int) input[i];
   return checksum;
}

bool validateChecksum(string msg){
    
    int check = stoi(msg.substr(msg.size() - 10, 10));
    return check ==  checksum(msg.substr(0, msg.size()-10));

}


// Envia paquetes individuales
bool sendPackageRDT3(struct sockaddr_in objSocket, string msg){

    char send_data[packSize];
    char ack_data[packSize];
    string data_str;

    int bytes_read;
    int trys = 2;
    bool messageSent = 0;

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
        this_thread::sleep_for(chrono::seconds(timeout));

        // Try recive ACK 1
        bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addr_len);

        if (bytes_read == -1){
            cout << "Ningun ACK llego... Reenviando" << endl;
            continue;
        }
        
        // FIRST ACK
        data_str.assign(ack_data, bytes_read);
        cout << "Read str (" << data_str.size() << "): " << data_str << endl;
        cout << "Validation: " << (bool) (stoi(data_str) == sequence_number -1) << endl;

        // Try recive ACK 2
        bytes_read = recvfrom(sock, ack_data, packSize, MSG_DONTWAIT, (struct sockaddr *)&objSocket, &addr_len);
        
        if (bytes_read == -1){
            cout << "Solo 1 ACK... Enviado con exito!" << endl;
            messageSent = 1;
            break;
        }

        // Second ACK
        cout << "Dos ACK's llegaron... Reenviando" << endl;
        data_str.assign(ack_data, bytes_read);
        cout << "Read str (" << data_str.size() << "): " << data_str << endl;
        cout << "Validation: " << (bool) (stoi(data_str) == sequence_number -1) << endl;

    }
    
    return messageSent;

}

// Calcula numero de paquetes y divide mensaje en varios
bool sendMsg(struct sockaddr_in objSocket, string msg){

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
      
      if (msg.size() > realSize) msg.substr(realSize, msg.size() - realSize); 

   }

   return send;

}


// Recibe paquetes individuales
string recivePackageRDT3(struct sockaddr_in objSocket){

    cout << "\nEsperando recepcion de paquete... " << endl;

    int bytes_read;
    string seqNum, pack = "";
    char buffData[packSize];   

    while (1){
        bytes_read = recvfrom(sock, buffData, packSize, 0, (struct sockaddr *)&objSocket, &addr_len);
        pack.assign(buffData, bytes_read);
        seqNum = pack.substr(pack.size()-20, 10);

        if (validateChecksum(pack)){ // Send 1 ACK and exit
            sendto(sock, seqNum.c_str(), strlen(seqNum.c_str()), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));

            // NO HA SIDO AGREGADO EN LAS OTRAS FUNCIONES
            this_thread::sleep_for(chrono::seconds(timeout)); // PARA QUE NO SE ENVIE OTRO PAQUETE Y SE CONFUNDA CON 2do ACK
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
string reciveMsg(struct sockaddr_in objSocket){

   int packNum;
   string pack;
   string totalMsg = "";

   // Recibimos el primer paquete
   pack = recivePackageRDT3(objSocket);

   // Le quitamos el numero de paquetes, el seqNumber y el checkSum
   totalMsg += pack[0] + pack.substr(4, pack.size()-20);

   packNum = stoi(pack.substr(1, 3));

   while (--packNum){
      pack = recivePackageRDT3(objSocket);
      totalMsg += pack.substr(4, pack.size()-20);
   }
   
   return totalMsg;

}




void insert(string name1, string name2, string relation){

    if (Data.find(name1) == Data.end()){ // No hay ninguna relacion, se crea vector
        Data[name1] = vector<pair <string, string>> ();
    }
    Data[name1].push_back({name2, relation});
    cout << "Insertando: " << name1 << " " << name2 << " " << relation << endl;

}


bool deleteRelation(string name1, string name2, string relation){

    bool find = 0;

    if (Data.find(name1) == Data.end()) return 0; // No existe relacion 

    for (int i = 0; i < Data[name1].size(); ++i){
        if (Data[name1][i].first == name2 && Data[name1][i].second == relation){ // relacion encontrada
            Data[name1].erase(Data[name1].begin() + i);
            find = 1;
            --i;
        }
    }

    if (Data[name1].size() == 0) Data.erase(name1);

    return find;

}


bool deleteAll(string name1){

    if (Data.find(name1) == Data.end()) return 0; // No existe relacion 
    Data.erase(name1);
    return 1;

}

// Funcion que retorna un vector de las relaciones de un nombre por ejemplo:
// read("Juan") -> {"Juan padre es", "Juan hijo es", "Juan hermano es"}
string read(string name1){
    string ans = "";

    if (Data.find(name1) == Data.end()){
        cout << "No existe relacion" << endl;
        return ans; // No existe relacion 
    }

    for (int i = 0; i < Data[name1].size(); ++i){
        ans += Data[name1][i].first + " " + Data[name1][i].second + ";";
    }

    ans = ans.substr(0, ans.size()-1);
    ans = "A" + to_string_parse(ans.size(), 4) + ans;

    cout << ans;

    return ans;
}

void Update(string name1, string name2, string relation, string newName1, string newName2, string newRelation){

    if (Data.find(name1) == Data.end()) return; // No existe relacion 

    deleteRelation(name1, name2, relation);
    insert(newName1, newName2, newRelation);

}





void parsing(string msg){
    if(msg[0] == 'C'){
        int size1 = stoi(msg.substr(1, 2));
        string name1 = msg.substr(3, size1);
        int size2 = stoi(msg.substr(3+size1, 2));
        string name2 = msg.substr(5+size1, size2);
        int size3 = stoi(msg.substr(5+size1+size2, 2));
        string relation = msg.substr(7+size1+size2, size3);
        insert(name1, name2, relation);
    } else if (msg[0] == 'R'){
        int size1 = stoi(msg.substr(1, 2));
        string name1 = msg.substr(3, size1);
        string ans = read(name1);

        // Send answer to main server
        sendMsg(server_addr, ans);

    } else if (msg[0] == 'U'){
        int size1 = stoi(msg.substr(1, 2));
        string name1 = msg.substr(3, size1);
        int size2 = stoi(msg.substr(3+size1, 2));
        string name2 = msg.substr(5+size1, size2);
        int size3 = stoi(msg.substr(5+size1+size2, 2));
        string relation = msg.substr(7+size1+size2, size3);
        int size4 = stoi(msg.substr(7+size1+size2+size3, 2));
        string newName1 = msg.substr(9+size1+size2+size3, size4);
        int size5 = stoi(msg.substr(9+size1+size2+size3+size4, 2));
        string newName2 = msg.substr(11+size1+size2+size3+size4, size5);
        int size6 = stoi(msg.substr(11+size1+size2+size3+size4+size5, 2));
        string newRelation = msg.substr(13+size1+size2+size3+size4+size5, size6);
        Update(name1, name2, relation, newName1, newName2, newRelation);
    } else if (msg[0] == 'D'){
        int size1 = stoi(msg.substr(1, 2));
        string name1 = msg.substr(3, size1);
        int size2 = stoi(msg.substr(3+size1, 2));
        string name2 = msg.substr(5+size1, size2);
        if(name2 == "*"){
            deleteAll(name1);
        } else {
            int size3 = stoi(msg.substr(5+size1+size2, 2));
            string relation = msg.substr(7+size1+size2, size3);
            deleteRelation(name1, name2, relation);
        }
    }

}


void initSubServer(){

    struct hostent *host = (struct hostent *) gethostbyname((char *)"127.0.0.1");

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(server_addr.sin_zero),8);

}

void sayHi(){
    
    string okidoki = "OkiDoki";
    char send_data[packSize];
    bzero(send_data, packSize);
    for (int i = 0; i < okidoki.size(); ++i) send_data[i] = okidoki[i];

    sendto(sock, send_data, strlen(send_data), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
}

void listenQuerys(){

    int bytes_read;
    char recv_data[packSize];
    string recived_data;
    
    while(1){

        recived_data = reciveMsg(server_addr);

        // Parsing
        parsing(recived_data);      

    }


}

int main(){

    // Init subserver socket
    initSubServer();

    // Say hi to main server
    sayHi();

    // Solve querys
    listenQuerys();

}