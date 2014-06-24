/*
train_app.c
　FOMA and RP version
　by Daikoku
*/
#include "include/sb_kddi_module.h"

void *thread_send_gps(void *arg);
void *thread_get_range(void *arg);
void *thread_receive_jrc(void *arg);
int maintenance_process();

int fd;
int res;
int size, downloaded;
int dcd_now;


char buf[BUF_SIZE+8]; // up 2012/03/08 SIZE+1
char mbuf[BUF_SIZE+8];

char req_buf[128]; //Buffer for Request
char print_str[256];
char version[128];

char msg[BUF_SIZE+8]; // 

//send_gps
int send_span = 0;
int server_request = 0;

//char *discnum = "0000";


char testbuf[128];

int main(int argc, char *argv[])
{
	pthread_t gps_thread, range_thread, jrc_receive_thread ;
	
	puts("Wait for Login file access.");
	SB_msleep(1000); 
	log_initialize();
	puts(" ");
	add_log("**** FOMA PPP ******");
	puts("**Program start.****");
	puts("******2013/04/03****");
	puts(APP_Ver);
	puts("********************");
	SB_msleep(100);
 
	if(load_ini() < 0) 
	{
		puts("load_ini failed");
		system("reboot"); 
		return EXIT_FAILURE;
	}
	lcd_initialize();

	display_connect_status(APP_Ver);
	
	if(foma_module_power_on() < 0)
	{
		puts("Foma_module_power failed");
		system("reboot");
		return EXIT_FAILURE;
	}
	
	if(foma_module_initialize() < 0)
	{
		puts("Foma_module_initialize failed");
		system("reboot");
		return EXIT_FAILURE;
	}
	
	at_pincodes();

//	foma_cardcheck();
//	foma_setting();
//	SB_msleep(1000);
//	foma_atd_command();		// ANT(PACKET-tanshi-check)
//	SB_msleep(1000);

	puts("ppp-on-dialer");
	display_status("PPPD on Dialer Start!");
	//
	// Layout following files in /flash/ 
	// this file is put in load_ini()  apropliately
	// options		operation configulation for PPP
	// ppp-on		operation configularion for PPP-connect, permission must be 700
	// chat-script	ppp-connection-script
	//
	system("pppd /dev/ttyE3 19200 connect /etc/ppp/ppp-on ");
	SB_msleep(5000);
	puts("default gw 192.168.253.1 Set");
	system("route add default gw 192.168.253.1");
	
	// connecting to maintainance server
	if(maintenance_process() <0)
	{
		add_error("Maintenance error!");
	}
	mt_socket_close(); 

	/* load CSV data */ 
	if(load_csv() < 0)
	{
		add_error("load_csv Error!");
	}
	
	/* connecting to Rekkei-server */
	display_status("Connecting train server.");
	jr_udp_socket_open();

	puts("Connect train server OK.");
	
	/* Making thread for confirming communication-stat */
	if(pthread_create(&range_thread, NULL, thread_get_range, NULL) != 0)
	{
		add_error("range_thread");
		system("reboot");
		return EXIT_FAILURE;
	}
	
	if(initialize() < 0) return EXIT_FAILURE;
	

	/* Making thread for getting and sending GPS */
	if(pthread_create(&gps_thread, NULL, thread_send_gps, NULL) != 0)
	{
		add_error("gps_thread");
		system("reboot");
		return EXIT_FAILURE;
	}
	
	/* Making thread for Data recieving from JRC-Module, do path-through */
	if(pthread_create(&jrc_receive_thread, NULL, thread_receive_jrc, NULL) != 0)
	{
		add_error("jrc_receive_thread");
		system("reboot");
		return EXIT_FAILURE;
	}


	int trange;
	int tnow;
	int tlast;

	trange =3580; // 35分= 60*35 = 2100
	tnow = time(NULL);
	tlast = time(NULL);
	
	/* 通信モジュール受信待ち及び日本無線モジュールへのパススルー */
	/* Path-throgh to wait for com-module receiving data, and JRC module */
	
	while(1)
	{
		SB_msleep(100);
	
		// Sending data from Server to JRC-module
		res = socket_udp_receive_data(mbuf, BUF_SIZE);
		if(res > 0)
		{
			// 本来ならデータを判別する。
			// confirm and select data(If possible)
			jrc_send_data(mbuf, res);	
		}
	}
	puts("main is over");
	at_disconnect();
	terminate();
	puts("Program end.");
	log_terminate();
	
	return 0;
}

