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

using namespace std;

int sock;
socklen_t addr_len;
struct sockaddr_in server_addr;
const int packSize = 1024;

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
            cout << "Sending ACK" << endl;
            break;
        }
        else{ // SEND NAK
            cout << "Sending NAK" << endl;
            sendto(sock, "UNU", strlen("UNU"), 0, (struct sockaddr *)&objSocket, sizeof(struct sockaddr));
        }  
    }

    cout << "MSG recived: " << aux << endl;    

    return aux;
   
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
        //ans.push_back(name1 + " " + Data[name1][i].second + " " + Data[name1][i].first);
    }

    ans = ans.substr(0, ans.size()-1);

    cout << ans;

    return ans;
}

void Update(string name1, string name2, string relation, string newName1, string newName2, string newRelation){

    if (Data.find(name1) == Data.end()) return; // No existe relacion 

    deleteRelation(name1, name2, relation);
    insert(newName1, newName2, newRelation);

}


void sendAnswer(string msg){

    string subMsg;
    int size = min(1006, (int) msg.size());

    subMsg = "A" + to_string_parse(msg.size(), 7) + msg.substr(0, size);
    subMsg = subMsg + to_string_parse(checksum(subMsg), 10);

    sendMsg(server_addr, subMsg);

    msg = msg.substr(size, msg.size()-size);
    
    while(msg.size()){

        subMsg = msg.substr(0, 1014);
        subMsg = subMsg + to_string_parse(checksum(subMsg), 10);
        sendMsg(server_addr, subMsg);
        msg = msg.substr(1014, msg.size() - 1014);

    }

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
        sendAnswer(ans);

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