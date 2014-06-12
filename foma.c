/*
kddi_module.c
@FOAM‘Î‰ž
@ì¬: •s“®“c
*/
#include "include/sb_kddi_module.h"

//void *at_udp_send_func(void *arg);

typedef struct {
	char str[1024];
	int len;
} send_data_t;

typedef struct {
	char id[36];
	char pass[36];
	char id_num[12];
	char dummy[16];
} connect_info_t;

typedef struct {
	int gps_send_span;
	int detect_train_stop;
	int stop_error_range;
	int diagram_matching;
	int diagram_match_span;
} setting_info_t;

typedef struct {
	char dia_name[32];
	char name[32];
	int arrival;
	int departure;
} diagram_data_t;

typedef struct {
	int km;
	double ido;
	double keido;
} km_data_t;

typedef struct {
	char name[32];
	double ido;
	double keido;
} station_data_t;

int fd_gpio, fd_data, fd_ctrl, fd_gps, fd_jrc_gps, fd_jrc_send, fd_lcd;
struct termios data_init_tio, data_tio, ctrl_init_tio, ctrl_tio, gps_init_tio, gps_tio, jrc_gps_init_tio, jrc_gps_tio, jrc_send_init_tio, jrc_send_tio, lcd_init_tio, lcd_tio;
int last_mctl_set[1024];
connect_info_t connect_info;

char info_ip_mente[36];
char info_ip_jr[36];
char info_cln_port[16];

setting_info_t setting_info;
diagram_data_t diagram_data[BUF_SIZE+8];
km_data_t km_data[4096];
station_data_t station_data[128] ;//hachiko:34 chyuo:74
int diagram_data_count;
int km_data_count;
int station_data_count;
char gps_last_get[34];
char ido_str[12];
char keido_str[12];
char current_dia_name[34];
double ido_d, keido_d, last_send_ido, last_send_keido;
char lrange[5];

struct gpio_struct
{
	unsigned int value[3];
	unsigned int mode[3];
	unsigned int pullup[3];
	unsigned int enable[3];
};

//ˆÈ‰ºAFoma—p
int mt_sock;
int jrs_sock;
int jrr_sock;

struct sockaddr_in client_addr, udp_snd_addr, udp_rcv_addr;

int foma_ledg_gnd_8s();
int foma_ledg_open_40s();
int foma_ledg_open_60s();
//int check_command_result();

//‚±‚±‚Ü‚Å

int initialize()
{
	if(gps_initialize() < 0)
	{
		return -1;
	}
	
	if(jrc_initialize() < 0)
	{
		return -1;
	}
	
	return 0;
}

void terminate()
{
	/* ‚±‚±‚Éƒpƒ[ƒIƒtƒV[ƒPƒ“ƒX‚à‚¢‚ê‚é‚×‚«‚©‚à‚µ‚ê‚È‚¢ */
	
	// at_close(fd_data, &data_init_tio);
	// at_close(fd_ctrl, &ctrl_init_tio);
	// at_close(fd_gps, &gps_init_tio);
	// at_close(fd_jrc_gps, &jrc_gps_init_tio);
	// at_close(fd_jrc_send, &jrc_send_init_tio);
	// close(fd_gpio);
	system("reboot");  // add 2012/03/08
	
}

int lcd_initialize()
{
	char clear = 0x0c;
	char set_mode[64];
	
	fd_lcd  = lcd_open(LCD_PORT, &lcd_init_tio, &lcd_tio);
	if(fd_lcd < 0)
	{
		return -1;
	}
	
	write(fd_lcd, &clear, 1);		/* •\Ž¦ƒNƒŠƒA */
	
	set_mode[0] = 0x1b;
	set_mode[1] = 0x40;
	write(fd_lcd, &set_mode, 2); /* ƒCƒjƒVƒƒƒ‰ƒCƒY */
	
	set_mode[0] = 0x1b;
	set_mode[1] = 0x52;
	set_mode[2] = 0x08;
	write(fd_lcd, &set_mode, 3); /* ‘Û•¶ŽšƒZƒbƒg:“ú–{ */
	
	set_mode[0] = 0x1f;
	set_mode[1] = 0x28;
	set_mode[2] = 0x67;
	set_mode[3] = 0x01;
	set_mode[4] = 0x02;
	write(fd_lcd, &set_mode, 5); /* ƒtƒHƒ“ƒgƒTƒCƒY:8x16 */
	
	display_status("LCD start.");
	display_connect_status("Disconnect.");
	
	puts("lcd_initialize OK");
	return 0;
}

int load_ini()
{
	FILE *fp;
	char *p;
	char buf[BUF_SIZE+8];
	char command[BUF_SIZE+8];
	mode_t mode =  S_IRWXU | S_IRWXG | S_IRWXO;

	// GPIO’è‹`ƒtƒ@ƒCƒ‹‰Šú‰»	
	sprintf(command, "cp %s %s", Eddy_GPIO_org, Eddy_GPIO_cfg);
	system(command);
	puts("GPIO File ini");
	puts(command);

	/* GPIO‚ÌƒI[ƒvƒ“ */
	fd_gpio = open(SB_GPIO_DEVICE, O_RDWR);
	if(fd_gpio < 0)
	{
		puts("GPIO open error!");
		return -1;
	}
	vcc_off();

	// PPP’è‹`ƒtƒ@ƒCƒ‹‰Šú‰»	
	puts("PPP File ini");
	sprintf(command, "rm %s", PPP_1_rem);
	system(command);
	puts(command);
	sprintf(command, "rm %s", PPP_2_rem);
	system(command);
	puts(command);
	sprintf(command, "rm %s", PPP_3_rem);
	system(command);
	puts(command);
	sprintf(command, "rm %s", PPP_4_rem);
	system(command);
	puts(command);

	mkdir(PPP_mk, mode); 
	sprintf(command, "cp %s %s", PPP_1_org, PPP_1_cfg);
	system(command);
	puts(command);
	
	sprintf(command, "cp %s %s", PPP_2_org, PPP_2_cfg);
	system(command);
	puts(command);
		
	sprintf(command, "cp %s %s", PPP_3_org, PPP_3_cfg);
	system(command);
	puts(command);
	
	sprintf(command, "cp %s %s", PPP_4_org, PPP_4_cfg);
	system(command);
	puts(command);
	puts("load ini Files.");
	puts(CONNECT_INFO_PATH);
	
	fp = fopen(CONNECT_INFO_PATH, "r");
	if(fp == NULL)
	{
		add_error("open connect_info");
		return -1;
	}
	puts("---");
	fgets(connect_info.id, 32, fp);
	fgets(connect_info.pass, 32, fp);
	fgets(connect_info.id_num, 10, fp);
	fgets(info_ip_mente, 32, fp);
	fgets(info_ip_jr, 32, fp);
	fgets(info_cln_port, 16, fp);
	
	remove_crlf(connect_info.id);
	remove_crlf(connect_info.pass);
	remove_crlf(connect_info.id_num);
	remove_crlf(info_ip_mente);
	remove_crlf(info_ip_jr);
	remove_crlf(info_cln_port);

	puts(connect_info.id);
	puts(connect_info.pass);
	puts(connect_info.id_num);
	puts(info_ip_mente);
	puts(info_ip_jr);
	puts(info_cln_port);
	
	fclose(fp);

	puts("---");
	
	setting_info.gps_send_span = 20;
	setting_info.detect_train_stop = 1;
	setting_info.stop_error_range = 50;
	setting_info.diagram_matching = 0;
	setting_info.diagram_match_span = 120;
	
	fp = fopen(SETTING_INI_PATH, "r");
	if(fp == NULL)
	{
		puts("open setting_ini");
	}
	else
	{
		while(1)
		{
			p = fgets(buf, BUF_SIZE, fp);
			if(p == NULL)
			{
				fclose(fp);
				return 0;
			}
			remove_crlf(buf);
			puts(buf);
			
			p = strtok(buf, "=");
			if(strcmp(p, "gps_send_span") == 0)
			{
				p = strtok(NULL, "\0");
				setting_info.gps_send_span = atoi(p);
			}
			else if(strcmp(p, "detect_train_stop") == 0)
			{
				p = strtok(NULL, "\0");
				setting_info.detect_train_stop = atoi(p);
			}
			else if(strcmp(p, "stop_error_range") == 0)
			{
				p = strtok(NULL, "\0");
				setting_info.stop_error_range = atoi(p);
			}
			else if(strcmp(p, "diagram_matching") == 0)
			{
				p = strtok(NULL, "\0");
				setting_info.diagram_matching = atoi(p);
			}
			else if(strcmp(p, "diagram_match_span") == 0)
			{
				p = strtok(NULL, "\0");
				setting_info.diagram_match_span = atoi(p);
			}
		}
	}	
	return 0;
}

int get_setting_info(int id)
{
	int result;
	
	switch(id)
	{
	case 1:
		result = setting_info.gps_send_span;
		break;
	case 2:
		result = setting_info.detect_train_stop;
		break;
	case 3:
		result = setting_info.stop_error_range;
		break;
	case 4:
		result = setting_info.diagram_matching;
		break;
	case 5:
		result = setting_info.diagram_match_span;
		break;
	default:
		result = -1;
		break;
	}
	
	return result;
}

int set_setting_info(int id, int value)
{
	switch(id)
	{
	case 1:
		setting_info.gps_send_span = value;
		break;
	case 2:
		setting_info.detect_train_stop = value;
		break;
	case 3:
		setting_info.stop_error_range = value;
		break;
	case 4:
		setting_info.diagram_matching = value;
		break;
	case 5:
		setting_info.diagram_match_span = value;
		break;
	default:
		return -1;
		break;
	}
	
	return 0;
}

