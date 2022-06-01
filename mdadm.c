//CMPSC 311 SP22
//LAB 3
//Name: JunqiPi
//ID:jbp5713
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "cache.h"
#include "net.h"

int gmount=0;
//uint32_t diskpos=0, blkpos=0;
uint32_t Con_Jbod(uint32_t command, uint32_t disk, uint32_t blk){ //constructed a 32 bit command and return to caller
    uint32_t cmd=0;                     //create a 32 bytes jbod controller
    cmd=(command)<<26|(disk)<<22|(blk); //shift the command for 26 bytes, shift diskposition by 22, and leave block alone in the bottom

    return cmd;
}

void Des_Jbod(uint32_t *cmd){           //Deconstructor for jbod function
    uint32_t des=0;                     //32 bits destructor of 0
    *cmd=des;                           //making the recieved jbod command be 0
}

int mdadm_mount(void) {
  if(gmount!=0){    return -1;}         //mark if the driver was mounted already
  gmount=1;                             //mark the driver as mounted
  uint32_t mount=Con_Jbod(JBOD_MOUNT,0,0);  //create a jbod controller by calling my constructor of jbod passing mount op code
  int dt=jbod_client_operation(mount,NULL);        //calling the jbod operation by sending the op and block
  if(dt==-1){       return -1;}         //check if the operation went well
  return 1;
}

int mdadm_unmount(void) {
  if(gmount==1){                        //check if the driver was not mounted 
    gmount=0;
    uint32_t unmount=Con_Jbod(JBOD_UNMOUNT,0,0);    //create a jbod controller by calling my constructor of jbod passing unmount op code
    int dt=jbod_client_operation(unmount,NULL);            //calling the jbod operation by sending the op and block
    if(dt==-1){      return -1;}
    return 1;
  }
  return -1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  if(gmount==0){              return -1;}           //check if it was mounted or not
  if(addr+len>1048576){      return -1;}           //check if it the address is overthe limit
  if(len>1024){               return -1;}           //check if the length is over the limit
  if(buf==NULL&&len>0){       return -1;}           //check if it is passing a impossible length
  else if(buf==NULL&&len==0){ return 0;}            
  
  uint8_t temp[JBOD_BLOCK_SIZE];                    //temporary value to store the block from the jbod read 
  
  int diskpos=addr/JBOD_DISK_SIZE;                      //calculating the disk&block position of the jbod
  int blkpos=(addr-JBOD_DISK_SIZE*(int)(diskpos))/JBOD_NUM_BLOCKS_PER_DISK;
  int left=len;                                     //how much length left after each iteration of jbod extra block space
  int blkadd=addr-diskpos*JBOD_DISK_SIZE-blkpos*JBOD_BLOCK_SIZE; //where to start to read from the block
  int cur=256-blkadd;                               //current block position
  int i=0;                                          //check point if it is the frist time the loop runs
  while(1){
    
  uint32_t seek=Con_Jbod(JBOD_SEEK_TO_DISK,diskpos,blkpos);  //seek to the disposition
  jbod_client_operation(seek,NULL);

  seek=Con_Jbod(JBOD_SEEK_TO_BLOCK,diskpos,blkpos);           //seek to the block position
  jbod_client_operation(seek,NULL);

  //printf("blkadd: %d disk: %d blk : %d ",blkadd,diskpos,blkpos);  
  uint32_t read=Con_Jbod(JBOD_READ_BLOCK,diskpos,blkpos);   //read to this block&disk position
  jbod_client_operation(read,temp);

  if(left==256&&blkadd==0){
   if(cache_enabled()){cache_lookup(diskpos,blkpos,temp);}
    memcpy(buf,&temp[blkadd],len);
    cache_insert(diskpos,blkpos,temp);
    return len;
  }

  if(i!=0&&left<256){                 //if the loop was called and the leftover length is less than the block size
    if(cache_enabled()){cache_lookup(diskpos,blkpos,temp);}
    memcpy(buf+cur,&temp[0],left);
    cache_insert(diskpos,blkpos,temp);
    return len;
  }

  if(left+blkadd<=256){                //if the loop frist time and no more than block size is needed
    if(cache_enabled()){cache_lookup(diskpos,blkpos,temp);}
    memcpy(buf,&temp[blkadd],len);
    cache_insert(diskpos,blkpos,temp);
    return len;
  }

  if(left+blkadd>256){                //if needed to read from the next block
    if(i!=0){                         //and if it was not the frist time bigger than block size
    if(cache_enabled()){cache_lookup(diskpos,blkpos,temp);}
      memcpy(buf+cur,&temp[0],256);   //size not the frist time, block address should always started at 0
      cache_insert(diskpos,blkpos,temp);
      if(blkpos==255){    //check if we needed more disk
        left=left-256;
        cur=cur+256;
        diskpos++;  //if needed more disk, diskposition + 1
        blkpos=0;
        i++;  //mark for iteration
        continue;
      }
      left=left-256;
      cur=cur+256;
      blkpos++;
      continue;
    }
    if(cache_enabled()){cache_lookup(diskpos,blkpos,temp);}
    memcpy(buf,&temp[blkadd],cur);    //copy the memory from the block address place to buf
    if(cache_enabled()){cache_insert(diskpos,blkpos,temp);}
    if(blkpos==255){    //check if we needed more disk
      diskpos++;  //if needed more disk, diskposition + 1
      blkpos=0;
      i++;  //mark for iteration
    }else{
      blkpos++; //if need more block block position +1
      i++;
    }
    left=blkadd+left-256; //mark the leftover length
  }

  }
  return len;
}

