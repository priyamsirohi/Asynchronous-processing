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
wait_queue_head_t consumers, producers;

/* flag for exiting threads */
int thread_exit = 0;





asmlinkage extern long (*sysptr)(void *args, int argslen);

static void netlink(pid_t pid, char *msg){

	struct nlmsghdr *nlh;
	struct sk_buff *skb_out;
	int size;
	
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
	
	int err = SUCCESS, len=0, pos =0;
	char *msg = kmalloc(1024,GFP_KERNEL);
	char id[3];
	
	len = strlen("\nJob ID :"); 
	memcpy(msg+pos,"\nJob ID :",len);
	pos+=len;
	
	sprintf(id,"%d", job->job_id);
	memcpy(msg+pos,id,3);
	
	
	pos+=1;
	len = strlen("\nJob type :"); 
	memcpy(msg+pos,"\nJob type :",len);
	pos+=len;
	
	if(job->job_type==ENCRYPT||job->job_type==DECRYPT){
		err = encryptDecrypt(job);
		len = strlen("ENCRYPT\n"); 
		
		if(job->job_type==ENCRYPT) 
			memcpy(msg+pos,"ENCRYPT\n",len);
		else 
			memcpy(msg+pos,"ENCRYPT\n",len);
		
		pos+=len;
		
		len = strlen("Infile is ")	; 
		memcpy(msg+pos,"Infile is ",len);
		pos+=len;
		
		len = strlen(job->infile)	; 
		memcpy(msg+pos,job->infile,len);
		pos+=len;
		
		len = strlen("\nOutfile is ")	; 
		memcpy(msg+pos,"\nOutfile is ",len);
		pos+=len;
		
		len = strlen(job->outfile)	; 
		memcpy(msg+pos,job->outfile,len);
		pos+=len;
		
		
	}

	else if(job->job_type==CHECKSUM){
		err = checksum(job);
		len = strlen("CHECKSUM\n")	; 
		memcpy(msg+pos,"CHECKSUM\n",len);
		pos+=len;
		
		len = strlen("Infile is ")	; 
		memcpy(msg+pos,"Infile is ",len);
		pos+=len;
		
		len = strlen(job->infile)	; 
		memcpy(msg+pos,job->infile,len);
		pos+=len;
		
	}
	else if(job->job_type==LIST){
		list_queue(prod_cons_q, job);
	}
	
	
	len = strlen("\nStatus is ")	; 
	memcpy(msg+pos,"\nStatus is ",len);
	pos+=len;
	

	if(err>=SUCCESS){
	len = strlen("SUCCESS")	; 
	memcpy(msg+pos,"SUCCESS",len);
	pos+=len;
	}
	
	
	else {
	len = strlen("FAILIURE")	; 
	memcpy(msg+pos,"FAILIURE",len);
	pos+=len;
	}
	
	memcpy(msg+pos,"\0",1);
	printk("Heyy %s", msg);
	netlink(job->pid, msg);
	
	kfree(job);
	return err;
	
	}

int consumer(void *data){

	struct job *job = NULL;
	int err = 0;
	
top:
	/* waiting for jobs */
	wait_event_interruptible(consumers, prod_cons_q_len > 0);
	

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
	wake_up_all(&producers);
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
		wait_event_interruptible(producers, prod_cons_q_len < MAX_LEN);
		goto top;
	}
	
	print_queue(prod_cons_q);
	
	mutex_unlock(&big_mutex);
	
	
	wake_up_all(&consumers);

	
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
	init_waitqueue_head(&producers);
	init_waitqueue_head(&consumers);
	
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
	wake_up_all(&consumers);
	
	netlink_kernel_release(nl_sk);
	
	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_submitjob module\n");
}
module_init(init_sys_submitjob);
module_exit(exit_sys_submitjob);
MODULE_LICENSE("GPL");