int save_setting_ini()
{
	FILE *fp;
	
	fp = fopen(SETTING_INI_PATH, "w");
	if(fp == NULL)
	{
		return -1;
	}
	
	fprintf(fp, "gps_send_span=%d\n", setting_info.gps_send_span);
	fprintf(fp, "detect_train_stop=%d\n", setting_info.detect_train_stop);
	fprintf(fp, "stop_error_range=%d\n", setting_info.stop_error_range);
	fprintf(fp, "diagram_matching=%d\n", setting_info.diagram_matching);
	fprintf(fp, "diagram_match_span=%d", setting_info.diagram_match_span);
	
	fclose(fp);
	
	chmod(SETTING_INI_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
	
	return 0;
}

int load_csv()
{
	FILE *fp;
	char line[BUF_SIZE+8];
	char *tmp;
	
	/* ƒ_ƒCƒ„î•ñ‚Ì“Ç‚Ýž‚Ý */
	fp = fopen(DIAGRAM_CSV_PATH, "r");
	if(fp == NULL)
	{
		puts("open diagram data failed.");
		return -1;
	}
	else
	{
		/* ˆês–Úiƒ^ƒCƒgƒ‹sj‚ð“Ç‚Ý”ò‚Î‚· */
		fgets(line, 128, fp);
		
		diagram_data_count = 0;
		while(1)
		{
			tmp = fgets(line, 128, fp);
			if(tmp == NULL)
			{
				break;
			}
			
			remove_blank(line);
			if(strlen(line) == 0) continue;
			
			tmp = strtok(line, ",");
			strcpy(diagram_data[diagram_data_count].dia_name, tmp);
			tmp = strtok(NULL, ",");
			strcpy(diagram_data[diagram_data_count].name, tmp);
			tmp = strtok(NULL, ",");
			diagram_data[diagram_data_count].arrival = atoi(tmp);
			tmp = strtok(NULL, ",");
			diagram_data[diagram_data_count].departure = atoi(tmp);
			
			diagram_data_count++;
		}
		
		fclose(fp);
	}
	
	/* ƒLƒ’öî•ñ‚Ì“Ç‚Ýž‚Ý */
	fp = fopen(KM_CSV_PATH, "r");
	if(fp == NULL)
	{
		puts("open km data failed.");
		return -1;
	}
	else
	{
		/* ˆês–Úiƒ^ƒCƒgƒ‹sj‚ð“Ç‚Ý”ò‚Î‚· */
		fgets(line, 128, fp);
		
		km_data_count = 0;
		while(1)
		{
			tmp = fgets(line, 128, fp);
			if(tmp == NULL)
			{
				break;
			}
			
			remove_blank(line);
			if(strlen(line) == 0) continue;
			
			tmp = strtok(line, ",");
			tmp = strtok(NULL, ",");
			km_data[km_data_count].km = atoi(tmp);
			tmp = strtok(NULL, ",");
			km_data[km_data_count].ido = atof(tmp);
			tmp = strtok(NULL, ",");
			km_data[km_data_count].keido = atof(tmp);
			
			km_data_count++;
		}
		
		fclose(fp);
	}
	
	/* ‰wî•ñ‚Ì“Ç‚Ýž‚Ý */
	fp = fopen(STATION_CSV_PATH, "r");
	if(fp == NULL)
	{
		puts("open station data failed.");
		return -1;
	}
	else
	{
		/* ˆês–Úiƒ^ƒCƒgƒ‹sj‚ð“Ç‚Ý”ò‚Î‚· */
		fgets(line, 128, fp);
		
		station_data_count = 0;
		while(1)
		{
			tmp = fgets(line, 128, fp);
			if(tmp == NULL)
			{
				break;
			}
			
			remove_blank(line);
			if(strlen(line) == 0) continue;
			
			tmp = strtok(line, ",");
			strcpy(station_data[station_data_count].name, tmp);
			tmp = strtok(NULL, ",");
			station_data[station_data_count].ido = atof(tmp);
			tmp = strtok(NULL, ",");
			station_data[station_data_count].keido = atof(tmp);
			
			station_data_count++;
		}
		
		fclose(fp);
	}
	
	return 0;
}

int foma_module_power_on()
{
	//char tmpbuf[BUF_SIZE+8];
	display_status("Power on sequence start.");
	// Variable declaration
	int value[3] = {0, 0, 0};
	int res;
	
	//GPIO“Çž
	res = ioctl(fd_gpio, GETGPIOVAL_LM, value);
	if(res < 0)
	{
		puts(strerror(errno));
	}
	
	//SB_msleep(30000);
	//sprintf(buf, "value[2]=%x",value[2]);
	//puts(buf);

	puts("POWER OFF");
	value[2] |= PC14;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	SB_msleep(1000); 
	
	puts("PWRKEY OPEN");
	value[2] |= PWRKEY;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	SB_msleep(1000); 
		
	puts("POWERON START");
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	value[2] &= ~PC14;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);

	puts("WAIT...");
	SB_msleep(6000); //5.0sˆÈã‚ ‚¯‚é
		
	puts("PWRKEY-GND");
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	value[2] &= ~PWRKEY; //PWRKEY@OFF(GND)
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	SB_msleep(3000); //2.3sˆÈã‚Å“dŒ¹ON—v‹‚ð”FŽ¯@

	if(foma_ledg_gnd_8s() < 0){ //8•bƒEƒFƒCƒgŒÅ’è‚ÌŠÖ”
		display_status("PWRON ERR: LEDG OPEN");	
		// “dŒ¹‹Ÿ‹‹’âŽ~ 
		vcc_off();
		SB_msleep(1000);
		return -1;
		
	}	

	puts("PWRKEY OPEN");
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	value[2] |= PWRKEY;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	SB_msleep(1500);
	
	return 0;	
	
}


int foma_module_power_off()
{
	/****************************
	   ŽÀs‚·‚é‘O‚É•K‚¸ATƒRƒ}ƒ“ƒh‘—o‚ð’âŽ~‚·‚é‚±‚ÆI
	 
	 ****************************/
	int value[3] = {0, 0, 0};
	
	display_status("PowerOFF Sequence Start.");
	
	puts("PowerOFF Start");
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	value[2] &= ~PWRKEY;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	SB_msleep(500); //500msˆÈã’âŽ~
	
	if((foma_ledg_open_40s()) < 0)
	{
		display_status("PowerOFF Failed:LEDG GND");
		puts("LEDG GND:System Reboot");

		/* “dŒ¹‹Ÿ‹‹’âŽ~ */
		vcc_off();
		system("reboot");
		return -1;
	}
	
	SB_msleep(1500); //1000msˆÈã‚ÌWait
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	value[2] |= PC14;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	puts("POWER OFF");
	
	return 0;

}

int foma_module_initialize()
{

	int s = 0; //secondi•bj
	int dsr;
	//char buf[BUF_SIZE+8];
	
	display_status("Power on sequence complete.");
	
	// ƒVƒŠƒAƒ‹ƒ|[ƒg‚ÌƒI[ƒvƒ“ 
	fd_data = at_data_open(DATA_PORT, &data_init_tio, &data_tio);

	if(fd_data < 0 )
	{
		return -1;
		
	}
	
	// DTR RTS ON
	puts("XER(DTR) XRS(RTS) GND");
	set_dtr(fd_data);	// DTR=XER GND 
	set_rts(fd_data);	// RTS=RTS GND 
	puts("CHECK");
	show_modembits();

	// XDR‚ªON‚È‚Ì‚ðŠm”F
	// XDR‚ÍUART‘¤‚Å‚ÍDSR‚ÉŒq‚ª‚Á‚Ä‚¢‚é
	while(s < 3)
	{
		dsr = get_dsr(fd_data);
		switch(dsr)
		{
		case ON:
			puts("DSR(XDR) GND");
			break;
		case OFF:
			puts("DSR(XDR) OPEN");
			break;
		default:
			return -1;
			break;
		}
		
		if(dsr == ON)
		{
			break;
		}
		SB_msleep(1000);
		s = s + 1;
	}
	
	if(s >= 3)
	{
		puts("XDR OPEN. REBOOT");
		SB_msleep(50);
		foma_module_power_off();
		return -1;
	}
	
	display_status("XDR(DSR) OK.");
	// 500msˆÈãƒEƒFƒCƒg‚ðŽæ‚é 
	SB_msleep(1000);
	display_status("SIRIAL OK.");
	SB_msleep(2000);
	
	display_status("Foma module initialize complete.");
	
	return 0;
}

int foma_module_reinitialize(){

	
	// ƒVƒŠƒAƒ‹ƒ|[ƒg‚ÌƒI[ƒvƒ“ 
	fd_data = at_data_open(DATA_PORT, &data_init_tio, &data_tio);
	if(fd_data < 0 )
	{
		puts("reopen error!");
		return -1;
	}
	puts("reopen ok!");
	return 0;
}
	
int foma_module_close(){
	
	close(fd_data);
	return 0;
}

void at_pincodes(){
	char buf[BUF_SIZE+8];
	
	at_send_command(fd_data, "+VERPIN0000"); /* APNÝ’è */
	at_get_command_result(fd_data, buf);
	puts(buf);
	
	at_send_command(fd_data, "S7=10"); /* NO CARRIER ŒŸoŽžŠÔ */
	at_get_command_result(fd_data, buf);
	puts(buf);
}

int foma_cardcheck()
{
	puts("SIM CHECK");
	int n = 0; //retry
	//int res;
	char buf[BUF_SIZE+8];

	//AT&P ”­M
	at_send_command(fd_data, "&P"); /* “d˜b”Ô†•\Ž¦ */
	at_get_command_result(fd_data, buf);
	puts(buf);	
	
	while(1){
		SB_msleep(300);
		at_get_command_result(fd_data, buf);
		puts(buf);
		
		/* ƒŠƒUƒ‹ƒgƒ`ƒFƒbƒN */
		if(strstr(buf, "OK") != NULL){
			
			puts("AT&P OK");
			break;
		}
		
		if(n < 3){
			n = n + 1; 
			SB_msleep(1000);	
		}else{
			/* 3‰ñƒŠƒgƒ‰ƒC‚µ‚Ä‘Ê–Ú‚Èê‡A•œ‹Œƒtƒ[‚É“ü‚é */
			display_status("UIM_Check Failed.");
			puts("foma UIM card read error: start recovery");
			foma_recovery();
			return -1;
		}
	}
	display_status("SIM OK");
	return 0;
	
}

int foma_setting()
{   
	char buf[BUF_SIZE+8];
	sprintf(buf, "+CGDCONT=1,\"PPP\",\"pkxvb.dcm.ne.jp\"");
	at_send_command(fd_data, buf); /* APNÝ’è */
	at_get_command_result(fd_data, buf);
	puts(buf);
	SB_msleep(100);
	
	
	return 0;
}

int foma_atd_command(){
	int n = 0;
	int s = 0;
	int retry = 0; //NO CARRIER 
	char buf[BUF_SIZE+8];
	int value[3] = {0, 0, 0};
	
	puts("ATD starts:");
	
	ioctl(fd_gpio, GETGPIOVAL_LM, value);	
	
	RETRY:
	while(1){
		if(n < 3){
			//if antena level 3
			if(value[2] & ANT)
			{
				SB_msleep(1000);
				n = n + 1;
			}else{
				puts("ANT OK");
				break;
			}
		}else{
			puts("ANT OPEN");
			return -1;
		}
	}

	display_status("ANT OK");

	return 0;

	//ATD*99***
	at_send_command(fd_data, "D*99***1#");
	at_get_command_result(fd_data, buf);
	puts(buf);
	
	while(s <= 125){
		if(strstr(buf, "CONNECT") != NULL){
			puts("ATD:OK");
			break;
		}
		SB_msleep(1000);
		at_get_command_result(fd_data, buf);
		puts(buf);
		s = s + 1;
	
	}
	
	
	if(s >= 125){
		s = 0;
		while(s <= 125){
			if(strstr(buf, "NO CARRIER") != NULL){
				SB_msleep(1000);
				retry = 1;
				goto RETRY;
			}
			SB_msleep(1000);
			s = s + 1;
			at_get_command_result(fd_data, buf);
			puts(buf);
		}
		
	}
	
	if(s >= 125){
		s = 0;
		while(s <= 125){
			if(strstr(buf, "RESTRICTION") != NULL){
				//–Ô‹K§Žž‚Ìˆ—ŒãAÄ‹Aˆ—
				if(foma_atd_control() < 0)
				{
					puts("atd_control");
					system("reboot");
					return -1;
				}
			}
			at_get_command_result(fd_data, buf);
			puts(buf);
		}
		
		puts("ATD ERROR:");
		system("reboot");
	}
	
	/* ƒEƒFƒCƒg‚Ì‘}“ü */
	SB_msleep(500);
	
	display_status("ATD OK");
	return 0;
}

int foma_PACKET_chk(){ 

	int value[3] = {0, 0, 0};
	
	ioctl(fd_gpio, GETGPIOVAL_LM, value);	
	
	if(value[2] & ANT)
		{
		puts("PACKET:out");
		display_status("PACKET:out");
		}
	else
		{
		puts("PACKET:in");
		display_status("PACKET:in");
		}
	return 0;
}


int foma_atd_control()
{
	//RESTRICTION‚ð“Ç‚ñ‚Å‚©‚ç”ò‚Ô‘O’ñ‚Å
	char buf[BUF_SIZE+8];
	
	display_status("ATD RESTRICTION");
	SB_msleep(60000);
	
	//–³ŒÀƒ‹[ƒv
	while(1)
	{
		at_send_command(fd_data, "+PNRII");	
		at_get_command_result(fd_data, buf);
		
		if(strstr(buf, "+PNRII:0") != NULL)
		{
			puts("PNRII OK");
			SB_msleep(1000);
			foma_atd_command();
			return 0;
		}
			
		SB_msleep(60000);
	}
	
	//–{—ˆ‚Í‚ ‚è‚¦‚È‚¢
	puts("atd_control:program ends");
	return -1;
}


