
#include <sys/io.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#pragma pack(1)

#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01 

// error consts
#define ATA_ER_BBK      0x80
#define ATA_ER_UNC      0x40
#define ATA_ER_MC       0x20
#define ATA_ER_IDNF     0x10
#define ATA_ER_MCR      0x08
#define ATA_ER_ABRT     0x04
#define ATA_ER_TK0NF    0x02
#define ATA_ER_AMNF     0x01

struct ide_id {
        short general_conf;
        short ob[9];
        char sn[20];
        short ob1[3];
        char fwver[8];
        char model[40];
        short f1;
        short res1;
        short cap[2];
        short ob2[9];
        int lba28sec;
        short ob3;
        short mwdmamodes;
        short piomodes;
        short ob4[15];
        short verma;
        short vermi;
        short features[6];
        short dmamodes;
        short secerasetime;
        short ensecerasetime;
        short apm;
        short mpr;
        short hwrs;
        short ob5[6]; // +1
        int lba48sec;
        short ob6[13];
        short wps;
        short ob7[8];
        short rms;
        short secstat;
        char vendorspec[62];
        short cpapower;
        char reserved_cf[30];
        char media_sn[60];
        char res[98];
        short in;
};


struct zone_header {
	short u1;
	short zones;
	char u2[0x10];
} ;

struct zone_rec {
	int start_cyl;
	int end_cyl;
	int start_sec;
	char pad[2];
	int end_sec;
	char pad2[2];
} ;

unsigned char regs;
unsigned int busy, drdy, df, dsc, drq, corr, idx, err, ready, drsc;
unsigned int bbk,unc,mc,idnf,mcr,abrt,tk0nf,amnf;
unsigned short b0, b1,b2,b3,b4,b5,b6,b7;
int p0,p1,p2,p3,p4,p5,p6,p7;
int sa_spt, sa_tracks, num_of_heads, verbose;
int in_head_num=0, in_spt=0, in_tracks=0, in_read_head=0;
int head_num=0,spt_num=0, tracks_num=0, read_head_num=0;

void read_regs();
void read_error();
void status(int);

int waitready(int secs);
int waitnobusy(int secs);
int waitdrq(int secs);
int waitnodrq(int secs);

int vsc_mode_on();
int vsc_mode_off();

void write_buffer (char *sector);
void read_buffer (char *sector);

int wd_dump_cp5 (char *buff);

int wd_rw_buffer_cmd(unsigned char length);
int wd_send_full_cmd(short command, short p1, short p2, short p3, short p4, short p5, short p6);
int wd_read_pchs(char *buffer, short head, int track, short sector, short scount);
int wd_write_pchs(char *buffer, short head, int track, short sector, short scount);
int wd_getcp(short cp, char **out_buff, int *len);
int wd_send_cmd(short command, short para1 , short para2);
int wd_id (struct ide_id *d);
int write_file_to_reserved(char *filename);
int read_file_from_reserved(char *filename);
void flip (char *buf,int len);
int get_sa_tracks(char *buf, int size);
int get_sa_spt(char *buf);

