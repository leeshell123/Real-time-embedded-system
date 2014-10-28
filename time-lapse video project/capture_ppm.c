#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <time.h>



#define HEIGHT (480)
#define WIDTH (640)
#define HRES_STR "640"  //width
#define VRES_STR "480"  //height
#define capture_signo (SIGRTMAX)
#define tar_upl_signo (SIGRTMAX-1)

char ppm_dumpname[64];
char ppm_header[512];

//^^misc global variables ----------------------------
	int howmanyframes=100;
	int lasthowmany=10;
	char local_path[100];
	char* local_yuvdir="yuvs";
	char* local_tardir="tars";
//	char* remote_user="mith";
//	char* remote_name="192.168.10.20";
//	char* remote_path="/home/mith/capt";
//	char* remote_tardir="tars";
	char* local_ramdisk="/mnt/ram0";
	int exitlevel;
	FILE* fp;						// This file is used for storing time stamping information
	int framecount=0;
	int ramtoflashcount=0;
	int tar_uplcount=0;	
	const int num_services=2;	
	int capture_dropout=0;
	int tar_upl_dropout=0;
	int capture_dropout_tolerence=5;
	int capture_dropout_refresh=100;
	struct timeval capture_init_time;
	struct timeval tar_upl_init_time;
	unsigned char* imgdata;
	char uname_a[256];
	int uname_size;
	char* timestamp;

//^^V4L declarations---------
	int fd;
	struct v4l2_requestbuffers reqbuf;
	enum v4l2_buf_type mytype;
	struct framebuffer {
		void* start;
		size_t len;
	};	
	struct framebuffer* frbuf=NULL;
	struct v4l2_buffer mybuf[2];
//The number of buffers to be allocated
	int nbufs=32;
	fd_set fds;
	struct timeval frtv;
	int k=0;
	int flag=0;
	
	struct v4l2_format fmt;

//^^service 1: capture - declarations --------------------------
	//^^timer and event signalling declarations  -----------
		const double capture_period=10.0;			//		T
		int capture_alive=0;					// To know if capture thread is still alive
		int capture_done=0;
		timer_t capture_timer;				
		struct itimerspec capture_newtspec,capture_oldtspec;	// For service 1: capturing the picture with camera
		void capture_handler(int signo);				// handler when timer interrupt is received		
	//^^Posix threadding declarations ------------------------
	//^^pthread declarations -----------------------
		pthread_t capture_th;
		pthread_attr_t capture_attrib;
		struct sched_param capture_param;
		void* capture_function(void*);			//	this is the function as thread
		const int capture_priority=-1;
//^^service 2: tar_upl - declarations ---------------------------
	//^^timer and event declarations --------------------------
		const double tar_upl_period=7.0;				// calculated 7.3
		int tar_upl_alive=0;							// to tell if the tar_upl thread is still running
		int tar_upl_done=0;
		timer_t tar_upl_timer;
		struct itimerspec tar_upl_newtspec,tar_upl_oldtspec;		// For service 2: tar_upling the images to a remote location
		//void tar_upl_handler(int signo);				// handler when timer interrupt is received
	//^^pthread declarations ----------------------------------
		pthread_t tar_upl_th;
		pthread_attr_t tar_upl_attrib;
		struct sched_param tar_upl_param;
		void* tar_upl_function(void*);			//	this is the function as thread
		const int tar_upl_priority=-50;
//^^service 0 , the scheler or the main function properties --------------
	struct sched_param main_param;
	const int main_priority=-0;
//^^service other: ramtoflash - declarations ---------------------------
	//^^pthread declarations ----------------------------------
		pthread_t ramtoflash_th;
		pthread_attr_t ramtoflash_attrib;
		struct sched_param ramtoflash_param;
		void* ramtoflash_function(void*);			//	this is the function as thread
		const int ramtoflash_priority=-2;
		sem_t S1, S2;

//^^other function prototypes ---------------
	void init(void);							// for initiallization
	void sig_handler_ctrlc(int singo);
	void cleanexit(void);
	int clamp (double);
	void saveyuv(char*, char*);

