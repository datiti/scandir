#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> //Header file for sleep(). man 3 sleep for details. 
#include <pthread.h> 
#include <time.h> 
#include <sys/time.h>
#include <dirent.h> 
#include <syslog.h>
#include <errno.h>
#include <ulfius.h>
#include <openssl/md5.h>
#include "main.h"

#define handle_error_en(en, msg) do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

// max allowed number of threads
#define MAX_NUM_THREADS 10

// log name in syslog
#define LOG_NAME "main"

// default port for the internal monitoring rest server
#define DEFAULT_PORT 9090

/**
 * struct used when creating thread. Used as argument to thread_start
 */
struct thread_info {    /* Used as argument to thread_start() */
    pthread_t thread_id;        /* ID returned by pthread_create() */
    int       thread_num;       /* Application-defined thread # */
    char     *thread_name;      /* Application-defined thread name */
    long      value;            /* Application-defined value data */
};

static void compute_md5(char * filename) {
    unsigned char c[MD5_DIGEST_LENGTH];
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);
        return 0;
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", c[i]);
    printf (" %s\n", filename);
    fclose (inFile);
}

// control variable for pthread_once
static pthread_once_t once_control = PTHREAD_ONCE_INIT;
/**
 * callback for pthread_once: called only once per the first thread that calls it.
 * It then updates the once_control global variable.
 */
static void thread_call_once(){
    syslog(LOG_DEBUG, "pthread_once call");
}

/**
 * Callback function for the web application on /helloworld url call
 */
int callback_hello_world (const struct _u_request * request, struct _u_response * response, void * user_data) {
    json_t * json_body = json_object();
    json_object_set_new(json_body, "message", json_string("Hello World!"));
    ulfius_set_json_body_response(response, 200, json_body);
    json_decref(json_body);
    return U_CALLBACK_CONTINUE;
}

/**
 * Callback function for the web application on /elapsed url call:
 * Returns the elapsed time since the start of the program in json format
 */
int callback_elapsed (const struct _u_request * request, struct _u_response * response, void * user_data) {
    struct thread_info *tinfo = (struct thread_info *) user_data;
    long from = tinfo->value;
    json_t * json_body = json_object();
    long asis = currentTimeMillis();
    json_object_set_new(json_body, "elapsed", json_integer(asis-from));
    json_object_set_new(json_body, "unit", json_string("ms"));
    ulfius_set_json_body_response(response, 200, json_body);
    json_decref(json_body);
    return U_CALLBACK_CONTINUE;
}

/**
 * Initialize the internal rest server for monitoring.
 * Called by pthread_create
 */
