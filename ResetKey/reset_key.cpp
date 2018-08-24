#include <stdio.h>      /* 標準輸入輸出定義 */
#include <stdlib.h>     /* 標準函數庫定義 */
#include <unistd.h>     /* Unix 標準函數定義 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>      /* 檔控制定義 */
#include <termios.h>    /* PPSIX 終端控制定義 */
#include <errno.h>      /* 錯誤號定義 */
#include <string.h>
#include <pthread.h>
#include <sys/signal.h>
#include <signal.h>
#include <bits/siginfo.h>
#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyACM0" //Arduino SA Mega 2560 R3 (CDC ACM)
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;  
int fd;    
struct termios oldtio,newtio;    

int main(void)
{
  /* open the device to be non-blocking (read will return immediatly) */
  fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);


  tcgetattr(fd,&oldtio); /* save current port settings */
  /* set new port settings for canonical input processing */
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR | ICRNL;
  newtio.c_oflag = 0;
  newtio.c_lflag = 0;
  newtio.c_cc[VMIN]=1;
  newtio.c_cc[VTIME]=0;
  tcflush(fd, TCIOFLUSH);
  tcsetattr(fd,TCSANOW,&newtio);

 
  int nwrite = -1;
  if((nwrite = write(fd, "{3|", 4))>0)
   {
	  printf("nwrite = %d\n", nwrite);
   } 

   sleep(1);
  
  int nread = -1;
  char received[512] = {'\0'};
  if((nread = read(fd, received, 512))>0)	printf("%s\n", received);
  if((nread = read(fd, received, 512))>0)	printf("%s\n", received);
  if((nread = read(fd, received, 512))>0)	printf("%s\n", received);
  if((nread = read(fd, received, 512))>0)	printf("%s\n", received);
  if((nread = read(fd, received, 512))>0)	printf("%s\n", received);
  if((nread = read(fd, received, 512))>0)	printf("%s\n", received);
  
  /* restore old port settings */
  tcsetattr(fd,TCSANOW,&oldtio);
  
  close(fd);
  printf( "End of Thread\n");

  return 0;
}
