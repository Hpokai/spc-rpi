#include <iostream>
#include "appspc.h"

using namespace std;

int main()
{
    cout << "Hello world!" << endl;

    printf("<<APP SPC start>>\n");
    LibSPC *lib_spc = new LibSPC();
    //char *dev  = "/dev/ttyACM0"; //Arduino SA Mega 2560 R3 (CDC ACM)

    int x = 0;
    while (x == 0)
        {
            sleep((3*60*60));
            printf("HAHA~~~~~~\n");
            x = 1;
        }
    lib_spc->Close();
    printf("stop~~~\n");
    sleep(2);

    return 0;
}
