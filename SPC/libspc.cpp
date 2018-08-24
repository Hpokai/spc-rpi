#include "libspc.h"

#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyACM0" //Arduino SA Mega 2560 R3 (CDC ACM)
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;
int fd;
struct termios oldtio, newtio, tempio;
Byte* r_data, *s_command;	//receiced "Data", sent "command".
int loop = 0;
bool isCorrectData = false, isInfoPrepared = false, isKeyError = false;

enum ACTION {NONE, CKD_STATE, WAIT_STATE, SEND_INFO, WAIT_KEY, RUN_TIME_CHECK, WAIT_RANDOM_CHECK, RANDOM_TIMER};
ACTION ACT = NONE;

static Byte* KEY = NULL;
int random_times = 0;
int usleep_in_mainloop = 1000000;
int check_times = 0;

int get_random_value ( int min, int max, bool isON = true )
{
    if ( true == isON )	srand ( time ( NULL ) );

    return ( rand() % ( max - min + 1 ) ) + min;
}

int parse_dongle_key ( Byte* r_key )
{
    int ret = -1;
    //printf("key - %s\n", r_key);
    Byte* one = ( r_key + 32 );

    if ( 0 != strcmp ( ( char* ) one, "1" ) )
    {
        puts ( "dongle_key 32 is not 1" );
    }
    else
    {
        for ( int i = 0; i < 16; i++ )
        {
            Byte temp_str[2] = {'\0'};
            temp_str[0] = * ( r_key + i * 2 );
            temp_str[1] = * ( r_key + i * 2 + 1 );
            * ( KEY + i ) = ( Byte ) strtol ( ( char* ) temp_str, NULL, 16 );
        }

        * ( KEY + 16 ) = ( Byte ) 0x01;
        //printf("KEY - %s\n", KEY);
        ret = 0;
    }

    return ret;
}

Byte* parse_received_data ( Byte data[] )
{
    int start_index = -1, end_index = -1;

    char *pch_s = strstr ( ( char* ) data, "7D" );

    if ( pch_s != NULL )	start_index = ( int ) ( pch_s - ( char* ) data ) + 2;

    char *pch_e = strstr ( ( char* ) data, "7E" );

    if ( pch_e != NULL )	end_index = ( int ) ( pch_e - ( char* ) data );

    Byte* temp_data = NULL;

    if ( ( pch_s != NULL ) && ( pch_e != NULL )	)
    {
        int length = end_index - start_index + 1;
        temp_data = ( Byte * ) malloc ( length * sizeof ( Byte ) );

        for ( int i = 0; i < length; i++ )	* ( temp_data + i ) = 0;

        for ( int i = 0; i < ( length - 1 ); i++ )	* ( temp_data + i ) = data[start_index + i];

        //printf("data=%s\n",data);
        //printf("temp_data=%s\n",temp_data);

        isCorrectData = true;
        return temp_data;
    }
    else
    {
        puts ( "NOT correct data!" );
        return NULL;
    }
}