int foma_recovery()
{
	int s = 0;
	int n = 0; //retry‰ñ”
	char buf[BUF_SIZE+8];
	
	display_status("foma_recovery start");
	
	//XCD‚ªGND‚É‚È‚Á‚Ä‚¢‚é‚©ƒ`ƒFƒbƒN
	//XCD = DCD
	if(get_dcd(fd_data) == OFF)
	{
		/* XER(DTR3) OPEN */
		set_dtr(fd_data); 
		at_get_command_result(fd_data, buf);
		while(s < 60){
			if(strstr(buf, "NO CARRIER") != NULL){
				break;			
			}else{
				SB_msleep(1000);
			}
			s = s + 1;
		}   
		if(s >= 60){
			//500msˆÈã‚ÌƒEƒFƒCƒg
			SB_msleep(700);
			/* XER(DTR3) GND */
			set_dtr_off(fd_data);			
			foma_repower_on();
		}
		//500msˆÈã‚ÌƒEƒFƒCƒg
		SB_msleep(700);
		/* XER(DTR3) GND */
		set_dtr_off(fd_data);
	}
	
	/* 3sˆÈã‚ÌƒEƒFƒCƒg */
	SB_msleep(3500);
	
	/* AT*DHWRST–½—ß‚ÌŽÀs */
	at_send_command(fd_data, "*DHWRST");
	at_get_command_result(fd_data, buf);
	puts(buf);
	
	while(1){
		// ƒŠƒUƒ‹ƒgƒ`ƒFƒbƒN
		if(strstr(buf, "Reboot") != NULL){
			at_get_command_result(fd_data, buf);
		}
		if(strstr(buf, "OK") != NULL){
					puts(buf);
					break;
		}
		SB_msleep(1000);
		at_get_command_result(fd_data, buf);
		
		if(n >= 5){
			puts("*DWWRST Error");
			foma_repower_on();			
			break;				
		}
		
		n = n + 1;
	}
	
	if((foma_ledg_open_40s()) < 0){
		foma_repower_on();
	}
	
	if((foma_ledg_gnd_8s()) < 0){
		foma_repower_on();
	}
	
	foma_cardcheck();
	
	return 0;
}
int foma_repower_on(){
	display_status("Foma retrys power on.");
	SB_msleep(500);
	if(foma_ledg_open_60s() < 0){
		vcc_off();
		display_status("Foma recover failed:VCC OFF");
		puts("re-poweron failed. VCC OFF");
		
		return -1;
	}
	SB_msleep(1000);
	
	foma_module_power_on();
	
	return 0;
		
}

int foma_reconnect()
{
	char buf[BUF_SIZE+8];
	
	SB_msleep(1000);
	at_send_command(fd_data, "+PNTII");
	at_get_command_result(fd_data, buf);
	display_status("MODULE RE-CONNECT");

	SB_msleep(1000);
	if(strstr(buf, "PNRII:0") != NULL)
	{
		puts("PNRII OK");
		SB_msleep(1000);
		foma_atd_command();
		return 0;
	}
	else if(strstr(buf, "PNRII:1") != NULL)
	{
		puts("PNRII=1");
		foma_atd_control();
	}
		
	return -1;
} 

int foma_ledg_open_40s(){
	
	int value[3] = {0, 0, 0};
	int i;

	puts("LEDG-Open Check:");
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	/* 40•bˆÈ“à‚ÉLEDG‚ªOpen‚É‚È‚è‚Ü‚· */
	/*@–³üó‘Ô‚É‚æ‚Á‚Äˆ—ŽžŠÔ‚É•Ï‰»:max 40s */
	for(i = 0; i < 40; i=i+1)
	{
		ioctl(fd_gpio, GETGPIOVAL_LM, value);
		if(value[2] & LEDG)
		{
			puts("LEDG Open OK");
			break;
		}
		else
		{
			puts("LEDG GND. Wait...");
		}

		SB_msleep(2000);

	}
	if (i > 40)
	{
		puts("LEDG GND:System Reboot");
		return -1;
	}

	return 0;
}

int foma_ledg_open_60s(){
	
	int value[3] = {0, 0, 0};
	int i;

	puts("LEDG-Open Check:");
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	/* 60•bˆÈ“à‚ÉLEDG‚ªOpen‚É‚È‚è‚Ü‚· */
	/*@–³üó‘Ô‚É‚æ‚Á‚Äˆ—ŽžŠÔ‚É•Ï‰»:max 60s */
	for(i = 0; i < 60; i=i+2)
	{
		ioctl(fd_gpio, GETGPIOVAL_LM, value);
		if(value[2] & LEDG)
		{
			puts("LEDG Open OK");
			break;
		}
		else
		{
			puts("LEDG GND. Wait...");
		}

		SB_msleep(2000);

	}
	
	if (i > 60)
	{
		puts("LEDG GND 60s");
		return -1;
	}

	return 0;
}

int foma_ledg_gnd_8s(){
	
	int i;
	//char buf[BUF_SIZE+8];
	int value[3] = {0, 0, 0};
	
	//sprintf(buf, "value[2]=%x",value[2]);
	//puts(buf);
	
	puts("LEDG-GND CHECK:");
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	/* 8•bˆÈ“à‚ÉLEDG‚ªGND‚É—Ž‚¿‚½‚©ƒ`ƒFƒbƒN‚µ‚Ü‚· */
	for(i = 0; i < 8; i++)
	{
		SB_msleep(1000);
		
		//sprintf(buf, "value[2]=%x",value[2]);
		//puts(buf);

		ioctl(fd_gpio, GETGPIOVAL_LM, value);
		if(value[2] & LEDG)
		{
			puts("LEDG OPEN, WAIT...");
		}
		else
		{
			puts("LEDG GND OK");
			break;
		}
	}
	if (i >= 8)
	{
		puts("LEDG OPEN(not GND)");
		return -1;
	}
	
	return 0;
}


int vcc_off(){
	/* “dŒ¹‹Ÿ‹‹’âŽ~ */
	int value[3] = {0, 0, 0};
	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	value[2] |= PC14;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	SB_msleep(500);
	return 0;
}

int kddi_module_power_on()  
{
	display_status("Power on sequence start.");
	// Variable declaration
	int value[3] = {0, 0, 0};
	int res;
	int i;
	
	/* GPIO‚ÌƒI[ƒvƒ“ */
	fd_gpio = open(SB_GPIO_DEVICE, O_RDWR);
	if(fd_gpio < 0)
	{
		puts("GPIO open error!");
		return -1;
	}

	TRY:
	res = ioctl(fd_gpio, GETGPIOVAL_LM, value);
	if(res < 0)
	{
		puts(strerror(errno));
	}
	value[2] &= ~PWR_ON;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);

//	puts("Initialize start for shutdown, wait 10 seconds...");
//	SB_msleep(10000);

	ioctl(fd_gpio, GETGPIOVAL_LM, value);
	if(value[2] & PS_HOLD)
	{
		puts("PS_HOLD=H, then now start to shutdown, wait 3 seconds...");

//		value[2] &= ~PS_HOLD;
		value[2] |= PWR_ON;
		ioctl(fd_gpio, SETGPIOVAL_LM, value);

		SB_msleep(3000);

		ioctl(fd_gpio, GETGPIOVAL_LM, value);
//		value[2] &= ~PS_HOLD;
		value[2] &= ~PWR_ON;
		ioctl(fd_gpio, SETGPIOVAL_LM, value);

		for(i = 0; i < 10; i++)
		{
			SB_msleep(1000);

			ioctl(fd_gpio, GETGPIOVAL_LM, value);
			if(value[2] & PS_HOLD)
			{
				puts("PS_HOLD=H, Restart waiting...");
			}
			else
			{
				puts("PS_HOLD=L, Restarted OK");
				break;
			}
		}
		if (i >= 10)
		{
			puts("PS_HOLD=H, Restart error, retry");
			goto TRY;
		}
	}
	else
	{
		puts("PS_HOLD=L, No action to shutdown");
	}
	
	value[2] |= PWR_ON;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	puts("PWR_ON=H(PC08:ON)");
	SB_msleep(1000);
	
	while(1)
	{
		ioctl(fd_gpio, GETGPIOVAL_LM, value);
		if(value[2] & PS_HOLD)
		{
			puts(".");
			break;
		}
		else
		{
			puts("PS_HOLD=L");
		}
		
		SB_msleep(1000);
	}
	
	value[2] &= ~PWR_ON;
	ioctl(fd_gpio, SETGPIOVAL_LM, value);
	puts("PWR_ON=L(PC08:OFF)");
	
	return 0;
}

