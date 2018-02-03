#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>   
#include <sys/stat.h>
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
char REQ_BUFFER[1024];

void error(char *msg) {
    perror(msg);
    exit(ERROR);
}

int main(int argc, char *argv[]) {

    if (argc != 3) { //Only specify port number and [0 or a pathname]  as an arguement
        fprintf(stderr,"Usage: ./server [port number] [0 or pathname]\n");
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
    memset(REQ_BUFFER, 0, 1024);
    numReqBytes = read(connectionfd, REQ_BUFFER, 1023);
    if (numReqBytes < 0) {
        error("ERROR reading from socket");
    }
    printf("Client Request\n======================================\n%s\n", REQ_BUFFER);
    printf("numReqBytes=%d\n",numReqBytes);//debugging purposes

    //Date: current timestamp
    //Server: what is our server name?

    if(atoi(argv[2])==1) { 
        write(connectionfd, "HTTP/1.1 200 OK\r\n",17);
        write(connectionfd,"Date: bougieeO'clock\r\n",22);//write current date header
        write(connectionfd,"Server: BestWestern\r\n",21);//write server name header
        write(connectionfd,"Content-Type: text/html\r\n",25);//content-type
        write(connectionfd,"Content-Length: 47\r\n",20);//content-length
        write(connectionfd,"Connection: Close\r\n",19);//connection header
        write(connectionfd,"\r\n<html><body><h1>Hello World!</h1></body></html>",49);//data

        close(connectionfd);
        close(sockfd);
        exit(FINISH);
    }
/*
    //get_object_pathname();//extract requested object name (even if spaces in name, not include initial '/')

    // if(!valid_object()){//404 Not Found, close connection and exit
    //     write(connectionfd, "HTTP/1.1 404 NOT FOUND\r\n",24);
    //     //write current date header
    //     //write server name header
    //     write(connectionfd,"Content-Type: text/html\r\n",25);//content-type
    //     write(connectionfd,"Content-Length: 48\r\n",20);//content-length
    //     write(connectionfd,"Connection: Close\r\n",19);//connection header
    //     write(connectionfd,"\r\n<html><body><h1>404 Not Found</h1></body></html>",50);//data

    //     close(connectionfd);
    //     close(sockfd);
    //     exit(FINISH);
    // }

    if (empty pathname){ 
        if no object is specified like 'localhost:8080' we can generate a basic welcome page?
    }

    // char modified_date[256];
    // char cont_type[256];
    // char cont_len[256];
    
    snprintf(modified_date,256,"%s",ctime(&objStats.st_mtime));
    snprintf(cont_len,256,"%lld",(long long) sb.st_size);
    // //snprintf(cont_type,256,"%s",);//content type depends on extension after '.' what if no dot?
*/
    else{
        if (stat(argv[2],&objStats)==-1){//object_name path not found
            printf("file not found");//debugging purposes
            return ERROR;
        }
        char modified_date[256];
        char cont_len[256];
    
        snprintf(modified_date,256,"%s",ctime(&objStats.st_mtime));
        snprintf(cont_len,256,"%lld",(long long) objStats.st_size);
        
        //valid non-empty object request
        write(connectionfd, "HTTP/1.1 200 OK\r\n",17);
        write(connectionfd,"Date: bougieeO'clock\r\n",22);//write current date header
        write(connectionfd,"Server: BestWestern\r\n",21);//write server name header
        
        write(connectionfd,"Last-Modified: ",15);
        write(connectionfd,modified_date,sizeof(modified_date));
        write(connectionfd,"\r\n",2);
        
        write(connectionfd,"Content-Type: text/html\r\n",25);        
        write(connectionfd,"Content-Length: ",16);
        write(connectionfd,cont_len,sizeof(cont_len));
        write(connectionfd,"\r\n",2);

        write(connectionfd,"Connection: Close\r\n\r\n",21);//connection header
        
        //send data to client
        FILE *file = NULL;
        char buffer[1024];
        size_t bytesRead = 0;

        file = fopen(argv[2], "r");   
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
}
