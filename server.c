#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>   
#include <sys/socket.h>  
#include <sys/wait.h>
#include <netinet/in.h>  
#include <unistd.h>
#include <signal.h>
#include <time.h>

//Defined Constants
#define ERROR 1
#define FINISH 0

//Global Vars/Buffers
int numReqBytes;
struct stat objStats;
char REQ_BUFFER[256];
char REQ_OBJECT[256];
char HTTP_V[8];

void error(char *msg) {
    perror(msg);
    exit(ERROR);
}

bool valid_http_request(){
    //check that GET and HTTP version exist
    if (numReqBytes<14){ //smallest valid request [GET[sp]HTTP/1.1\n\r]
        return false;
    }
    if (REQ_BUFFER[0] != 'G' && REQ_BUFFER[1] != 'E' && REQ_BUFFER[2] != 'T'){ //verify GET
        return false;
    }
    int i=3; //verify HTTP/1.1
    while(REQ_BUFFER[i] != '\n')
        i++;
    if (REQ_BUFFER[i-8] != 'H' && REQ_BUFFER[i-7] != 'T' && REQ_BUFFER[i-6] != 'T' && 
        REQ_BUFFER[i-5] != 'P' && REQ_BUFFER[i-4] != '/' &&){
        return false;
    }
    return true;
}

void get_http_version(){ //easier to assume 1.1
    //Use REQ_BUFFER and numReqBytes to fill HTTP_V
}

void get_object_pathname(){ //what if the request is empty, perhaps is did not specify an object?
    //Use REQ_BUFFER and numReqBytes to fill REQ_OBJECT
}

bool valid_object(){
    if (stat(REQ_OBJECT,&objStats)==-1)//object_name path not found
        return false;
    else
        return true;
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
     error("ERROR: Unable to accept client connection");
    }

    //REQ_BUFFER holds client's request message
    //read the client's message and output it to the console
    memset(REQ_BUFFER, 0, 256);
    numReqBytes = read(connectionfd, REQ_BUFFER, 255);
    if (numReqBytes < 0) {
        error("ERROR reading from socket");
    }
    printf("Client Request\n======================================\n%s\n", REQ_BUFFER);
    printf("numReqBytes=%d\n",numReqBytes);//debugging purposes

    //Date: current timestamp
    //Server: what is our server name?

    if(!valid_http_request()) { //400 Bad Request, close connection and exit
        write(connectionfd, "HTTP/1.1 400 BAD REQUEST\r\n",26);
        //write current date header
        //write server name header
        write(connectionfd,"Content-Type: text/html\r\n",25);//content-type
        write(connectionfd,"Content-Length: 50\r\n",20);//content-length
        write(connectionfd,"Connection: Close\r\n",19);//connection header
        write(connectionfd,"\r\n<html><body><h1>400 Bad Request</h1></body></html>",52);//data

        close(connectionfd);
        close(sockfd);
        exit(FINISH);
    }

    get_http_version();//extract http version type Maybe we can just assume HTTP/1.1
    get_object_pathname();//extract requested object name (even if spaces in name, not include initial '/')

    if(!valid_object()){//404 Not Found, close connection and exit
        write(connectionfd, "HTTP/1.1 404 NOT FOUND\r\n",24);
        //write current date header
        //write server name header
        write(connectionfd,"Content-Type: text/html\r\n",25);//content-type
        write(connectionfd,"Content-Length: 48\r\n",20);//content-length
        write(connectionfd,"Connection: Close\r\n",19);//connection header
        write(connectionfd,"\r\n<html><body><h1>404 Not Found</h1></body></html>",50);//data

        close(connectionfd);
        close(sockfd);
        exit(FINISH);
    }

    /*if (empty pathname){ 
        if no object is specified like 'localhost:8080' we can generate a basic welcome page?
    }
    */

    char modified_date[256];
    char cont_type[256];
    char cont_len[256];
    
    snprintf(modified_date,256,"%s",ctime(&objStats.st_mtime));
    snprintf(cont_len,256,"%lld",(long long) sb.st_size);
    //snprintf(cont_type,256,"%s",);//content type depends on extension after '.' what if no dot?

    //valid non-empty object request
    write(connectionfd, "HTTP/1.1 200 OK\r\n",17);
    //write current date header
    //write server header

    write(connectionfd,"Last-Modified: ",15);
    write(connectionfd,modified_date,sizeof(modified_date));
    write(connectionfd,"\r\n",2);

    write(connectionfd,"Content-Type: ",14);
    // :c    
    write(connectionfd,"\r\n",2);

    write(connectionfd,"Content-Length: ",16);
    write(connectionfd,cont_len,sizeof(cont_len));
    write(connectionfd,"\r\n",2);

    write(connectionfd,"Connection: Close\r\n\r\n",21);//connection header
    
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
        fclose(fp);
    }

    close(connectionfd);  // close connection
    close(sockfd);       // close socket
    exit(FINISH);
}