//^^The init() function creates the directory structure to store the outputs such as frames
void init(void) {
	char buff[256];
	struct dirent** dr;
	int i,j,a,b;
	int fduname;
	int max_prio_fifo;
	int min_prio_fifo;
	time_t timenow;	
	struct sigaction sigact1, sigact2;
	struct sigevent sig_spec1,sig_spec2;
//^^preparing the local filesystem to store the frames --------------- 		
//^^Creates/empties the directory local directories
	sprintf(local_path,"%s",get_current_dir_name());
	sprintf(buff,"rm -r -f %s/%s",local_path,local_yuvdir);
//	system(buff);
//	sprintf(buff,"rm -r -f %s/%s%d",local_path,local_yuvdir, 2);
//	system(buff);
//	sprintf(buff,"rm -r -f %s/%s%d",local_path,local_yuvdir, 3);
//	system(buff);
//	sprintf(buff,"rm -r -f %s/%s%d",local_path,local_yuvdir, 4);
//	system(buff);
//	sprintf(buff,"rm -r -f %s/%s%d",local_path,local_yuvdir, 5);
//	system(buff);
	
	sprintf(buff,"rm -f -r %s/%s",local_path,local_tardir);
	system(buff);
	sprintf(buff,"mkdir %s/%s",local_path,local_yuvdir);
	system(buff);
//	sprintf(buff,"mkdir %s/%s%d",local_path,local_yuvdir, 2);
//	system(buff);
//	sprintf(buff,"mkdir %s/%s%d",local_path,local_yuvdir, 3);
//	system(buff);
//	sprintf(buff,"mkdir %s/%s%d",local_path,local_yuvdir, 4);
//	system(buff);
//	sprintf(buff,"mkdir %s/%s%d",local_path,local_yuvdir, 5);
//	system(buff);
	
	sprintf(buff,"mkdir %s/%s",local_path,local_tardir);
	system(buff);
	
	sprintf(buff,"umount %s",local_ramdisk);
	system(buff);
	sprintf(buff,"mke2fs -m 0 /dev/ram0");
	system(buff);
	sprintf(buff,"mkdir %s",local_ramdisk);
	system(buff);
	sprintf(buff,"mount /dev/ram0 %s",local_ramdisk);
	system(buff);
	sprintf(buff,"rm -r -f %s/*",local_ramdisk);
	system(buff);
		
	sprintf(buff,"echo $(uname -a)>uname_file.txt");
	system(buff);
	fduname=open("uname_file.txt",O_RDONLY);
	if(-1==fduname){
		perror("Error in open()");
		exit(errno);
	}
//printf("%d\n",'\n');
	for(i=0;i<256;i++){
		read(fduname,&buff[i],1);
//printf("%d\t",buf[i]);
		if(10==buff[i]){
			break;
		}
	}
	buff[i]='\0';
	sprintf(uname_a,"uname -a= %s",buff);
//printf("---%s,%d,%d\n",buff,strlen(buff)+1,sizeof(unsigned long long));
	uname_size=strlen(uname_a)+1;
	
	
	
//^^end: prep local filesystem --------------------------------------



	imgdata=(unsigned char*)malloc(HEIGHT*WIDTH*2);		
	timestamp=(char*)malloc(16);
	sprintf(buff,"rm -f %s/timestamp.txt",local_ramdisk);
	system(buff);
	sprintf(buff,"%s/timestamp.txt",local_ramdisk);
	fp=fopen(buff,"a+");
	if(fp==NULL) {
		printf("ERROR: can't opne write data file\n");
		free(imgdata); free(timestamp);
		exit(errno);
	}	
	time(&timenow);
	sprintf(buff,"\n\n===================\t\t====================\nTime stamping results for\
 execution at %s===================\t\t====================\n",ctime(&timenow));
	fwrite(buff,1,strlen(buff),fp);
 	fflush(fp);
//^^V4L stuff -------------------------------
	
fd=open("/dev/video0",O_RDWR|O_NONBLOCK,0);
	if(-1==fd) {
		printf("Can't open LINE %d\n",__LINE__);
	}

	
	
		
	fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width=WIDTH;
	fmt.fmt.pix.height=HEIGHT;
	fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field=V4L2_FIELD_INTERLACED;
	a=ioctl(fd,VIDIOC_S_FMT,&fmt);
	if(-1==a) {
		perror("ERROR: can't set fmt");
		exit(errno);
	}
//printf("<<<%d>>>\n",__LINE__);
//printf("Bytesperline: %d, sizeimage: %d\n",fmt.fmt.pix.bytesperline,fmt.fmt.pix.sizeimage);
//printf("Math: %d\n",fmt.fmt.pix.bytesperline*fmt.fmt.pix.height);
 	reqbuf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory=V4L2_MEMORY_MMAP;
	reqbuf.count=nbufs;
	mytype=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a=ioctl(fd,VIDIOC_REQBUFS,&reqbuf);
	if(-1==a) {
		perror("ERROR: mmap buffer request");
		exit(errno);
	}
	printf("Number of buffers allocated is %d\n",reqbuf.count);
	if(reqbuf.count<nbufs) {
		printf("ERROR: REQBUFS out of memory\n");
		exit(-1);
	}
//printf("<<<%d>>>\n",__LINE__);
	nbufs=reqbuf.count;
	frbuf=(struct framebuffer*)calloc(reqbuf.count,sizeof(*frbuf));
	if(!frbuf) {
		printf("ERROR: out of memory at line %d\n",__LINE__);
		exit(-1);
	}

//Quering each and every buffer that has been allocated
for(i=0;i<reqbuf.count;i++) {
	mybuf[1].type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	mybuf[1].memory=V4L2_MEMORY_MMAP;
	mybuf[1].index=i;
	a=ioctl(fd,VIDIOC_QUERYBUF,&mybuf[1]);


	if(-1==a) {
		perror("ERROR: QUERYBUF failed");
		exit(-1);
	}
//printf("<<<%d>>>\n",__LINE__);
	
		frbuf[i].len=mybuf[1].length;
		frbuf[i].start=mmap(NULL,mybuf[1].length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,mybuf[1].m.offset);
		if(MAP_FAILED==frbuf[0].start) {
			perror("ERROR: mmap failed");
			exit(-1);
		}
	}



//printf("<<<%d>>>\n",__LINE__);	
		FD_ZERO(&fds);
	FD_SET(fd,&fds);
//printf("[%d]\n",__LINE__);	

	for(j=0;j<2;j++) {
		mybuf[1].type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		mybuf[1].memory=V4L2_MEMORY_MMAP;
		mybuf[1].index=j;


		a=ioctl(fd,VIDIOC_QBUF,&mybuf[1]);
		if(-1==a) {
			perror("ERROR: QBUF");
			exit(errno);
		}


	}

//Will wait till the device is free when the STREAMON is called.
	a=ioctl(fd,VIDIOC_STREAMON,&mytype);
	while(a==-1)
	{a=ioctl(fd,VIDIOC_STREAMON,&mytype);
	}
	
	if(-1==a) {
		perror("ERROR- STREAMON");
		free(imgdata); free(timestamp);
		exit(errno);
	}
		
	frtv.tv_sec=1;
	frtv.tv_usec=0;
	a=select(fd+1,&fds,NULL,NULL,&frtv);
	if(-1==a) {
		perror("ERROR - select");
		free(imgdata); free(timestamp);
		exit(errno);
	}

//^^end: v4l stuff --------------------------	
	


//^^pthread and semaphores init ---------------------------------
	max_prio_fifo=sched_get_priority_max(SCHED_FIFO);
	min_prio_fifo=sched_get_priority_min(SCHED_FIFO);	
//^^setting up main or scheduler thread	
	sched_getparam(getpid(),&main_param);
	main_param.sched_priority=max_prio_fifo+main_priority;
	sched_setscheduler(getpid(),SCHED_FIFO,&main_param);	
//^^setting up capture thread	
	pthread_attr_init(&capture_attrib);
	pthread_attr_setinheritsched(&capture_attrib,PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&capture_attrib,SCHED_FIFO);
	pthread_attr_setdetachstate(&capture_attrib,PTHREAD_CREATE_DETACHED);
	sched_getparam(getpid(),&capture_param);
	capture_param.sched_priority=max_prio_fifo+capture_priority;
	pthread_attr_setschedparam(&capture_attrib,&capture_param);		
	
	pthread_attr_init(&ramtoflash_attrib);
	pthread_attr_setinheritsched(&ramtoflash_attrib,PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&ramtoflash_attrib,SCHED_FIFO);
	pthread_attr_setdetachstate(&ramtoflash_attrib,PTHREAD_CREATE_DETACHED);
	sched_getparam(getpid(),&ramtoflash_param);
	ramtoflash_param.sched_priority=max_prio_fifo+ramtoflash_priority;
	pthread_attr_setschedparam(&ramtoflash_attrib,&ramtoflash_param);		

//^^end: pthread and semaphore init --------------------------


	signal(SIGINT,sig_handler_ctrlc);			// to handle Ctrl+c pressed in terminal
	

}