int mt_server_socket()
{
	// bzero((char *)&client_addr,sizeof(client_addr));
	// ‰Šú‰»
	memset(&client_addr,0,sizeof(client_addr));
	
	// \‘¢‘ÌÝ’è
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(info_ip_mente);
	client_addr.sin_port = htons(56090);
	puts(info_ip_mente);
	
	// ƒ\ƒPƒbƒg¶¬
	if((mt_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		puts("mt_client:socket Error");
		return -1;
	}
	// Ú‘±
	if (connect(mt_sock, (struct sockaddr *)&client_addr,
			sizeof(client_addr)) < 0 ) {
		puts("mt_client:connect Error");
		close(mt_sock);
		return -1;
	}
	
	return 0;
}



int jr_udp_socket_open()
{
	// ‘—Mƒ\ƒPƒbƒg¶¬
	udp_snd_addr.sin_family = AF_INET;
	udp_snd_addr.sin_addr.s_addr = inet_addr(info_ip_jr);
	udp_snd_addr.sin_port = htons(56001);
	if((jrs_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		puts("jr_client:socket");
		return EXIT_FAILURE;
	}
	// Ú‘±
//	if(bind(jrs_sock, (struct sockaddr *)&udp_snd_addr, sizeof(udp_snd_addr)) < 0){
//		puts("jrs_client:bind");
//		return EXIT_FAILURE;
//	}

	udp_rcv_addr.sin_family = AF_INET;
	udp_rcv_addr.sin_addr.s_addr = INADDR_ANY;
	udp_rcv_addr.sin_port = htons(56001);
	// ŽóMƒ\ƒPƒbƒg¶¬
	if((jrr_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		puts("jr_client:socket");
		return EXIT_FAILURE;
	}
	// Ú‘±
	if(bind(jrr_sock, (struct sockaddr *)&udp_rcv_addr, sizeof(udp_rcv_addr)) < 0){
		puts("jrr_client:bind");
		return EXIT_FAILURE;
	}

	
	return 0;
}

int gps_initialize()
{
	fd_gps  = gps_open(GPS_PORT, &gps_init_tio, &gps_tio);
	if(fd_gps < 0)
	{
		return -1;
	}
	
	puts("gps_initialize OK");
	
	return 0;
}

int jrc_initialize()
{
	// GPSƒf[ƒ^‚ð“ú–{–³ü‚É“n‚·
	fd_jrc_gps = jrc_gps_open(JRC_GPS_PORT, &jrc_gps_init_tio, &jrc_gps_tio);
	// ƒT[ƒo[‚©‚ç‚Ìƒf[ƒ^‚ð“ú–{–³ü‚ç“n‚·AŽó‚¯Žæ‚é
	fd_jrc_send = jrc_send_open(JRC_SEND_PORT, &jrc_send_init_tio, &jrc_send_tio);
	
	if(fd_jrc_gps < 0 || fd_jrc_send < 0)
	{
		return -1;
	}
	
	puts("jrc_initialize OK");
	
	return 0;
}

int foma_at_get_range()
{
	char buf[BUF_SIZE];
	int n = 0;
	
	while(n > 3)
	{
		at_send_command(fd_data, "*DANTE");
		SB_msleep(1);
		at_get_command_result(fd_data, buf);
		puts(buf);
		if(strstr(buf, "0") != NULL)
		{
			display_kddi_status("*DANTE Out of range");
			sprintf(buf, "POS Out of range: %s,%s at %s", ido_str, keido_str, gps_last_get);
			puts(buf);
			lrange[0] = 0x30;
			return 0;		
		}
		else if(strstr(buf, "1") != NULL)
		{
				display_kddi_status("Level 0");
				lrange[0] = 0x31;
				return 1;		
		}
		else if(strstr(buf, "2") != NULL)
		{
				display_kddi_status("Level 1");
				lrange[0] = 0x32;
				return 2;		
		}
		else if(strstr(buf, "3") != NULL)
		{
				display_kddi_status("Level 2");
				lrange[0] = 0x33;
				return 3;		
		}
		else if(strstr(buf, "4") != NULL)
		{
				display_kddi_status("Level 3");
				lrange[0] = 0x34;
				return 4;		
		}
		
		n = n+1;
	}


	return -1;
}

int at_get_range()
{
	char buf[BUF_SIZE+8];
	char *res_part;

	at_send_command(fd_ctrl, "$31?"); /* “ú•tŽž•\Ž¦ */
	at_get_command_result(fd_ctrl, buf);
	puts(buf);
	
	at_send_command(fd_ctrl, "$30=0"); /* ŽóMó‘Ô•\Ž¦ */
	at_get_command_result(fd_ctrl, buf);
	puts(buf);
	
	res_part = strtok(buf, "\r\n");
	while(res_part != NULL)
	{
		
		if(strcmp(res_part, "0") == 0)
		{
			display_kddi_status("$30 Out of range");
			sprintf(buf, "POS Out of range: %s,%s at %s", ido_str, keido_str, gps_last_get);
			puts(buf);
			lrange[0] = 0x30;
			return 0;
		}
		else if(strcmp(res_part, "1") == 0)
		{
			display_kddi_status("Level 0");
			lrange[0] = 0x31;
			return 1;
		}
		else if(strcmp(res_part, "2") == 0)
		{
			display_kddi_status("Level 1");
			lrange[0] = 0x32;
			return 2;
		}
		else if(strcmp(res_part, "3") == 0)
		{
			display_kddi_status("Level 2");
			lrange[0] = 0x33;
			return 3;
		}
		else if(strcmp(res_part, "4") == 0)
		{
			display_kddi_status("Level 3");
			lrange[0] = 0x34;
			return 4;
		}
		
		res_part = strtok(NULL, "\r\n");
	}

		
	return -1;
}

int at_connect()
{
	int dcd = OFF;
	int i;
	char buf[BUF_SIZE+8];
	char *res_part;
	
	at_send_command(fd_ctrl, "+CAD?"); /* Œ—ŠOEŒ—“à•\Ž¦ */
	at_get_command_result(fd_ctrl, buf);
	puts(buf);
	
	res_part = strtok(buf, "\r\n");
	while(res_part != NULL)
	{
		if(strcmp(res_part, "0") == 0)
		{
			puts("+CAD out of range");
			return -1;
		}
		
		res_part = strtok(NULL, "\r\n");
	}
	
	at_send_command(fd_ctrl, "$30=0"); /* ŽóMó‘Ô•\Ž¦ */
	at_get_command_result(fd_ctrl, buf);
	puts(buf);
	
	res_part = strtok(buf, "\r\n");
	while(res_part != NULL)
	{
		if(strcmp(res_part, "0") == 0)
		{
			puts("$30 out of range");
			return -1;
		}
		
		res_part = strtok(NULL, "\r\n");
	}
	
	display_connect_status("Connecting...");
	
	at_send_command(fd_data, "D"); /* ƒpƒPƒbƒg’ÊM”­M */
	if (at_get_command_result(fd_data, buf) == 0)
	{
		// add 2012/03/07 ATD ƒRƒ}ƒ“ƒh‰ž“š‚È‚µ‚ÍƒVƒXƒeƒ€ƒŠƒu[ƒg
		// kddi_module_reset();
		// add_error("ATD respons time out !! ");
		// SB_msleep(10000); // add 2012/03/08 
		// system("reboot");
		// return EXIT_FAILURE;
	}
	puts(buf);
	
	/* DCD‚ªON‚É‚È‚é‚Ì‚ð‘Ò‚Â */
	i = 0;
	while(1)
	{
		i++;
		dcd = get_dcd(fd_data);
		switch(dcd)
		{
		case ON:
			puts("DCD ON");
			break;
		case OFF:
			puts("DCD OFF");
			break;
		default:
			return -1;
			break;
		}
		
		if(dcd == ON) 
		{
			break;
		}
		
		if(i > 10)
		{
			puts("connection timeout.");
			display_connect_status("T.O. Disconnect.");

			at_send_command(fd_ctrl, "$31?"); /* “ú•tŽž•\Ž¦ */
			at_get_command_result(fd_ctrl, buf);
			puts(buf);
			return -1;
		}
		
		SB_msleep(3000);  // up 500 --> 2000  2012/03/08
	}
   	clear_gps(); // add 2012/03/08 

	display_connect_status("Connected.");
	at_send_command(fd_ctrl, "$31?"); /* “ú•tŽž•\Ž¦ */
	at_get_command_result(fd_ctrl, buf);
	puts(buf);
	return 0;
}

void socket_Connect(){
	
}


int at_send_data(char *str, int len)
{
	char logstr[128];//52¨55 12/06/30
	char buf[BUF_SIZE+8];
		
	// pthread_mutex_lock(&con_mutex);
	// puts("mutex lock:at send data");	 // add 2012/03/07
	
	if(get_dcd(fd_data) == OFF)
	{
		// pthread_mutex_unlock(&con_mutex);
		// puts("mutex unlock:at send data");	 // add 2012/03/07
		puts("udp_send:DCD OFF");
		at_send_command(fd_ctrl, "$31?"); /* “ú•tŽž•\Ž¦ */
		at_get_command_result(fd_ctrl, buf);
		puts(buf);
		SB_msleep(1000);                            // add 2012/03/10   2012/03/08
		system("reboot");
		return -1;	
	}
	else
	{
		write(fd_data, str, len);
		// pthread_mutex_unlock(&con_mutex);
		// puts("mutex unlock:at send data");	 // add 2012/03/07
	
		sprintf(logstr, "udp_send:%d >", len);
		puts(logstr);
		// puts(str);
	}
	
	return 0;
}

int socket_send_data(char *str, int len)
{
	char logstr[128];//52¨55 12/06/30
	//char buf[BUF_SIZE+2];
	
	if(mt_sock < 0){
		puts("socket_send_data:Not Connected");
	}
	write(mt_sock, str, len);
	sprintf(logstr, "socket_send:%d >", len);
	//puts(logstr);
	// puts(str);
	
	return 0;
}

int socket_udp_send_data(char *str, int len)
{
	char logstr[128];//52¨55 12/06/30
	//char buf[BUF_SIZE+2];
	
	if(jrs_sock < 0){
		puts("socket_udp_send:Not Connected");
	}
	sendto(jrs_sock, str, len, 0, (struct sockaddr *)&udp_snd_addr, sizeof(udp_snd_addr));
	sprintf(logstr, "socket_udp_send:%d >", len);
	//puts(logstr);

	
	return 0;
}

int at_receive_data(char *str, int len)
{
	int res;
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */
	
	SB_msleep(500); //ƒf[ƒ^‚ª’™‚Ü‚é‚Ü‚Å‘Ò‚Â
	
	
	// pthread_mutex_lock(&con_mutex);
	// puts("mutex lock:at receive data");	 // add 2012/03/07
	
	if(get_dcd(fd_data) == OFF)
	{
		puts("receive");
		at_disconnect();
		if(at_connect() < 0)
		{
			pthread_mutex_unlock(&con_mutex);
			// puts("mutex unlock:at receive data");	 // add 2012/03/07
			puts("udp_receive:DCD OFF");
			SB_msleep(1000);                            // add 2012/03/10   2012/03/08
			system("reboot");
			return -1;	
		}
	}
	
	// pthread_mutex_unlock(&con_mutex);
	// puts("mutex unlock:at receive data");	 // add 2012/03/07
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
    Timeout.tv_usec = RECV_TIMEOUT_USEC;	/* ƒ}ƒCƒNƒ•b */
    Timeout.tv_sec = RECV_TIMEOUT_SEC;	/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(fd_data, &readfs);
	
	/* ƒƒ‚ƒŠ‘|œ */
	memset(str, 0 , len);
	
	while(1)
	{
        if(select(fd_data + 1, &readfs, NULL, NULL, &Timeout) <= 0)
        {
            return 0;
        }
        res = read(fd_data, str, len - 1);
        
        if (res > 0)
        {
        	break;
        }
	}
	
	str[res] = '\0';
	//puts(str);
	
	if(res > 0)
	{
		if(strstr(str, "CONNECT") != NULL || strstr(str, "NO CARRIER") != NULL || strstr(str, "RING") != NULL)
		{
			puts(str);
			puts("udp_receive:AT Commad@clear");
			return 0;
		}
		char logstr[128];
		sprintf(logstr, "udp_receve:%d >", res);
		puts(logstr);

	}
	
	return res;
}

int socket_receive_data(char *str, int len)
{
	int res;
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */
	
	SB_msleep(500); //ƒf[ƒ^‚ª’™‚Ü‚é‚Ü‚Å‘Ò‚Â

	if(mt_sock < 0){
		puts("socket_receive_data:Not Connected");
	}
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
    Timeout.tv_usec = RECV_TIMEOUT_USEC;	/* ƒ}ƒCƒNƒ•b */
    Timeout.tv_sec = RECV_TIMEOUT_SEC;	/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(mt_sock, &readfs);
	
	/* ƒƒ‚ƒŠ‘|œ */
	memset(str, 0 , len);
	
	while(1)
	{
        if(select(mt_sock + 1, &readfs, NULL, NULL, &Timeout) <= 0)
        {
            return 0;
        }
        res = read(mt_sock, str, len - 1);
        
        if (res > 0)
        {
        	break;
        }
	}
	
	str[res] = '\0';
	//puts(str);
	
	if(res > 0)
	{
		char logstr[128];
		sprintf(logstr, "socket_receve:%d >", res);
		// puts(logstr);

	}
	
	return res;
}

int socket_udp_receive_data(char *str, int len)
{
	int res;
	int udp_srvlen;
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */


	if(jrr_sock < 0){
		puts("udp_socket_receive:Not Connected");
	}
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
    Timeout.tv_usec = 100;	/* ƒ}ƒCƒNƒ•b */
    Timeout.tv_sec = 0;	/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(jrr_sock, &readfs);
	
	/* ƒƒ‚ƒŠ‘|œ */
	memset(str, 0 , len-1);
	
	udp_srvlen = sizeof(udp_rcv_addr);

	while(1)
	{
	//	res = select(jr_sock + 1, &readfs, NULL, NULL, &Timeout);
    //    sprintf(pbuf, "select %d",res);
    //    puts(pbuf);
    //    if(res <= 0)
    //    {
    //        return 0;
    //    }
        //res = recvfrom(jr_sock, str, len - 1, 0, (struct sockaddr *)&udp_rev_addr, &udp_srvlen);
        res = recv(jrr_sock, str, len - 1, 0);
         if (res > 0)
        {
        	break;
        }
	}
	
	if(res > 0)
	{
		char logstr[1024];
		char logstr2[1024];
		char wkstr[1024];
		int wi;
		
		sprintf(logstr, "udp_receve:%d >", res);
		puts(logstr);

		strcpy(logstr, "[");
		strcpy(logstr2, "NM:");
		if(res > 0)
		{
			for(wi = 0; wi < res; wi++)
			{
				sprintf(logstr, "%s%02X ",logstr, *(str+wi)); 
				sprintf(logstr2, "%s%02X",logstr2, *(str+wi)); 
			}
			sprintf(wkstr, "---> NM_Recv:%d %s]", res, logstr);
			puts(wkstr);	
	
			sprintf(wkstr, "Recv %.24s",logstr2);
			display_status(wkstr);  //

		}	
	}
	return res;
}

int at_receive_update_data(char *str, int len)
{
	int res;
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */

	SB_msleep(200); //ƒoƒbƒtƒ@‚Éƒf[ƒ^‚ª’™‚Ü‚é‚Ì‚ð‘Ò‚Â ‘z’èG500ByteŽóM
	
	pthread_mutex_lock(&con_mutex);
	// puts("mutex lock:at receive update");	 // add 2012/03/07
	
	if(get_dcd(fd_data) == OFF)
	{
		puts("receive_update");
		at_disconnect();
		if(at_connect() < 0)
		{
			pthread_mutex_unlock(&con_mutex);
			// puts("mutex unlock:at receive update");	 // add 2012/03/07
			puts("udp_receive:DCD OFF");
			return -1;
		}
	}
	
	pthread_mutex_unlock(&con_mutex);
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
    Timeout.tv_usec = RECV_TIMEOUT_USEC;	/* ƒ}ƒCƒNƒ•b */
    Timeout.tv_sec = RECV_TIMEOUT_SEC;	/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(fd_data, &readfs);
	
	/* ƒƒ‚ƒŠ‘|œ */
	memset(str, 0 , len);
	

	while(1)
	{
        if(select(fd_data + 1, &readfs, NULL, NULL, &Timeout) <= 0)
        {
            return 0;
        }
        res = read(fd_data, str, len - 1);
        
        if (res > 0)
        {
        	break;
        }
	}
	
	str[res] = '\0';
	
	if(res > 0)
	{
		char logstr[128];
		sprintf(logstr, "udp_receve:%d >", res);
		puts(logstr);
	}
	
	return res;
}

int socket_receive_update_data(char *str, int len)
{
	int res;
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */

	SB_msleep(200); //ƒoƒbƒtƒ@‚Éƒf[ƒ^‚ª’™‚Ü‚é‚Ì‚ð‘Ò‚Â ‘z’èG500ByteŽóM

	if(mt_sock < 0){
		puts("socket_receive_update:Not Connected");
	}
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
    Timeout.tv_usec = RECV_TIMEOUT_USEC;	/* ƒ}ƒCƒNƒ•b */
    Timeout.tv_sec = RECV_TIMEOUT_SEC;	/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(mt_sock, &readfs);
	
	/* ƒƒ‚ƒŠ‘|œ */
	memset(str, 0 , len);
	

	while(1)
	{
        if(select(mt_sock + 1, &readfs, NULL, NULL, &Timeout) <= 0)
        {
            return 0;
        }
        res = read(mt_sock, str, len - 1);
        
        if (res > 0)
        {
        	break;
        }
	}
	
	str[res] = '\0';
	
	if(res > 0)
	{
		char logstr[128];
		sprintf(logstr, "udp_receve:%d >", res);
		puts(logstr);
	}
	
	return res;
}

int at_disconnect()
{
	char buf[BUF_SIZE+2];
	
	display_connect_status("Disconnect.");
	at_send_command(fd_ctrl, "H"); /* ƒpƒPƒbƒg’ÊMØ’f */
	at_get_command_result(fd_ctrl, buf);
	puts(buf);
	
	return 0;
}

int mt_socket_close()
{
	close(mt_sock);
	
	return 0;
}

int jr_socket_close()
{
	close(jrs_sock);
	close(jrr_sock);
	
	return 0;
}

int at_data_open(char *dev_name, struct termios *p_oldtio, struct termios *p_newtio)
{
	int fd;
	
	fd = open(dev_name, O_RDWR | O_NOCTTY );
	if(fd < 0)
	{
		add_error("at_data_open");
		return -1;
	}
	
	/* ƒVƒŠƒAƒ‹ƒ|[ƒgÝ’è‘Þ”ð */
	tcgetattr(fd, p_oldtio);
	bzero(p_newtio, sizeof(*p_newtio));
	tcgetattr(fd, p_newtio);
	
	/* ƒtƒ‰ƒOƒZƒbƒg */
	p_newtio->c_cflag = DATA_BRATE | CS8 | CREAD | CMSPAR | PARODD; /* 1200 - 230400 */
	p_newtio->c_iflag = IGNPAR;
	p_newtio->c_oflag = 0;
	p_newtio->c_lflag = 0;
	
	/* §Œä•¶Žš‚Ì‰Šú‰» */
	p_newtio->c_cc[VINTR]		= 0;	/* Ctrl-c */ 
	p_newtio->c_cc[VQUIT]		= 0;	/* Ctrl-\ */
	p_newtio->c_cc[VERASE]		= 0;	/* del */
	p_newtio->c_cc[VKILL]		= 0;	/* @ */
	p_newtio->c_cc[VEOF]		= 4;	/* Ctrl-d */
	p_newtio->c_cc[VTIME]		= 0;	/* ƒLƒƒƒ‰ƒNƒ^ŠÔƒ^ƒCƒ}‚ðŽg‚í‚È‚¢ */
	p_newtio->c_cc[VMIN]		= 1;	/* 1•¶Žš—ˆ‚é‚Ü‚ÅC“Ç‚Ýž‚Ý‚ðƒuƒƒbƒN‚·‚é */
	p_newtio->c_cc[VSWTC]		= 0;	/* '\0' */
	p_newtio->c_cc[VSTART]		= 0;	/* Ctrl-q */ 
	p_newtio->c_cc[VSTOP]		= 0;	/* Ctrl-s */
	p_newtio->c_cc[VSUSP]		= 0;	/* Ctrl-z */
	p_newtio->c_cc[VEOL]		= 0;	/* '\0' */
	p_newtio->c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	p_newtio->c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	p_newtio->c_cc[VWERASE]		= 0;	/* Ctrl-w */
	p_newtio->c_cc[VLNEXT]		= 0;	/* Ctrl-v */
	p_newtio->c_cc[VEOL2]		= 0;	/* '\0' */
	
	cfsetospeed(p_newtio, DATA_BRATE);
	cfsetispeed(p_newtio, DATA_BRATE);
	
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, p_newtio);
	return fd;
}

int at_ctrl_open(char *dev_name, struct termios *p_oldtio, struct termios *p_newtio)
{
	int fd;
	
	fd = open(dev_name, O_RDWR | O_NOCTTY );
	if(fd < 0)
	{
		add_error("at_ctrl_open");
		return -1;
	}
	
	/* ƒVƒŠƒAƒ‹ƒ|[ƒgÝ’è‘Þ”ð */
	tcgetattr(fd, p_oldtio);
	bzero(p_newtio, sizeof(*p_newtio));
	tcgetattr(fd, p_newtio);
	
	/* ƒtƒ‰ƒOƒZƒbƒg */
	p_newtio->c_cflag = CTRL_BRATE | CS8 | CREAD | CMSPAR | PARODD;
	p_newtio->c_iflag = IGNPAR;
	p_newtio->c_oflag = 0;
	p_newtio->c_lflag = 0;
	
	/* §Œä•¶Žš‚Ì‰Šú‰» */
	p_newtio->c_cc[VINTR]		= 0;	/* Ctrl-c */ 
	p_newtio->c_cc[VQUIT]		= 0;	/* Ctrl-\ */
	p_newtio->c_cc[VERASE]		= 0;	/* del */
	p_newtio->c_cc[VKILL]		= 0;	/* @ */
	p_newtio->c_cc[VEOF]		= 4;	/* Ctrl-d */
	p_newtio->c_cc[VTIME]		= 0;	/* ƒLƒƒƒ‰ƒNƒ^ŠÔƒ^ƒCƒ}‚ðŽg‚í‚È‚¢ */
	p_newtio->c_cc[VMIN]		= 1;	/* 1•¶Žš—ˆ‚é‚Ü‚ÅC“Ç‚Ýž‚Ý‚ðƒuƒƒbƒN‚·‚é */
	p_newtio->c_cc[VSWTC]		= 0;	/* '\0' */
	p_newtio->c_cc[VSTART]		= 0;	/* Ctrl-q */ 
	p_newtio->c_cc[VSTOP]		= 0;	/* Ctrl-s */
	p_newtio->c_cc[VSUSP]		= 0;	/* Ctrl-z */
	p_newtio->c_cc[VEOL]		= 0;	/* '\0' */
	p_newtio->c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	p_newtio->c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	p_newtio->c_cc[VWERASE]		= 0;	/* Ctrl-w */
	p_newtio->c_cc[VLNEXT]		= 0;	/* Ctrl-v */
	p_newtio->c_cc[VEOL2]		= 0;	/* '\0' */
	
	cfsetospeed(p_newtio, CTRL_BRATE);
	cfsetispeed(p_newtio, CTRL_BRATE);
	
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, p_newtio);
	
	return fd;
}

int gps_open(char *dev_name, struct termios *p_oldtio, struct termios *p_newtio)
{
	int fd;
	
	fd = open(dev_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(fd < 0)
	{
		add_error("gps_open");
		return -1;
	}
	
	/* ƒVƒŠƒAƒ‹ƒ|[ƒgÝ’è‘Þ”ð */
	tcgetattr(fd, p_oldtio);
	bzero(p_newtio, sizeof(*p_newtio));
	tcgetattr(fd, p_newtio);
	
	/* ƒtƒ‰ƒOƒZƒbƒg */
	p_newtio->c_cflag = GPS_BRATE | CS8 | CREAD;
	p_newtio->c_iflag = 0;
	p_newtio->c_oflag = OCRNL;
	p_newtio->c_lflag = ICANON;
	
	/* §Œä•¶Žš‚Ì‰Šú‰» */
	p_newtio->c_cc[VINTR]		= 0;	/* Ctrl-c */ 
	p_newtio->c_cc[VQUIT]		= 0;	/* Ctrl-\ */
	p_newtio->c_cc[VERASE]		= 0;	/* del */
	p_newtio->c_cc[VKILL]		= 0;	/* @ */
	p_newtio->c_cc[VEOF]		= 4;	/* Ctrl-d */
	p_newtio->c_cc[VTIME]		= 0;	/* ƒLƒƒƒ‰ƒNƒ^ŠÔƒ^ƒCƒ}‚ðŽg‚í‚È‚¢ */
	p_newtio->c_cc[VMIN]		= 1;	/* 1•¶Žš—ˆ‚é‚Ü‚ÅC“Ç‚Ýž‚Ý‚ðƒuƒƒbƒN‚·‚é */
	p_newtio->c_cc[VSWTC]		= 0;	/* '\0' */
	p_newtio->c_cc[VSTART]		= 0;	/* Ctrl-q */ 
	p_newtio->c_cc[VSTOP]		= 0;	/* Ctrl-s */
	p_newtio->c_cc[VSUSP]		= 0;	/* Ctrl-z */
	p_newtio->c_cc[VEOL]		= 0;	/* '\0' */
	p_newtio->c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	p_newtio->c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	p_newtio->c_cc[VWERASE]		= 0;	/* Ctrl-w */
	p_newtio->c_cc[VLNEXT]		= 0;	/* Ctrl-v */
	p_newtio->c_cc[VEOL2]		= 0;	/* '\0' */
	
	cfsetospeed(p_newtio, GPS_BRATE);
	cfsetispeed(p_newtio, GPS_BRATE);
	
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, p_newtio);
	
	return fd;
}

int jrc_gps_open(char *dev_name, struct termios *p_oldtio, struct termios *p_newtio)
{
	int fd;
	
	fd = open(dev_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(fd < 0)
	{
		add_error("jrc_gps_open");
		return -1;
	}
	
	/* ƒVƒŠƒAƒ‹ƒ|[ƒgÝ’è‘Þ”ð */
	tcgetattr(fd, p_oldtio);
	bzero(p_newtio, sizeof(*p_newtio));
	tcgetattr(fd, p_newtio);
	
	/* ƒtƒ‰ƒOƒZƒbƒg */
	p_newtio->c_cflag = JRC_GPS_BRATE | CS8 | CREAD;
	p_newtio->c_iflag = IGNPAR;
	p_newtio->c_oflag = 0;
	p_newtio->c_lflag = 0;
	
	/* §Œä•¶Žš‚Ì‰Šú‰» */
	p_newtio->c_cc[VINTR]		= 0;	/* Ctrl-c */ 
	p_newtio->c_cc[VQUIT]		= 0;	/* Ctrl-\ */
	p_newtio->c_cc[VERASE]		= 0;	/* del */
	p_newtio->c_cc[VKILL]		= 0;	/* @ */
	p_newtio->c_cc[VEOF]		= 4;	/* Ctrl-d */
	p_newtio->c_cc[VTIME]		= 0;	/* ƒLƒƒƒ‰ƒNƒ^ŠÔƒ^ƒCƒ}‚ðŽg‚í‚È‚¢ */
	p_newtio->c_cc[VMIN]		= 1;	/* 1•¶Žš—ˆ‚é‚Ü‚ÅC“Ç‚Ýž‚Ý‚ðƒuƒƒbƒN‚·‚é */
	p_newtio->c_cc[VSWTC]		= 0;	/* '\0' */
	p_newtio->c_cc[VSTART]		= 0;	/* Ctrl-q */ 
	p_newtio->c_cc[VSTOP]		= 0;	/* Ctrl-s */
	p_newtio->c_cc[VSUSP]		= 0;	/* Ctrl-z */
	p_newtio->c_cc[VEOL]		= 0;	/* '\0' */
	p_newtio->c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	p_newtio->c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	p_newtio->c_cc[VWERASE]		= 0;	/* Ctrl-w */
	p_newtio->c_cc[VLNEXT]		= 0;	/* Ctrl-v */
	p_newtio->c_cc[VEOL2]		= 0;	/* '\0' */
	
	cfsetospeed(p_newtio, JRC_GPS_BRATE);
	cfsetispeed(p_newtio, JRC_GPS_BRATE);
	
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, p_newtio);
	
	return fd;
}

int jrc_send_open(char *dev_name, struct termios *p_oldtio, struct termios *p_newtio)
{
	int fd;
	
	fd = open(dev_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(fd < 0)
	{
		add_error("jrc_send_open");
		return -1;
	}
	
	/* ƒVƒŠƒAƒ‹ƒ|[ƒgÝ’è‘Þ”ð */
	tcgetattr(fd, p_oldtio);
	bzero(p_newtio, sizeof(*p_newtio));
	tcgetattr(fd, p_newtio);
	
	/* ƒtƒ‰ƒOƒZƒbƒg */
	p_newtio->c_cflag = JRC_SEND_BRATE | CS8 | CREAD;
	p_newtio->c_iflag = IGNPAR;
	p_newtio->c_oflag = 0;
	p_newtio->c_lflag = 0;
	
	/* §Œä•¶Žš‚Ì‰Šú‰» */
	p_newtio->c_cc[VINTR]		= 0;	/* Ctrl-c */ 
	p_newtio->c_cc[VQUIT]		= 0;	/* Ctrl-\ */
	p_newtio->c_cc[VERASE]		= 0;	/* del */
	p_newtio->c_cc[VKILL]		= 0;	/* @ */
	p_newtio->c_cc[VEOF]		= 4;	/* Ctrl-d */
	p_newtio->c_cc[VTIME]		= 0;	/* ƒLƒƒƒ‰ƒNƒ^ŠÔƒ^ƒCƒ}‚ðŽg‚í‚È‚¢ */
	p_newtio->c_cc[VMIN]		= 1;	/* 1•¶Žš—ˆ‚é‚Ü‚ÅC“Ç‚Ýž‚Ý‚ðƒuƒƒbƒN‚·‚é */
	p_newtio->c_cc[VSWTC]		= 0;	/* '\0' */
	p_newtio->c_cc[VSTART]		= 0;	/* Ctrl-q */ 
	p_newtio->c_cc[VSTOP]		= 0;	/* Ctrl-s */
	p_newtio->c_cc[VSUSP]		= 0;	/* Ctrl-z */
	p_newtio->c_cc[VEOL]		= 0;	/* '\0' */
	p_newtio->c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	p_newtio->c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	p_newtio->c_cc[VWERASE]		= 0;	/* Ctrl-w */
	p_newtio->c_cc[VLNEXT]		= 0;	/* Ctrl-v */
	p_newtio->c_cc[VEOL2]		= 0;	/* '\0' */
	
	cfsetospeed(p_newtio, JRC_SEND_BRATE);
	cfsetispeed(p_newtio, JRC_SEND_BRATE);
	
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, p_newtio);
	lrange[0] = 0x20;
	lrange[1] = 0x00;
	
	return fd;
}

int lcd_open(char *dev_name, struct termios *p_oldtio, struct termios *p_newtio)
{
	int fd;
	
	fd = open(dev_name, O_RDWR | O_NOCTTY);
	if(fd < 0)
	{
		add_error("lcd_open");
		return -1;
	}
	
	/* ƒVƒŠƒAƒ‹ƒ|[ƒgÝ’è‘Þ”ð */
	tcgetattr(fd, p_oldtio);
	bzero(p_newtio, sizeof(*p_newtio));
	tcgetattr(fd, p_newtio);
	
	/* ƒtƒ‰ƒOƒZƒbƒg */
	p_newtio->c_cflag = LCD_BRATE | CS8;
	p_newtio->c_iflag = IGNPAR;
	p_newtio->c_oflag = 0;
	p_newtio->c_lflag = 0;
	
	/* §Œä•¶Žš‚Ì‰Šú‰» */
	p_newtio->c_cc[VINTR]		= 0;	/* Ctrl-c */ 
	p_newtio->c_cc[VQUIT]		= 0;	/* Ctrl-\ */
	p_newtio->c_cc[VERASE]		= 0;	/* del */
	p_newtio->c_cc[VKILL]		= 0;	/* @ */
	p_newtio->c_cc[VEOF]		= 4;	/* Ctrl-d */
	p_newtio->c_cc[VTIME]		= 0;	/* ƒLƒƒƒ‰ƒNƒ^ŠÔƒ^ƒCƒ}‚ðŽg‚í‚È‚¢ */
	p_newtio->c_cc[VMIN]		= 1;	/* 1•¶Žš—ˆ‚é‚Ü‚ÅC“Ç‚Ýž‚Ý‚ðƒuƒƒbƒN‚·‚é */
	p_newtio->c_cc[VSWTC]		= 0;	/* '\0' */
	p_newtio->c_cc[VSTART]		= 0;	/* Ctrl-q */ 
	p_newtio->c_cc[VSTOP]		= 0;	/* Ctrl-s */
	p_newtio->c_cc[VSUSP]		= 0;	/* Ctrl-z */
	p_newtio->c_cc[VEOL]		= 0;	/* '\0' */
	p_newtio->c_cc[VREPRINT]	= 0;	/* Ctrl-r */
	p_newtio->c_cc[VDISCARD]	= 0;	/* Ctrl-u */
	p_newtio->c_cc[VWERASE]		= 0;	/* Ctrl-w */
	p_newtio->c_cc[VLNEXT]		= 0;	/* Ctrl-v */
	p_newtio->c_cc[VEOL2]		= 0;	/* '\0' */
	
	cfsetospeed(p_newtio, LCD_BRATE);
	cfsetispeed(p_newtio, LCD_BRATE);
	
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, p_newtio);
	
	return fd;
}

