/*
Mighty DMA driver library and structures 
Copyright Terrence McGuckin for Ephemeron Labs
terrence@ephemeron-labs.com
*/

#define MTY_START		0x1
#define MTY_STOP		0x0

struct  mighty_bccnt{
	unsigned int bcnt;
	unsigned int ccnt;
};