static void * thread_start_internalweb(void * data) {
    struct thread_info *tinfo = (struct thread_info *) data;
    struct _u_instance instance;
    // Initialize instance with the port number
    if (ulfius_init_instance(&instance, DEFAULT_PORT, NULL, NULL) != U_OK) {
        fprintf(stderr, "Error ulfius_init_instance, abort\n");
        return 0;
    }
    // Endpoint list declaration
    ulfius_add_endpoint_by_val(&instance, "GET", "/helloworld", NULL, 0, &callback_hello_world, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/elapsed", NULL, 0, &callback_elapsed, data);

    // Start the framework
    if (ulfius_start_framework(&instance) == U_OK) {
        printf("Start framework on port %d\n", instance.port);

        // Wait for the user to press <enter> on the console to quit the application
        getchar();
    } else {
        fprintf(stderr, "Error starting framework\n");
    }
    printf("End framework\n");

    ulfius_stop_framework(&instance);
    ulfius_clean_instance(&instance);
}

// A normal C function that is executed as a thread 
// when its name is specified in pthread_create() 
static void * thread_start(void * data) {
    struct thread_info *tinfo = (struct thread_info *) data;
    syslog(LOG_DEBUG, "Thread %d: thread_name=%s\n",
                   tinfo->thread_num, tinfo->thread_name);
    pthread_once(&once_control, thread_call_once); // initialize only once what needs to be initialize 
	// wait for 1000 microsecond
    //usleep(1000); 
    // wait for 60 second
    sleep(60);
	printf("Printing GeeksQuiz from Thread %d\n", tinfo->thread_num); 
    displayts();
    syslog(LOG_DEBUG, "Stopping thread %d...", tinfo->thread_num);
	return 0; 
} 

// display current timestamp in the format: Sun 2019-12-15 20:27:43 CET
void displayts() {
    time_t     now;
    struct tm  ts;
    char       buf[80];
    // Get current time
    time(&now);
    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    printf("%s\n", buf);
}

/**
 * Returns the current time in milliseconds
 */
static long currentTimeMillis() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

// the file filter callback applied on each file by scandir function
int filefilter(const struct dirent *dir) {
    printf("%s\n", dir->d_name);
    printf("\t%d\n", dir->d_type);
    printf("\t%d\n", dir->d_reclen);
    printf("\t%ld\n", dir->d_off);
    return 1;
}

// display content of a directory from readdir function
void read_dir(char * dirname) {
    struct dirent *de;  // Pointer for directory entry   
    // opendir() returns a pointer of DIR type.  
    DIR *dr = opendir("."); 
    if (dr == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        printf("Could not open current directory" ); 
        return; 
    }
    unsigned long count=0;
    // Refer http://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html 
    // for readdir() 
    while ((de = readdir(dr)) != NULL) {
            printf("%s %ld\n", de->d_name, telldir(dr)); 
            count++;
    }
    closedir(dr);
    syslog(LOG_DEBUG, "readdir size: %ld\n", count);
}

// display content of a directory with scandir function: seems faster than readdir function
void scan_dir(char * dirname) {
    struct dirent **namelist;
    int n = scandir(dirname, &namelist, filefilter, alphasort);
    if (n < 0)
        perror("scandir");
    else {
        while (n--) {
            printf("%s\n", namelist[n]->d_name);
            free(namelist[n]);
        }
        free(namelist);
    }
    syslog(LOG_DEBUG, "scandir size: %d\n", n);
}

// Affiche l'aide du programme
void print_help(char * prgName) {
    printf("%s\n", prgName);
    printf(" Displays the content of a directory\n");
    printf(" Usage:\n");
    printf(" %s <opt> <directory_name> \n", prgName);
    printf(" where:\n");
    printf(" <directory_name> is the name of directory to scan.\n");
}

// MAIN
int main(int argc, char *argv[]) {
    long realstart = currentTimeMillis();

    long a = currentTimeMillis();
    compute_md5("main.c");
    printf("md5 duration: %ld ms\n", (currentTimeMillis()-a));

    long begin_time = currentTimeMillis();
    // initializing logging
    setlogmask (LOG_UPTO (LOG_DEBUG)); // log any message
    openlog(LOG_NAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "Program started by User %d", getuid ());

    displayts();

	printf("Before Thread\n"); 

    displayts();

    struct thread_info tinfo_web;
    tinfo_web.value = realstart;
    int s = pthread_create(&(tinfo_web.thread_id), NULL, thread_start_internalweb, &tinfo_web);
    if (s != 0)
        handle_error_en(s, "pthread_create internalweb");

    // Define number of threads and threads arguments
    int num_threads = 3; // number of threads
    if (num_threads > MAX_NUM_THREADS) {
        num_threads = MAX_NUM_THREADS;
    }
    struct thread_info *tinfo;
    /* Allocate memory for pthread_create() arguments */
    tinfo = calloc(num_threads, sizeof(struct thread_info));
    if (tinfo == NULL)
        handle_error("calloc");
    
    // create all the threads
    for(int tnum = 0; tnum<num_threads; tnum++) {
        printf("Thread %d begin: %ld\n", tnum+1, currentTimeMillis());
        tinfo[tnum].thread_num = tnum + 1;
        // max(num_threads) is 10; so thread_name will be 0 character max
        tinfo[tnum].thread_name = (char*)malloc(9 * sizeof(char));
        sprintf(tinfo[tnum].thread_name , "Thread %d", tinfo[tnum].thread_num);

        /* The pthread_create() call stores the thread ID into
            corresponding element of tinfo[] */
        syslog(LOG_DEBUG, "create thread: %d", tinfo[tnum].thread_num);
        int s = pthread_create(&tinfo[tnum].thread_id, NULL, thread_start, &tinfo[tnum]);
        if (s != 0)
            handle_error_en(s, "pthread_create");

        printf("Thread %d end: %ld\n", tinfo[tnum].thread_num, currentTimeMillis());
    }
    // wait for each thread to finish its job
    for(int tnum=0; tnum<num_threads; tnum++) {
        printf("%ld\n", currentTimeMillis());
        pthread_join(tinfo[tnum].thread_id, NULL); 
        printf("%ld\n", currentTimeMillis());
    }
	printf("After Thread\n"); 

    printf("%ld\n", currentTimeMillis());

    // display directory
    long start_fun = currentTimeMillis();
    read_dir("."); 
    printf("Time elapsed readdir: %ld ms\n", currentTimeMillis()-start_fun);

    // scanning directory
    start_fun = currentTimeMillis();
    scan_dir(".");
    printf("Time elapsed scandir: %ld ms\n", currentTimeMillis()-start_fun);

    closelog();

    long end_time = currentTimeMillis();
    printf("Time elapsed: %ld ms\n", end_time-begin_time);
	exit(EXIT_SUCCESS); 
}