int display_status(char *str)
{
	char disp_str[128];
	char cur_init[20];
	
	sprintf(disp_str, "%-32.32s", str);
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x24;
	cur_init[2] = 0x00;
	cur_init[3] = 0x00;
	cur_init[4] = 0x00;
	cur_init[5] = 0x00;
	
	pthread_mutex_lock(&lcd_mutex);
	write(fd_lcd, cur_init, 6);		/* ƒJ[ƒ\ƒ‹‚ð“KØ‚ÈˆÊ’u‚ÉˆÚ“® */
	write(fd_lcd, disp_str, 32);	/* î•ñ‚Ì‘‚«ž‚Ý */
	pthread_mutex_unlock(&lcd_mutex);
	
	return 0;
}

int display_gps(double ido, double keido, int km, char *name)
{
	char disp_str[128];
	char cur_init[20];
	
	sprintf(disp_str, "%.4lf,%.4lf(%d):", ido, keido, km);
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x24;
	cur_init[2] = 0x00;
	cur_init[3] = 0x00;
	cur_init[4] = 0x02;
	cur_init[5] = 0x00;
	
	pthread_mutex_lock(&lcd_mutex);
	write(fd_lcd, cur_init, 6);		/* ƒJ[ƒ\ƒ‹‚ð“KØ‚ÈˆÊ’u‚ÉˆÚ“® */
	write(fd_lcd, disp_str, strlen(disp_str));	/* î•ñ‚Ì‘‚«ž‚Ý */
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x28;
	cur_init[2] = 0x67;
	cur_init[3] = 0x02;
	cur_init[4] = 0x01;
	
	write(fd_lcd, cur_init, 5);		/* Š¿Žšƒ‚[ƒh:ON */
	write(fd_lcd, name, strlen(name));	/* î•ñ‚Ì‘‚«ž‚Ý */
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x28;
	cur_init[2] = 0x67;
	cur_init[3] = 0x02;
	cur_init[4] = 0x00;
	
	write(fd_lcd, cur_init, 5);		/* Š¿Žšƒ‚[ƒh:OFF */
	pthread_mutex_unlock(&lcd_mutex);
	
	return 0;
}

