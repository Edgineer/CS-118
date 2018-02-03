#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>   
#include <sys/socket.h>
#include <sys/stat.h>  
#include <sys/wait.h>
#include <netinet/in.h>  
#include <unistd.h>
#include <signal.h>
#include <time.h>

//Defined Constants
#define ERROR 1
#define FINISH 0

//Global Vars/Buffers
struct stat objStats;
char REQ_BUFFER[1024];

char REQ_OBJECT[256];
char DATE[64] = "Date: ";
char SERVER[29] = "Server: Microsoft-IIS/10.0\r\n\0";
char HTTP_V1[9] = "HTTP/1.0\0";   // 0
char HTTP_V11[9] = "HTTP/1.1\0";  // 1
char HTTP_V2[7] = "HTTP/2\0";     // 2
int HTTP_VERSION = 3;             // BAD

char CONT_HTML[10] = "text/html\0"; // 0
char CONT_JPEG[11] = "image/jpeg\0";// 1 
char CONT_GIF[10] = "image/gif\0";  // 2
char CONT_BIN[25] = "application/octet-stream\0"; // 3
int CONT_VERSION = 4;               // BAD
int REQLINE_SIZE;

void error(char *msg) {
    perror(msg);
    exit(ERROR);
}

void get_http_version(){ //HTTP_VERSION=3 initially BAD request
  //Use REQ_BUFFER to fill HTTP_V
  if (strlen(REQ_BUFFER) < 14)
    return;
  if (REQ_BUFFER[0] != 'G' || REQ_BUFFER[1] != 'E' || REQ_BUFFER[2] != 'T' || REQ_BUFFER[3] != ' ')//verify GET
    return;
  
  int i=4;
  while(i+1 < strlen(REQ_BUFFER)){
    if(REQ_BUFFER[i] == '\r' && REQ_BUFFER[i+1] == '\n'){
      break;
    }
    else
      i++;
  }
  REQLINE_SIZE = i; //second to last index
  printf("LOG: RELINE_SIZE is %d\n", REQLINE_SIZE);

  //printf("LOG: before v1\n");
  char v1[9];
  strncpy(v1, REQ_BUFFER + (REQLINE_SIZE - 8), 8);
  v1[8] = '\0';
  int ret = strcmp(v1, HTTP_V1);
  if (!ret)
    HTTP_VERSION = 0;

  //printf("LOG: before v11\n");
  char v11[9];
  strncpy(v11, REQ_BUFFER + (REQLINE_SIZE - 8), 8);
  v11[8] = '\0';
  ret = strcmp(v11, HTTP_V11);
  if (!ret)
    HTTP_VERSION = 1;

  //printf("LOG: before v2\n");
  char v2[7];
  strncpy(v2, REQ_BUFFER + (REQLINE_SIZE - 6), 6);
  v2[6] = '\0';
  ret = strcmp(v2, HTTP_V2);
  if (!ret)
    HTTP_VERSION = 2;
}

void get_object_pathname(){
  if (HTTP_VERSION == 0 || HTTP_VERSION == 1)
    strncpy(REQ_OBJECT, REQ_BUFFER + 5, (REQLINE_SIZE - 14));    // 5 (for "GET /") + 9 (for " HTTP/1.0" or " HTTP/1.1")
  else if (HTTP_VERSION == 2)
    strncpy(REQ_OBJECT, REQ_BUFFER + 5, (REQLINE_SIZE - 12));    // 5 (for "GET /") + 7 (for " HTTP/2")
}

void get_date(){
  time_t raw;
  time(&raw);
  struct tm *timeAtts = localtime(&raw);
  char formattedTime[32];
  strftime(formattedTime, 32, "%a, %d %b %Y %T %Z", timeAtts);
  strcat(DATE, formattedTime);
  strcat(DATE, "\r\n");
}