Byte select_cpu_type ( char data[] )
{
    char* pch = NULL;
    Byte ret = '\0';

    if ( ( pch = strstr ( data, "Intel" ) ) != NULL )
    {
        if ( ( pch = strstr ( data, "Celeron" ) ) != NULL )			ret = ( Byte ) ( 0x21 );
        else if ( ( pch = strstr ( data, "Atom" ) ) != NULL )		ret = ( Byte ) ( 0x22 );
        else if ( ( pch = strstr ( data, "Pentium" ) ) != NULL )		ret = ( Byte ) ( 0x23 );
        else if ( ( pch = strstr ( data, "Core i3" ) ) != NULL )		ret = ( Byte ) ( 0x24 );
        else if ( ( pch = strstr ( data, "Core i5" ) ) != NULL )		ret = ( Byte ) ( 0x25 );
        else if ( ( pch = strstr ( data, "Core i7" ) ) != NULL )		ret = ( Byte ) ( 0x26 );
        else if ( ( pch = strstr ( data, "Xeon E3" ) ) != NULL )		ret = ( Byte ) ( 0x27 );
        else if ( ( pch = strstr ( data, "Xeon E5" ) ) != NULL )		ret = ( Byte ) ( 0x28 );
        else 														ret = ( Byte ) ( 0x29 );
    }
    else if ( ( pch = strstr ( data, "AMD" ) ) != NULL )
    {
        if ( ( pch = strstr ( data, "Pro A" ) ) != NULL )			ret = ( Byte ) ( 0x31 );
        else if ( ( pch = strstr ( data, "A" ) ) != NULL )		ret = ( Byte ) ( 0x32 );
        else if ( ( pch = strstr ( data, "C" ) ) != NULL )		ret = ( Byte ) ( 0x33 );
        else if ( ( pch = strstr ( data, "E" ) ) != NULL )		ret = ( Byte ) ( 0x34 );
        else if ( ( pch = strstr ( data, "FX" ) ) != NULL )		ret = ( Byte ) ( 0x35 );
        else if ( ( pch = strstr ( data, "GX" ) ) != NULL )		ret = ( Byte ) ( 0x36 );
        else if ( ( pch = strstr ( data, "Ryzen" ) ) != NULL )	ret = ( Byte ) ( 0x37 );
        else if ( ( pch = strstr ( data, "Z" ) ) != NULL )		ret = ( Byte ) ( 0x38 );
        else 													ret = ( Byte ) ( 0x39 );
    }
    else
    {
        if ( ( pch = strstr ( data, "BCM" ) ) != NULL )				ret = ( Byte ) ( 0x31 );
        else if ( ( pch = strstr ( data, "Qualcomm" ) ) != NULL )	ret = ( Byte ) ( 0x41 );
        else if ( ( pch = strstr ( data, "Atmel" ) ) != NULL )		ret = ( Byte ) ( 0x51 );
        else	ret = ( Byte ) ( 0x61 );
    }

    return ret;
}

Byte get_cpu_type_info()
{
    FILE *cpu_info = popen ( "grep 'Hardware' /proc/cpuinfo", "r" );

    size_t n;
    char get_data[64] = {'\0'}, parse_data[64] = {'\0'};
    Byte return_byte = {'\0'};

    if ( ( n = fread ( get_data, 1, sizeof ( get_data ) - 1, cpu_info ) ) <= 0 )	puts ( "Get Model Name Fail!!\n" );
    else
    {
        //puts(get_data);
        char* pch = strstr ( get_data, ":" );

        if ( pch != NULL )
        {
            memmove ( parse_data, ( pch + 1 ), strlen ( pch ) - 1 );
            return_byte = select_cpu_type ( parse_data );
        }
        else
        {
            puts ( "ptr is null" );
            return_byte = ( Byte ) ( 0xFF );
        }
    }

    pclose ( cpu_info );
    //printf("CPU return_byte=%x\n",return_byte);
    return return_byte;
}

Byte select_memory_size ( char data[] )
{
    double memory_size = 0.0;
    Byte ret = '\0';
    int ram_kb = -1;

    if ( sscanf ( data, " %d kB", &ram_kb ) == 1 )
    {
        memory_size = ram_kb / 1024;
        memory_size  = memory_size / 1024;
        //printf("mem= %lf\n",memory_size);

        if ( 0.85 > memory_size )									ret = ( Byte ) ( 0x22 );		// less than 1G
        else if ( 1.85 > memory_size && 0.85 <= memory_size )		ret = ( Byte ) ( 0x33 );		// between 1G ~ 2G
        else if ( 3.85 > memory_size && 1.85 <= memory_size )		ret = ( Byte ) ( 0x44 );		// between 2G ~ 4G
        else if ( 7.85 > memory_size && 3.85 <= memory_size )		ret = ( Byte ) ( 0x55 );		// between 4G ~ 8G
        else if ( 15.85 > memory_size && 7.85 <= memory_size )	ret = ( Byte ) ( 0x66 );		// between 8G ~ 16G
        else 														ret = ( Byte ) ( 0x77 );		// more than 16G
    }

    return ret;
}

Byte get_memory_size_info()
{
    Byte return_byte = {'\0'};

    size_t n;
    char get_data[64] = {'\0'}, parse_data[64] = {'\0'};
    FILE *mem_info = popen ( "grep 'MemTotal' /proc/meminfo", "r" );

    if ( ( n = fread ( get_data, 1, sizeof ( get_data ) - 1, mem_info ) ) <= 0 )	puts ( "Get Mem Total Fail!!\n" );
    else
    {
        //puts(get_data);
        char* pch = strstr ( get_data, ":" );

        if ( pch != NULL )
        {
            memmove ( parse_data, ( pch + 1 ), strlen ( pch ) - 1 );
            return_byte = select_memory_size ( parse_data );
        }
        else
        {
            puts ( "ptr is null" );
            return_byte = ( Byte ) ( 0xFF );
        }
    }

    pclose ( mem_info );
    //printf("MEM return_byte=%x\n",return_byte);
    return return_byte;
}

