# Call C Parallel Function from Python

C language is fast. Let alone parallelism in C. It is a great speed boost especially for heavy operations.

Here we will see how we can use or C parallel program in Python using the help of Cython language. Of course, Python has its own libraries, like multiprocessing and threading, to apply concurrency and thread-based parallelism which will speed up python applications greatly. But in case you love C, as much as I do, and you have an existing C code that you want to use in Python quickly without having to write a wrapper for it. Or you usually like to write some C code in your Python applications. In this blog, we will see how to do so easily using Cython.

The weather application is based on https://openweathermap.org/api API. The final application will have **C** code, **command line** commands, **Cython** wrapper functions and will be called from **Python**. This blog demonstrates how to **combine so many tools together in one application** to achieve **better performance**.

We will also have a look at topics like memoization and Python decorators. Let’s get started!

## C Header
We define the variables that we will use along with declaring the functions in our C program. The header is so important because it is the one Cython will use to read the data from. Here’s our “**cthreads.h**” file:

```c
// length of strings
#define STRLEN 256
// construct first command with the city
#define CMD1 "curl -s 'http://api.openweathermap.org/data/2.5/weather?q=%s"
// append api key and extract only needed info and convert to csv
#define CMD2 "&appid=<your-api-key-here>' | jq -r '{name: .name, temperature: .main.temp, desc: .weather[].description} | [.name, .temperature, .desc] | @csv'"
// error handling
void error(char * msg);
// the function to be parallelized
void * request(void * inp);
// the function to be wrapped
char ** weather_work(char ** cities, int NUM_CITIES);
```

## C Main Code
In our main application, we define the function that we will pass to the threads to sumbit the requests to the API. We also define the function that’ll create the threads and join the results from them. The result will be a C array that the Cython code will convert into a Python list.

###### N.B. Comments on the code are so important. 

The following is our “**cthreads.c**” file.

```c
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
  /* the array containing the gathered responses from threads
  will be freed from with Cython */
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
    // passing arg pointer as void*
    if (pthread_create( & threads[i], NULL, request, (void * ) cities[i]) == -1)
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
```

The comments along with the previos blogs explain the code thoroughly.

This is assumed to be our existing working C application. We now need to see how we are going to wrap it in Cython code.

## Cython Wrapper
In Cython, we will wrap the “weather_work” function in a Cython function that’ll work as an interface to be called from Python.

First, we import the needed functions from the header file, and we also write a function that converts Python lists into C arrays. Here is the “**pyxthreads.pyx**”

```cython
cimport numpy as np
import numpy as np
from functools import wraps
from libc.stdlib cimport malloc, free
# import the interface function declaration from the header file
cdef extern from "cthreads.h":
    char ** weather_work(char ** cities, int NUM_CITIES)
    
cdef extern from "Python.h":
    char* PyUnicode_AsUTF8(object unicode)
# converting python lists into C arrays to pass it to C code
cdef char ** to_c_string_list(str_list):
    cdef int l = len(str_list)
    cdef char **c_string = <char **>malloc(l * sizeof(char *))
    for i in range(l):
        c_string[i] = PyUnicode_AsUTF8(str_list[i])
    return c_string
# a function that splits the message into a tuple of values instead of a single string
cdef np.ndarray fill_array(np.ndarray arr, char ** carr, int l):
    cdef int i = 0
    for i in range(l):
        arr[i] = tuple(carr[i].decode().replace('"', "").split(","))
    
    # free the memory for the response array allocated in the C code
    for s in range(l):
        free(carr[s])
    free(carr)
    return arr
```

This is the first part of the file. Straightforward. Now, we define a **Python decorator** that applies **memoization** in our application.

So what is memoization and what are the Python decorators?

### Python Decorators
Simply, a decorator is a callable Python object, a function for instance, that takes, wraps, another function, or any other object, and extends its behavior and adds more functionalities to it without having to change the wrapped function itself. In decorators definition, we pass the decorated function as argument. Inside the decorator body, we define a wrapper function that takes the arguments of the decorated functions as its arguments, adds the functionalities and returns the desired return from the whole process. Then the decorator returns the wrapper function as its return value.

```python
## decorator syntax
import functools
def decorator(decorated_func):
    # wraps is a decorator that copies the attributes
    # of the decorated func to the wrapper func
    @functools.wraps(decorated_func)
    def wrapper_func(decorated_func_args):
        ## do the stuff here
        return modified_output
    return wrapper_func
## calling the decorated function
@decorator
decorated_func(decorated_func_args)
## this call is equal to 
decorator(decorated_func(decorated_func_args))
```

### Memoization
Wikipedia definition: In computing, memoization or memoisation is an optimization technique used primarily to speed up computer programs by storing the results of expensive function calls and returning the cached result when the same inputs occur again.

