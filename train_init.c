#include "include/sb_kddi_module.h"

int main(int argc, char *argv[])
{
	int fd;
	int res;
	int size, downloaded;
	char buf[BUF_SIZE];
	char print_str[64];
	char version[32];
	
	puts("Wait for file access.");
	
	SB_msleep(5000);
	
	log_initialize();
	
	if(load_ini() < 0)
	{
		system("reboot");
		return EXIT_FAILURE;
	}
	
	add_log("Program start.");
	
	pthread_mutex_init(&lcd_mutex, NULL);
	lcd_initialize();
	
	if(foma_module_power_on() < 0)
	//if(kddi_module_power_on() < 0)
	{
		system("reboot");
		return EXIT_FAILURE;
	}
	
	if(foma_module_initialize() < 0)
	//if(kddi_module_initialize() < 0)
	{
		system("reboot");
		return EXIT_FAILURE;
	}
	
	/* ƒƒ“ƒeƒiƒ“ƒXƒT[ƒo‚Ö‚ÌÚ‘± */
	display_status("Connect maintenance server.");
	mt_server_setting();
	res = at_connect();
	
	if(res == 0)
	{
		/* ƒo[ƒWƒ‡ƒ“‚ÌŠm”FEÅV”ÅƒAƒbƒvƒf[ƒg */
		display_status("Check app version.");
		fd = open(VERSION_INFO_PATH, O_RDONLY);
		if(fd < 0)
		{
			add_error("open version file");
			strcpy(buf, "0.0");
		}
		else
		{
			res = read(fd, buf, BUF_SIZE);
			buf[res] = '\0';
			
			close(fd);
		}
		
		socket_send_data("version", 7);
		res = socket_receive_data(version, 31);
		remove_crlf(version);
		
		if(res == 0)
		{
			display_status("Version check failed.");
			add_log("version receive failed.");
		}
		else if(strcmp(buf, version) == 0)
		{
			display_status("App version OK.");
			add_log("App already new.");
		}
		else
		{
			display_status("Starting app update.");
			socket_send_data("update_size", 11);
			res = socket_receive_update_data(buf, BUF_SIZE);
			size = atoi(buf);
			
			if(res == 0 || size == -1)
			{
				display_status("App update failed(can't get size).");
				add_log("update_size receive failed.");
			}
			else
			{
				at_send_data("update", 6);
				
				fd = open(UPDATE_TMP_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
				if(fd < 0)
				{
					display_status("App update failed(can't open file).");
					add_error("update");
				}
				else
				{
					downloaded = 0;
					while(1)
					{
						res = at_receive_data(buf, BUF_SIZE);
						if(res <= 0) break;
						
						write(fd, buf, res);
						downloaded += res;
						sprintf(print_str, "App updating: %d%% ", downloaded*100/size);
						display_status(print_str);
					}
					
					close(fd);
					
					if(get_filesize(UPDATE_TMP_FILE_PATH) == size)
					{
						unlink(UPDATE_FILE_PATH);
						rename(UPDATE_TMP_FILE_PATH, UPDATE_FILE_PATH);
						
						/* ƒo[ƒWƒ‡ƒ“ƒtƒ@ƒCƒ‹XV */
						fd = open(VERSION_INFO_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
						write(fd, version, strlen(version));
						close(fd);
						
						display_status("App update complete.");
						add_log("update complete.");
						socket_send_data("disconnect", 10);
						display_status("Auto reboot.");
						system("reboot");
						return 0;
					}
					else
					{
						display_status("App update failed(data loss).");
						add_log("update failed.");
						unlink(UPDATE_TMP_FILE_PATH);
					}
				}
			}
		}
		
		
		/* ’ÊMØ’f */
		socket_send_data("disconnect", 10);
		mt_socket_close();
	}
	else
	{
		display_status("Can't connect maintenance server.");
	}
	
	system("reboot");
	
	add_log("Program end.");
	
	log_terminate();
	
	return 0;
}
