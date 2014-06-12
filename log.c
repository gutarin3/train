#include "include/sb_include.h"
#include "include/sb_error_log.h"

#define SDA_DEV "/dev/sda1"
#define MOUNT_DIR "/tmp/mmc"
#define LOG_DIR "/tmp/mmc/log"
#define LOG_FILE "/tmp/mmc/log/debug_log"

int fd_log;

int log_initialize()
{
	struct stat sb;
	char command[128];
	mode_t mode =  S_IRWXU | S_IRWXG | S_IRWXO;
	
	fd_log = -1;
	
	/* mount sdcard if it was not mounted */
	if(stat(MOUNT_DIR, &sb) < 0)
	{
		sprintf(command, "mount %s %s", SDA_DEV, MOUNT_DIR);
		system(command);
	}
	
	/* make log dir if not exists */
	if(stat(LOG_DIR, &sb) < 0)
	{
		if(mkdir(LOG_DIR, mode) < 0)
		{
			perror("mkdir");
			return -1;
		}
	}
	
	fd_log = open(LOG_FILE, O_RDWR | O_CREAT | O_APPEND, mode);
	if(fd_log < 0)
	{
		perror("log_open");
		return -1;
	}
	
	return 0;
}

int add_log(char *log)
{
	puts(log);
	if(fd_log >= 0)
	{
		if(write(fd_log, log, strlen(log)) < 0)
		{
			perror("log");
		}
		
		write(fd_log, "\n", 1);
	}
	
	return 0;
}

int add_error(char *name)
{
	char error_str[256];
	
	perror(name);
	
	if(fd_log >= 0)
	{
		sprintf(error_str, "error at %s: %s\n", name, strerror(errno));
		write(fd_log, error_str, strlen(error_str));
	}
	
	return 0;
}

int log_terminate()
{
	int res;
	res = close(fd_log);
	fd_log = -1;
	
	return res;
}