Byte* get_ethernet_info()
{
    size_t n;
    char get_data[64] = {'\0'};
    Byte* return_byte = ( Byte* ) malloc ( 13 * sizeof ( Byte ) );

    for ( int i = 0; i < 13; i++ )
    {
        * ( return_byte + i ) = ( Byte ) 0xFF;
    }

    * ( return_byte + ( strlen ( ( char* ) return_byte ) - 1 ) ) = '\0';

    FILE *ethernet_info = fopen ( "/sys/class/net/eth0/address", "r" );

    if ( ( n = fread ( get_data, 1, sizeof ( get_data ) - 1, ethernet_info ) ) <= 0 )
    {
        puts ( "Get eth0 MAC Fail!!\n" );		// send FF:FF:FF:FF:FF:FF
    }
    else
    {
        //puts(get_data);
        char* token = strtok ( get_data, ":" );
        * ( return_byte + 0 ) = ( Byte ) strtol ( token, NULL, 16 );

        for ( int i = 1; i < 6; i++ )
        {
            token = strtok ( NULL, ":" );
            * ( return_byte + i ) = ( Byte ) strtol ( token, NULL, 16 );
            //printf("i=%X\n",*(return_byte+i));
        }
    }

    ethernet_info = fopen ( "/sys/class/net/wlan0/address", "r" );

    if ( ( n = fread ( get_data, 1, sizeof ( get_data ) - 1, ethernet_info ) ) <= 0 )
    {
        puts ( "Get wlan0 MAC Fail!!\n" );		// send FF:FF:FF:FF:FF:FF
    }
    else
    {
        //puts(get_data);
        char* token = strtok ( get_data, ":" );
        * ( return_byte + 6 ) = ( Byte ) strtol ( token, NULL, 16 );

        for ( int i = 1; i < 6; i++ )
        {
            token = strtok ( NULL, ":" );
            * ( return_byte + 6 + i ) = ( Byte ) strtol ( token, NULL, 16 );
        }
    }

    fclose ( ethernet_info );
    //for (int i = 0; i < 13; i++)	printf("return_byte=%X\n",*(return_byte+i));
    return return_byte;
}

Byte* get_tranform_key ( char* word )
{
    Byte* return_byte = ( Byte* ) malloc ( ( 61 - 3 + 1 + 1 ) * sizeof ( Byte ) );

    for ( int i = 0; i < ( 61 - 3 + 1 + 1 ); i++ )
    {
        * ( return_byte + i ) = 0;
    }

    for ( int i = 0; i < ( 61 - 3 + 1 ); i++ )
    {
        int temp = get_random_value ( ( int ) 0x01, ( int ) 0xFF, false );

        if ( temp > 0x7A && temp < 0x7F )   temp -= 10;

        * ( return_byte + i ) = ( Byte ) temp;
    }

    //for (int i = 0; i < 60; i++)	printf("%d, get_random_value= %X\n",i,*(return_byte+i));


    Byte* inverse_key = ( Byte* ) malloc ( 18 * sizeof ( Byte ) );

    for ( int i = 0; i < 18; i++ )
    {
        * ( inverse_key + i ) = 0;
    }

    for ( int i = 0; i < 17; i++ )
    {
        * ( inverse_key + i ) = ~* ( KEY + i );
    }

    //for ( int i = 0; i < 17; i++ )	printf ( "%d, KEY=%X\n", i, * ( KEY + i ) );
    //for ( int i = 0; i < 17; i++ )	printf ( "%d, inverse_key=%X\n", i, * ( inverse_key + i ) );

    int prime[17]	= {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59};
    int even[12]	= {2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24};
    int odd[16]	= {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};

    int n = 0;
    printf ( "word= %X\n", *word );

    switch ( *word )
    {
    case ( 0x5E ) :

        //puts("0x5E");
        for ( int i = 0; i < 59; i++ )
        {
            if ( i == ( prime[n] - 1 ) )
            {
                //printf("n = %d: prime = %d, i = %d\n",n, prime[n],i);
                memmove ( ( return_byte + i ), ( inverse_key + n ), 1 );
                n++;
            }
        }

        break;

    case ( 0x5F ) :

        //puts("0x5F");
        for ( int i = 0; i < 24; i++ )
        {
            if ( i == ( even[n] - 1 ) )
            {
                //printf("even = %d, i = %d\n",even[n],i);
                memmove ( ( return_byte + i ), ( inverse_key + n ), 1 );
                n++;
            }
        }

        break;

    case ( 0x60 ) :

        //puts("0x60");
        for ( int i = 0; i < 31; i++ )
        {
            if ( i == ( odd[n] - 1 ) )
            {
                //printf("odd = %d, i = %d\n",odd[n],i);
                memmove ( ( return_byte + i ), ( inverse_key + n ), 1 );
                n++;
            }
        }

        break;

    default:
        break;
    }

    //for (int i = 0; i < (int)strlen((char*)return_byte); i++)	printf("%d, return_byte= %X\n",i,*(return_byte+i));
    return return_byte;
}

