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

using namespace std;

// Variables Globales
int sock;
socklen_t addr_len;
struct sockaddr_in server_addr;
const int packSize = 1024;
int sequence_number = 0;
int pkt_number = 0;

// Funcion que formatea un numero menor a 10
string numberFormat (int num){
   // Si el numero es menor a 10, le agregamos un 0 al inicio
   if (num < 10) return "0" + to_string(num);
   else return to_string(num);
}

string to_string_parse(int num, int size){
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
   cout << msg.size() << " - " << msg << endl;
   cout << msg.substr(msg.size() - 11, 10) << endl;
   return 1;
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

   return aux;
   
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
      string sequence_number_str = numberFormat(sequence_number);
      // Obtenemos el tamaño de la cadena
      int size1 = name1.length();
      int size2 = name2.length();
      int size3 = relation.length();
      // Formateamos el tamaño de la cadena
      string size1_str = numberFormat(size1);
      string size2_str = numberFormat(size2);
      string size3_str = numberFormat(size3);
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
      string size1_str = numberFormat(size1);
      string size2_str = numberFormat(size2);
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
      string size1_str = numberFormat(size1);
      string size2_str = numberFormat(size2);
      string size3_str = numberFormat(size3);
      string size4_str = numberFormat(size4);
      string size5_str = numberFormat(size5);
      string size6_str = numberFormat(size6);
      // Imprimimos los nombres y la relacion
      cout << "Name 1: " << name1 << endl;
      cout << "Name 2: " << name2 << endl;
      cout << "Relation: " << relation << endl;
      cout << "New Name 1: " << newName1 << endl;
      cout << "New Name 2: " << newName2 << endl;
      cout << "New Relation: " << newRelation << endl;
      // Guardamos en el char "U" + tamaño1 + nombre1 + tamaño2 + nombre2 + tamaño3 + relacion + tamaño4 + newName1 + tamaño5 + newName2 + tamaño6 + newRelation
      string new_char = "U" + size1_str + name1 + size2_str + name2 + size3_str + relation + size4_str + newName1 + size5_str + newName2 + size6_str + newRelation;
      new_char = new_char + to_string_parse(checksum(new_char), 10) + '\0';
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
         string size1_str = numberFormat(size1);
         string size2_str = numberFormat(size2);
         string size3_str = numberFormat(size3);
         // Imprimimos los nombres y la relacion
         cout << "Name 1: " << name1 << endl;
         cout << "Name 2: " << name2 << endl;
         cout << "Relation: " << relation << endl;
         // Guardamos en el char "D" + tamaño1 + nombre1 + tamaño2 + nombre2 + tamaño3 + relacion
         string new_char = "D" + size1_str + name1 + size2_str + name2 + size3_str + relation;
         new_char = new_char + to_string_parse(checksum(new_char), 10) + '\0';

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
         string size1_str = numberFormat(size1);
         string size2_str = numberFormat(size2);
         // Imprimimos el nombre y la opcion
         cout << "Name: " << name1 << endl;
         cout << "Option: " << option << endl;
         // Guardamos en el char "D" + tamaño1 + nombre1 + tamaño2 + opcion
         string new_char = "D" + size1_str + name1 + size2_str + option;
         new_char = new_char + to_string_parse(checksum(new_char), 10) + '\0';

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