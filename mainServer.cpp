#include "comunication.h"

bool onlineServers[serversNum];
mutex querySubServers[serversNum];

// To init Sockets
struct sockaddr_in initSockClient;
struct sockaddr_in initSockSubServer;
struct sockaddr_in initSockKeepAlive;

socklen_t addr_lenClient;
struct sockaddr_in client_addr;

socklen_t addr_lenSubServer;
struct sockaddr_in servers[serversNum];

socklen_t addr_lenKeepAlive;
struct sockaddr_in keepAlive[serversNum];

// Sockets for each task
int sockClient;
int sockKeepAlive;
int sockSubServers;

bool sendMsgToSubServer(int index, string msg){ // ESTO PUEDES CAMBIARLO

    bool answ;

    querySubServers[index].lock();
    answ = sendMsg(servers[index], addr_lenSubServer, msg, sockSubServers);
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
    sendMsgToSubServer(mod, querySubServer);

    // Recive the answer (all relations of query)
    // all results separated by ";"
    string rawAnsw = reciveMsg(servers[mod], addr_lenSubServer, sockSubServers); 
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

    // Main Server Socket (for client) -----------------------------------------------------------------------
    initSocket(sockClient, initSockClient, addr_lenClient, basePort);
	cout << "\nUDPServer Waiting for CLIENT on port " << basePort << endl;

    // Main Server Sockets (for SubServers) ------------------------------------------------------------------
    initSocket(sockSubServers, initSockSubServer, addr_lenSubServer, basePort + 1);
    cout << "UDPServer Waiting for subServers  on port " << basePort + 1 << endl;

    // Main Server Sockets (for KeepAlive's) -------------------------------------------------------------------
    initSocket(sockKeepAlive, initSockKeepAlive, addr_lenKeepAlive, basePort + 2);
    cout << "UDPServer Waiting for keepAlive  on port " << basePort + 2 << endl;

}

void waitSubServers(){

    int bytes_read;
    string recived_data;
    char recv_data[packSize];

    struct sockaddr_in client_addr;

    cout << "\n\nWaiting for subservers: " << endl;

    for (int i = 0; i < serversNum; ++i){

        // SubServer Arrive 
        bytes_read = recvfrom(sockSubServers,recv_data,1024,MSG_WAITALL, (struct sockaddr *)&(servers[i]), &addr_lenSubServer);
	    recived_data.assign(recv_data, bytes_read);
        recived_data += '\0';

        if (recived_data.substr(0, 7) != "OkiDoki") {
            --i;
            cout << "No es SubServidor (Mal Protocolo)" << endl;
        }
        else{
            cout << "\nSubServer " << i <<  ": " << recived_data << " " << inet_ntoa(servers[i].sin_addr) << " " << ntohs(servers[i].sin_port) << endl;
            onlineServers[i] = 1;
        }

        // SubServer keepAlive
        bytes_read = recvfrom(sockKeepAlive,recv_data,1024,MSG_WAITALL, (struct sockaddr *)&(keepAlive[i]), &addr_lenKeepAlive);
	    recived_data.assign(recv_data, bytes_read);
        recived_data += '\0';

        if (recived_data.substr(0, 7) != "OkiDoki") {
            --i;
            cout << "No es SubServidor (Mal Protocolo)" << endl;
        }
        else{
            cout << "SubServer (KeepAlive) " << i <<  ": " << recived_data << " " << inet_ntoa(keepAlive[i].sin_addr) << " " << ntohs(keepAlive[i].sin_port) << endl;
        }

    }
}

// Funcion que escucha al cliente
void listenClients(){
    string recived_data;
    string query_answ;

    while(1){
        recived_data = reciveMsg(client_addr, addr_lenClient, sockClient);
        if (recived_data[0] != 'R') thread(simpleRedirect, recived_data).detach();
        else {
            query_answ = readRedirect(recived_data); // Si es read se usa readRedirect
            cout << "Query answ:\n" << query_answ << endl;
            sendMsg(client_addr, addr_lenClient, "A" + to_string_parse(query_answ.size(), 4) + query_answ, sockClient);
        }
    }
}

int main(){

    // Inicializar el servidor
    initServer();

    // Need to know all subservers ports before keep alive and query's
    waitSubServers();
    cout << "\nALL SUBSERVERS READY" << endl;

    // Keep Alive Thread Execution

    // listen client
    listenClients();

    return 0;
}