#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig){
printf("\nSIGINT recieved!\n");
printf("Signal number = %d\n",sig);
}

int main()
{
signal(SIGINT, handler);

while(1)

{
printf("Program is running...\n");
sleep(2);
}
return 0;
}
