#include "sys_submitjob.h"
#include "common.h"

struct queue *queue_init(void){
	
	struct queue *q = kmalloc(sizeof(struct queue*), GFP_KERNEL);
		
	if(q == NULL){
		return ERR_PTR(-ENOMEM);
	}
	else{
		q->front = NULL;
		q->rear = NULL;
	}
	
	mutex_init(&q->q_mutex);
	return q;
}

struct queue* add_job(struct queue *queue, struct job *job){
	
	int err = 0;
	struct node *node;
	
	
	if(queue == NULL){
		err = -EINVAL;
		goto out;
	}
	
	mutex_lock(&queue->q_mutex);

	node = kmalloc(sizeof(struct node*), GFP_KERNEL);
	
	if(node == NULL){
		err = -ENOMEM;
		goto out;
	}

	node->job = job;
	node->next = NULL;

	
		if(queue->front == NULL && queue-> rear == NULL){
			queue->front = node;
			queue->rear = node;
		}
		else{
			queue->rear->next = node;
			queue->rear = node;
		}
	mutex_unlock(&queue->q_mutex);
	
	
out:
	if(err != 0)
		return ERR_PTR(err);
	else 
		return queue;
}

void print_queue(struct queue *queue){
		
	struct node *temp;
	
	printk("List of jobs in queue is\n");
	
	mutex_lock(&queue->q_mutex);

	temp = queue->front;
	
	while(temp){
		printk("Job id: %d\n", temp->job->job_id);
		temp = temp->next;
	}

	mutex_unlock(&queue->q_mutex);
}

void list_queue(struct queue *queue, struct job *job){
		
	int err = SUCCESS;	
	struct node *temp;
	void *buff = NULL;
	int pos = 0,len=0;
	unsigned long *user = (unsigned long *)job->key;
	char id[3];
	
	buff = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if( buff == NULL ){
		printk(
		"[Queue]: Insufficient memory\n");
	}
	
	
	mutex_lock(&queue->q_mutex);

	temp = queue->front;
	
	if(temp == NULL){
		memcpy(buff,"empty",strlen("empty")+1);
		pos += strlen("empty")+1;
		goto out;
	}
	
	while(temp){
		

		len = strlen("\nJob ID :"); 
		memcpy(buff+pos,"\nJob ID :",len);
		pos+=len;
		
		sprintf(id,"%d", job->job_id);
		memcpy(buff+pos,id,3);
		
		
		pos+=1;
		len = strlen("\nJob type :"); 
		memcpy(buff+pos,"\nJob type :",len);
		pos+=len;
	
		if(temp->job->job_type == ENCRYPT){
		memcpy(buff+pos,"ENCRYPT",strlen("ENCRYPT")+1);
		pos +=strlen("ENCRYPT")+1;
		}
		else if(temp->job->job_type == DECRYPT){
		memcpy(buff+pos,"DECRYPT",strlen("DECRYPT")+1);
		pos +=strlen("DECRYPT")+1;
		}
		else if(temp->job->job_type == CHECKSUM){
		memcpy(buff+pos,"CHECKSUM",strlen("CHECKSUM")+1);
		pos +=strlen("CHECKSUM")+1;
		}
		memcpy(buff+pos,"\n",sizeof("\n"));
		pos += sizeof("\n");
		temp = temp->next;
	}


	mutex_unlock(&queue->q_mutex);
	
out:	
	memcpy(buff+pos,"\0",1);
	
	err = copy_to_user(user,buff,PAGE_SIZE);
	if(err<0)
			printk("[queue]: copy_to_user failed\n");
	
	if(buff) kfree(buff);
		
}

struct job* remove_job(struct queue *queue){

	int err = 0;
	struct job *job;
	struct node *to_remove = NULL;
	
	
	if(queue == NULL){
		err = -EINVAL;
		goto out;
	}
	
	mutex_lock(&queue->q_mutex);
	
	if(queue->front == NULL && queue->rear == NULL){
		job = NULL;
		err = -EINVAL;
		goto out;	
	}
	
	to_remove = queue->front;
	queue->front = queue->front->next;

	if(queue->front == NULL)
		queue->rear = NULL;
	mutex_unlock(&queue->q_mutex);

	job = to_remove->job;
	kfree(to_remove);

out:
	if(err != 0)
		return ERR_PTR(err);
	else 
		return job;
}

int delete_job_id(struct queue *queue, int job_id){

	int err = SUCCESS, flag = 0;
	struct node *tmp1 = NULL, *tmp2 = NULL;
	
	
	mutex_lock(&queue->q_mutex);
	
	tmp1 = queue->front;
	
	while(tmp1 != NULL){
		if(tmp1->job->job_id == job_id){
			
			flag = 1;
			
			if(tmp1 == queue->front){
				queue->front = tmp1->next;
			}
			else{
				tmp2->next = tmp1->next;
				if(!tmp1->next){
					queue->rear = tmp2;
				}
			}
			
			break;
		}
		tmp2 = tmp1;
		if(tmp1)
			tmp1 = tmp1->next;
	}

	if(queue->front == NULL)
		queue->rear = NULL;
	
	mutex_unlock(&queue->q_mutex);
	
	if(flag == 1){
		kfree(tmp1->job);
		kfree(tmp1);
	}
	else{
		printk("No such job found \n");
		err= -ENOENT;
	}
	return err;
}


int exit_queue(struct queue *queue){
	
	
	struct job *job;
	struct node *temp, *prev;

	temp = queue->front;
	while(temp){
		job = remove_job(queue);
		kfree(job);
		prev = temp;
		temp = temp->next;
		kfree(prev);
	}

	kfree(queue);
	
	
	return SUCCESS;
}

