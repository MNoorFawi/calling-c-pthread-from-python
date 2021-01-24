#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "cthreads.h"

// error handling
void error(char * msg) {
  printf("%s: %s\n", msg, strerror(errno));
  exit(1);
}

void * request(void * inp) {
  // the final command to be called
  char cmd[STRLEN];
  // void * into char *
  char * city = inp;
  // the result string from a thread
  char * res = (char * ) malloc(STRLEN * sizeof(char));
  // to keep track of number of outbound requests and memoization
  printf(" Thread %ld is getting weather data for %s\n", pthread_self(), city);
  // construct the command
  sprintf(cmd, CMD1, city);
  strcat(cmd, CMD2);
  // save the command output into a variable
  FILE * fp;
  fp = popen(cmd, "r");
  // error handling
  if (fp == NULL) {
    error("Failed to run command\n");
  }
  // read command output including spaces and commas
  // until new line
  fscanf(fp, "%[^\n]s", res);
  pclose(fp);
  // return the pointer to res as void *
  return (void * ) res;
}

// the function to be called from Cython
char ** weather_work(char ** cities, int NUM_CITIES) {
  // the array containing the gathered responses from threads
  char ** response = (char ** ) malloc(NUM_CITIES * sizeof(char * ));
  int m = 0;
  // allocate memory for individual strings in the array
  for (; m < NUM_CITIES; ++m)
    response[m] = (char * ) malloc(STRLEN * sizeof(char));
  // variable to collect responses in  
  void * result;
  // i variable will be used to loop in threads creating and joining
  int i = NUM_CITIES;
  // create threads (backwards) to go in parallel hitting the api
  pthread_t threads[NUM_CITIES];
  while (i--> 0) {
    // create the threads and pass the request function
    if (pthread_create( & threads[i], NULL, request, (void * ) cities[i]) == -1) // passing arg pointer as void*
      error("Can't create thread\n");
  }
  // collecting results from threads and fill the response array
  while (++i < NUM_CITIES) {
    if (pthread_join(threads[i], & result) == -1)
      error("Can't join thread\n");
    // void * to char *
    strcpy(response[i], (char * ) result);
  }
  return response;
}