void *thread_send_gps(void *arg)
{
	char buf[BUF_SIZE+16];  // up 256-->257 2012/03/07
	char rbuf[BUF_SIZE+16]; // up 256-->257 2012/03/07
	time_t now, last_send_gps;
	int res;
	int gps_fewest_send_span = 2;
	int display_erase_span = 5;
	int first = 1; // flag
	
	int grange; // this gadget will restart, if this couldn't recieve data for $grange sec continuously
	int gnow; // unknown variable
	int glast; // get the time of now

	grange =60; // GPS non communicate timeout-set 1minute equall 60 second
	gnow = time(NULL);
	glast = time(NULL);
	
	gps_send_span = get_setting_info(1); // Get the GPS sending-kankaku from Shasaiki's conf
	gnow = time(NULL); // Reget
	last_send_gps = time(NULL); // Time that GPS send last
	
	clear_gps(); // Clear GPS data


	while(1){
		SB_msleep(50);
		/* cleaning memory */

		memset(buf, 0 , BUF_SIZE);    // putting NULL
		memset(rbuf, 0 , BUF_SIZE);

		if((read_gps(buf, BUF_SIZE-2)) < 0)   // Read GPS-data
		{
	    	gnow = time(NULL); // compare now-time and glast-time
 		 	if(difftime(gnow, glast) >= grange)
			{
				display_status("GPS recv timeout ! Reboot.");
				puts("GPS recv timeout ! Reboot.");
				SB_msleep(1000);
				system("reboot");  // add 2012/03/26
			}
			SB_msleep(50);						
			continue;
		}
		
		glast = time(NULL);// From confirmation of getting gps data to Confirmation for GPGGA-alaysis 
		jrc_send_gps(buf);				// Send raw data to Rekkei
		//puts(buf);
		
		pthread_mutex_lock(&con_mutex); 
		SB_msleep(100);
		res = analyze_gps(buf, rbuf);	// Editing GPGGA
		//puts(rbuf);
		pthread_mutex_unlock(&con_mutex);
		if(send_span > 0)
		{
			gps_send_span = send_span;
		}else
		{
			puts("send_span equal 0");
		}
				
		now = time(NULL);
		//
		if(server_request > 0) // What's this?
		{			
			if(rbuf[0]==0x00)
			{
				continue;				
			}
			pthread_mutex_lock(&con_mutex);
			strncpy((rbuf+38),(req_buf+12),4);// Replace the format of APP_Ver to Reqquest->Data type
			socket_send_data(rbuf, strlen(rbuf));
			SB_msleep(100);	
			puts("< Server Request:");
			puts(rbuf);
			pthread_mutex_unlock(&con_mutex);
			last_send_gps = time(NULL);			
			server_request = 0;
			display_status("Request from server: GPS send");
			
		}
		else
		{
			// 八高線の列警システムは位置情報によって処理はない
			// 			
			//sprintf(buf, "Now  times %s",ctime(&time));
			// puts(buf);
			sprintf(buf, "Last times %s",ctime(&last_send_gps));
			// puts(buf);
			sprintf(buf, "Span times %d",gps_send_span);
			//puts(buf);
						
			if(first || difftime(time(NULL), last_send_gps) >= gps_send_span || difftime(now, last_send_gps) >= gps_fewest_send_span)
			{
				if(strlen(rbuf) == 0) 
				{
					//puts("no GPS Send Data.");
					SB_msleep(100);	
					continue; 
				}
				pthread_mutex_lock(&con_mutex);
				socket_udp_send_data(rbuf, strlen(rbuf));

				remove_crlf(rbuf);
				sprintf(buf,"---> Send GPS Data: %s",rbuf);
				puts(buf);	
				pthread_mutex_unlock(&con_mutex);
				last_send_gps = time(NULL);
				first = 0;
				
				sprintf(buf, "Send %.16s",rbuf);
				display_status(buf);  //
				update_last_send_idokeido();
			}
		}
		if(difftime(now, last_send_gps) >= display_erase_span) // Displaytime actually
		{
			//display_status(" ");
		}
	}
}