int main(int argc, char **argv)
{

	int c=0,portset=0;
	int read_file=0, write_file=0,dump_zone_file=0;
	int baseport=0;
	char *bp=NULL;
	char *filename=NULL;

	while ( (c = getopt(argc, argv, "hvzp:r:w:s:d:t:k:")) != -1 )

	{

		switch (c)
		{
			case	'p':
				bp=optarg;
				portset=1;
				break;
			case	'r':
				filename=optarg;
				read_file=1;
				break;
			case	'w':
				filename=optarg;
				write_file=1;
				break;
			case	'z':
				dump_zone_file=1;
				break;
			case	'v':
				verbose=1;
				break;
			case	's':
				spt_num=atoi(optarg);
				in_spt=1;
				break;
			case	'd':
				head_num=atoi(optarg);
				in_head_num=1;
				break;
			case	't':
				tracks_num=atoi(optarg);
				in_tracks=1;
				break;
			case	'k':
				read_head_num=atoi(optarg);
				in_read_head=1;
				break;	
			case 	'h':
				printf("usage: %s -p port [-r file] [-w filename] [-z] [-v]\n",argv[0]);
				printf("-r file		: read entire reserved area to file\n");
				printf("-w file		: write file onto reserved area\n");
				printf("-v 		: verbose output\n");
				printf("-z 		: dump zone file\n");
				printf("-h		: print this help\n");
				exit(1);
				break;
		}
		

	}


	if (portset!=1)
	{
		printf("usage: %s -p port\ne.g. %s -p 0x0170\n",argv[0],argv[0]);
		return(1);
	}

        if (sscanf(bp, "0x%4x", &baseport))
        {
                printf("using port address: 0x%04X\n\n", baseport);
        } else {
        	if (sscanf(bp, "%4x", &baseport))
                	printf("using port address: 0x%04X\n", baseport);
		else
		{
			printf("unable to parse port argument (%s), aborting\n\n",bp);
			return(1);
		}
        }



	// check for permissions
	if (0!=iopl(3))
	{
		printf("can't change io privilege level, aborting\n");
		return(1);
	}

	p0=baseport;	

	p1=p0+1;
	p2=p1+1;
	p3=p2+1;
	p4=p3+1;
	p5=p4+1;
	p6=p5+1;
	p7=p6+1;


	// also check if the drive model is what this POC is for... 
	//char *poc_for_model="WDC WD2500KS-00MJB0";
	char *poc_for_model="WDC WD2500JS-00MHB0";

	// disable interrupts?
	
	struct ide_id d;
	wd_id(&d);

	if (verbose)
		status(3);

	char model[64];
	memset(model,0,64);
	memcpy(model,d.model,40);
	flip(model,40);
	printf("Model: %s\n",model);

	char temp[64]={0};
	memcpy(temp,d.sn,20);
	flip(temp,20);
	printf("S/N: %s\n",temp);

	memset(temp,0,64);
	memcpy(temp,d.fwver,8);
	flip(temp,8);
	printf("F/W Ver:\t\t%s\n",temp);
	printf ("LBA24:%d\tLBA48:%d\n",d.lba28sec, d.lba48sec);

	// checking for a specific 250GB hawk, but any other 250GB hawk would work just fine.
	/*	
	if (0!=strncmp (poc_for_model, model, strlen(poc_for_model)))
	{
		printf("POC works only on %s, the drive on port (%04X) is (%s), aborting\n",poc_for_model,p0,model);
		exit(1);
	}
	*/

	char *out_buff;
	int len;
	// read configuration page 0x05 (zone list)
	if (0==wd_getcp(0x05, &out_buff,&len))
	{
		if(in_spt==1)
		{
		    sa_spt= spt_num;
		    printf("sa_spt=%d\n",sa_spt);
		}
		else
		    sa_spt=get_sa_spt(out_buff);

		if(in_tracks==1)
	    	    sa_tracks=tracks_num;
		else
		    sa_tracks=get_sa_tracks(out_buff,len*512);
		
		if(in_head_num==1)
		{
		    num_of_heads=head_num;
		    printf("num_of_heads=%d\n",num_of_heads);
		}
		else
		    num_of_heads=6;	// constant for now.

		printf("\n\nService area sectors-per-track (%d)\nService area tracks (%d)\nNum of heads(%d)\nUnused reversed space (%d sectors [%d bytes])\n",sa_spt,sa_tracks,num_of_heads, sa_tracks*sa_spt*(num_of_heads - 2), sa_tracks*sa_spt*(num_of_heads - 2) * 512);
	}


	if (dump_zone_file)
	{
		wd_dump_cp5(out_buff);
	}

	if (read_file)
	{
		printf("reading from SA to (%s) returned (%d)",filename,read_file_from_reserved(filename));
	} else if (write_file) {
		printf("writing to SA (%s) returned (%d)",filename,write_file_to_reserved(filename));
	}

	return(vsc_mode_off());
}

void write_buffer (char *sector)
{
	outsw(p0, sector, 256);
}

void read_buffer (char *sector)
{
	insw(p0, sector, 256);
}

