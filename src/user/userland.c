/* 
 * Simple userspace interface to EDMADEV
 * 
 * Currently designed to work on ARM
 * 	AM335x Cortex-A8 MPU
 *
 * This code will interface to /dev/mighty_dma device node to 
*  	access the mighty_dma.ko kernel module
*/
#include <stdio.h>
//#include <unistd.h>
#include <string.h>
//#include <stdlib.h>

#include <dirent.h>
#include <signal.h>

// Driver header file



#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "/home/mightydev/BBB/rootfs/home/ubuntu/am335x_edma/src/edma/mighty.h"

//#include <u.h>
//#include <libc.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SRV_IP "10.0.1.33"
#define BUFLEN  4096
#define PORT	5009

static void diep(char *s)
{
  perror(s);
  exit(1);
}


int main(int argc, char **argv) {
	/* Our File Descriptor */ 
	int fd;
	int rc = 0;
	//char *rd_buf[19];   //This caused seg fault if it is different GRRR look this up
	int k = 0;
	int value = 0;
	int ret = 0;
	struct mighty_bccnt mighty;
	int char_to_read;
	int data_points;
	int rd;

	//Setup socket stuff
	struct sockaddr_in si_other;
	int s, i, slen=sizeof(si_other);
	char buf[BUFLEN];
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        	diep("socket");

	memset((char *) &si_other, 0, sizeof(si_other));

        si_other.sin_family = AF_INET;
        si_other.sin_port = htons(PORT);
	if (inet_aton(SRV_IP, &si_other.sin_addr)==0) {
          fprintf(stderr, "inet_aton() failed\n");
          exit(1);
	}

	data_points = 8 * 4 + 4;


	char_to_read = 4096;
	char *rd_buf[char_to_read];   // Have to make sure these values add up and set them in main userland control



	printf("%s: entered\n", argv[0]);

	/*Open the device */
	fd = open("/dev/mighty_dma", O_RDWR);
	if (fd == -1) {
		perror("open failed");
		rc = fd;
		exit(-1);
	}

	printf("%s: open: successful\n", argv[0]);

	while(1){	
		/* Issue a read */
		rc = read(fd, rd_buf, char_to_read); // 76 = 4*(header(3) + data(16))
		if (rc < 0) {
			//printf("return value was %d",rc);
			//perror("read failed");
			//close(fd);//will need to take this out in final version
			//exit(-1); // take out this too
			usleep(1000);
		}
		else{
			//rd = rc/4;
			//for (k = 0; k < rd ; k++) {
				//printf("Rc = %d", rc);
				//value = (int)rd_buf[k];
			 	//value = value & 0x3ffff; 
				//printf("printing value from buffer %#010x \n", value);
			//}
			//printf("rc = %d",rc);
			//Send Data out over socket
			//sendto(s, buf, rc, 0, &si_other, slen);
			if (sendto(s, rd_buf, BUFLEN, 0,(struct sockaddr *) &si_other, slen)==-1)
            		diep("sendto()");
			//sendto(s, rd_buf, rc, 0,(struct sockaddr *) &si_other, slen);
			//usleep(100);

		}
	}



	//printf(" read: returning %d bytes!\n", rc);
	close(s);
	close(fd);
	return 0;
}




