#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"
/*$ gdb ./tester
(gdb) set args -w traces/simple-input
(gdb) r
*/
static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  if(num_entries<2||num_entries>4096){    return -1;} //check if the size is over or under the limit
  if(cache!=NULL){                        return -1;} //check if the cache is already created
  cache_size=num_entries;
  cache=malloc(sizeof(cache_entry_t)*num_entries);
  for(int i=0;i<num_entries;i++){ //for each cache initialize it
      cache[i].valid=false;     //set the cache to false as inital so that we know this cache is not yet used
      cache[i].disk_num=0;   //initialzing the disk and block number
      cache[i].block_num=0;
      //memset(cache[i].block, 0, 255);   //if possible this was setting the block to null
      cache[i].access_time=0; //set the access time of cache to 0
  }
  return 1;
}

int cache_destroy(void) {
  if(cache==NULL){                        return -1;} //check if the cache is not created
  if(cache_size==0){                      return -1;} //check if the cache is not ctreated
  free(cache);  //free the dynamic allocated address
  cache=NULL;   //cache set to null after free
  cache_size=0; //cache size set to 0 after free
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if(cache==NULL){    return -1;} //check if cache exist
  if(buf==NULL){      return -1;} //check if buf is empty
  if(cache_size==0){  return -1;} //check if cache is empty
  if(disk_num>15||disk_num<0){      return -1;} //check if disk over or under
  if(block_num>255||block_num<0){   return -1;}//check if block over or under
  if(clock==0){       return -1;} //if no cache have inserted
  num_queries++; //number of queries 
  for(int i=0;i<cache_size;i++){  //check if theres a cache with the same disk and block
    if(cache[i].block_num==block_num&&cache[i].disk_num==disk_num&&cache[i].valid==true){//check if theres a cache with the same disk and block
            
      if(cache[i].valid==true){        //if exist then cache his ++
        num_hits++;                     //the number of time it hits 
        memcpy(buf,cache[i].block,256); // copy the memory from block to buffer
        clock++;                        //upate clock
        cache[i].access_time=clock;     //update the access time to clock number
        }
        return 1;
      
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
    for(int i=0;i<cache_size;i++){ //loop through the caches
    if(cache[i].block_num==block_num&&cache[i].disk_num==disk_num){  //check if theres a cache with the same disk and block
      if(cache[i].valid==true){        //if theres exist then update the datainside
        memcpy(cache[i].block,buf,256);//if theres exist then update the datainside
        clock++;
        cache[i].access_time=clock; //update the lru time
        }
      
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if(cache==NULL){    return -1;} //check if cache exist
  if(buf==NULL){      return -1;} //check if buf is empty
  if(cache_size==0){  return -1;} //check if cache is empty
  if(disk_num>15||disk_num<0){      return -1;} //check if disk over or under
  if(block_num>255||block_num<0){   return -1;}//check if block over or under

  for(int i=0;i<cache_size;i++){ //check if there already one same cache
    if(cache[i].block_num==block_num&&cache[i].disk_num==disk_num&&cache[i].valid==true){ //check if the cache is already exist
      return -1;
      }
  }
  for(int i=0;i<cache_size;i++){ //check if the cache set is not full
    if(cache[i].valid==false){ //if not fill one of them with the data
      clock++;
      cache[i].access_time=clock;   //used cache should be more recently
      cache[i].block_num=block_num; //replacing the disk and block number
      cache[i].disk_num=disk_num;
      cache[i].valid=true;      //set the cache of that iteration to true(cache is activated)
      memcpy(cache[i].block,buf,256);
      return 1;
    }
  }
  int lmin=cache[0].access_time;  //if it is full then find the least recent cache and substitude
  int iter=0;
  for(int i=1;i<cache_size;i++){  //search for the least
    if(cache[i].access_time<lmin){    //basic binary search to get the minimum access time and replace it
      lmin=cache[i].access_time;      //
      iter=i;                         //mark the iteration so we can replace all the data later  
      }
  }
  clock++;
  cache[iter].valid=true;   //replacing the cache with the newiest one
  cache[iter].disk_num=disk_num;  //replacing the disk and block number
  cache[iter].block_num=block_num;
  memcpy(cache[iter].block,buf,JBOD_BLOCK_SIZE);  //copy all the data inside of the buf into cache block
  cache[iter].access_time=clock;  //update cache

  return 1;
}

bool cache_enabled(void) {
  if(cache_size>2){    return true;}  //check if the cache is still enabled
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