int display_gps_fail()
{
	char disp_str[128];
	char cur_init[20];
	
	sprintf(disp_str, "Can't get GPS.");
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x24;
	cur_init[2] = 0x00;
	cur_init[3] = 0x00;
	cur_init[4] = 0x02;
	cur_init[5] = 0x00;
	
	pthread_mutex_lock(&lcd_mutex);
	write(fd_lcd, cur_init, 6);		/* ƒJ[ƒ\ƒ‹‚ð“KØ‚ÈˆÊ’u‚ÉˆÚ“® */
	write(fd_lcd, disp_str, strlen(disp_str));	/* î•ñ‚Ì‘‚«ž‚Ý */
	pthread_mutex_unlock(&lcd_mutex);
	
	return 0;
}

int display_time(char *utc ,char *cmd)
{
	char disp_str[128];
	char cur_init[20];
	
	sprintf(disp_str, "GPS TIME  %.8s [%.6s]     ", utc, cmd);
//	puts(disp_str);
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x24;
	cur_init[2] = 0x00;
	cur_init[3] = 0x00;
	cur_init[4] = 0x04;
	cur_init[5] = 0x00;
	
	pthread_mutex_lock(&lcd_mutex);
	write(fd_lcd, cur_init, 6);					/* ƒJ[ƒ\ƒ‹‚ð“KØ‚ÈˆÊ’u‚ÉˆÚ“® */
	write(fd_lcd, disp_str, strlen(disp_str));	/* î•ñ‚Ì‘‚«ž‚Ý */
	pthread_mutex_unlock(&lcd_mutex);
	
	return 0; 
}

int display_foma_status(char *str)
{
	char disp_str[128];
	char cur_init[20];
	
	sprintf(disp_str, "FOMA status: %-18.18s", str);
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x24;
	cur_init[2] = 0x00;
	cur_init[3] = 0x00;
	cur_init[4] = 0x06;
	cur_init[5] = 0x00;
	
	pthread_mutex_lock(&lcd_mutex);
	write(fd_lcd, cur_init, 6);		/* ƒJ[ƒ\ƒ‹‚ð“KØ‚ÈˆÊ’u‚ÉˆÚ“® */
	write(fd_lcd, disp_str, strlen(disp_str));	/* î•ñ‚Ì‘‚«ž‚Ý */
	pthread_mutex_unlock(&lcd_mutex);
	
	return 0;
}

int display_kddi_status(char *str)
{
	char disp_str[128];
	char cur_init[20];
	
	sprintf(disp_str, "%-18.18s", str);
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x24;
	cur_init[2] = 0x00;
	cur_init[3] = 0x00;
	cur_init[4] = 0x06;
	cur_init[5] = 0x00;
	
	pthread_mutex_lock(&lcd_mutex);
	write(fd_lcd, cur_init, 6);		/* ƒJ[ƒ\ƒ‹‚ð“KØ‚ÈˆÊ’u‚ÉˆÚ“® */
	write(fd_lcd, disp_str, strlen(disp_str));	/* î•ñ‚Ì‘‚«ž‚Ý */
	pthread_mutex_unlock(&lcd_mutex);
	
	return 0;
}

int display_connect_status(char *str)
{
	char disp_str[128];
	char cur_init[20];
	
	sprintf(disp_str, "%-32.32s", str);
	
	cur_init[0] = 0x1f;
	cur_init[1] = 0x24;
	cur_init[2] = 0x00;
	cur_init[3] = 0x00;
	cur_init[4] = 0x06;
	cur_init[5] = 0x00;
	
	pthread_mutex_lock(&lcd_mutex);
	write(fd_lcd, cur_init, 6);		/* ƒJ[ƒ\ƒ‹‚ð“KØ‚ÈˆÊ’u‚ÉˆÚ“® */
	write(fd_lcd, disp_str, 32);	/* î•ñ‚Ì‘‚«ž‚Ý */
	pthread_mutex_unlock(&lcd_mutex);
	
	return 0;
}

int at_close(int fd, struct termios *inittio)
{
	tcsetattr(fd, TCSANOW, inittio);
	close(fd);
	
	return 0;
}


