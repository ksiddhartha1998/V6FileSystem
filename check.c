#include<stdio.h>
int main(int argc, char* argv[])
{
	if(argc==3)
	{
		
	
		FILE *r1,*r2;
		char ch1,ch2;
		int flag=0;
		r1=fopen(argv[1],"r");
		r2=fopen(argv[2],"r");
		while(1)
		{
			ch1=getc(r1);
			ch2=getc(r2);
			if(ch1==EOF&&ch2==EOF)
			{
				flag=1;
				break;
			}
			if(ch1!=ch2)
			{
				flag=0;
				break;
			}
		}
		if(flag)
			printf("BOTH FILES ARE SAME\n");
		else
			printf("BOTH FILES ARE NOT SAME\n");
	}
	else
	{
		printf("Invalid number of arguments\n");
	}
	return 0;
}
