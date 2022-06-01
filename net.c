#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  for(int i=0;i<len;i=len-i){ //if there's anything didnt finish reading the loop continues
    if ( read( fd, buf+i, len-i) != len ) { //go into the network read function and check if the read length is equal or not
      printf( "Error reading network data [%s]\n", strerror(errno) );
      return( false );
    }
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  for(int i=0;i<len;i=len-i){//if there's anything didnt finish reading the loop continues
      if ( write( fd, buf, len) != len ) {//go into the network write function and check if the read length is equal or not
        printf( "Error writing network data [%s]\n", strerror(errno) );
        return( false );
      }
  }
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {

  uint8_t header[HEADER_LEN]; //i created a header array buf that have the length of the entire header for later on to pass
                              //it into read

  if(nread(sd,HEADER_LEN,header)==false){ //if the header read was did not succeed then we return false
    return false; //return false if
  }
  //int length=header[0]+header[1];
  uint16_t length;  //basic length for the entire protocol
  memcpy(&length,header,2); //after read, copy the header size information into the length 
  memcpy(op,&header[2],4); //copy the opcode from the header i just read
  memcpy(ret,&header[6],2); //copy the return value from the header i just got
  length=ntohs(length); //using ntohs to get the actual length to check if the length 


  if(length == HEADER_LEN + JBOD_BLOCK_SIZE){ //if the size of the gotten header is equal to the size of entire protocol then we 
    uint8_t temp[JBOD_BLOCK_SIZE];            //should get the block to read into the block we sent from mdadm.c
    if(nread(sd,256,temp)==false){     //if nread failed then return false
      return false;
      }
      memcpy(block,temp,JBOD_BLOCK_SIZE); //copy the memory from temp to block we want
  }

  return true;
}



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  int opcode=op>>26;  //decoding the opcode to what we can read
  uint8_t pocket[HEADER_LEN+JBOD_BLOCK_SIZE]; //create the entire packet protocol of 263

  uint16_t byte, pocketl; //the header length variables
  uint32_t netop=htonl(op); //the net readable op code
  
  memcpy(&pocket[2],&netop,4);  //putting opcode into the packet

  if(opcode==JBOD_WRITE_BLOCK){ //if the op command is to write then
    byte=HEADER_LEN+JBOD_BLOCK_SIZE;  //give the byte its size wich is 8+256
    pocketl=htons(byte);  //the network readable length
    
    memcpy(&pocket[8],block,256); //giving the packet its block data we want to write 
  }else{  //if the command is not write that means we only have to ssend the basic header op code to the server
    byte=HEADER_LEN;    //setting byte length to header length
    pocketl=htons(byte);  //network transforming it
    
  }

  memcpy(pocket,&pocketl,2);  //transfer the data of length into packet

  if(nwrite(sd,byte,pocket)==false){ //if nwrite failed then the return false
    return false;
  }
  
  return true;
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  struct  sockaddr_in caddr;  //address informations struct
  caddr.sin_family = AF_INET; //giving it its address family
  caddr.sin_port = htons(port); //asign the struct its won port
  if ( inet_aton(ip, &caddr.sin_addr) == 0 ) {  //giving its own ip address
    return( false);
  }

  if(socket(AF_INET,SOCK_STREAM,0)==-1){  //creating a socket to use
    printf("error when creating socket [%s]\n",strerror(errno));
    return false;
  }
  cli_sd=socket(AF_INET,SOCK_STREAM,0);   //setting client socket default to socket 

  if ( connect(cli_sd, (const struct sockaddr *)&caddr, //connect to the server
    sizeof(caddr)) == -1 ) {  
    return( false );
  } 
  return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);  //close the client
  cli_sd=-1;  //set the client sd to -1
}

/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint16_t rv;
  //HEADER_LEN header=plen+op+rv;
  if(send_packet(cli_sd,op,block)==false){  //check if the send packet function failed 
    return -1;
  }
  if(recv_packet(cli_sd,&op,&rv,block)==false){//check if the recv_packet function failed 
    return -1;
  }
  if(rv==-1){ //check if the return value is not valid
    return -1;
  }
  return 0;
}
