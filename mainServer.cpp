#include "comunication.h"

const int serversNum = 4;
bool onlineServers[serversNum];
mutex querySubServers[serversNum];

struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
struct sockaddr_in servers[serversNum];


// KeepAlive Function (thread)
void keepAlive(){

    int bytes_read;
    char sendData[packSize];
    char AnswData[packSize];
    string data_str;

    bzero(sendData, packSize);
    strncpy(sendData, keepAliveStr.c_str(), (int) keepAliveStr.size());

    while (1){

        this_thread::sleep_for(chrono::seconds(keepAliveSecs));

        for (int i = 0; i < serversNum; ++i){
            sendto(sock, sendData, strlen(sendData), 0, 
                   (struct sockaddr *)&(servers[i]), sizeof(struct sockaddr));

            this_thread::sleep_for(chrono::milliseconds(keepAliveSecs * 100)); // Wait Respond?

            bytes_read = recvfrom(sock, AnswData, packSize, MSG_DONTWAIT, (struct sockaddr *)&(servers[i]), &addr_len);

            if (bytes_read == -1) onlineServers[i] = 0;
            else{
                data_str.assign(AnswData, bytes_read);
                onlineServers[i] = (bool) (data_str == keepAliveStrAnsw);
            }
            
        }

        cout << "\nKeepAlive: ";
        for (int i = 0; i < serversNum; ++i) cout << onlineServers[serversNum] << " ";
        cout << endl;

    }
}


bool sendMsgToSubServer(int index, string msg){

    bool answ;

    querySubServers[index].lock();
    answ = sendMsg(servers[index], msg);
    querySubServers[index].unlock();

    return answ;
}

// Funcion que redirige el mensaje a dos servidores
void simpleRedirect(string msg){
    int mod = msg[3] % serversNum; // Seleccionar el subserver
    sendMsgToSubServer(mod, msg);
    sendMsgToSubServer((mod+1) % serversNum, msg);
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
    
    // sendMsg(servers[mod], querySubServer);
    if (!sendMsgToSubServer(mod, querySubServer)){ // Si no se envio msg, usar backup (Modificable con keep alive)
        sendMsgToSubServer((mod+1)%serversNum, querySubServer);
    }

    // Recive the answer (all relations of query)
    // all results separated by ";"
    string rawAnsw = reciveMsg(servers[mod]); 
    // cout << "RAW ANSW: " << rawAnsw << endl;

    // Parse protocols
    int size = stoi(rawAnsw.substr(1, 4));
    string answ = rawAnsw.substr(5, size);
    // cout << "SemiParsed ANSW: " << answ << endl;

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
    // cout << "Parsed Answers: " << endl;
    // for (int i = 0; i < parsedAns.size(); ++i) cout << parsedAns[i] << endl; 

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
void listenClients(){
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
    waitSubServers();                           // May be deleted with keepAlive()
    cout << "ALL SUBSERVERS READY" << endl;     // May be deleted with keepAlive()

    // thread(keepAlive).detach();

    // listen client
    listenClients();

    return 0;
}