So memoization is used to make the code remember the last queries so that it returns the answer without redoing the work again if the same call of a function is called. This saves time especially if the work is heavy which is valid in our case. Fo example, imagine we already requested the weather data of Cairo city. and now we want to do it again. So instead of hitting the API again for Cairo’s weather, we make the code record the data for Cairo and returns it immediately once it sees Cairo again.

Of course in weather application you may want to have the data more and more again multiple times during the day. But the example here to demonstrate the concept.

So we will define a decorator that modifies the weather_query function to make it remember last calls and apply memoization:

```cython
def memoize(weather_query):
    queried = {}
    @wraps(weather_query)
    def memorize(cities):
        response = []
        # if a single string, convert it to list
        # C code takes an array
        if isinstance(cities, str):
            cities = [cities]
        # check which cities are already queried before
        for c in cities:
            if c in queried:
                # add the already queried data to the output 
                response.append(queried.get(c))
                # remove the city from the cities list to be queried
                #cities.remove(c) # for some reason remove doesn't work properly
        # this filter is used because .remove not working well
        cities = [c for c in cities if c not in queried]
        if len(cities) > 0:
            # call the C function on the shortlisted cities list
            weather = weather_query(cities)
            # add the new responses to the queried data
            for i in range(len(weather)):
                queried[weather[i][0]] = weather[i]
                response.append(weather[i])
        # return the desired output
        return response
    # return the wrapper function
    return memorize
    
## decorate the weather_query function
@memoize
def weather_query(cities):
    # convert python list to C array
    cdef char ** ccities = to_c_string_list(cities)
    cdef int ln = len(cities)
    # get responses
    cdef char ** msg = weather_work(ccities, ln)
    # define an empty numpy array to be filled with data
    cdef np.ndarray[dtype = object, ndim = 1] arr = np.empty(ln, dtype = object)
    # free the C array memory    
    free(ccities)
    # return the clean version of the output
    # the msg array will be free in the fill_array function defined above
    return fill_array(arr, msg, ln)
```

Now, we have our Cython wrapper script ready, it is time to create the “**setup.py**” file and compile everything.

```python
from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext
import numpy
# first arg in Extension is the name of the produced module
# second arg, the list, is the cython file and the c file
# third is the numpy dir as it will be used inside the cython code
ext_modules = [Extension("weather_threads", ["pyxthreads.pyx", "cthreads.c"],
                         include_dirs = [numpy.get_include()])]
setup(
  cmdclass = {"build_ext": build_ext},
  ext_modules = ext_modules
)
```

## Setup & Run
Let’s now produce the Python module to use it in a Python session:

```bash
python3 setup.py build_ext --inplace
```

This will produce “weather_threads” module. Let’s open a Python session and try to test everything

```python
# import everything from the module
from weather_threads import *
# to prettily print output
from pprint import pprint
cities = ["Barcelona", "Cairo", "Dubai", "Berlin", "Amsterdam", "Kuala Lumpur"]
pprint(weather_query(cities))
# Thread 140359502006016 is getting weather data for Kuala Lumpur
# Thread 140359493551872 is getting weather data for Amsterdam
# Thread 140359485097728 is getting weather data for Berlin
# Thread 140359476643584 is getting weather data for Dubai
# Thread 140359262734080 is getting weather data for Cairo
# Thread 140359254279936 is getting weather data for Barcelona
#[('Barcelona', '287.69', 'few clouds'),
# ('Cairo', '291.41', 'few clouds'),
# ('Dubai', '294.3', 'dust'),
# ('Berlin', '276.67', 'clear sky'),
# ('Amsterdam', '278.14', 'few clouds'),
# ('Kuala Lumpur', '298.82', 'few clouds')]
```
The code is working fine. A different thread is created to get data for each city in parallel.

Let’s now test the memoization

```python
pprint(weather_query(cities))
#[('Barcelona', '287.69', 'few clouds'),
# ('Cairo', '291.41', 'few clouds'),
# ('Dubai', '294.3', 'dust'),
# ('Berlin', '276.67', 'clear sky'),
# ('Amsterdam', '278.14', 'few clouds'),
# ('Kuala Lumpur', '298.82', 'few clouds')]
```
No threads were created and no API requests were made. The function quickly returned the stored response. Let’s test it again

```python
# adding one new city
cities2 = ["Barcelona", "Cairo", "Rome"]
pprint(weather_query(cities2))
# Thread 140359502006016 is getting weather data for Rome
#[('Barcelona', '287.69', 'few clouds'),
# ('Cairo', '291.41', 'few clouds'),
# ('Rome', '278.98', 'clear sky')]
pprint(weather_query("Barcelona"))
#[('Barcelona', '287.69', 'few clouds')]
pprint(weather_query("Madrid"))
# Thread 140359502006016 is getting weather data for Madrid
#[('Madrid', '281.93', 'broken clouds')]
```
Great! Everything is working well. Now we have an application that is working in Python and calls C and Command Line code through Cython 😉 .
