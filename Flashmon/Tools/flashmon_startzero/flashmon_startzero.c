/**
 * flashmon_startzero.c
 * ====================
 * This simple tool takes the output of /proc/flashmon_log (a csv file
 * is expected) and modify the time values so that the first flash 
 * operation has a time equal to 0.00 s.
 * This make the trace more readable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

int main(int argc, char **argv)
{
	FILE *f;
	char line[256];
	int lineNbr;
	double time, offset;
	uint64_t addr;
	char type;
	char processName[16];
	int withProcessName = 0;
	
	if(argc != 2)
	{
		printf("Usage : %s <flashmon_log_csv_output_file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	f = fopen(argv[1], "r");
	if(f == NULL)
	{
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	
	lineNbr=0;
	while(fgets(line, 255, f) != NULL)
	{
		lineNbr++;
		if(sscanf(line, "%lf;%c;%llu\n", &time, &type, &addr) != 3)
		{
			withProcessName = 1;
			if(sscanf(line, "%lf;%c;%llu;%s\n", &time, &type, &addr, processName) != 4)
			{
				printf("Error line %d\n", lineNbr);
				fclose(f);
				return EXIT_FAILURE;
			}
		}
		
		if(lineNbr == 1)
			offset = time;
		if(!withProcessName)
			printf("%lf;%c;%llu\n", time-offset, type, addr);
		else
			printf("%lf;%c;%llu;%s\n", time-offset, type, addr, processName);
	}
	
	fclose(f);
	return EXIT_SUCCESS;
}
