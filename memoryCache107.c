#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
 #define RESOURCE_SERVER_PORT 1071 // Change this!
 #define BUF_SIZE 256

#define SIZE 32
  // We make this a global so that we can refer to it in our signal handler
 static pthread_mutex_t methodLock = PTHREAD_MUTEX_INITIALIZER;
 typedef struct{
 int size;//returns size in bytes
 char sizeStr[SIZE]; //size stored as a string
 char contents[BUF_SIZE]; //the actual contents of the file
 char filename[SIZE]; //the name of the file
} file;

file cache[8];

void initializeCache(){
 for(int i = 0; i<8; i++){
  cache[i].size = -1;
 }
}

void printCache(){
 for(int i = 0; i<8; i++)
 {
  if(cache[i].size!=-1){
   printf("%s", cache[i].filename);
  }
 }
}


void store(char filename[SIZE], int size, char* contents, char* sizeStr){
 file F;
 F.size = size;

strcpy(F.contents, contents);
strcpy(F.sizeStr, sizeStr);
 strcpy(F.filename, filename);
 cache[(strlen(filename))%8] = F;
}

char* load(char filename[SIZE]){
 if(cache[(strlen(filename))%8].size!=-1){
  //glue size and contents
  
   char fileSize[SIZE];
   file f = cache[(strlen(filename))%8];
   int size = f.size;
   snprintf(fileSize,5,"%d", size);
   char* contents = malloc(size+ 1+ strlen(fileSize));
   char* colon = malloc(1);
   strcpy(colon, ":");
   strcat(contents, fileSize);
   strcat(contents, colon);
   free(colon);
   strcat(contents, cache[(strlen(filename))%8].contents);
   return contents;
 }
 //return char* of 0:""
 char* contents = malloc(2+1);
 strcpy(contents, "0:");
 return contents;
}

void rm (char filename[SIZE]){
 if(cache[(strlen(filename)%8)].size!=-1){
  cache[((strlen(filename))%8)].size = -1;
 }
}

void parseStore(char receiveLine[BUF_SIZE], char functionName[SIZE], int key){
 char fileName[SIZE];
 for(int i = key; i<BUF_SIZE; i++){
  if(receiveLine[i]!=' ')
  {
   fileName[i-key] = receiveLine[i];
  }
  else{
   key = i+1;
   break;
  }
 }

 char fileSize[SIZE];
 for(int i = key; i<BUF_SIZE; i++){
  if(receiveLine[i]!= ':'){
   fileSize[i-key] = receiveLine[i];
  }

  else{
   key = i+1;
   break;
  }
 }
 
 int size = atoi(fileSize); 
 char contents[BUF_SIZE];
 for(int i = key; i<key+size; i++){
  contents[i-key] = receiveLine[i];
 }
//mutex
 pthread_mutex_lock(&methodLock);
 store(fileName, size, contents, fileSize);
 pthread_mutex_unlock(&methodLock);
}
//char* and return from null
 char* parseOthers(char receiveLine[BUF_SIZE], char functionName[SIZE], int key, int bytesReadFromClient){
  char filename[SIZE];
  char* start = &receiveLine[key];
  strncpy(filename, start, bytesReadFromClient-key);
  char* fileName;
  fileName = filename;
  return fileName;
}
 int serverSocket;
 
 void closeConnection() {
      printf("\nClosing Connection with file descriptor: %d \n", serverSocket);
      close(serverSocket);
      exit(1);
  }
 
  // Create a separat emethod for
 
  void * processClientRequest(void * request) {

	int connectionToClient = *(int *)request;
      
      char receiveLine[BUF_SIZE];
      char sendLine[BUF_SIZE];
 
      int bytesReadFromClient = 0;
      // Read the request that the client has
     while ( (bytesReadFromClient = read(connectionToClient, receiveLine, BUF_SIZE)) > 0) {
          receiveLine[bytesReadFromClient] = 0;
          int key = 0;
           char functionName[SIZE];
          for(int i = 0; i<SIZE;i++){
            if(receiveLine[i]!=' '){
             functionName[i] = receiveLine[i];
            }
            else{
	     key = i+1;
             break;
            }
           }
	  if(strncmp(functionName, "store", 5 )==0){

	   parseStore(receiveLine, functionName, key);
	  }
	  else{
	   char* filename;
	   filename  = parseOthers(receiveLine, functionName, key, bytesReadFromClient);
	   if(strncmp(functionName, "rm", 2)==0){
	    pthread_mutex_lock(&methodLock);
	    rm(filename);
	    pthread_mutex_unlock(&methodLock);
	   }
	   else{
	    char* contents;
	    pthread_mutex_lock(&methodLock);
	    contents = load(filename);
	    pthread_mutex_unlock(&methodLock);
            strcpy(sendLine, contents);
	    free(contents);
	   }
	  }
 
         // Print text out to buffer, and then write it to client (connfd)
 
          write(connectionToClient, sendLine, strlen(sendLine));
          // Zero out the receive line so we do not get artifacts from before
          bzero(&receiveLine, sizeof(receiveLine));
          close(connectionToClient);
      }
  }
 
  int main(int argc, char *argv[]) {
      initializeCache();      
         
      
      int connectionToClient, bytesReadFromClient;
 
      // Create a server socket
      serverSocket = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in serverAddress;
      bzero(&serverAddress, sizeof(serverAddress));
      serverAddress.sin_family      = AF_INET;
 
      // INADDR_ANY means we will listen to any address
      // htonl and htons converts address/ports to network formats
      serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
      serverAddress.sin_port        = htons(RESOURCE_SERVER_PORT);
 
      // Bind to port
      if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
          printf("Unable to bind to port just yet, perhaps the connection has to be timed out\n");
          exit(-1);
      }
 
      // Before we listen, register for Ctrl+C being sent so we can close our connection
      struct sigaction sigIntHandler;
      sigIntHandler.sa_handler = closeConnection;
      sigIntHandler.sa_flags = 0;
 
      sigaction(SIGINT, &sigIntHandler, NULL);
 
      // Listen and queue up to 10 connections
      listen(serverSocket, 10);
 
      while (1) {
          /*
           Accept connection (this is blocking)
           2nd parameter you can specify connection
           3rd parameter is for socket length
 1          */
          // Kick off a thread to process request
         connectionToClient = accept(serverSocket, (struct sockaddr *) NULL, NULL);	   
	 pthread_t someThread;
          pthread_create(&someThread, NULL, processClientRequest, (void *)&connectionToClient);
      }
  }

