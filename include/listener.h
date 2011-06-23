#define NUM_THREADS 20
#define HEADER_LENGTH 6
#define MEGASIZE 110
#define XL3_MAXPAYLOADSIZE_BYTES (MEGASIZE*12) 
#define MAX_BUFFER_LEN (XL3_MAXPAYLOADSIZE_BYTES+HEADER_LENGTH)

int sockfd, thread_sockfd[NUM_THREADS];
void close_sockets();
void handler(int signal);
void die(const char *msg);
void* listener_child(void* psock);
void* listener(void* ptr);

typedef enum { PMTBUNDLE } PacketType;

typedef struct
{
    uint16_t type;
} PacketHeader;

typedef struct
{
  uint16_t packet_num;
  uint8_t packet_type;
  uint8_t num_bundles;
} XL3_CommandHeader;

typedef struct
{
  PacketHeader header;
  XL3_CommandHeader cmdHeader;
  char payload[XL3_MAXPAYLOADSIZE_BYTES];
} XL3Packet;

