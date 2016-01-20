#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>


#define MAX_PAYLOAD 4096
#define NETLINK_USER 17



int nl_receive(void)
{
	struct sockaddr_nl src_addr;
	struct nlmsghdr *nlh = NULL;
	struct iovec iov;
	int sock_fd;
	struct msghdr msg;
	
	
	
	sock_fd = socket(AF_NETLINK, SOCK_RAW, 17);
	if(sock_fd<0){
		printf("Socket creation failed, err = %d \n",sock_fd);
		return FAILIURE;
	}
	memset(&src_addr, 0, sizeof(src_addr));
	
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); /* self pid */

	bind(sock_fd,(struct sockaddr*)&src_addr, sizeof(src_addr));

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));

	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	recvmsg(sock_fd, &msg, 0);
	
	printf("The job output is :\n");
	printf("%s\n", (char *)NLMSG_DATA(nlh));

	close(sock_fd);
	
	return SUCCESS;
}