void status(int times)
{
	int i=0;
	printf("byte\tBSY\tDRDY\tDF\tDSC\tDRQ\tCORR\tIDX\tERR\tBBK\tUNC\tMC\tIDNF\tMCR\tABRT\tTK0NF\tAMNF\n");

	for (i=0;i<times;i++)
	{
		read_regs();
		read_error();
		printf("(%x)\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",regs,busy,drdy,df,dsc,drq,corr,idx,err,bbk,unc,mc,idnf,mcr,abrt,tk0nf,amnf);
		usleep(200000);
	}
}

void read_regs()
{

	unsigned char s ;
	s = inb(p7);

	regs=s;
	
	busy = ATA_SR_BSY == (s & ATA_SR_BSY);
	drdy = ATA_SR_DRDY == (s & ATA_SR_DRDY);		//drive ready
	df = ATA_SR_DF == (s & ATA_SR_DF);
	dsc = ATA_SR_DSC == (s & ATA_SR_DSC);			// drive seek complete
	drq = ATA_SR_DRQ == (s & ATA_SR_DRQ);
	corr = ATA_SR_CORR == (s & ATA_SR_CORR);
	idx = ATA_SR_IDX == (s & ATA_SR_IDX);
	err = ATA_SR_ERR == (s & ATA_SR_ERR);

	ready = drsc = drdy & dsc;
	
}	

void read_error()
{
	unsigned char e;
	e = inb(p1);
	bbk = ATA_ER_BBK == (e & ATA_ER_BBK);
	unc = ATA_ER_UNC == (e & ATA_ER_UNC);
	mc = ATA_ER_MC == (e & ATA_ER_MC);
	idnf = ATA_ER_IDNF == (e & ATA_ER_IDNF);
	mcr = ATA_ER_MCR == (e & ATA_ER_MCR);
	abrt = ATA_ER_ABRT == (e & ATA_ER_ABRT);
	tk0nf = ATA_ER_TK0NF == (e & ATA_ER_TK0NF);
	amnf = ATA_ER_AMNF == (e & ATA_ER_AMNF);

}	

int wd_id (struct ide_id *d)
{
	if (verbose)
		printf ("wd_id()\n");

	// send 0xa0 to p6	--select master
	outb(0xa0, p6);

	// send 0xec to p7	--send identify cmd
	outb(0xec, p7);

	if (waitdrq(10))
	{
		printf("not ready to read, aborting\n");
	}

	// read buffer
	char buff[512];
	read_buffer (buff);

	memcpy(d,buff,sizeof(struct ide_id));

	return(0);	
}

int wd_read_pchs ( char *buffer, short head, int track, short sector, short scount)
{

	int sectorsread;
	long int sectorslow;
	long int sectorshigh;
	char a[512];
	int len;
	int rc;

	// send 0xa0 to p6	--select master
	outb(0xa0, p6);

	if (verbose)
		printf("wd_read_pchs(head=%d)(track=%d)(sector=%d)(scount=%d)\n",head,track,sector,scount);

	if (waitready(3))
	{
		printf("wd_read_pchs: drive not ready, aborting\n");
		status(3);
		return(1);
	}
	
	if (vsc_mode_on())
	{	
		printf("failed to get into vsc mode, aborting\n");
		return (1);
	}

	rc=wd_send_full_cmd(0x000c, 0x0001, (track & 0xffff), (track >> 16) & 0xffff , head, sector, scount);
	if (rc) 
	{
		printf("wd_read_pchs: wd_send_full_cmd failed, aborting\n");
		return(1);
	}

	if (waitnodrq(3)) {
		printf("DRQ=1, ERROR\n");
		return(1);
	} else {
		if (verbose)
		{
			printf("DRQ=0, OK\n");
			status(4);
		}
	}

	sectorslow = inb(p4);
	sectorshigh = inb(p5);
	len = sectorslow+(sectorshigh<<8);

	if (len<1)
	{ 
		printf("len<1, aborting\n");
		return(1);
	}

	sectorsread=0;

	while (len>sectorsread)
	{
		rc=wd_rw_buffer_cmd(1);
	   if (rc) 
		{
			printf("wd_rw_buffer_cmd, failed\n");
			return(1);
		}

		read_regs();
		while (drq) 
		{
			memset(a,0,512);
			read_buffer(a);

			memcpy(buffer+(sectorsread*512), a, 512);

			sectorsread++;

			rc=waitready(60);
			if (rc) return(1);

			read_regs();
			if(err) 
			{
				printf("Error:\n");
				status(1);
				return(1);	
			}
		}
	}

	if (sectorsread!=len) 
	{
		printf("didn't complete read operation\n");
		return(1);
	}

	return(0);
}