unsigned char bigbuffer[(2560*1920)];
static void dump_ppm(const void *p, int size, unsigned int tag, struct timespec *time)
{
	char buff[256];
    int written, i, total, dumpfd;
   
    //sprintf(ppm_dumpname, "test-%d.ppm", tag);
    sprintf(buff,"%s/test-%d.ppm",local_ramdisk,tag);
    dumpfd = open(buff,(O_RDWR|O_CREAT),0666);
    if(-1==dumpfd)
    {
    	perror("ERROR: ppm file open");
    	free(imgdata);
    	free(timestamp);
    	exit (errno);
    }
//    sprintf(ppm_header, "P6\n#9999999999 sec 9999999999 msec \n#%s\n"HRES_STR" "VRES_STR"\n255\n", uname_a);
//    snprintf(&ppm_header[4], 11, "%010d", (int)time->tv_sec);
//	strncat(&ppm_header[14], " sec ", 5);
//	snprintf(&ppm_header[19], 11, "%010d", (int) ((time->tv_nsec) / 1000000));
//	char newbuf[128];
//	sprintf(newbuf, " msec \n#%s\n"HRES_STR" "VRES_STR"\n255\n", uname_a);
//	strncat(&ppm_header[29], newbuf, strlen(newbuf));
    
    sprintf(ppm_header, "P6\n#%010d sec %010d msec \n#%s\n"HRES_STR" "VRES_STR"\n255\n", ((int)time->tv_sec), ((int) ((time->tv_nsec) / 1000000)), uname_a);
	written = write(dumpfd, ppm_header, strlen(ppm_header));
	//printf("ppm header len: %d\n", strlen(ppm_header));
	//printf("%s", ppm_header);
	//fwrite(buff,1,strlen(buff),fp);

    total=0;

    do
    {
        written=write(dumpfd, p, size);
        total+=written;
    } while(total < size);

    printf("wrote %d bytes\n", total);

    close(dumpfd);
    
}

