#define __NR_submitjob	359	/* new syscall number */

/* Operations supported */
#define ENCRYPT 1
#define DECRYPT 2
#define COMPRESS 3
#define DECOMPRESS 4
#define CHECKSUM 5
#define LIST 6
#define REMOVE 7


/* Priority */
#define HIGH_PRIORITY 1
#define LOW_PRIORITY 2

/* Return Flags */
#define SUCCESS 0
#define FAILIURE -1

#define MY_BUFFER_SIZE 4096



/*Job structure*/
struct job{
	int job_type;
	int job_id;
	pid_t pid;
	unsigned long *crc32;
	int priority;
	char *infile;
	char *outfile;
	unsigned char *key;
};

void nl_init(void);
int nl_receive(void);