int seeks(int disk, int blk){
  uint32_t seek=Con_Jbod(JBOD_SEEK_TO_DISK,disk,blk);  //seek to the disposition
  jbod_client_operation(seek,NULL);

  seek=Con_Jbod(JBOD_SEEK_TO_BLOCK,disk,blk);           //seek to the block position
  jbod_client_operation(seek,NULL);

  return 0;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  if(gmount==0){              return -1;}           //check if it was mounted or not
  if(addr+len>1048576){       return -1;}           //check if it the address is overthe limit
  if(len>1024){               return -1;}           //check if the length is over the limit
  if(buf==NULL&&len>0){       return -1;}           //check if it is passing a impossible length
  else if(buf==NULL&&len==0){ return 0;} 
  uint8_t temp[JBOD_BLOCK_SIZE];                    //temporary value to store the block from the jbod read 
  
  int diskpos=addr/JBOD_DISK_SIZE;                      //calculating the disk&block position of the jbod
  int blkpos=(addr-JBOD_DISK_SIZE*(int)(diskpos))/JBOD_NUM_BLOCKS_PER_DISK;
  int left=len;                                     //how much length left after each iteration of jbod extra block space
  int blkadd=addr-diskpos*JBOD_DISK_SIZE-blkpos*JBOD_BLOCK_SIZE; //where to start to read from the block
  int cur=256-blkadd;                               //current block position
  int i=0;                                          //check point if it is the frist time the loop runs
  while(1){
  if(cache_enabled()){cache_lookup(diskpos,blkpos,temp);}

  seeks(diskpos,blkpos);

  //printf("blkadd: %d disk: %d blk : %d ",blkadd,diskpos,blkpos);  
  uint32_t read=Con_Jbod(JBOD_READ_BLOCK,diskpos,blkpos);   //read to this block&disk position
  jbod_client_operation(read,temp);

  //uint32_t write=Con_Jbod(JBOD_WRITE_BLOCK,diskpos,blkpos);   

  if(left==256&&blkadd==0){
    memcpy(&temp[0],buf,len);
    if(cache_enabled()) {cache_insert(diskpos,blkpos,temp);cache_update(diskpos,blkpos,temp);}
    seeks(diskpos,blkpos);
    uint32_t write=Con_Jbod(JBOD_WRITE_BLOCK,diskpos,blkpos); 
    jbod_client_operation(write,temp);
    return len;
  }

  if(i!=0&&left<=256){                 //if the loop was called and the leftover length is less than the block size
    memcpy(&temp[0],buf+cur,left);
    if(cache_enabled()) {
      cache_insert(diskpos,blkpos,temp);
      cache_update(diskpos,blkpos,temp);
      }
    seeks(diskpos,blkpos);
    uint32_t write=Con_Jbod(JBOD_WRITE_BLOCK,diskpos,blkpos); 
    jbod_client_operation(write,temp);
    return len;
  }
  
  if(left+blkadd<=256){                //if the loop frist time and no more than block size is needed
    memcpy(&temp[blkadd],buf,left);
    if(cache_enabled()) {cache_insert(diskpos,blkpos,temp);cache_update(diskpos,blkpos,temp);}
    //printf("blkadd: %d ",blkadd);
    seeks(diskpos,blkpos);
    uint32_t write=Con_Jbod(JBOD_WRITE_BLOCK,diskpos,blkpos); 
    jbod_client_operation(write,temp);
    return len;
  }

  if(left+blkadd>256){                //if needed to read from the next block
    if(i!=0){                         //and if it was not the frist time bigger than block size   //size not the frist time, block address should always started at 0
      memcpy(&temp[0],buf+cur,256);
    if(cache_enabled()) {cache_insert(diskpos,blkpos,temp);cache_update(diskpos,blkpos,temp);}
      seeks(diskpos,blkpos);
      uint32_t write=Con_Jbod(JBOD_WRITE_BLOCK,diskpos,blkpos); 
      jbod_client_operation(write,temp);
      if(blkpos==255){    //check if we needed more disk
        left=left-256;
        cur=cur+256;
        diskpos++;        //if needed more disk, diskposition + 1
        blkpos=0;
        i++;              //mark for iteration
        continue;
      }
      left=left-256;
      cur=cur+256;
      blkpos++;
      continue;

    }    //copy the memory from the block address place to buf
    memcpy(&temp[blkadd],buf,cur);
    if(cache_enabled()) {cache_insert(diskpos,blkpos,temp);cache_update(diskpos,blkpos,temp);}
    seeks(diskpos,blkpos);
    uint32_t write=Con_Jbod(JBOD_WRITE_BLOCK,diskpos,blkpos); 
    jbod_client_operation(write,temp);

    if(blkpos==255){    //check if we needed more disk
      diskpos++;        //if needed more disk, diskposition + 1
      blkpos=0;
      i++;              //mark for iteration
    }else{
      blkpos++;         //if need more block block position +1
      i++;
    }
    left=blkadd+left-256; //mark the leftover length
  }

  }
  return len;
}