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
#include <math.h>

using namespace std;

// Variables Globales
int sock;
socklen_t addr_len;
struct sockaddr_in server_addr;

const int packSize = 1024;
const int timeout = 1;
int sequence_number = 0;
int pkt_number = 0;


string to_string_parse(int num, int size = 2){
	string s = to_string(num);
	while(s.size() < size) s = "0" + s;
	return s;	
}

// Funcion que calcula el checksum
int checksum(const string input) {
   int checksum = 0;
   for (int i = 0; i < input.size(); ++i) checksum += (int) input[i];
   return checksum;
}

// Funcion que valida el checksum
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

      // this_thread::sleep_for(chrono::seconds(timeout)); // ESPERA A QUE PROCESEN COSAS :c

      
      if (msg.size() > realSize) msg.substr(realSize, msg.size() - realSize); 

   }

   return send;

}


// Recibe paquetes individuales
string recivePackageRDT3(struct sockaddr_in &objSocket){

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


// Funcion que parsea el mensaje
void parsing( char *data){
   // Guardamos todo el contenido del mensaje en un string
   string msg(data);
   if (msg[0] == 'C'){
      // Buscamos el segundo espacio
      size_t pos = msg.find(" ", 2);
      // Buscamos el tercer espacio
      size_t pos2 = msg.find(" ", pos+1);
      // Obtenemos el primer nombre que se encuentra antes del segundo espacio
      string name1 = msg.substr(2, pos-2);
      // Obtenemos el segundo nombre que se encuentra despues del segundo espacio
      string name2 = msg.substr(pos+1, pos2-pos-1);
      // Obtenemos la relacion que se encuentra despues del tercer espacio
      string relation = msg.substr(pos2+1);
      // Obtenemos el numero de secuencia
      string sequence_number_str = to_string_parse(sequence_number);
      // Obtenemos el tamaño de la cadena
      int size1 = name1.length();
      int size2 = name2.length();
      int size3 = relation.length();
      // Formateamos el tamaño de la cadena
      string size1_str = to_string_parse(size1);
      string size2_str = to_string_parse(size2);
      string size3_str = to_string_parse(size3);
      // Imprimimos los nombres y la relacion
      cout << "Name 1: " << name1 << endl;
      cout << "Name 2: " << name2 << endl;
      cout << "Relation: " << relation << endl;
      // Guardamos en el char "C" + tamaño1 + nombre1 + tamaño2 + nombre2 + tamaño3 + relacion
      string new_char = "C" + size1_str + name1 + size2_str + name2 + size3_str + relation;
      strcpy(data, new_char.c_str());      

   } else if (msg[0] == 'R'){
      // Buscamos segundo espacio
      size_t pos = msg.find(" ", 2);
      // Obtenemos el primer nombre que se encuentra antes del segundo espacio
      string name1 = msg.substr(2, pos-2);
      // Obtenemos la recurrencia que se encuentra despues del segundo espacio
      string recurrence = msg.substr(pos+1);
      // Obtenemos el tamaño de la cadena
      int size1 = name1.length();
      int size2 = recurrence.length();
      // Formateamos el tamaño de la cadena
      string size1_str = to_string_parse(size1);
      string size2_str = to_string_parse(size2);
      // Imprimimos el nombre y la recurrencia
      cout << "Name: " << name1 << endl;
      cout << "Recurrence: " << recurrence << endl;
      // Guardamos en el char "R" + tamaño1 + nombre1 + tamaño2 + recurrencia
      string new_char = "R" + size1_str + name1 + size2_str + recurrence;
      strcpy(data, new_char.c_str());
   } else if (msg[0] == 'U'){
      // Buscamos segundo espacio
      size_t pos2 = msg.find(" ", 2);
      // Buscamos el tercer espacio
      size_t pos3 = msg.find(" ", pos2+1);
      // Buscamos el cuarto espacio
      size_t pos4 = msg.find(" ", pos3+1);
      // Buscamos el quinto espacio
      size_t pos5 = msg.find(" ", pos4+1);
      // Buscamos el sexto espacio
      size_t pos6 = msg.find(" ", pos5+1);
      // Obtenemos el primer nombre que se encuentra antes del segundo espacio
      string name1 = msg.substr(2, pos2-2);
      // Obtenemos el segundo nombre que se encuentra despues del segundo espacio y antes del tercer espacio
      string name2 = msg.substr(pos2+1, pos3-pos2-1);
      // Obtenemos la relacion que se encuentra despues del tercer espacio y antes del cuarto espacio
      string relation = msg.substr(pos3+1, pos4-pos3-1);
      // Obtenemos el nuevo nombre1 que se encuentra despues del cuarto espacio y antes del quinto espacio
      string newName1 = msg.substr(pos4+1, pos5-pos4-1);
      // Obtenemos la nuevo nombre2 que se encuentra despues del quinto espacio y antes del sexto espacio
      string newName2 = msg.substr(pos5+1, pos6-pos5-1);
      // Obtenemos la nueva relacion que se encuentra despues del sexto espacio
      string newRelation = msg.substr(pos6+1);
      // Obtenemos el tamaño de la cadena
      int size1 = name1.length();
      int size2 = name2.length();
      int size3 = relation.length();
      int size4 = newName1.length();
      int size5 = newName2.length();
      int size6 = newRelation.length();
      // Formateamos el tamaño de la cadena
      string size1_str = to_string_parse(size1);
      string size2_str = to_string_parse(size2);
      string size3_str = to_string_parse(size3);
      string size4_str = to_string_parse(size4);
      string size5_str = to_string_parse(size5);
      string size6_str = to_string_parse(size6);
      // Imprimimos los nombres y la relacion
      cout << "Name 1: " << name1 << endl;
      cout << "Name 2: " << name2 << endl;
      cout << "Relation: " << relation << endl;
      cout << "New Name 1: " << newName1 << endl;
      cout << "New Name 2: " << newName2 << endl;
      cout << "New Relation: " << newRelation << endl;
      // Guardamos en el char "U" + tamaño1 + nombre1 + tamaño2 + nombre2 + tamaño3 + relacion + tamaño4 + newName1 + tamaño5 + newName2 + tamaño6 + newRelation
      string new_char = "U" + size1_str + name1 + size2_str + name2 + size3_str + relation + size4_str + newName1 + size5_str + newName2 + size6_str + newRelation;
      strcpy(data, new_char.c_str());
   } else if (msg[0] == 'D'){
      // Buscamos segundo espacio
      size_t pos2 = msg.find(" ", 2);
      // Si hay tercer espacio
      if (pos2 != string::npos){
         //Buscamos el tercer espacio
         size_t pos3 = msg.find(" ", pos2+1);
         // Obtenemos el primer nombre que se encuentra antes del segundo espacio
         string name1 = msg.substr(2, pos2-2);
         // Obtenemos el segundo nombre que se encuentra despues del segundo espacio y antes del tercer espacio
         string name2 = msg.substr(pos2+1, pos3-pos2-1);
         // Obtenemos la relacion que se encuentra despues del tercer espacio
         string relation = msg.substr(pos3+1);
         // Obtenemos el tamaño de la cadena
         int size1 = name1.length();
         int size2 = name2.length();
         int size3 = relation.length();
         // Formateamos el tamaño de la cadena
         string size1_str = to_string_parse(size1);
         string size2_str = to_string_parse(size2);
         string size3_str = to_string_parse(size3);
         // Imprimimos los nombres y la relacion
         cout << "Name 1: " << name1 << endl;
         cout << "Name 2: " << name2 << endl;
         cout << "Relation: " << relation << endl;
         // Guardamos en el char "D" + tamaño1 + nombre1 + tamaño2 + nombre2 + tamaño3 + relacion
         string new_char = "D" + size1_str + name1 + size2_str + name2 + size3_str + relation;
         strcpy(data, new_char.c_str());
      } else {
         // Obtenemos el primer nombre que se encuentra antes del segundo espacio
         string name1 = msg.substr(2);
         // Obtenemos la opcion que se encuentra despues del segundo espacio
         string option = msg.substr(pos2+1);
         // Obtenemos el tamaño de la cadena
         int size1 = name1.length();
         int size2 = option.length();
         // Formateamos el tamaño de la cadena
         string size1_str = to_string_parse(size1);
         string size2_str = to_string_parse(size2);
         // Imprimimos el nombre y la opcion
         cout << "Name: " << name1 << endl;
         cout << "Option: " << option << endl;
         // Guardamos en el char "D" + tamaño1 + nombre1 + tamaño2 + opcion
         string new_char = "D" + size1_str + name1 + size2_str + option;
         strcpy(data, new_char.c_str());
      }

   }
   
}

void mainThread(){

   string msg;
   char send_data[packSize];
   bzero(send_data, packSize);

   while (1) {

      printf("\n$:");
      cin.getline(send_data, packSize);
      parsing(send_data);

      // cout << "Send data: " << send_data << endl;

      if ((strcmp(send_data , "q") == 0) || strcmp(send_data , "Q") == 0) break;

      sendMsg(server_addr, send_data);

      if (send_data[0] == 'R'){
         cout << "ESPERANDO DATA: " << endl;
         msg = reciveMsg(server_addr);
         // cout << msg << endl;
         cout << msg.substr(5, msg.size() - 5) << endl;
      }

   }

}


int main(){

   struct hostent *host = (struct hostent *) gethostbyname((char *)"127.0.0.1");

   if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
      perror("socket");
      exit(1);
   }

   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(5000);
   server_addr.sin_addr = *((struct in_addr *)host->h_addr);
   bzero(&(server_addr.sin_zero),8);

   mainThread();

}