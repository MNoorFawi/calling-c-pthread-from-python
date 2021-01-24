cimport numpy as np
import numpy as np
from functools import wraps
from libc.stdlib cimport malloc, free

cdef extern from "cthreads.h":
	char ** weather_work(char ** cities, int NUM_CITIES)
	
cdef extern from "Python.h":
	char* PyUnicode_AsUTF8(object unicode)

cdef char ** to_c_string_list(str_list):
	cdef int l = len(str_list)
	cdef char **c_string = <char **>malloc(l * sizeof(char *))
	for i in range(l):
		c_string[i] = PyUnicode_AsUTF8(str_list[i])
	return c_string

cdef np.ndarray fill_array(np.ndarray arr, char ** carr, int l):
	cdef int i = 0
	for i in range(l):
		arr[i] = tuple(carr[i].decode().replace('"', "").split(","))
	
	for s in range(l):
		free(carr[s])
	free(carr)
	return arr
	
def memoize(weather_query):
	queried = {}
	@wraps(weather_query)
	def memorize(cities):
		response = []
		if isinstance(cities, str):
			cities = [cities]
		for c in cities:
			if c in queried:
				response.append(queried.get(c))
				#cities.remove(c)
		cities = [c for c in cities if c not in queried]

		if len(cities) > 0:
			weather = weather_query(cities)
			for i in range(len(weather)):
				queried[weather[i][0]] = weather[i]
				response.append(weather[i])

		#print("number of api requests %d" % len(cities))
		return response
	return memorize

@memoize
def weather_query(cities):
	cdef char ** ccities = to_c_string_list(cities)
	cdef int ln = len(cities)
	cdef char ** msg = weather_work(ccities, ln)
	cdef np.ndarray[dtype=object, ndim=1] arr = np.empty(ln, dtype=object)
	free(ccities)
	return fill_array(arr, msg, ln)