void yuv2rgb(int y, int u, int v, unsigned char *r, unsigned char *g, unsigned char *b)
{
   int r1, g1, b1;

   // replaces floating point coefficients
   int c = y-16, d = u - 128, e = v - 128;       

   // Conversion that avoids floating point
   r1 = (298 * c           + 409 * e + 128) >> 8;
   g1 = (298 * c - 100 * d - 208 * e + 128) >> 8;
   b1 = (298 * c + 516 * d           + 128) >> 8;

   // Computed values may need clipping.
   if (r1 > 255) r1 = 255;
   if (g1 > 255) g1 = 255;
   if (b1 > 255) b1 = 255;

   if (r1 < 0) r1 = 0;
   if (g1 < 0) g1 = 0;
   if (b1 < 0) b1 = 0;

   *r = r1 ;
   *g = g1 ;
   *b = b1 ;
}

void* capture_function(void* arg) {
	//capture_alive=1;
	char buff[512];
	int i,j,a;
	int oldi, newi, newsize=0;
	struct timespec frame_time;
	int y_temp, y2_temp, u_temp, v_temp;
	int fpyuv=-1;
	int x=0;
	double time_in_sec,time_in_sec2;
	struct timeval timenow;
	int* count;
	int yuv_header_len;
	unsigned long long yuv_time_usec;




////printf("[%d]\n",__LINE__);	
	count=(int*)arg;
////printf("[%d]\n",__LINE__);	
////printf("--- %d,%d\n",howmanyframes,*count);
//
//	if(howmanyframes==*count){						// done
//		timer_delete(capture_timer);
//		capture_done=1;
//		capture_alive=0;
//		return;
//	}
////printf("[%d]\n",__LINE__);	

//the while condition is gathering those many frames in one go.
while(1)
{

//this delay is to make the buffers ready
//usleep(1000000);
	
	gettimeofday(&timenow, NULL); 				// Taking end time stamp
	time_in_sec=-capture_init_time.tv_sec-(double)capture_init_time.tv_usec/1000000+timenow.tv_sec+(double)timenow.tv_usec/1000000;	
//printf("[%d]\n",__LINE__);	
	sprintf(buff,"CAPTURE started AT TIME %f seconds\n",time_in_sec);
	printf(buff,"CAPTURE started AT TIME %f seconds\n",time_in_sec);
//printf("[%d]\n",__LINE__);	
	fwrite(buff,1,strlen(buff),fp);
//^^Begin task---------------------
	//printf("CAPTURING ...................\n");

int b;

//indexing the buffers in a FIFO manner
if (k>=32)
{
k=0;
}
	mybuf[1].type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	mybuf[1].memory=V4L2_MEMORY_MMAP;
	mybuf[1].index=k;

	a=ioctl(fd,VIDIOC_DQBUF,&mybuf[1]);
	
k++;
//printf("[%d]\n",__LINE__);
	/*while(a==-1)
	{//printf("hereeee\n");
	a=ioctl(fd,VIDIOC_DQBUF,&mybuf[1]);}	*/
	if(-1==a) {
		
		if(EAGAIN==errno) {
//printf("(%d)\n",j);
//b=ioctl(fd,VIDIOC_QBUF,&mybuf[1]);
printf("in the DQbuf\n");
			//continue;
		}
	}

//printf("[%d]\n",__LINE__);
	if(mybuf[1].index>=nbufs){
		free(imgdata); free(timestamp);
		exit(-1);
	}
// here is image

//		sprintf(buff, "%s/pic-%d.yuv", local_ramdisk, framecount + 1);
////printf("---%s,%d\n",buff,strlen(buff));	
//		fpyuv = open(buff, (O_RDWR | O_CREAT), 0666);
//		if (-1 == fpyuv) {
//			perror("ERROR: yuv file open");
//			free(imgdata);
//			free(timestamp);
//			exit (errno);
//		}
////printf("[%d]\n",__LINE__);
//		yuv_time_usec = floor(time_in_sec * 1000000);
////printf("[%d]\n",__LINE__);
//		write(fpyuv, (void*) &time_in_sec, 8);
////printf("[%d]\n",__LINE__);
//		write(fpyuv, (void*) &uname_size, 4);
//		write(fpyuv, (void*) uname_a, uname_size);
//		write(fpyuv, (void*) frbuf[1].start, HEIGHT * WIDTH * 2);
//		close(fpyuv);
	
	unsigned char* pptr = (unsigned char*)frbuf[1].start;
	int size = frbuf[1].len;
	//int size = HEIGHT*WIDTH*2;
	printf("Dump YUYV converted to RGB size %d\n", size);

		// Pixels are YU and YV alternating, so YUYV which is 4 bytes
		// We want RGB, so RGBRGB which is 6 bytes
		//
		for (oldi = 0, newi = 0; oldi < size; oldi = oldi + 4, newi = newi + 6) {
			y_temp = (int) pptr[oldi];
			u_temp = (int) pptr[oldi + 1];
			y2_temp = (int) pptr[oldi + 2];
			v_temp = (int) pptr[oldi + 3];
			yuv2rgb(y_temp, u_temp, v_temp, &bigbuffer[newi],
					&bigbuffer[newi + 1], &bigbuffer[newi + 2]);
			yuv2rgb(y2_temp, u_temp, v_temp, &bigbuffer[newi + 3],
					&bigbuffer[newi + 4], &bigbuffer[newi + 5]);
		}
		
		struct timespec frame_time;
		clock_gettime(CLOCK_REALTIME, &frame_time);
		
		dump_ppm(bigbuffer, ((size*6)/4), (framecount+1), &frame_time);
		pthread_create(&ramtoflash_th,&ramtoflash_attrib,ramtoflash_function,(void*)(*count));
		pthread_join(ramtoflash_th, NULL);
		
	
	printf("framecount is %d\n",(framecount+1));


	a=ioctl(fd,VIDIOC_QBUF,&mybuf[1]);
	
	if(-1==a) {
		perror("ERROR: QBUF unsuccessful");
		free(imgdata); free(timestamp);
		exit(errno);
	}
		

	(*count)++;
	framecount=(*count);

//^^End task ------------------------------
	gettimeofday(&timenow, NULL); 				// Taking end time stamp
	time_in_sec2=-capture_init_time.tv_sec-(double)capture_init_time.tv_usec/1000000+timenow.tv_sec+(double)timenow.tv_usec/1000000;	
	sprintf(buff,"@CAPTURE ended AT TIME %f seconds\t[%f sec]\n",time_in_sec2,time_in_sec2-time_in_sec);
	printf("@CAPTURE ended AT TIME %f seconds\t[%f sec] %d\n",time_in_sec2,(time_in_sec2-time_in_sec),count);
	



	fwrite(buff,1,strlen(buff),fp);	
	sem_post(&S2);
	sem_wait (& S1);
}
	
}


