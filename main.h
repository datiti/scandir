#ifndef H_MAIN
#define H_MAIN

struct thread_name;
struct thread_info;

void tellmeonce();
static void *thread_start(void * data);
void displayts();
static long currentTimeMillis();
int filefilter(const struct dirent *dir);

static void * thread_start_internalweb(void * data);
int callback_elapsed (const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_hello_world (const struct _u_request * request, struct _u_response * response, void * user_data);
#endif