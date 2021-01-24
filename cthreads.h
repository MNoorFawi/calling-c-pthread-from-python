// length of strings
#define STRLEN 256
// how many bytes the shared memory object is
#define SIZE 2048
// name of shared object
#define SHM_OBJ "weather_data"
// named pipe (FIFO)
#define CITY_FIFO "cityfifo"
// construct first command with the city
#define CMD1 "curl -s 'http://api.openweathermap.org/data/2.5/weather?q=%s"
// append api key and extract only needed info and convert to csv
#define CMD2 "&appid=<your-api-key-here>' | jq -r '{name: .name, temperature: .main.temp, desc: .weather[].description} | [.name, .temperature, .desc] | @csv'"

void error(char * msg);
void * request(void * inp);
char ** weather_work(char ** cities, int NUM_CITIES);