int vsc_mode_on()
{

	int rc;

	// send 0xa0 to p6	--select master
	outb(0xa0, p6);

	if (verbose)
		printf("vsc_mode_on_start:\n");

	rc = waitnobusy(3);
	if (rc)
	{
		printf("vsc_mode_on: busy, exiting\n");
		status(3);
		return(1);
	}

	outb(0x45,p1);
	outb(0x00,p2);
	outb(0x00,p3);
	outb(0x44,p4);
	outb(0x57,p5);
	outb(0xA0,p6);
	outb(0x80,p7);

	rc=waitnobusy(10);
	if (rc) 
	{
		printf("wait-no-busy, timed out, aborting\n");
		return(1);
	}

	if(waitnodrq(3)) 
	{
		printf("drq still 1, aborting\n");
		status(3);
		return(1);
	}
	
	if (verbose)
		printf ("vsc_mode_on: OK\n");

	return(0);
	
}

int vsc_mode_off()
{

	int rc;

	// send 0xa0 to p6	--select master
	outb(0xa0, p6);

	if (verbose)
		printf("vsc_mode_on_start:\n");

	rc = waitnobusy(3);
	if (rc)
	{
		printf("vsc_mode_on: busy, exiting\n");
		status(3);
		return(1);
	}

	outb(0x44,p1);
	outb(0x00,p2);
	outb(0x00,p3);
	outb(0x44,p4);
	outb(0x57,p5);
	outb(0xA0,p6);
	outb(0x80,p7);

	rc=waitnobusy(10);
	if (rc) 
	{
		printf("wait-no-busy, timed out, aborting\n");
		return(1);
	}

	read_regs();
	if(drq) 
	{
		printf("drq still 1, aborting\n");
		status(3);
		return(1);
	}
	return(0);
}

int wd_rw_buffer_cmd(unsigned char length)	
{

	int rc;

	// send 0xa0 to p6	--select master
	outb(0xa0, p6);

	rc=waitnobusy(3);
	if (rc)	
	{	
		printf ("drive busy, aborting\n");
		status(3);
		return 1;
	}

	outb(0xD5,p1);
	outb(length,p2);
	outb(0xbf,p3);
	outb(0x4f,p4);
	outb(0xc2,p5);
	outb(0xa0,p6);
	outb(0xb0,p7);

	rc=waitnobusy(60);
	if (rc) {
		printf ("io-wait-on-busy timeout,aborting\n");
		status(2);
		return(1);
	}

	if (0!=waitdrq(3)) {
		printf ("drq not on, aborting\n");
		status(2);
		return(1);
	}

	if (verbose)
		printf("wd_rw_buffer_cmd: OK\n");

	return(0);
}