int foma_at_send_command(int fd, char *command)
{
	int n = 0; //retry
	char buf[BUF_SIZE+8];

	while(n >= 3)
	{
		at_send_command(fd, command);
		at_get_command_result(fd, buf);
		if(buf != NULL){
			if(strstr(buf, "OK") != NULL){
				SB_msleep(500);
				return 0;
			}
		}
		
		
		n = n + 1;
	}
	
	return -1;
	
}

int at_send_command(int fd, char *command)
{
	char full_com[128];
	
	sprintf(full_com, "AT COMMAND SEND: AT%s", command);
	puts(full_com);
	
//	at_write(fd, "AT");
//	SB_msleep(20);
//	at_write(fd, command);
//	at_write(fd, "\r");
	
	sprintf(full_com, "AT%s\r", command);
	at_write(fd, full_com);
	
	SB_msleep(100);
	
	return 0;
}

int at_get_command_result(int fd, char *result)
{
	return at_read(fd, result);
}

int at_write(int fd, char *str)
{
	int res;
	res = write(fd, str, strlen(str));
	return res;
}

int at_read(int fd, char *str)
{
	int res = 0;			/* read()‚µ‚½ƒTƒCƒY */
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
	Timeout.tv_usec = SELECT_TIMEOUT_USEC;	/* ƒ}ƒCƒNƒ•b */
	Timeout.tv_sec = SELECT_TIMEOUT_SEC;	/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(fd, &readfs);
	
	while(1)
	{
        if(select(fd + 1, &readfs, NULL, NULL, &Timeout) <= 0)
        {
            return -1;    // 0 --> -1 2012/04/05 UP
        }
        res = read(fd, str, BUF_SIZE - 1);
        
        if (res > 1) { break; }           /* ‰üs‚Ì‚Ý‚Í“Ç‚Ý”ò‚Î‚·*/
	}
	
	str[res] = '\0';
	
	return res;
}

int get_dsr(int fd)
{
	int ctrl_bits;
	
	if(ioctl(fd, TIOCMGET, &ctrl_bits) < 0)
	{
		add_error("get_dsr");
		return -1;
	}
	
	//if(ctrl_bits & TIOCM_DSR)
	if(ctrl_bits & TIOCM_CTS)
	{
		return(1); /* ON */
	}
	else
	{
		return(0); /* OFF */
	}
}

int get_cts(int fd)
{
	int ctrl_bits;
	
	if(ioctl(fd, TIOCMGET, &ctrl_bits) < 0)
	{
		add_error("get_cts");
		return -1;
	}
	
	//if(ctrl_bits & TIOCM_CTS)
	if(ctrl_bits & TIOCM_SR)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}

int get_dcd(int fd)
{
	int ctrl_bits;
	
	if(ioctl(fd, TIOCMGET, &ctrl_bits) < 0)
	{
		add_error("get_dcd");
		return -1;
	}
	
	//if(ctrl_bits & TIOCM_CAR)
	if(ctrl_bits & TIOCM_RNG)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}

int get_dcd_now()
{	
	int rcdc ;
	
	rcdc = (get_dcd(fd_data));	
	return rcdc;
}

int set_dtr(int fd)
{
	int ctrl_bits = last_mctl_set[fd];
	ctrl_bits |= TIOCM_DTR;
//	ctrl_bits |= TIOCM_LE;
	
	if(ioctl(fd, TIOCMSET, &ctrl_bits) < 0)
	{
		add_error("set_dtr");
		return -1;
	}
	last_mctl_set[fd] = ctrl_bits;
	return 0;
}

int set_dtr_off(int fd)
{
	int ctrl_bits = last_mctl_set[fd];
	ctrl_bits &= ~TIOCM_DTR;
//	ctrl_bits &= ~TIOCM_LE;
	
	if(ioctl(fd, TIOCMSET, &ctrl_bits) < 0)
	{
		add_error("set_dtr_off");
		return -1;
	}
	last_mctl_set[fd] = ctrl_bits;
	return 0;
}

int get_xer_now()
{	
	int ctrl_bits;
	
	if(ioctl(fd_data, TIOCMGET, &ctrl_bits) < 0)
	{
		add_error("get_xer");
		return -1;
	}
	
	if(ctrl_bits & TIOCM_DTR)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}


int set_rts(int fd)
{
	int ctrl_bits = last_mctl_set[fd];
	ctrl_bits |= TIOCM_RTS;
//	ctrl_bits |= TIOCM_DTR;
	
	if(ioctl(fd, TIOCMSET, &ctrl_bits) < 0)
	{
		add_error("set_rts");
		return -1;
	}
	last_mctl_set[fd] = ctrl_bits;
	return 0;
}

int set_rts_off(int fd)
{
	int ctrl_bits = last_mctl_set[fd];
	ctrl_bits &= ~TIOCM_RTS;
//	ctrl_bits &= ~TIOCM_DTR;
	
	if(ioctl(fd, TIOCMSET, &ctrl_bits) < 0)
	{
		add_error("set_rts_off");
		return -1;
	}
	last_mctl_set[fd] = ctrl_bits;
	return 0;
}

int show_modembits()
{
	char lbuf[BUF_SIZE+8];
	int ctrl_bits;
	
	lbuf[0]=(char)NULL;
	if(ioctl(fd_data, TIOCMGET, &ctrl_bits) < 0)
	{
//		puts(strerror(errno));
		return -1;
	}

#define ONMSG(name) (name"_G,")
#define OFFMSG(name) (name"_O,")
#define CHECK_BIT_AND_MESSAGE(bit, n) \
	if(ctrl_bits&(bit)) \
		strcat(lbuf, ONMSG(n)); \
	else \
		strcat(lbuf, OFFMSG(n)); 

	CHECK_BIT_AND_MESSAGE(TIOCM_CTS, "SR");
	CHECK_BIT_AND_MESSAGE(TIOCM_SR, "CS");
	CHECK_BIT_AND_MESSAGE(TIOCM_RNG, "CD");

	CHECK_BIT_AND_MESSAGE(TIOCM_DTR, "ER");
	CHECK_BIT_AND_MESSAGE(TIOCM_RTS, "RS");

	puts(lbuf);
	display_connect_status(lbuf); 
	return 0;
	
#undef ONMSG
#undef OFFMSG
#undef CHECK_BIT_AND_MESSAGE
}


int clear_gps()
{
	return tcflush(fd_gps, TCIFLUSH);
}


int analyze_gps(char *str, char *res)
{
	char *command, *utc, *ido,*ido1, *ido2, *keido,*keido1, *keido2, *eisei;
	char dst[BUF_SIZE+8]; // up 2012/03/08  1024--> BUF_SIZE+1
	char buf[BUF_SIZE+8];
	char gpscmd[128];
	double distance = (double)(setting_info.stop_error_range + 1);
	int km;
	int in_station;
	static int pre_km = 0;
	static char pre_location[BUF_SIZE+8];
	static int delayed = 0;
	static int pre_in_station = 0;
	
	int sakaori; // ykane
	int sinanosakai;
	
	//set_send_span_from_pointF“Á’è‹æŠÔ“à‚Ìsend_span•ÏX
	//’·‚­‚È‚Á‚½ê‡‚ÍŠÖ”‰»‚àH
	//—]—T‚ð‚à‚Á‚Ä”ÍˆÍ‚ðÝ’è‚·‚éŽ–‚ª–]‚Ü‚µ‚¢
	sakaori = get_number_from_point(35.6596108, 138.5990774); // ykane 0
	sinanosakai = get_number_from_point(35.88473168, 138.2757842);  // ykane 5
	send_span = set_send_span_from_point(ido_d, keido_d, sakaori, sinanosakai);
		
	//set_send_span Ý’è—p@‚±‚±‚Ü‚Å
	
	//str[strlen(str)]='\0';
	memset(dst, 0, BUF_SIZE);
	memset(buf, 0, BUF_SIZE);
	memset(gpscmd, 0, 30);
	strcpy(dst, str);

	command = strtok(dst, ",");
	if(command == 0)
	{
		memcpy(gpscmd,"---------",6);
	}
	else
	{
		memcpy(gpscmd,command,6);
		//puts(gpscmd);
	}
	if(strcmp(gpscmd, "$GPGGA") == 0)
	{
    	//puts(str);
		memset(gps_last_get, 0, 30);

		utc = strtok(NULL, ".");
		if(utc == NULL || strlen(utc) != 6)
		{
			return -1;
		}
		
		utc_to_formatted_str(gps_last_get, utc);
		
		strtok(NULL, ",");
		ido = strtok(NULL, ",");
		if(ido == NULL || strlen(ido) == 0)
		{
			return -1;
		}
		strtok(NULL, ",");
		keido = strtok(NULL, ",");
		if(keido == NULL || strlen(keido) == 0)
		{
			return -1;
		}
		strtok(NULL, ",");
		strtok(NULL, ",");
		eisei = strtok(NULL, ",");
		if(eisei == NULL || strlen(eisei) == 0)
		{
			return -1;
		}
		
		ido1 = strtok(ido, ".");
		ido2 = strtok(NULL, "\0");
		if(ido1 == NULL || ido2 == NULL)
		{
			display_gps_fail();
			
			sprintf(res, "TR:%s:%s:00.000000,000,000000:%s000 %s\r\n", connect_info.id_num, utc, APP_Ver, lrange);
			display_time(gps_last_get,gpscmd);
			return 2;
		}
		
		keido1 = strtok(keido, ".");
		keido2 = strtok(NULL, "\0");
		if(keido1 == NULL || keido2 == NULL)
		{
			display_gps_fail(); // Can't get GPS
			
			sprintf(res, "TR:%s:%s:00.000000,000,000000:%s000 %s\r\n", connect_info.id_num, utc, APP_Ver, lrange);
			
			display_time(gps_last_get,gpscmd);
			return 2;	// ˆÜ“xŒo“x‚ª@0‚Ìê‡‚Ì–ß‚è’l
		}
		
		sprintf(ido_str, "%c%c.%c%c%s", ido1[0], ido1[1], ido1[2], ido1[3], ido2);
		sprintf(keido_str, "%c%c%c.%c%c%s", keido1[0], keido1[1], keido1[2], keido1[3], keido[4], keido2);
		
		ido_d = nmea0183_to_google(ido_str);
		keido_d = nmea0183_to_google(keido_str);
		
		if(last_send_ido != 0.0 && last_send_keido != 0.0)
		{
			distance = calc_distance(ido_d, keido_d, last_send_ido, last_send_keido);
		}
		
		sprintf(res, "TR:%s:%s:%s,%s:%s000 %s\r\n", connect_info.id_num, utc, ido_str, keido_str, APP_Ver, lrange);
		//discnum = NULL;
		// puts(res); // add 2012/03/07
		
		if(get_name_from_point(dst, ido_d, keido_d) < 0)
		{
			strcpy(dst, "          ");
			// puts(dst);
			in_station = 0;
		}
		else
		{
			in_station = 1;
			if(pre_in_station == 0)
			{
				sprintf(buf, "arrived at %s by %s", dst, gps_last_get);
				puts(buf);
			}
		}
		
		km = get_km_from_point(ido_d, keido_d);
		if(km != pre_km)
		{
			sprintf(buf, "km updated: %d(%lf,%lf) at %s", km, ido_d, keido_d, gps_last_get);
			puts(buf);
		}
		display_gps(ido_d, keido_d, km, dst);
		
		display_time(gps_last_get,gpscmd);
		
		if(in_station == 0 && pre_in_station == 1)
		{
			delayed = diagram_matching(pre_location, utc);
		}
		
		pre_km = km;
		strcpy(pre_location, dst);
		pre_in_station = in_station;
		
		if(setting_info.diagram_matching == 1 && delayed == 0)
		{
			return 1;  // ƒLƒ’ö‚ª‚Ç‚±‚©‚É“ü‚Á‚½‚ÌŽž‚Ì–ß‚è’l?
		}
		else if(setting_info.detect_train_stop == 1 && distance < (double)(setting_info.stop_error_range))
		{
			sprintf(buf, "stop dettected at %s", gps_last_get);
			puts(buf);
			return 1;
		}
		else
		{
			// puts(res);
			return 0;  // ’Êí‚Ì³í
		}
	}
	else
	{
		display_time(gps_last_get,gpscmd);
		return -1;
	}
}

