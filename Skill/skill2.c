#include<stdio.h>
#include<unistd.h>
#include<string.h>

int main(){
char command[100];
while(1)
{
printf("2520030066_SHELLFORGE$");
fgets(command,sizeof(command),stdin);
command[strcspn(command,"\n")]='\0';
if(strcmp(command,"exit")==0){
break;
}
printf("You Entered %s \n",command);
}
return 0;
}
