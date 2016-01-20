#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "common.h"


int main(int argc,  char *argv[])
{
	int rc, netlink = 0, eflag=0, pflag=0, option;
	int err = 0, argslen =0; 
	struct job *job;
	long pass_len;	
	unsigned char *str;
	unsigned long crc32;
	
	
	/* Missing arguments check*/
	if( argc < 2 ){
		printf("Input format:./xhw3 job_type infile outfile...\n");
		rc=-EINVAL;
		errno=EINVAL;
		goto out;
	}
	
	
	job = malloc(sizeof(struct job));
	if(job == NULL){
		rc= -ENOMEM;
		errno= ENOMEM;
		goto out;
	}
	
	
	job->key = NULL;
	job->priority = LOW_PRIORITY;
	
	while( ( option = getopt( argc, argv, "clr:CEDHLk:" ) ) != -1 )
	{	
 		switch(option)
		{	
		
			case 'c' : 
			netlink = 1;
			break;
			
			case 'l' : 
			eflag ++;
			job->job_type = LIST;
			break;
			
			case 'r' : 
			eflag++;
			job->job_type = REMOVE;
			argslen = atoi(optarg);
			
			break;
				
			case 'C' : 
			eflag++;
			job->job_type = CHECKSUM;
			break;

			case 'D' : 
			eflag++;
			job->job_type = DECRYPT;
			break;

			case 'E' : 
			eflag++;
			job->job_type = ENCRYPT;
			break;	

			case 'H' : 
			job->priority = HIGH_PRIORITY;
			break;
				
			case 'L' : 
			job->priority = LOW_PRIORITY;
			break;
				

			case 'k' : 
			pflag = 1;
			str = (unsigned char *)optarg;
			break;	
			
			case '?' :
			rc=-EINVAL;
			errno=EINVAL;
			printf("Error: Incorrect syntax  entered\nError %d", errno);
			goto out;

		}

	}

	if (eflag != 1) {	
		printf("%s: wrong -E/-D/-C/-l/-r option\n", argv[0]);
		err=-1;
		goto out;
	} 
	else if ((pflag == 0)&&(job->job_type == ENCRYPT ||job->job_type == DECRYPT)) {	
		printf("%s: missing -k option\n", argv[0]);
		err=-1;
		goto out;
		
	} else if ((optind) > argc) {	
		/* need at least one argument (change +1 to +2 for two, etc. as needeed) */
		printf("optind = %d, argc=%d\n", optind, argc);
		printf("%s: missing name\n", argv[0]);
		goto out;
		
	} else if (err) {
		err=-1;
		goto out;
	}
	
	if (optind < argc)	{/* these are the arguments after the command-line options */
		job->infile=argv[optind];
		if(job->job_type == ENCRYPT ||job->job_type == DECRYPT)
		job->outfile=argv[optind+1];
	}
	else {
		if(job->job_type == ENCRYPT ||job->job_type == DECRYPT){
		printf("Input and output filename not specified, XHW3 FAILED\n");
		err = -1;
		goto out;
	}
	}
	

	/* setting process id for callback */
	job->pid = getpid();
	
	if(job->job_type == ENCRYPT ||job->job_type == DECRYPT){
		
		job->key = malloc(MD5_DIGEST_LENGTH);
		if(job->key == NULL){
			rc= -ENOMEM;
			errno= ENOMEM;
			goto out;
		}
		
		pass_len=strlen((char *)str);
		
		MD5(str,pass_len,job->key);
	}
	
	else if(job->job_type == CHECKSUM){
		job->crc32 =&crc32;
	}
	else if(job->job_type == LIST){
		
		job->key = malloc(MY_BUFFER_SIZE);
		if(job->key == NULL){
			rc= -ENOMEM;
			errno= ENOMEM;
			goto out;
		}

	}
	
	/* callback */
	if (netlink ==1 && job->job_type != LIST){
		nl_init();	
	}
	
	rc = syscall(__NR_submitjob, (void *)job, argslen);
	
	if(job->job_type == CHECKSUM){
		printf("crc32 is %lu\n",crc32);
	}
	
	else if(job->job_type == LIST){
		printf("List of jobs in queue is :\n");
		printf("%s\n", job->key);	
		
	}
	

	if (rc == 0)
		printf("syscall returned %d\n", rc);
	else
		printf("syscall returned %d (errno=%d)\n", rc, errno);

	/* callback */
	if (netlink ==1 && job->job_type != LIST){
		nl_receive();	
	}
	
	
	out:
		
		if(job->key) free(job->key);
		if(job) free(job);
		exit(rc);
}