int wd_send_full_cmd(short command, short par1, short par2, short par3, short par4, short par5, short par6) 
{
	char a[512];
	int rc;

	memset(a,0, 512);

	a[1]=(command>>8) & 0xff ;
	a[0]=(command & 0x00ff);

	a[3]=(par1>>8) & 0xff;
	a[2]=(par1 & 0x00ff);

	a[5]=(par2>>8) & 0xff;
	a[4]=(par2 & 0x00ff);

	a[7]=(par3>>8) & 0xff;
	a[6]=(par3 & 0x00ff);

	a[9]=(par4>>8) & 0xff;
	a[8]=(par4 & 0x00ff);

	a[11]=(par5>>8) & 0xff;
	a[10]=(par5 & 0x00ff);

	a[13]=(par6>>8) & 0xff;
	a[12]=(par6 & 0x00ff);


	outb(0xa0,p6);
	read_regs();

	if (busy) 
	{
		printf("wd_send_full_cmd: busy, exiting\n");
		return 1;
	}

	outb(0xd6,p1);	
	outb(0x01,p2);
	outb(0xbe,p3);
	outb(0x4f,p4);
	outb(0xc2,p5);
	outb(0xa0,p6);
	outb(0xb0,p7);

	rc=waitnobusy(60);
	if(rc) 
	{

		printf("wd_send_full_cmd: waitnobusy (1), exiting\n");
		return(1);
	}

	read_regs();
	if(!drq) {
		printf ("wd_send_full_cmd: drq(%d), exiting\n",drq);
		return(1);
	}

	write_buffer(a);

	rc=waitnobusy(5);
	if (rc) {
		printf("wd_send_full_cmd: waitnobusy=%d, exiting\n,",rc);
		return(1);
	}

	read_regs();
	if (err) 
	{
			
		printf("wd_send_full_cmd: error register is on..\n");
		status(2);
		
		return(1);
	}

	return(0);
}

int waitdrq(int secs)
{

	int wait_time=500;
	int retries=secs*1000000/(wait_time);

	read_regs();
	while ((!drq) && (retries>=0))
	{
		usleep (wait_time);
		retries--;
		read_regs();
	}	

	if (retries>0)
		return (0);
	else
		return(1);
	
}

int waitnodrq(int secs)
{

	int wait_time=500;
	int retries=secs*1000000/(wait_time);

	read_regs();
	while ((drq) && (retries>=0))
	{
		usleep (wait_time);
		retries--;
		read_regs();
	}	

	if (retries>0)
		return (0);
	else
		return(1);
	
}

int waitready(int secs)
{
	
	int wait_time=500;
	int retries=secs*1000000/(wait_time);

	read_regs();
	while ( (!ready) && (retries>=0))
	{
		usleep (wait_time);
		retries--;
		read_regs();
	}	
	
	if (retries>0)
		return (0);
	else
		return(1);
	
}

int waitnobusy(int secs)
{

	int wait_time=500;
	int retries=secs*1000000/(wait_time);

	read_regs();
	while ((busy) && (retries>=0))
	{
		usleep (wait_time);
		retries--;
		read_regs();
	}	
	
	if ((retries==0) && busy)
		return (1);
	else
		return(0);
	
}


int wd_send_cmd(short command, short par1 , short par2)
{

	char a[512];
	int rc;

	if (verbose)
		printf("debug: (command=%04X)(p1=%04X)(p2=%04X)\n",command,par1,par2);

	memset(a,0, 512);

	a[1]=(command>>8) & 0xff ;
	a[0]=(command & 0x00ff);

	a[3]=(par1>>8) & 0xff;
	a[2]=(par1 & 0x00ff);

	a[5]=(par2>>8) & 0xff;
	a[4]=(par2 & 0x00ff);

	// select master
	outb(0xa0,p6);

	rc = waitnobusy(3);
	if (rc) 
	{
		printf("wd_send_full_cmd: drive busy, exiting\n");
		return 1;
	}

	outb(0xd6,p1);
	outb(0x01,p2);
	outb(0xbe,p3);
	outb(0x4f,p4);
	outb(0xc2,p5);
	outb(0xa0,p6);
	outb(0xb0,p7);

	rc=waitnobusy(3);
	if(rc) 
	{

		printf("wd_send_full_cmd: waitnobusy (1), exiting\n");
		return(1);
	}

	read_regs();
	if(!drq) {
		printf ("wd_send_full_cmd: drq(%d), exiting\n",drq);
		return(1);
	}

	write_buffer(a);

	rc=waitnobusy(10);
	if (rc) {
		printf("wd_send_full_cmd: waitnobusy=%d, exiting\n,",rc);
		return(1);
	}

	read_regs();
	if (err) 
	{
			
		printf("wd_send_full_cmd: error register is on..\n");
		status(2);
		
		return(1);
	}

	return(0);
}