Byte* set_RUN_TIME_CHECK_command()
{
    // (StartByte) + (CmdByte) + (CountBytes) + (KeyBytes) + (EndByte) + '\0' = 1+1+14+1+1 = 18
    Byte* cmd = ( Byte * ) malloc ( 64 * sizeof ( Byte ) );

    for ( int i = 0; i < 64; i++ )
    {
        * ( cmd + i ) = 0;
    }

    * ( cmd + 0 ) = ( Byte ) ( 0x7B );							//Start Byte
    * ( cmd + 1 ) = ( Byte ) ( 0x27 + get_random_value ( 1, 6 ) );	//CMD Byte
    * ( cmd + 2 ) = ( Byte ) ( 0x5D + get_random_value ( 1, 3 ) );	//Count Byte
    //* ( cmd + 2 ) = ( Byte ) ( 0x5F );
    //printf("cmd+2= %X\n",*(cmd+2));
    memmove ( ( cmd + 3 ), get_tranform_key ( ( char* ) ( cmd + 2 ) ), ( 61 - 3 + 1 ) );		//para3~61: Transform Data Bytes
    * ( cmd + 62 ) = ( Byte ) ( 0x7C );							//EndByte


    //for ( int i = 0; i < ( int ) strlen ( ( char* ) cmd ); i++ )	printf ( "%d, cmd= %X\n", i, * ( cmd + i ) );

    return cmd;
}

Byte* set_SEND_INFO_command()
{
    // (StartByte) + (CmdByte) + (ParameterBytes) + (EndByte) + '\0' = 1+1+14+1+1 = 18
    Byte* cmd = ( Byte * ) malloc ( 18 * sizeof ( Byte ) );

    for ( int i = 0; i < 18; i++ )
    {
        * ( cmd + i ) = 0;
    }

    * ( cmd + 0 ) = ( Byte ) ( 0x7B );							//Start Byte
    * ( cmd + 1 ) = ( Byte ) ( 0x22 );							//CMD Byte
    * ( cmd + 2 ) = get_cpu_type_info();				//para1: CPU Type
    * ( cmd + 3 ) = get_memory_size_info();  			//para2: RAM Size
    memmove ( ( cmd + 4 ), get_ethernet_info(), 12 );	//para3~14: Ethernat and Wifi MAC
    * ( cmd + 16 ) = ( Byte ) ( 0x7C );						//EndByte

    //printf("cmd = %s\n",cmd);
    //for (int i = 0; i < 18; i++)	printf("return_byte=%X\n",*(cmd+i));
    return cmd;
}

Byte* set_CKD_STATE_command()
{
    Byte* cmd = ( Byte * ) malloc ( 8 * sizeof ( Byte ) );

    for ( int i = 0; i < 8; i++ )
    {
        * ( cmd + i ) = 0;
    }

    * ( cmd + 0 ) = ( Byte ) ( 0x7B );
    * ( cmd + 1 ) = ( Byte ) ( 0x21 );
    * ( cmd + 2 ) = ( Byte ) ( 0x7C );
    // printf("%s\n",cmd);
    return cmd;
}