char *get_filename_ext(char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void get_cont_version(){
  char* extension = get_filename_ext(&REQ_OBJECT);
  char ext_htm[3] = "htm";
  char ext_html[4] = "html";
  char ext_jpg[3] = "jpg";
  char ext_jpeg[4] = "jpeg";
  char ext_gif[3] = "gif";
    
  int i = 0;
  while(strlen(extension) > i)
    extension[i] = tolower(extension[i]);
  
  if (!strcmp(extension, ext_htm) || !strcmp(extension, ext_html)) {
    CONT_VERSION = 0;
    return;
  }
  if (!strcmp(extension, ext_jpg) || !strcmp(extension, ext_jpeg)) {
    CONT_VERSION = 1;
    return;
  }
  if (!strcmp(extension, ext_gif)) {
    CONT_VERSION = 2;
    return;
  }
  if (!strlen(extension))
    CONT_VERSION = 3;
    return;
}

void remove_spaces(){
    int numSpaces=0,i=0;
    while(i+2<sizeof(TEMP_REQ_OBJECT)){
        if (TEMP_REQ_OBJECT[i]=='%' && TEMP_REQ_OBJECT[i+1]=='2' && TEMP_REQ_OBJECT[i+2]=='0') {
            numSpaces++;
            i+=3;    
        }
        else{i++;}
    }
    if (numSpaces==0){
       REQ_OBJECT = TEMP_REQ_OBJECT;
       return;
    }
    else{
        int pathSize = sizeof(TEMP_REQ_OBJECT) - 3(numSpaces);
        char OF_OBJECT[pathSize];
        int it=0,j=0;
        while(it+2<sizeof(TEMP_REQ_OBJECT)){
            if (TEMP_REQ_OBJECT[i]=='%' && TEMP_REQ_OBJECT[i+1]=='2' && TEMP_REQ_OBJECT[i+2]=='0') {
                OF_OBJECT[j] = ' ';
                it+=3;
                j++;
            }
            else{
                OF_OBJECT[j] = TEMP_REQ_OBJECT[it];
                it++;
                j++;
            }
        }
    }
}

int main(int argc, char *argv[]) {

    if (argc != 2) { //Only specify port number as an arguement
        fprintf(stderr,"Usage: ./server [port number]\n");
        exit(ERROR);
    }

    int sockfd, connectionfd, portno; 
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len;

    //validate port number
    portno = atoi(argv[1]);
    if (portno<1024 || portno>65535) {
        fprintf(stderr, "Please enter a port number between [1024,65535].\n");
        exit(ERROR);
    }

    //create the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR: Unable to open new socket");
    }

    // Reset server_addr memory and fill 
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR: Unable to perform binding");
    }

    if (listen(sockfd, 5) < 0) {
        error("ERROR on listen, while expecting connection");
    }

    //accept connections
    connectionfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_addr_len);
    if (connectionfd < 0){
        close(sockfd);
        error("ERROR: Unable to accept client connection");
    }

    //REQ_BUFFER holds client's request message
    //read the client's message and output it to the console
    memset(REQ_BUFFER, 0, 1024);
    if(read(connectionfd, REQ_BUFFER, 1023)<0){
        close(connectionfd);
        close(sockfd);
        error("ERROR reading from socket");
    }
    printf("Client Request\n======================================\n%s\n", REQ_BUFFER);

    get_date(); //Date: current timestamp
    //Server in SERVER

    get_http_version(); //fills HTTP_VERSION
    if (HTTP_VERSION==3){
        write(connectionfd, "HTTP/1.1 400 BAD REQUEST\r\n",26);
        write(connectionfd,DATE,sizeof(DATE));//write current date header
        write(connectionfd,SERVER,sizeof(SERVER));//write server name header
        write(connectionfd,"Content-Type: text/html\r\n",25);//content-type
        write(connectionfd,"Content-Length: 50\r\n",20);//content-length
        write(connectionfd,"Connection: Close\r\n",19);//connection header
        write(connectionfd,"\r\n<html><body><h1>400 Bad Request</h1></body></html>",52);//data
        close(connectionfd);
        close(sockfd);
        exit(FINISH);
    }

    get_object_pathname();
    // remove_spaces();

    if (!strlen(REQ_OBJECT)){ // homepagw
        if (HTTP_VERSION==0) {write(connectionfd,HTTP_V1,sizeof(HTTP_V1));}
        else if (HTTP_VERSION==1) {write(connectionfd,HTTP_V11,sizeof(HTTP_V11));}
        else if (HTTP_VERSION==2) {write(connectionfd,HTTP_V2,sizeof(HTTP_V2));}
         write(connectionfd, " 200 OK\r\n",9);
        write(connectionfd,DATE,sizeof(DATE));//write current date header
        write(connectionfd,SERVER,sizeof(SERVER));//write server name header
        write(connectionfd,"Content-Type: text/html\r\n",25);//content-type
        write(connectionfd,"Content-Length: 90\r\n",20);//content-length
        write(connectionfd,"Connection: Close\r\n",19);//connection header
        write(connectionfd,"\r\n<html><body><h1>Welcome! :) Please restart server to make new requests!</h1></body></html>",92);//data
        close(connectionfd);
        close(sockfd);
        exit(FINISH);        
    }

    if (stat(REQ_OBJECT,&objStats)==-1){//object_name path not found, 404 Not Found, close connection and exit
        printf("ERROR: File not found");//debugging purposes
        if (HTTP_VERSION==0) {write(connectionfd,HTTP_V1,sizeof(HTTP_V1));}
        else if (HTTP_VERSION==1) {write(connectionfd,HTTP_V11,sizeof(HTTP_V11));}
        else if (HTTP_VERSION==2) {write(connectionfd,HTTP_V2,sizeof(HTTP_V2));}
        write(connectionfd, " 404 NOT FOUND\r\n",16);
        write(connectionfd,DATE,sizeof(DATE));//write current date header
        write(connectionfd,SERVER,sizeof(SERVER));//write server name header
        write(connectionfd,"Content-Type: text/html\r\n",25);//content-type
        write(connectionfd,"Content-Length: 48\r\n",20);//content-length
        write(connectionfd,"Connection: Close\r\n",19);//connection header
        write(connectionfd,"\r\n<html><body><h1>404 Not Found</h1></body></html>",50);//data
        close(connectionfd);
        close(sockfd);
        exit(FINISH);
    }
    // Fill buffers with Last-Modified Date and Content-Length of requested object
    char modified_date[256];
    char cont_len[256];
    snprintf(modified_date,256,"%s",ctime(&objStats.st_mtime));
    snprintf(cont_len,256,"%lld",(long long) objStats.st_size);
       
    // Finally, make valid non-empty object request
    if (HTTP_VERSION==0) {write(connectionfd,HTTP_V1,sizeof(HTTP_V1));}
    else if (HTTP_VERSION==1) {write(connectionfd,HTTP_V11,sizeof(HTTP_V11));}
    else if (HTTP_VERSION==2) {write(connectionfd,HTTP_V2,sizeof(HTTP_V2));}
    write(connectionfd, " 200 OK\r\n",9);
    write(connectionfd,DATE,sizeof(DATE));//write current date header
    write(connectionfd,SERVER,sizeof(SERVER));//write server name header
            
    write(connectionfd,"Last-Modified: ",15);
    write(connectionfd,modified_date,sizeof(modified_date));
    write(connectionfd,"\r\n",2);
    
    write(connectionfd,"Content-Type: ",12);
    if (CONT_VERSION==0) {write(connectionfd,CONT_HTML,sizeof(CONT_HTML));}
    else if (CONT_VERSION==1) {write(connectionfd,CONT_JPEG,sizeof(CONT_JPEG));}
    else if (CONT_VERSION==2) {write(connectionfd,CONT_GIF,sizeof(CONT_GIF));}
    else if (CONT_VERSION==3 || CONT_VERSION==4) {write(connectionfd,CONT_BIN,sizeof(CONT_BIN));}
    write(connectionfd,"\r\n",2);


    write(connectionfd,"Content-Length: ",16);
    write(connectionfd,cont_len,sizeof(cont_len));
    write(connectionfd,"\r\n",2);
    write(connectionfd,"Connection: Close\r\n\r\n",21);
    
    //send data to client
    FILE *file = NULL;
    char buffer[1024];
    size_t bytesRead = 0;

    file = fopen(REQ_OBJECT, "r");   
    if (file != NULL) {
        // read up to sizeof(buffer) bytes
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            write(connectionfd,buffer,sizeof(buffer));
        }
        fclose(file);
    }
    close(connectionfd);  // close connection
    close(sockfd);       // close socket
    exit(FINISH);
}