int wd_getcp(short cp, char **out_buff, int *len)
{

	char buff[512];
	char *new_buff;
	unsigned char i;
	int rc = 0;

	// choose primary port
	outb(0xa0,p6);

	read_regs();
	if (busy)
	{
		printf("wd_getcp: drive busy, leaving");
		return(1);	
	}
	
	if (vsc_mode_on())
	{
		printf("wd_getcp: vsc_mode_on failed, aborting\n");
		return(1);
	}

	rc = wd_send_cmd(0x000d, cp, 0x0000);
	if (rc)
	{
		printf("wd_getcp: wd_send_cmd failed, aborting\n");
		status(2);
		return(1);
	}

	read_regs();
	if (drq)
	{
		printf("wd_getcp: drq on, error, aborting\n");
		status(2);
		return(1);
	}	


	// read len 
	(*len) = inb(p4);

	new_buff=malloc((*len)*512);

	if (verbose)
		printf("wd_getcp: CP (%x) len is (%x)\n", cp, *len);

	rc = wd_rw_buffer_cmd((*len));
	if (rc)
	{
		printf("wd_getcp: wd_rw_buffer_cmd failed, aborting\n");
		return(1);
	}

	for (i=0;i<(*len);i++)
	{
		if (verbose)
			printf("reading part (%d) of (%d)\n", i,(*len));

		if (0==waitdrq(3))
		{
			read_buffer(buff);
			memcpy(new_buff+(i*512), buff,512);
			waitnobusy(3);
		}
	}

#ifdef DEBUG
	FILE *f;
	char fn[64];
	sprintf(fn, "cp_%x.bin",cp);
	f = fopen(fn, "w");
	if (f) {
		fwrite(new_buff, 512*(*len),1, f);
	   fclose(f);
	} else {
		printf ("can't open file for writing... exiting\n");
		return(1);
	}
#endif
	(*out_buff)=new_buff;
	return(0);
}

int get_sa_spt(char *buff)
{

	int pos=0;
	short spt;

	pos+=sizeof(struct zone_header);
	pos+=sizeof(struct zone_rec);
	memcpy(&spt,buff+pos, 2);
	return(spt);
}


int get_sa_tracks(char *buff, int size)
{
	struct zone_rec rec1;

	if ( (sizeof(struct zone_header) + sizeof(struct zone_rec)) < size )
	{
		memcpy(&rec1, buff + sizeof(struct zone_header), sizeof(struct zone_rec));
		return(abs(rec1.start_cyl));
	} else {
		return(0);
	}
}



int wd_dump_cp5 (char *buff)
{

	int i=0;
	int h=0;
	int num_of_heads=6;

	struct zone_rec rec1;
	struct zone_header z1;

	memcpy(&z1, buff, sizeof(struct zone_header));

	int pos=0;
	short spt;
	pos+=sizeof(struct zone_header);	
	for (i=0;i<z1.zones;i++)	
	{
		memcpy(&rec1, buff+pos, sizeof(struct zone_rec));
		printf("cyl(%d,%d) sector(%d,%d)",rec1.start_cyl,rec1.end_cyl,rec1.start_sec,rec1.end_sec);


		pos+=sizeof(struct zone_rec);

		for(h=0;h<num_of_heads;h++)
		{
			memcpy(&spt,buff+pos, 2);
			pos+=2;
			printf (" (h=%d)(spt=%d)",h,spt);
		}
		printf("\n");
	}

	return(0);
}

