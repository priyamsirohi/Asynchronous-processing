
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>



/* Queue structures and global declarations*/

struct node{
	struct job *job;
	struct node *next;
};

/* queue head */
struct queue{
	struct node *head;
	struct node *tail;
	struct mutex q_mutex;
};

/* Return Flags */
#define SUCCESS 0
#define FAILIURE -1
#define MAX_LEN 3


struct queue *queue_init(void);

struct queue* add_job(struct queue *queue, struct job *job);

struct job* remove_job(struct queue *queue);

int exit_queue(struct queue *queue);

int rwfile(struct file *filp1, struct file *filp2, struct job *in);

void print_queue(struct queue *queue);

int encryptDecrypt(struct job *in);

int checksum(struct job *in);

void list_queue(struct queue *queue, struct job *job);

int delete_job_id(struct queue *queue, int job_id);

