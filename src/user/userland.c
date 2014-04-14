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
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "/home/mightydev/BBB/rootfs/home/ubuntu/am335x_edma/src/edma/mighty.h"

int main(int argc, char **argv) {
	/* Our File Descriptor */ 
	int fd;
	int rc = 0;
	char *rd_buf[19];
	int i = 0;
	int value = 0;
	int ret = 0;
	struct mighty_bccnt mighty;

	printf("%s: entered\n", argv[0]);

	/*Open the device */
	fd = open("/dev/mighty_dma", O_RDWR);
	if (fd == -1) {
		perror("open failed");
		rc = fd;
		exit(-1);
	}

	printf("%s: open: successful\n", argv[0]);


	mighty.bcnt = 8;
	mighty.ccnt = 2;

	ret = ioctl(fd, MTY_START, &mighty);

	assert(ret == 0);





	/* Issue a read */
	rc = read(fd, rd_buf, 76);
	if (rc < 0) {
		perror("read failed");
		close(fd);
		exit(-1);
	}
	else{
		for (i = 0; i < 19; i++) {
			printf("Rc = %d", rc);
			value = (int)rd_buf[i];
			 value = value & 0x3ffff; 
			printf("printing value from buffer %#010x \n", value);
		}

	
	}



	printf("%s: read: returning %d bytes!\n", argv[0], rc);

	close(fd);
	return 0;
}
