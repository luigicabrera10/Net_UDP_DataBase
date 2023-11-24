#include "comunication.h"

struct sockaddr_in server_addr;
map< string, vector< pair<string, string> > > Data;

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
        return "A0001;"; // No existe relacion 
    }

    for (int i = 0; i < Data[name1].size(); ++i){
        ans += Data[name1][i].first + " " + Data[name1][i].second + ";";
    }

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
    else if (msg[0] == 'T'){ // Test that you are alive (MAY BE DELETED BY KEEP ALIVE)
        // return; // to unlock mutex
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
    
    char send_data[packSize];
    bzero(send_data, packSize);
    
    for (int i = 0; i < keepAliveStrAnsw.size(); ++i) send_data[i] = keepAliveStrAnsw[i];

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
    sayHi(); // may be deleted and will use keep alive

    // Solve querys
    listenQuerys();

}