void update_last_send_idokeido()
{
	last_send_ido = ido_d;
	last_send_keido = keido_d;
}

double calc_distance(double ido1, double keido1, double ido2, double keido2)
{
	double ido_distance;
	double keido_distance;
	double result;
	
	ido_distance = (ido1-ido2)*111263.283;
	keido_distance = (keido1-keido2)*(6378150.0*0.5877852523*2.0*3.14159265359/360.0);
	
	result = sqrt(ido_distance*ido_distance+keido_distance*keido_distance);
	
	return result;
}

int get_km_from_point(double ido, double keido)
{
	km_data_t result = km_data[0];
	double least = 1000000.0;
	double tmp_d;
	int i;
	
	for(i = 0; i < km_data_count; i++)
	{
		tmp_d = calc_distance(km_data[i].ido, km_data[i].keido, ido, keido);
		if(least > tmp_d)
		{
			result = km_data[i];
			least = tmp_d;
		}
	}
	
	return result.km;
}

int get_name_from_point(char *str, double ido, double keido)
{
	double tmp_d;
	int i;
	
	for(i = 0; i < station_data_count; i++)
	{
		tmp_d = calc_distance(station_data[i].ido, station_data[i].keido, ido, keido);
		if(tmp_d <= 100.0)
		{
			strcpy(str, station_data[i].name);
			return 0;
		}
	}
	
	return -1;
}

int get_number_from_point(double ido, double keido)
{
	double tmp_d;
	int i;
	
	for(i = 0; i < km_data_count; i++)
	{
		tmp_d = calc_distance(km_data[i].ido, km_data[i].keido, ido, keido);
		if(tmp_d <= 100.0)
		{
			
			return i;
		
		}
	}
	//puts("get_number_from_point");
	return -1;
}

int set_send_span_from_point(double ido, double keido, int start, int end)
{
	double tmp_d;
	int default_time = 3600; //ƒfƒtƒHƒ‹ƒg‚ÌŽžŠÔ@1ŽžŠÔ3600•b
	int change_time = 60; //•Ï‰»ŽžŠÔ 1•ª60•b
	int i;
	
	if(start < 0 || end < 0)
	{
		//display_status("error:get_number");
		return default_time;
		//return get_setting_info(1); 
		//ƒLƒ’öŠO‚ÌÀ•W‚ÉŠÖ‚·‚éÝ’èFŽÔÚ‹@“à‚ÌÝ’è‚Ó‚Ÿ‚¢‚é‚É]‚¤
	}
	
	for(i = 0; i < km_data_count; i++)
	{
		tmp_d = calc_distance(km_data[i].ido, km_data[i].keido, ido, keido);
		if(tmp_d <= 120.0 && start <= i && i <= end)
		{
			display_status("Span changed");
			//puts("span_time_change");
			return change_time;
		}
	}
	//display_status("Shitei_hani_gai");
	//puts("default");	
	return default_time; // 1ŽžŠÔ
}

int diagram_matching(char *str, char *depart_time)
{
	time_t depart, dia_depart;
	struct tm work_tm;
	diagram_data_t time_match_data, match_data;
	int matched = 0;
	int h, m, s;
	int i;
	int dia_diff;
	char tmp[8];
	char buf[512];
	
	strcpy(match_data.dia_name, "");
	strcpy(time_match_data.dia_name, "");
	
	/* Šî–{“I‚ÉA‰½‚©ƒf[ƒ^‚ª‘«‚è‚È‚¢ê‡‚Í’x‰„—L‚è‚Æ”»’f‚·‚éB */
	if(strlen(str) < 6) return 1;
	
	tmp[0] = str[0];
	tmp[1] = str[1];
	tmp[2] = '\0';
	
	h = atoi(tmp);
	h += 9;
	h %= 24;
	
	tmp[0] = str[2];
	tmp[1] = str[3];
	tmp[2] = '\0';
	
	m = atoi(tmp);
	
	tmp[0] = str[4];
	tmp[1] = str[5];
	tmp[2] = '\0';
	
	s = atoi(tmp);
	
	work_tm.tm_year = 1990 - 1900;
	work_tm.tm_mon = 1 - 1;
	work_tm.tm_mday = 1;
	work_tm.tm_hour = h;
	work_tm.tm_min = m;
	work_tm.tm_sec = s;
	work_tm.tm_isdst = 0;
	
	depart = mktime(&work_tm);
	if(depart == -1)
	{
		return 1;
	}
	
	for(i = 0; i < diagram_data_count; i++)
	{
		if(strcmp(str, diagram_data[i].name) != 0) continue;
		
		h = diagram_data[i].departure / 10000;
		m = (diagram_data[i].departure - h*10000) / 100;
		s = diagram_data[i].departure - h*10000 - m*100;
		
		work_tm.tm_year = 1990 - 1900;
		work_tm.tm_mon = 1 - 1;
		work_tm.tm_mday = 1;
		work_tm.tm_hour = h;
		work_tm.tm_min = m;
		work_tm.tm_sec = s;
		work_tm.tm_isdst = 0;
		
		dia_depart = mktime(&work_tm);
		if(depart == -1) continue;
		
		if(difftime(dia_depart, depart) < 1.0 && difftime(depart, dia_depart) < 1800.0)
		{
			time_match_data = diagram_data[i];
			if(strcmp(current_dia_name, diagram_data[i].dia_name) == 0)
			{
				match_data = time_match_data;
			}
			matched = 1;
			break;
		}
	}
	
	if(matched == 0)
	{
		return 1;
	}
	else if(strlen(match_data.dia_name) == 0)
	{
		match_data = time_match_data;
		strcpy(current_dia_name, match_data.dia_name);
	}
	
	dia_diff = difftime(depart, match_data.departure);
	sprintf(buf, "diagram diff is %d seconds.(%s) %s", dia_diff, current_dia_name, gps_last_get);
	
	if(dia_diff >= setting_info.diagram_match_span)
	{
		return 1;
	}
	
	return 0;
}

int read_gps(char *str, int length)
{
	int res;
	char buf[BUF_SIZE+2];
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */
	static int stop_count = 0;
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
    Timeout.tv_usec = GPS_TIMEOUT_USEC;	/* ƒ}ƒCƒNƒ•b */
    Timeout.tv_sec = GPS_TIMEOUT_SEC;	/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(fd_gps, &readfs);
	
	/* ƒƒ‚ƒŠ‘|œ */
	memset(str, 0 , length);
	
	while(1)
	{
        if(select(fd_gps + 1, &readfs, NULL, NULL, &Timeout) <= 0)
        {
			
			sprintf(buf, "read gps failed.[%d]", stop_count); //add 2013/03/08
			puts(buf);
			// puts("read gps failed.");
        	
        	/* ˜A‘±Žæ“¾Ž¸”s‰ñ”‚ª3‰ñ‚ð’´‚¦‚½ê‡‚ÍŽÔÚŠí‚ðÄ‹N“®‚·‚é */
        	if(stop_count > 3)
        	{
				display_status("Auto reboot(GPS read error).");
				puts("Auto reboot(GPS read error).");
				SB_msleep(1000);
        		system("reboot");  // add 2012/03/08
        	
        	}
			stop_count ++;  // add 2012/03/08    	
        	clear_gps();
        	
            return -1;
        }
        
        res = read(fd_gps, str, length - 1);
        
        if (res > 0)
        {
			str[length] = '\0';
			stop_count = 0;
			return 0;
        }
	}
}

int jrc_send_gps(char *str)
{
	return write(fd_jrc_gps, str, strlen(str));
}

int jrc_send_data(char *str, int len)
{
	return write(fd_jrc_send, str, len);
}

int jrc_receive_data(char *str, int len)
{
	int res;
	int wi;
	char logstr[BUF_SIZE+8];
	char logstr2[BUF_SIZE+8];
	char wkstr[BUF_SIZE+8];
	struct timeval Timeout;	/* select() ‚Ì ƒ^ƒCƒ€ƒAƒEƒg—p */
	fd_set readfs;			/* select() ‚Ìˆø”—p */
	
	/* select()‚Ìƒ^ƒCƒ€ƒAƒEƒg’l‚ðÝ’è */
	Timeout.tv_usec = 20;	/* ƒ}ƒCƒNƒ•b*/
	Timeout.tv_sec = 0;		/* •b */
    
	/* select()‚ÉŽg—p */
	FD_ZERO(&readfs);
	FD_SET(fd_jrc_send, &readfs);
	
	if(select(fd_jrc_send + 1, &readfs, NULL, NULL, &Timeout) <= 0)
	{	// 	ƒ^ƒCƒ€ƒAƒEƒg==0
		return -1;
	}
	SB_msleep(100);  // ‘Sƒf[ƒ^ŽóM‘Ò‚¿		
	res = read(fd_jrc_send, str, len-1);


// ŽŽŒ±ƒf[ƒ^‘—M
//	memcpy(str,"\x10\x02\xA5\x02\x05\x14\x01\x00\xFF\xFF\xFF\xFF\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x30\x31\x30\x32\x65\x00\x00\x010\x10\x00\x00\x00\x00\x10\x03\xA9\xB3\xFF\xFF\xFF\xFF\xFF",37);
//	res = 37;
//	SB_msleep(20000);  // 
//	ŽŽŒ±ƒf[ƒ^‚±‚±‚Ü‚Å
//
	strcpy(logstr, "[");
	strcpy(logstr2, "NN:");
	if(res > 0)
	{
		for(wi = 0; wi < res; wi++)
		{
			sprintf(logstr, "%s%02X ",logstr, *(str+wi)); 
			sprintf(logstr2, "%s%02X",logstr2, *(str+wi)); 
		}
		sprintf(wkstr, "---> NM_Send:%d %s]", res, logstr);
		puts(wkstr);	

		sprintf(wkstr, "Send %.24s",logstr2);
		display_status(wkstr);  //

	}
	return res;
}

int remove_crlf(char *target)
{
	char *p;
	
	p = strchr(target, '\r');
	if(p != NULL)
	{
		*p = '\0';
	}
	p = strchr(target, '\n');
	if(p != NULL)
	{
		*p = '\0';
	}
	
	return 0;
}

int remove_blank(char *target)
{
	char *p;
	
	remove_crlf(target);
	
	p = strchr(target, ' ');
	if(p != NULL)
	{
		*p = '\0';
	}
	p = strchr(target, '\t');
	if(p != NULL)
	{
		*p = '\0';
	}
	
	return 0;
}

int get_filesize(char *path)
{
	FILE *fp;
	int fsize = 0;
	
	fp = fopen(path, "rb"); 
	if(fp == NULL)
	{
		return -1;
	}
	
	/* ƒtƒ@ƒCƒ‹ƒTƒCƒY‚ð’²¸ */ 
	fseek(fp, 0, SEEK_END); 
	fgetpos(fp, (fpos_t *)&fsize); 
	
	fclose(fp);
	
	return fsize;
}

int utc_to_formatted_str(char *str, char *utc)
{
	char tmp[8];
	int h, m, s;
	
	if(strlen(utc) < 6) return -1;
	
	tmp[0] = utc[0];
	tmp[1] = utc[1];
	tmp[2] = '\0';
	
	h = atoi(tmp);
	h += 9;
	h %= 24;
	
	tmp[0] = utc[2];
	tmp[1] = utc[3];
	tmp[2] = '\0';
	
	m = atoi(tmp);
	
	tmp[0] = utc[4];
	tmp[1] = utc[5];
	tmp[2] = '\0';
	
	s = atoi(tmp);
	
	sprintf(str, "%02d:%02d:%02d", h, m, s);
	
	return 0;
}

double nmea0183_to_google(char *str)
{
	double degree, minute, second;
	double temp, result;
	
	temp = atof(str);
	degree = floor(temp);
	temp -= degree;
	temp *= 100.0;
	minute = floor(temp);
	temp -= minute;
	second = temp * 60.0;
	
	result = degree + (minute / 60.0) + (second / 3600.0);
	
	return result;
}