/* -------------------------------
 * Main Loop
 * Input -> Process -> Output
 * -------------------------------
 */

void InputData()
{
    int nread = 0;
    Byte data[1024] = {'\0'};

    if ( ( nread = read ( fd, data, sizeof ( data ) ) ) > 0 )
    {
        printf ( "data: %s\n", data );
        //printf("%d\n", loop);

        Byte* temp_data = parse_received_data ( data );

        if ( NULL != temp_data )
        {
            r_data = temp_data;
            printf ( "r_data = %s\n", r_data );
        }
        else
        {
            puts ( "resent cmd!!!!!!!!!!!!!!!!!!!!!!" );

            switch ( ACT )
            {
            case WAIT_STATE:
                ACT = CKD_STATE;
                break;

            case WAIT_KEY:
                ACT = SEND_INFO;
                break;

            case WAIT_RANDOM_CHECK:
                ACT = RUN_TIME_CHECK;
                break;

            default:
                break;
            }
        }
    }
}
void ProcessData()
{
    switch ( ACT )
    {
    case CKD_STATE:
        s_command = set_CKD_STATE_command();
        printf ( "s_command: %s\n", s_command );

        break;

    case WAIT_STATE:
        if ( true == isCorrectData )
        {
            if ( 0 == strcmp ( "F0", ( char* ) r_data ) )
            {
                printf ( "state = F0\n" );
                isInfoPrepared = false;
                ACT = SEND_INFO;
            }
            else if ( 0 == strcmp ( "A1", ( char* ) r_data ) )
            {
                printf ( "state = A1\n" );
                ACT = SEND_INFO;
            }
            else
            {
                printf ( "state = WTF\n" );
            }
        }

        break;

    case SEND_INFO:
        s_command = set_SEND_INFO_command();
        printf ( "s_command: %s\n", s_command );
        isInfoPrepared = true;
        break;

    case WAIT_KEY:
        if ( true == isCorrectData )
        {
            if ( 0 == strcmp ( "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", ( char* ) r_data ) )
            {
                printf ( "Got Key already\n" );

                if ( 0 == chdir ( "./.config" ) )
                {
                    FILE* rfp = fopen ( "./key", "rb" );

                    if ( NULL != rfp )
                    {
                        fread ( KEY, 18, 1, rfp );
                        //printf ( "KEY read: %s\n", KEY );
                        fclose ( rfp );

                        ACT = RUN_TIME_CHECK;
                    }
                    else
                    {
                        isKeyError = true;
                        puts ( "No File, read fail" );

                        ACT = NONE;
                    }
                }
                else
                {
                    isKeyError = true;
                    puts ( "No Dir, read fail" );

                    ACT = NONE;
                }
            }
            else
            {
                printf ( "Needs to parse & save Key\n" );

                if ( 0 <= parse_dongle_key ( r_data ) )
                {
                    if ( 0 != chdir ( "./.config" ) )
                    {
                        system ( "mkdir ./.config" );
                    }

                    FILE* wfp = fopen ( "./.config/key", "wb+" );

                    if ( NULL != wfp )
                    {
                        fwrite ( KEY, strlen ( ( char* ) KEY ), 1, wfp );
                        fclose ( wfp );

                        ACT = RUN_TIME_CHECK;
                    }
                    else
                    {
                        puts ( "write fail" );
                    }
                }
                else
                {
                    puts ( "Parse KEY fail" );
                    ACT = NONE;
                }
            }

            //for ( int i = 0; i < 18; i++ )	printf ( "%d, KEY=%X\n", i, * ( KEY + i ) );
        }

        break;

    case RUN_TIME_CHECK:
        s_command = set_RUN_TIME_CHECK_command();
        printf ( "s_command: %s\n", s_command );
        isInfoPrepared = true;
        break;

    case WAIT_RANDOM_CHECK:
        if ( true == isCorrectData )
        {
            if ( 0 == strcmp ( "AA", ( char* ) r_data ) )
            {
                printf ( "WAIT_RANDOM_CHECK = AA\n" );
                isInfoPrepared = false;
                //random_times = get_random_value(15,20 ) * 60 * (int)(1000000/usleep_in_mainloop);		//minute = 15 ~ 20
                random_times = get_random_value ( 1, 2 ) * 1 * ( int ) ( 1000000 / usleep_in_mainloop );		//test minute = 1-3
                printf ( "random times = %d\n", random_times );

                check_times = 0;
                ACT = RANDOM_TIMER;
            }
            else
            {
                if  ( 2 < check_times )
                {
                    isKeyError = true;
                    printf ( "WAIT_RANDOM_CHECK = GG lar => no more!\n" );
                    ACT = NONE;
                }
                else
                {
                    check_times++;
                    printf ( "WAIT_RANDOM_CHECK = GG lar => %d times\n", check_times );
                    ACT = RUN_TIME_CHECK;   // resend cmd
                }
            }
        }

        break;

    case RANDOM_TIMER:
        if ( 0 == ( random_times-- )	)
        {
            ACT = RUN_TIME_CHECK;
        }

        break;

    default:
        //printf("gggggggggggggggggg");
        break;
    }
}
void OutputData()
{
    int nwrite = -1;

    switch ( ACT )
    {
    case CKD_STATE:
        tcflush ( fd, TCIOFLUSH );
        isCorrectData = false;

        if ( ( nwrite = write ( fd, s_command, strlen ( ( char* ) s_command ) ) ) > 0 )
        {
            //printf("CKD_STATE nwrite = %d\n", nwrite);
            ACT = WAIT_STATE;
        }

        break;

    case SEND_INFO:
        if ( true == isInfoPrepared )
        {
            tcflush ( fd, TCIOFLUSH );
            isCorrectData = false;

            if ( ( nwrite = write ( fd, s_command, strlen ( ( char* ) s_command ) ) ) > 0 )
            {
                //printf("SEND_INFO nwrite = %d\n", nwrite);
                isInfoPrepared = false;
                ACT = WAIT_KEY;
            }
        }

        break;

    case RUN_TIME_CHECK:
        if ( true == isInfoPrepared )
        {
            tcflush ( fd, TCIOFLUSH );
            isCorrectData = false;

            if ( ( nwrite = write ( fd, s_command, strlen ( ( char* ) s_command ) ) ) > 0 )
            {
                //printf("RUN_TIME_CHECK nwrite = %d\n", nwrite);
                isInfoPrepared = false;
                ACT = WAIT_RANDOM_CHECK;
            }
        }

        break;

    default:
        break;
    }
}