void* ramtoflash_function(void* arg){
	char buff[128];
	int a,i,b,fdram;
	b=(int)(arg);
	//int folder;
	//folder=((int)floor((b+1)/500))+1;
	sprintf(buff,"mv %s/test-%d.ppm %s/%s",local_ramdisk,b+1,local_path,local_yuvdir);
	system(buff);
	ramtoflashcount++;

}

void sig_handler_ctrlc(int signo) {
	char buff[256];
	fflush(fp);	
	fclose(fp);
	sprintf(buff,"cat %s/timestamp.txt %s/timestamp.txt>temp.txt",local_ramdisk,local_path);
	system(buff);
	sprintf(buff,"mv -f %s/temp.txt %s/timestamp.txt",local_path,local_path);
	system(buff);
	printf("\nInterrupt from user. Exiting...\n");\
	free(imgdata); free(timestamp);
	//cleanexit();
	exit(-1);
}
	
void cleanexit(void) {
	char buff[200];
	int a;	
	double time_in_sec;
	struct timeval timenow;	
	
	a=ioctl(fd,VIDIOC_STREAMOFF,&mytype);
	if(-1==a) {
		perror("ERROR- STREAMOFF");
		free(imgdata); free(timestamp);
		exit(errno);
	}

	gettimeofday(&timenow, NULL); 				// Taking end time stamp
	time_in_sec=-capture_init_time.tv_sec-(double)capture_init_time.tv_usec/1000000+timenow.tv_sec+(double)timenow.tv_usec/1000000;	

	sprintf(buff,"-------------------\tSummary\t-------------------------------\n");
	fwrite(buff,1,strlen(buff),fp);	
	sprintf(buff,"Capture period: %f sec\nConvert period: %f sec\nCapture dropout %d times\nConvert dropouts %d times\n",\
		capture_period,tar_upl_period,capture_dropout,tar_upl_dropout);
	fwrite(buff,1,strlen(buff),fp);
	sprintf(buff,"Total realtime elapsed is: %f sec\nTotal number of frames captured is: %d\n",time_in_sec,framecount);
	fwrite(buff,1,strlen(buff),fp);

	sprintf(buff,"-------------------\tEnd of this session-------------------------------\n");
	fwrite(buff,1,strlen(buff),fp);


	fflush(fp);
	fclose(fp);
	sprintf(buff,"cat %s/timestamp.txt %s/timestamp.txt>temp.txt",local_ramdisk,local_path);
	system(buff);
	sprintf(buff,"mv -f %s/temp.txt %s/timestamp.txt",local_path,local_path);
	system(buff);

	free(imgdata); free(timestamp);
	exit(0);

}

int main(int argc, char* argv[]) {
	init();
	sem_init(&S1, 0, 0);
	sem_init(&S2, 0, 0);
	static int count=0;
	pthread_create(&capture_th,&capture_attrib,capture_function,(void*)&count);
	while(1)
	{
		sem_wait (& S2);
		printf("in main thread\n");
		usleep(919000);
		sem_post (& S1);
	}
	pthread_join(capture_th, NULL);
}