void *thread_get_range(void *arg)
{

	int dcd_off_cnt;
	
	dcd_off_cnt = 0;
	while(1)
	{
		SB_msleep(5000);
		// get communication status now

		if(get_dcd_now() == 0)
		{
			puts("DCD_now: == 0");		// NO CARRIER
			vcc_off();
			display_status("DCD OFF reboot.");
			system("reboot");
		}
		
		if(get_xer_now() == 0)
		{
			puts("XER_now: == 0");		// NO CARRIER	
			vcc_off();
			display_status("XER OFF reboot.");
			system("reboot");
		}
		
		show_modembits();
		
	}
	puts("*********************************** thread_get_range end.");
}

void *thread_receive_jrc(void *arg)
{ // Sendig JRC-data to Server
	int res = 0;
	char buf[BUF_SIZE+8];

	
	while(1)
	{
		SB_msleep(10);

 		res = jrc_receive_data(buf, BUF_SIZE-2); // add BUF_SIZE-1 2012/03/07
		if(res > 0)
		{
			// If JRC-data exists, send data to Server	
			pthread_mutex_lock(&con_mutex);
			socket_udp_send_data(buf, res);
			pthread_mutex_unlock(&con_mutex);
		}
	}
}


int maintenance_process()
{
	int wki;
	char mbuf[BUF_SIZE+8];
		
	wki = 0;
	display_status("Connect maintenance server.");
	
	while(1)
	{
		/* Connect to maintainance Server */

		res = mt_server_socket();
		if(res == 0) 
		{
			break;
		}
		wki++;
		if(wki > 10)
		{
			return -1;
		}
		SB_msleep(2000);
	}
	
	if(res == 0)
	{
		/* バージョンの確認・最新版アップデート */
		/* Confirm of version, and update to newest */
		display_status("Check app version.");
		puts("Check app version.");
				
		fd = open(VERSION_INFO_PATH, O_RDONLY);
		if(fd < 0)
		{
			puts("open version file!");
			strcpy(buf, "0.0");
		}
		else
		{
			res = read(fd, buf, BUF_SIZE);
			buf[res] = '0';
			close(fd);
		}
		socket_send_data("version", 7); // sending "version" to server 7byte
		res = socket_receive_data(version, 31); //
		remove_crlf(version);

		sprintf(msg, "Server:%s , Client:%s", buf, version);
		puts(msg);

		if(res == 0)
		{
			display_status("Version check failed.");
			puts("version receive failed.");
		}
		else if(strcmp(buf, version) == 0)
		{
			display_status("App version OK.");
			puts("App already new.");
		}
		else
		{
			display_status("Starting app update.");
			socket_send_data("update_size", 11);
			res = socket_receive_update_data(buf, BUF_SIZE);
			size = atoi(buf);

			sprintf(msg, "update_size:%s (%d)", buf, size);
			puts(msg);
			
			if(res == 0 || size == -1)
			{
				display_status("App update failed(can't get size).");
				puts("update_size receive failed.");
				return -1;
			}
			else
			{
				socket_send_data("update", 6);
				fd = open(UPDATE_TMP_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
				if(fd < 0)
				{
					display_status("App update failed(can't open file).");
					add_error("update　open error!");
					return -1;
				}
				else
				{
					downloaded = 0;
					while(1)
					{
						SB_msleep(50); 
						res = socket_receive_data(buf, BUF_SIZE);
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
						rename(UPDATE_TMP_FILE_PATH, APP_FILE_PATH);
						sprintf(mbuf, "chmod a+rx %s", APP_FILE_PATH); 
						puts(mbuf);
						system(mbuf); 
						//SB_msleep(90000);	// 	Wait for test
						
						/* Update version file */
						fd = open(VERSION_INFO_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
						write(fd, version, strlen(version));
						close(fd);
						display_status("App update complete.");
						puts("APP update complete.");
						socket_send_data("disconnect", 10);
						display_status("Auto reboot.");
						system("reboot");
						return 0;
					}
					else
					{
						display_status("App update failed(data loss).");
						puts("update failed.error size");
						unlink(UPDATE_TMP_FILE_PATH);
						return -1;
					}
				}
			}
		}
		
		
		/* update daiya-information CSV and update it */
		display_status("Check diagram data.");
		fd = open(DIAGRAM_LAST_UPDATE_PATH, O_RDONLY);
		if(fd < 0)
		{
			add_error("open diagram_last_update file");
			strcpy(buf, "0");
		}
		else
		{
			res = read(fd, buf, BUF_SIZE);
			buf[res] = '¥0';
			close(fd);
		}
		socket_send_data("diagram_last_update", 19);
		res = socket_receive_data(version, 31);
		remove_crlf(version);
		
		if(res == 0)
		{
			display_status("Check diagram data failed.");
			puts("diagram_last_update receive failed.");
			return -1;
		}
		else if(strcmp(buf, version) == 0)
		{
			display_status("Diagram data OK.");
			puts("diagram.csv already new.");
		}
		else
		{
			display_status("Starting diagram update.");
			socket_send_data("diagram_size", 12);
			res = socket_receive_data(buf, BUF_SIZE);
			size = atoi(buf);
			
			if(res == 0 || size == -1)
			{
				display_status("Diagram update failed(can't get size).");
				puts("diagram_size receive failed.");
				return -1;
			}
			else
			{
				socket_send_data("get_diagram", 11);
				fd = open(DIAGRAM_NEW_CSV_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
				if(fd < 0)
				{
					display_status("Diagram update failed(can't open file).");
					add_error("get_diagram");
				}
				else
				{
					downloaded = 0;
					while(1)
					{
						sprintf(print_str, "Diagram updating: %d%% ", downloaded*100/size);
						display_status(print_str);
						res = socket_receive_data(buf, BUF_SIZE);
						if(res <= 0) break;
						write(fd, buf, res);
						downloaded += res;
					}
					close(fd);
					
					if(get_filesize(DIAGRAM_NEW_CSV_PATH) == size)
					{
						unlink(DIAGRAM_CSV_PATH);
						rename(DIAGRAM_NEW_CSV_PATH, DIAGRAM_CSV_PATH);
						
						/* update version file */
						fd = open(DIAGRAM_LAST_UPDATE_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
						write(fd, version, strlen(version));
						close(fd);
						
						display_status("Diagram update complete.");
						puts("get_diagram complete.");
					}
					else
					{
						display_status("Diagram update failed(data loss).");
						puts("get_diagram failed.");
						unlink(DIAGRAM_NEW_CSV_PATH);
						return -1;
					}
				}
			}
		}
		
		/* confirm of KIROTEI information CSV and update to new */
		display_status("Check km data.");
		fd = open(KM_LAST_UPDATE_PATH, O_RDONLY);
		if(fd < 0)
		{
			add_error("open km_last_update file");
			strcpy(buf, "0");
			return -1;
		}
		else
		{
			res = read(fd, buf, BUF_SIZE);
			buf[res] = '¥0';	
			close(fd);
		}
		socket_send_data("km_last_updsockete", 14);
		res = socket_receive_data(version, 31);
		remove_crlf(version);
		
		if(res == 0)
		{
			display_status("Check km data failed.");
			puts("km_last_update receive failed.");
			return -1;
		}
		else if(strcmp(buf, version) == 0)
		{
			display_status("km data OK.");
			puts("km.csv already new.");
		}
		else
		{
			display_status("Starting km update.");
			socket_send_data("km_size", 7);
			res = socket_receive_data(buf, BUF_SIZE);
			size = atoi(buf);
			
			if(res == 0 || size == -1)
			{
				display_status("km update failed(can't get size).");
				puts("km_size receive failed.");
				return -1;
			}
			else
			{
				socket_send_data("get_km", 6);
				fd = open(KM_NEW_CSV_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
				if(fd < 0)
				{
					display_status("km update failed(can't open file).");
					add_error("get_km");
					return -1;
				}
				else
				{
					downloaded = 0;
					while(1)
					{
						SB_msleep(50); 
						res = socket_receive_data(buf, BUF_SIZE);
						if(res <= 0) break;
						
						write(fd, buf, res);
						downloaded += res;
						sprintf(print_str, "km updating: %d%% ", downloaded*100/size);
						display_status(print_str);
					}
					
					close(fd);
					
					if(get_filesize(KM_NEW_CSV_PATH) == size)
					{
						unlink(KM_CSV_PATH);
						rename(KM_NEW_CSV_PATH, KM_CSV_PATH);
						
						/* update version-file */
						fd = open(KM_LAST_UPDATE_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
						write(fd, version, strlen(version));
						close(fd);
						
						display_status("km update complete.");
						puts("get_km complete.");
					}
					else
					{
						display_status("km update failed(data loss).");
						puts("get_km failed.");
						unlink(KM_NEW_CSV_PATH);
						return -1;
					}
				}
			}
		}
		
		
		/* confirm of Station information CSV and update to new */
		display_status("Check station data.");
		fd = open(STATION_LAST_UPDATE_PATH, O_RDONLY);
		if(fd < 0)
		{
			add_error("open station_last_update file");
			strcpy(buf, "0");
		}
		else
		{
			res = read(fd, buf, BUF_SIZE);
			buf[res] = '¥0';
			
			close(fd);
		}
		socket_send_data("station_last_update", 19);
		res = socket_receive_data(version, 31);
		remove_crlf(version);
		
		if(res == 0)
		{
			display_status("Check station data failed.");
			puts("station_last_update receive failed.");
		}
		else if(strcmp(buf, version) == 0)
		{
			display_status("Station data OK.");
			puts("station.csv already new.");
		}
		else
		{
			display_status("Starting station update.");
			socket_send_data("station_size", 12);
			res = socket_receive_data(buf, BUF_SIZE);
			size = atoi(buf);
			
			if(res == 0 || size == -1)
			{
				display_status("Station update failed(can't get size).");
				puts("station_size receive failed.");
			}
			else
			{
				socket_send_data("get_station", 11);
				fd = open(STATION_NEW_CSV_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
				if(fd < 0)
				{
					display_status("Station update failed(can't open file).");
					add_error("get_station");
				}
				else
				{
					downloaded = 0;
					while(1)
					{
						SB_msleep(50); 
						res = socket_receive_data(buf, BUF_SIZE);
						if(res <= 0) break;
						
						write(fd, buf, res);
						downloaded += res;
						sprintf(print_str, "Station updating: %d%% ", downloaded*100/size);
						display_status(print_str);
					}
					
					close(fd);
					
					if(get_filesize(STATION_NEW_CSV_PATH) == size)
					{
						unlink(STATION_CSV_PATH);
						rename(STATION_NEW_CSV_PATH, STATION_CSV_PATH);
						
						/* バージョンファイル更新 */
						fd = open(STATION_LAST_UPDATE_PATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
						write(fd, version, strlen(version));
						close(fd);
						
						display_status("Station update complete.");
						puts("get_station complete.");
					}
					else
					{
						display_status("Station update failed(data loss).");
						puts("get_station failed.");
						unlink(STATION_NEW_CSV_PATH);
						return -1;
					}
				}
			}
		}
		
		/* Get the newest config dataset */

		socket_send_data("gps_send_span", 13);
		res = socket_receive_data(buf, BUF_SIZE);
		if(res > 0)
		{
			set_setting_info(1, atoi(buf));
		}
		socket_send_data("detect_train_stop", 17);
		res = socket_receive_data(buf, BUF_SIZE);
		if(res > 0)
		{
			set_setting_info(2, atoi(buf));
		}
		socket_send_data("stop_error_range", 16);
		res = socket_receive_data(buf, BUF_SIZE);
		if(res > 0)
		{
			set_setting_info(3, atoi(buf));
		}
		socket_send_data("diagram_matching", 16);
		res = socket_receive_data(buf, BUF_SIZE);
		if(res > 0)
		{
			set_setting_info(4, atoi(buf));
		}
		socket_send_data("diagram_match_span", 18);
		res = socket_receive_data(buf, BUF_SIZE);
		if(res > 0)
		{
			set_setting_info(5, atoi(buf));
		}
		
		save_setting_ini();

	}
	else
	{
		display_status("Can't connect maintenance server.");
		return -1;
	}
	/* disconnect communication */
	socket_send_data("disconnect", 10);

	return 0;	
	
}
