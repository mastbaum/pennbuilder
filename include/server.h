#define NUM_THREADS 20

int sockfd, thread_sockfd[NUM_THREADS];
void close_sockets();
void handler(int signal);
void die(const char *msg);
void* dostuff(void* psock);
void* listener(void* ptr);

