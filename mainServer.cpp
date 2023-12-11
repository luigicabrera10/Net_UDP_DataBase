#include "comunication.h"


// Asegurar Sincronizacion
mutex querySubServer;
mutex keepAliveVectMtx;

// KeepAliveVector
bool onlineServers[serversNum];

// Sockets
Server mainServer_Sub;
Server mainServer_Client;
Server mainServer_KeepAlive;

// Guardar las direcciones y puertos de los subServers y KeepAlive's
struct sockaddr_in subServers[serversNum];
struct sockaddr_in keepAlive[serversNum];


void keepAliveThread(){

    while(1){
        
        keepAliveVectMtx.lock(); 
        for (int i = 0; i < serversNum; ++i){
            onlineServers[i] = mainServer_KeepAlive.serverSend(keepAlive[i], keepAliveData);
        }
        keepAliveVectMtx.unlock();

        this_thread::sleep_for(chrono::seconds(keepAliveSecs));

    }
}

// Enviar solo un msg a cualquier subserver a la vez (evitar que ACK's se mezclen)
bool sendMsgToSubServer(int index, string msg){

    bool answ;

    querySubServer.lock();
    answ = mainServer_Sub.serverSend(subServers[index], msg);
    querySubServer.unlock();

    return answ;
}

// Funcion que redirige el mensaje a dos servidores
void simpleRedirect(string msg){
    int mod = msg[3] % serversNum; // Seleccionar el subserver
    sendMsgToSubServer(mod, msg);
    sendMsgToSubServer((mod+1) % serversNum, msg);
}

// Funcion para querys recursivas
string recursiveRead(string base, string query, int maxRecursive, int deep = 0){

    // Max recursion
    if (deep > maxRecursive) return base;

    // Check for online servers
    bool alive;
    int mod = query[0] % serversNum;

    keepAliveVectMtx.lock();
    alive = onlineServers[mod];
    keepAliveVectMtx.unlock();

    if (!alive) {
        mod = (mod+1)%serversNum;

        keepAliveVectMtx.lock();
        alive = onlineServers[mod];
        keepAliveVectMtx.unlock();

        if (!alive){
            cout << "Server " << mod << " Its not online for query " << query << "!" << endl;
            return base; // No server online for that query
        }
    }

    // Set tabs
    string tabs = "";
    for (int i = 0; i < deep+1; ++i) tabs += "\t";

    // Send Query
    string querySubServer = "R" + to_string_parse(query.size(), 2) + query;
    cout << "Sending SubServerQuery: " << querySubServer << endl;
    sendMsgToSubServer(mod, querySubServer);

    // Recive the answer (all relations of query)
    // all results separated by ";"
    string rawAnsw = mainServer_Sub.serverRecive(); 
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

// Funcion que ejecuta la query de lectura (R)
string readRedirect(string msg){ 

    string answ;

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


// Funcion que recive a los subservers (Como si fueran clientes)
void waitSubServers(){

    string answ;
    cout << "\n\nWaiting for subservers: " << endl;

    for (int i = 0; i < serversNum; ++i){

        // Get SubServers 
        answ = mainServer_Sub.serverRecive();
        if (answ == subServerWave) subServers[i] = mainServer_Sub.getLastReadClient();
        else --i;

        // Get KeepAlive's
        answ = mainServer_KeepAlive.serverRecive();
        if (answ == keepAliveData) keepAlive[i] = mainServer_KeepAlive.getLastReadClient();
        else  --i;

    }
}

// Funcion que escucha a los cliente
void listenClients(){
    string recived_data;
    string query_answ;

    while(1){
        // recived_data = reciveMsg(client_addr, addr_lenClient, sockClient);
        recived_data = mainServer_Client.serverRecive();


        if (recived_data[0] != 'R') thread(simpleRedirect, recived_data).detach();
        else {

            query_answ = readRedirect(recived_data); // Si es read se usa readRedirect

            cout << "Query answ:\n" << query_answ << endl;
            mainServer_Client.serverSendLast("A" + to_string_parse(query_answ.size(), 4) + query_answ);

        }
    }
}

int main(){

    // Inicializar Sockets
    mainServer_Client = Server(basePort);
    mainServer_Sub = Server(basePort+1);
    mainServer_KeepAlive = Server(basePort+2);

    // Need to know all subservers ports before keep alive and query's
    waitSubServers();
    cout << "\nALL SUBSERVERS READY" << endl;


    // Keep Alive Thread Execution
    thread(keepAliveThread).detach();

    // listen client
    listenClients();

    return 0;
}