void* SpcThreadRun ( void *arg )
{
    /* open the device to be non-blocking (read will return immediatly) */
    fd = open ( MODEMDEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK );
    //fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NDELAY);

    tcgetattr ( fd, &oldtio ); /* save current port settings */
    /* set new port settings for canonical input processing */
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR | ICRNL;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VMIN] = 1;
    newtio.c_cc[VTIME] = 0;
    tcflush ( fd, TCIOFLUSH );
    tcsetattr ( fd, TCSANOW, &newtio );

    /* loop while waiting for input.
     * normally we would do something useful here.
     */

    ACT = CKD_STATE;
    KEY = ( Byte* ) malloc ( 18 * sizeof ( Byte ) );

    for ( int i = 0; i < 18; i++ )	* ( KEY + i ) = 0;

    while ( STOP == FALSE )
    {
        if ( -1 != ( tcgetattr ( fd, &tempio ) ) )
        {
            //printf("InputData\n");
            InputData();
            //printf("ProcessData\n");
            ProcessData();
            //printf("OutputData\n");
            OutputData();
        }
        else
        {
            isKeyError = true;
            puts ( "Key Error!" );
        }

        usleep ( usleep_in_mainloop );

        //if(2147483647 > loop)	{loop++; }
        //else	{loop = 0;}
    }

    /* restore old port settings */
    tcsetattr ( fd, TCSANOW, &oldtio );

    close ( fd );
    puts ( "End of Thread\n" );
    return NULL;
}


/* Class is HERE
 * LibSPC
 * */
LibSPC::LibSPC()
{
    int err = pthread_create ( &this->thread_id, NULL, &SpcThreadRun, NULL );

    if ( err != 0 )
    {
        printf ( "Can't create thread: [%s]\n", strerror ( err ) );
    }
    else
    {
        puts ( "Thread created successfully\n" );
    }
}

void LibSPC::Close()
{
    STOP = TRUE;
}

bool LibSPC::IsKeyError()
{
    return isKeyError;
}

