#ifndef LIBSPC_H
#define LIBSPC_H

#include <stdio.h>      /* 標準輸入輸出定義 */
#include <stdlib.h>     /* 標準函數庫定義 */
#include <unistd.h>     /* Unix 標準函數定義 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>      /* 檔控制定義 */
#include <termios.h>    /* PPSIX 終端控制定義 */
#include <errno.h>      /* 錯誤號定義 */
#include <string.h>
#include <pthread.h>
#include <sys/signal.h>
#include <signal.h>
#include <bits/siginfo.h>
#include <dirent.h>

typedef unsigned char Byte;

class LibSPC
{

public:
    LibSPC();
    void Close();
    bool IsKeyError();

private:
    pthread_t thread_id;

};

#endif // LIBSPC_H
