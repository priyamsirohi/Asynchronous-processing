#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/pid_namespace.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include "sys_submitjob.h"
#include "common.h"
#define MD5_KEY_LENGTH 16

/* Job ID Sequence */
int job_id = 0;

/* consumer threads */
struct task_struct *kthread1;
struct task_struct *kthread2;

/* netlink socket for callback */
struct sock *nl_sk = NULL;
#define NETLINK_USER 31

/*  Global consumer-producer queue */
struct queue *prod_cons_q;
int prod_cons_q_len = 0;
struct mutex big_mutex;
wait_queue_head_t wqconsumer, wqproducer;

/* flag for exiting threads */
int thread_exit = 0;





asmlinkage extern long (*sysptr)(void *args, int argslen);

static void netlink(pid_t pid, char *msg){

	struct nlmsghdr *nlh;
	struct sk_buff *skb_out;
	int size;
	
	
	printk("Callback called in kernel\n");
	size=strlen(msg);

	skb_out = nlmsg_new(size,0);

	if(!skb_out)
	{
		printk("[netlink]: nlmsg failed ");
	    return;
	} 

	nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,size,0);  
	NETLINK_CB(skb_out).dst_group = 0; 
	strncpy(nlmsg_data(nlh),msg,size);

	nlmsg_unicast(nl_sk,skb_out,pid);

}


int process_job(struct job *job){
	
	int err = SUCCESS, len=0;
	char *msg = kmalloc(1024,GFP_KERNEL);
	
	
	len+= strlen("Job type :")+1; 
	memcpy(msg,"Job type :",len);
	
	if(job->job_type==ENCRYPT||job->job_type==DECRYPT){
		err = encryptDecrypt(job);
	/*	if(job->job_type==ENCRYPT) 
			strcat(msg, "ENCRYPT\n");
		else 
			strcat(msg, "DECRYPT\n");
		
		strcat(msg, "Infile is ");
		strcat(msg, job->infile);
		strcat(msg, "\nOutfile is ");
		strcat(msg, job->outfile);
	*/		
		
	}

	else if(job->job_type==CHECKSUM){
		err = checksum(job);
	/*	strcat(msg, "CHECKSUM\n");
		strcat(msg, "Infile is ");
		strcat(msg, job->infile);	*/
	}
	else if(job->job_type==LIST){
		list_queue(prod_cons_q, job);
	}
	
	
	
	/*strcat(msg, "\nStatus is ");
	if(err==SUCCESS)
		strcat(msg, "SUCCESS");
	else 
		strcat(msg, "FAILIURE");	*/
	
	netlink(job->pid, msg);
	printk("In kernel, msg is %s\n",msg);
	kfree(job);
	return err;
	
	}

int consumer(void *data){

	struct job *job = NULL;
	int err = 0;
	
top:
	/* waiting for jobs */
	wait_event_interruptible(wqconsumer, prod_cons_q_len > 0);
	

	/* module exit, killing the thread */
	if(thread_exit == 1)
		goto exit;

	
	mutex_lock(&big_mutex);

	if(prod_cons_q_len > 0){

		job = remove_job(prod_cons_q);
		if(IS_ERR(job)){
			err = PTR_ERR(job);
			goto out;
		}

		prod_cons_q_len--;

	
	}

	mutex_unlock(&big_mutex);
	wake_up_all(&wqproducer);
	err = process_job(job);
	
	schedule();
	goto top;
out:
	mutex_unlock(&big_mutex);
exit:
	return err;
}


asmlinkage long submitjob(void *args, int argslen)
{
	struct job *job = NULL;
	struct job *u_args = (struct job *) args;
	int err=0;
	struct queue *q;
	
	if(args == NULL){
		printk("SUBMITJOB: Invalid arguments\n");
		err = -EINVAL;
		goto out;
	}
		
	/* memory allocation for user arguments into kernel space*/
	job = kmalloc(sizeof(struct job), GFP_KERNEL);
	if( job == NULL ){
		printk(
		"SUBMITJOB: Insufficient memory\n");
		err = -ENOMEM;
		goto out;
	}
	
	/* Copying and validation of user space arguments */
	err = copy_from_user(job, u_args, sizeof(struct job));
	if (err != 0){
		printk("SUBMITJOB: copy_from_user failed\n");
		err = -EFAULT;
		goto out;
	}
	
	
	if(job->job_type==ENCRYPT||job->job_type==DECRYPT){
		
	job->key = kmalloc(MD5_KEY_LENGTH, GFP_KERNEL);
	if( job->key == NULL ){
		printk(
		"SUBMITJOB: Insufficient memory\n");
		err = -ENOMEM;
		goto out;
	}
	
	/* Copying and validation of user space arguments */
	err = copy_from_user(u_args->key, u_args->key, MD5_KEY_LENGTH);
	if (err != 0){
		printk("SUBMITJOB: copy_from_user failed\n");
		err = -EFAULT;
		goto out;
	}
	}
	
	else if(job->job_type==REMOVE){
		delete_job_id(prod_cons_q, argslen);
		kfree(job);
		goto out;
	}
	

	printk("job_type %d\n",job->job_type);
	
	job_id++;
	job->job_id = job_id;
	
	
	
top:
	mutex_lock(&big_mutex);
	
	/* adding job to the queue */
	if(prod_cons_q_len < MAX_LEN){
		q = add_job(prod_cons_q, job);
		if(IS_ERR(q)){
			err = PTR_ERR(q);
			goto out;
		}
		else
			prod_cons_q_len++;
	}

	else if(prod_cons_q_len == MAX_LEN){
		
		printk("[sys_submitjob]: Producer going to sleep\n");
		mutex_unlock(&big_mutex);
		wait_event_interruptible(wqproducer, prod_cons_q_len < MAX_LEN);
		goto top;
	}
	
	print_queue(prod_cons_q);
	
	mutex_unlock(&big_mutex);
	
	
	wake_up_all(&wqconsumer);

	
	out:
		return err;
}

static int __init init_sys_submitjob(void)
{
	struct netlink_kernel_cfg cfg = {
    .input = (void *)netlink,
	};
	/* Initialize main and wait queue */
	
	prod_cons_q = queue_init();
	
	
	/* initializing wait queues for consumer and producer */
	init_waitqueue_head(&wqproducer);
	init_waitqueue_head(&wqconsumer);
	
	/* initializing mutex */
	mutex_init(&big_mutex);
	
	/* initializing consumer threads */
	kthread1 = kthread_create(consumer, NULL, 
				"consumer1");
	wake_up_process(kthread1);

	kthread2 = kthread_create(consumer, NULL, 
				"consumer2");
	wake_up_process(kthread2);
	
	nl_sk = netlink_kernel_create(&init_net, 17, &cfg);
	
	if(!nl_sk)
	{
	    printk("Socket creation failed\n");
	    return -ECHILD;
	}
	

	
	printk("installed new sys_submitjob module\n");
	if (sysptr == NULL)
		sysptr = submitjob;
	return SUCCESS;
}
static void  __exit exit_sys_submitjob(void)
{
	/* queue exit */
	exit_queue(prod_cons_q);
	
	/* a hook to make all consumers exit */
	prod_cons_q_len = 1;
	thread_exit += 1;
	wake_up_all(&wqconsumer);
	
	netlink_kernel_release(nl_sk);
	
	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_submitjob module\n");
}
module_init(init_sys_submitjob);
module_exit(exit_sys_submitjob);
MODULE_LICENSE("GPL");