int wd_write_pchs ( char *buffer, short head, int track, short sector, short scount)
{
	int sectorswritten;
	long int sectorslow;
	long int sectorshigh;
	char a[512];
	int len;
	int rc;

	if (verbose)
		printf("wd_write_pchs(head=%d)(track=%d)(sector=%d)(scount=%d)\n",head,track,sector,scount);

	outb(0xa0, p6);

	if (waitready(3))
	{
		printf("wd_write_pchs, drive not ready, aborting\n");
		status(3);
		return(1);
	}
	
	if (vsc_mode_on())
	{	
		printf("failed to get into vsc mode, aborting\n");
		return (1);
	} else {
		if (verbose)
			printf("vsc_mode_on: OK\n");
	}


	if (verbose)
		printf("debug: track(%04X)(%02X)(%02X)\n",track, (track & 0xffff), (track >> 16) & 0xffff);

	rc=wd_send_full_cmd(0x000c, 0x0002, (track & 0xffff), (track >> 16) & 0xffff , head, sector, scount);
	if (rc!=0) 
	{
		printf("wd_send_full_cmd failed, aborting\n");
		return(1);
	} else {
		if (verbose)
			printf("wd_send_full_cmd: OK\n");
	}

	read_regs();
	if (drq) {
		printf("DRQ=1, ERROR\n");
		return(1);
	} else {
		if (verbose)
		{
			printf("DRQ=0, OK\n");
			status(2);
		}
	}

	sectorslow = inb(p4);
	sectorshigh = inb(p5);
	len = sectorslow+(sectorshigh<<8);

	if (len<1) return(1);

	sectorswritten=0;

	while (len>sectorswritten)
	{
	    	if (wd_rw_buffer_cmd(1))
		{
			printf("wd_rw_buffer_cmd, failed\n");
			return(1);
		}

		read_regs();
		while (drq) 
		{
			if (memcpy(a,buffer+(sectorswritten*512),512))
			{
				write_buffer(a);
			} else {
				printf("error reading input buffer\n");
				return(1);
			}
			sectorswritten++;

			rc=waitready(10);
			if (rc < 0) 
			{	
				printf("not ready aborting\n");
				return(1);
			}
			read_regs();
			if(err) 
			{
				printf("write completed with errors, aborting\n");
				return(1);
			}
		}
	}

	if (sectorswritten!=len) 
	{	
		printf("didn't complete writing\n");
		return(1);
	}

	return(0);
}

void flip (char *buf,int len)
{
	int i=0;
	for (i=0;i<len-1;i+=2)
	{
		char k;
		k=buf[i];
		buf[i]=buf[i+1];
		buf[i+1]=k;
	}
}

int read_file_from_reserved(char *filename)
{
	// read from reserved areas 2,3,4,5

	char *whole_track;
	int h=0;

	if (sa_spt<=0)
	{
		printf("can't read sa_spt, aborting");
		return(1);
	}

	whole_track = malloc(sa_spt*512);

	FILE *f;
	f = fopen(filename, "w");
	
	if(in_read_head==1)
	    h=read_head_num;
	else
	    h=2;	// first head with unused "service-area" space.	
		
	int head,track;
	for (head=h;head<num_of_heads;head++)
	{
		for (track=-1;track>=(-1*sa_tracks);track--)
		{	
			printf("reading head(%d) track(%d)\n",head,track);
			if (0==wd_read_pchs(whole_track,head,track,1,sa_spt))
			{
				fwrite(whole_track, sa_spt*512, 1, f);
			} else {
				printf("wd_read_pchs failed, aborting\n");
				return(1);
			}
		}
	}

	fclose(f);
	return(0);
}

int write_file_to_reserved(char *filename)
{
	// write to reserved areas 2,3,4,5

	char *whole_track;
	int size,rc=0;

	struct stat st;
	stat(filename, &st);
	size = st.st_size;

	if (sa_spt<=0)
	{
		printf("can't read sa_spt, aborting");
		return(1);
	}

	whole_track = malloc(sa_spt*512);

	if (size > (sa_spt * (sa_tracks) * 4 * 512))	// 4 is a fixed set of available heads for this drive.
	{
		printf("can't fit %s in SA, aborting (%d > %d)\n", filename, size, (sa_spt*(sa_tracks-1)*4*512));
		return(1);
	}

	FILE *f;
	f = fopen(filename, "r");

	int head=2;
	int track=-1;
	while ( (rc = fread(whole_track, 1, sa_spt*512, f)) )
	{
		// trim if not a complete sector.
		printf("writing head(%d) track(%d)\n",head,track);
		if (0==wd_write_pchs(whole_track, head, track, 1, (int)rc/512))
		{
			track--;
			if (track==-65)
			{
				head++;
				track=-1;
			}
		} else {
			printf("wd_write_pchs failed, aborting\n");
		}
	}

	printf("leaving write to file rc=%d\n",rc);
	return 0;
}
