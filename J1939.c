 /*
 * Copyright (c) 2013 EIA Electronics
 *
 * Authors:
 * Kurt Van Dijck <kurt.van.dijck@eia.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under tfhe terms of the version 2 of the GNU General Public License
 * as published by the Free Software Foundation
 */

#include <signal.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <getopt.h>
#include <error.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/j1939.h>

#include <sys/ioctl.h>
#include <linux/can/raw.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <semaphore.h>  /* Semaphore */

#include <netinet/tcp.h>

#include "lib.h"

pthread_mutex_t conditon_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_var   = PTHREAD_COND_INITIALIZER;

void *functionCount1();
void *functionCount2();
void *functionCount3();
void *functionCount4();

int  count = 0;

#define PROCESS                         0x88
#define ACCEPT	                        0x00
#define CONNECT				0x04
#define DISCONNECT			0x05
#define RECEIVE				0x72
#define J1939_ADDRESS_CLAIM		0x10
#define SET_CAN_BAUDRATE		0x47
#define SEND_MESSAGE			0x40
#define J1939_DEL_ADDRESS_CLAIM         0x31
#define SET_FILTER         		0x08
#define RP1210_Set_Message_Filtering			0x0B
#define RP1210_Set_All_Filters_States_to_Pass           0x01
#define RP1210_Set_All_Filters_States_to_Discard	0x00
#define RP1210_Set_Message_Filtering_For_J1939          0x0C

#define RP1210_Echo_Transmitted_Messages		0x16
#define RP1210_GET_HDWR_STATUS                0x12

#define MAX_FILTER                      40
#define FILTER                        	0x02
#define PASS_ALL                      	0x01
#define DISCARD_ALL                     0x00



/* Programming*/
int state ;
sem_t CAN_receive;
int listenfd, connfd ;
struct sockaddr_in servaddr,cliaddr;
socklen_t clilen;
pid_t     childpid;



char msgRecv[10];
char AddClaimTemp[] = "su -c \"ip addr add dev can0 j1939 0xFF \"";
char AddClaim[] = "su -c \"ip addr add dev can0 j1939 0xFF \"";
char DelClaim[] = "su -c \"ip addr del dev can0 j1939 0xF9 \"";
char Command[][20] = { "01","02","19","47","05","31","03","04","17","16" };
char Address[10];
char * pch;
/*Function used In J1939*/

	int ret, sock, opt, j, verbose;
	socklen_t peernamelen;
	struct sockaddr_can sockname = {
		.can_family = AF_CAN,
		.can_addr.j1939 = {
			.addr = J1939_IDLE_ADDR ,
			.name = J1939_NO_NAME,
			.pgn = 0x0ee00,
		},
	}, peername = {
		.can_family = AF_CAN,
		.can_addr.j1939 = {
			.addr = J1939_NO_ADDR,
			.name = J1939_NO_NAME,
			.pgn = J1939_NO_PGN,
		},
	}, J1939Filter = {
                .can_family = AF_CAN,
	                .can_addr.j1939 = {
                        .addr = J1939_NO_ADDR,
                        .name = J1939_NO_NAME,
                        .pgn = J1939_NO_PGN,
                },
        };

	char recData[4100];
        uint8_t *dat;

	uint8_t temp_dat[4100];
	unsigned char mesg[4100];
	uint8_t datrec[4100];

	int valid_peername = 0;
	int todo_send = 0, todo_recv = 0, todo_echo = 0, todo_prio = -1;
	int todo_connect = 0, todo_names = 0;
	int ReceiveStart = 0;

char Pgn[6];
int mesgLen, idx, dlc, len;
unsigned char tmp;

struct j1939_filter j1939_filt[MAX_FILTER];
int filter = 0, filter_type = 0, filterCnt = 0, FilterType;
const char *device = "can0";

/* CAN FRAME */
int s; /* can raw socket */
int nbytes;
struct sockaddr_can addr;
struct can_frame frame;
struct ifreq ifr;
char singleFrame[18];
char NumOfSock = 0;

uint8_t J1939_Claimed_Address[10];
uint8_t Num_Of_Claimed_Address = 0;
uint8_t CMD_RP1210_Echo_Transmitted_Messages = 0;
uint8_t recvStart = 10;
int recvIndex = 0;
struct timeval timeout, timeout_config = { 0, 0 }, *timeout_current = NULL;

struct timeval tv, last_tv;
const int dropmonitor_on = 1;
int  msgLength = 0, ack, firstTime = 0,msgID = 0;
int s_ackMsgId ,s_ackType;

struct t_recvMsg{
	uint8_t ctrlByte;
        uint8_t msgType;
	int len;
	int sendByte;
	uint8_t msgId;
        uint8_t adMsgId;
	int pgn;
	int hts;
        int index;
        int ackIndex;
	int byteRec;
        int startPos;
        uint8_t sendData[4000];
	uint8_t da;
	uint8_t sa;
        uint8_t ackData[40];
	uint8_t *data;
	uint8_t checksum;
}recvMsg;

union {
	uint8_t t_Time[4];
	__u32 i_Time;
}Timestamp;

union {
        uint8_t len[2];
        int dataRec;
}recLen;

time_t currtime;
struct tm now;
int flag = 1 ;

void send_ack( struct t_recvMsg ack)
{
	uint8_t ackmsg[20];
	uint8_t ackChkSum = 0;
	int ackindex = 0,ackCount = 0 ;

           ackmsg[ackindex++] = 0xAE;
           ackmsg[ackindex++] = ack.msgType;
           ackmsg[ackindex++] = ack.len >> 8;
           ackmsg[ackindex++] = ack.len;
           ackmsg[ackindex++] = recvMsg.ctrlByte;
           ackmsg[ackindex++] = recvMsg.adMsgId++;
   	   for(ackCount  = 0 ;ackCount < ack.ackIndex ;ackCount++)
        	ackmsg[ackindex++] = ack.ackData[ackCount];

           ackmsg[ackindex++] = 0xFF;
           ackmsg[ackindex] = '\0';

           sendto(connfd,ackmsg,ackindex,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
}


int check_filter( struct sockaddr_can sockname ,int filter_Count)
{
int filterLen = 0 , result = 0;
	if( FilterType == PASS_ALL)return 1;
        if( FilterType == DISCARD_ALL)return 0;
 
	for(filterLen = 0; filterLen <  filter_Count; filterLen++)
	{
//		printf("Filter addr: %02x\n", j1939_filt[filterLen].addr);
		if((sockname.can_addr.j1939.addr == j1939_filt[filterLen].addr)||(j1939_filt[filterLen].addr == 0xFF))
		{
//		        printf("Filter addr find :%02x\n", j1939_filt[filterLen].addr);
			result |= 0x01;
		}

//                printf("Filter pgn: %05x\n", j1939_filt[filterLen].pgn);
                if((sockname.can_addr.j1939.pgn == j1939_filt[filterLen].pgn)||(j1939_filt[filterLen].pgn == 0x00))
                {
//                        printf("Filter pgn find :%05x\n",j1939_filt[filterLen].pgn);
                        result |=  0x10;
                }

		if(result == 0x11 )
		{
			 result = 1;
			 break;
		}
		else{
			result = 0;
//                        printf("Filter Not Match\n");
		}
	}
	return result;
}

main()
{
	state =  ACCEPT;

//	sem_init(&CAN_receive, 1, 0);      /* initialize mutex to 1 - binary semaphore */
                                 /* second param = 0 - semaphore is local */
	pthread_t thread1, thread2, thread3, thread4;
///////////////////////////////////////////////////////////////////////////////////////////////////////
        /* open socket */

	printf("Start \n");

        sock = ret = socket(PF_CAN, SOCK_DGRAM, CAN_J1939);
        if (ret < 0)
           error(1, errno, "socket(j1939)");


	ret = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
           	device, strlen(device));
	if (ret < 0)
        	error(1, errno, "bindtodevice %s", device);

//   	ret = setsockopt(sock, SOL_CAN_J1939, SO_J1939_FILTER,
//           	&filt, sizeof(filt));
//   	if (ret < 0)
//	       	error(1, errno, "setsockopt filter");

//	if (setsockopt(sock, SOL_SOCKET, SO_RXQ_OVFL,
//                       &dropmonitor_on, sizeof(dropmonitor_on)) < 0) {
//                perror("setsockopt SO_RXQ_OVFL not supported by your Linux Kernel");
//                return 1;
//            }

        printf("socket open \n");

//        ret = bind(sock, (void *)&sockname, sizeof(sockname));
//        if(ret < 0)
//                error(1, errno, "bind()");

//        printf("Bind ok \n");



//        parse_canaddr(SocketAddr, &sockname);

//         sockname.can_addr.j1939.addr =  0xFE;
//         sockname.can_addr.j1939.pgn =  0x0ee00;

        ret = bind(sock, (void *)&sockname, sizeof(sockname));
        if(ret < 0)
        	error(1, errno, "bind()");

        printf("Bind ok \n");


////////////////////////////////////////////////////////////////////////////////////////////////////////

        listenfd=socket(AF_INET,SOCK_STREAM,0);
        bzero(&servaddr,sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
        servaddr.sin_port=htons(32000);//(32000);
        bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

        listen(listenfd,1024);


 	setsockopt( listenfd,            /* socket affected */
                        IPPROTO_TCP,     /* set option at TCP level */
                        TCP_NODELAY,     /* name of option */
                        (char *) &flag,  /* the cast is historical cruft */
                        sizeof(int));    /* length of option value */

        if (time(&currtime) == (time_t)-1) {
            perror("time");
            return 1;
        }
 
        localtime_r(&currtime, &now);
 
	pthread_create( &thread1, NULL, &functionCount1, NULL);
	pthread_create( &thread2, NULL, &functionCount2, NULL);

	pthread_join( thread1, NULL);
	pthread_join( thread2, NULL);
//        pthread_join( thread3, NULL);
//        pthread_join( thread4, NULL);
        printf("System Crash\n");
 
	exit(0);
}

/* Receive Thread From PC*/

void *functionCount1()
{
	for(;;)
	{
		switch(state)
		{
                case ACCEPT:
			printf("In Accept state\n");
                        clilen=sizeof(cliaddr);
                        connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);
                        //printf("In Accepted state\n");

                        state = RECEIVE;

                break;

		case CONNECT:
                        printf("In connect state\n ");
			for(filterCnt = 0; filterCnt < MAX_FILTER; filterCnt++)
			{
				j1939_filt[filterCnt].pgn = 0x00;
				j1939_filt[filterCnt].addr = 0xFF;
			}
                        filter  = 0;
			ReceiveStart = 1;
			Num_Of_Claimed_Address = 0;
//			sem_post(&CAN_receive);       /* up semaphore */
                        ioctl(sock, SIOCGSTAMP, &tv);
//                        printf("(Timestamp %x.%06x) ", tv.tv_sec, tv.tv_usec);

                        state = RECEIVE;
			recvStart = 10;
			recvMsg.byteRec  = 0;
           		recvMsg.len = 0x0004;
                        recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
			recvMsg.ackData[recvMsg.ackIndex++] = 0x21;
			send_ack(recvMsg );
		break;

		case DISCONNECT:
                        //printf("In disconnect state\n");
			ReceiveStart = 0;
                        for(filterCnt = 0; filterCnt < MAX_FILTER; filterCnt++)
                        {
                                j1939_filt[filterCnt].pgn = 0x00;
                                j1939_filt[filterCnt].addr = 0xFF;
                        }
                        filter  = 0;
			//Release claimed Address
			close(connfd);
//			J1939_Claimed_Address[Num_OF_Claimed_Address++]
//			if(Num_Of_Claimed_Address)
//				system(DelClaim);

//		        sockname.can_addr.j1939.addr =  0xFE;
//         		sockname.can_addr.j1939.pgn =  0x0ee00;

//        		ret = bind(sock, (void *)&sockname, sizeof(sockname));
//        		if(ret < 0)
//                		error(1, errno, "bind()");

                        Num_Of_Claimed_Address = 0;
	                ioctl(sock, SIOCGSTAMP, &tv);
//                      printf("(Timestamp %x.%06x) ", tv.tv_sec, tv.tv_usec);
                        recvMsg.byteRec  = 0;
                        recvMsg.len = 0x0004;
                        recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
                        recvMsg.ackData[recvMsg.ackIndex++] = 0x21;
                        send_ack(recvMsg );
			usleep(10);
                        if (ret < 0)
                                error(1, errno, "setsockopt filter");

			state =  ACCEPT;
			/*Disconnect J1939*/
			break;

		case RECEIVE:
                        recvMsg.startPos = 0 ;
			recvMsg.ackIndex = 0;
                       	recvMsg.byteRec = recvfrom(connfd,temp_dat,4000,0,(struct sockaddr *)&cliaddr,&clilen);
			if(recvMsg.byteRec <= 0)
			{
         	                printf("In client crash state %d:\n",recvMsg.byteRec);
                         	close(connfd);
				state =  ACCEPT;
				break;
			}
                        //for (j = 0; j < recvMsg.byteRec; ++j)
			//{
	                //        printf("%02x ",temp_dat[j]);
			//}
			//printf("\n");
                        state = PROCESS;

	        case PROCESS:
                        //printf("In process state\n");
                        state = RECEIVE;
			for(mesgLen = recvMsg.startPos ; mesgLen < recvMsg.byteRec ; mesgLen++ )
			{
				if(temp_dat[mesgLen]== 0xea)
				{
					state = PROCESS;
                                        break;
				}
			}

			if(state == RECEIVE)
                        {
//                        	printf("EA not received \n");
                                recvMsg.byteRec = 0 ;
                                recvMsg.startPos = 0;
                                break;
                        }

			recvMsg.index =  2;
                        recvMsg.msgType = temp_dat[1];
                        recvMsg.len = temp_dat[2] << 8 |temp_dat[3];
//			printf("recvMsg.len  %d:\n", recvMsg.len);


		        if( recvMsg.msgType == 0xC0 )
			{
                                recvMsg.ctrlByte = temp_dat[4];
//                                printf("recvMsg.ctrlByte  %x:\n",recvMsg.ctrlByte);
	                        state = recvMsg.ctrlByte ;
	                        recvMsg.startPos  += (recvMsg.len) + 6;
			}
			else if(recvMsg.msgType == 0x40 )
			{
				recvMsg.ctrlByte = SEND_MESSAGE;
	                        state = recvMsg.msgType ;
                                recvMsg.startPos  += (recvMsg.len ) + 5 ;// temp Change
//                                printf("recvMsg.ctrlByte  %x:\n",recvMsg.ctrlByte);
			}
			else
			{
				printf("wrong message Type\n");
                                recvMsg.byteRec = 0 ;
                                recvMsg.startPos = 0;
                        	state = RECEIVE ;
			}
                        recvMsg.msgId = temp_dat[5];
			break;

		case J1939_ADDRESS_CLAIM:
                        for (j = 0; j < recvMsg.byteRec; ++j)
                        {
                                printf("%02x ",temp_dat[j]);
                        }
                        printf("\n");
 
                        sprintf(&Address[0], "%02x",temp_dat[6] );
                        Address[2] = '\0';

			Num_Of_Claimed_Address +=1;
			AddClaim[36]= Address[0];
			AddClaim[37]= Address[1];
//			printf("AddClaim: %s bytes \n",AddClaim);
			system(AddClaim);
//			mesg[recvMsg.byteRec] = 0;

                        for (recvMsg.sendByte = 0 ; recvMsg.sendByte < recvMsg.len - 3; recvMsg.sendByte++)
                        {
                                 recvMsg.sendData[recvMsg.sendByte] = temp_dat[7 + recvMsg.sendByte];
                        }

 			sockname.can_addr.j1939.addr = temp_dat[6];
			sockname.can_addr.j1939.pgn = 0x0ee00;

     			ret = bind(sock, (void *)&sockname, sizeof(sockname));
			if(ret < 0)
				error(1, errno, "bind()");

			valid_peername = 1;
                        recvMsg.len = 0x0004;
//                        ret = send(sock, recvMsg.sendData, recvMsg.sendByte, 0);
                        peername.can_addr.j1939.addr = 0xFF;
			peername.can_addr.j1939.pgn = 0x0ee00;
                        ack = sendto(sock, recvMsg.sendData, recvMsg.sendByte, 0,(void *)&peername, sizeof(peername));
                        recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
                        recvMsg.ackData[recvMsg.ackIndex++] = 0x01;
                        send_ack(recvMsg );

			state = PROCESS;

			break;

		case J1939_DEL_ADDRESS_CLAIM:
                        printf("In Del addressclaim state\n");

                        DelClaim[36]= dat[12];
                        DelClaim[37]= dat[13];
                        system(DelClaim);
                        send_ack(recvMsg );
                        state = PROCESS;
                        break;

		case SET_CAN_BAUDRATE:
			printf("In Baud Rate state\n");
                        send_ack(recvMsg );
			state = PROCESS;
			break;

                case RP1210_GET_HDWR_STATUS:
	          	printf("RP1210_GET_HDWR_STATUS\n");
			recvIndex = 0 ;
                        datrec[recvIndex++] = 0xAE;
                        datrec[recvIndex++] = 0xC0;
                        datrec[recvIndex++] = 0x00;
                        datrec[recvIndex++] = 0x0F;
                        datrec[recvIndex++] = 0x12;
                        datrec[recvIndex++] = recvMsg.msgId;
                        datrec[recvIndex++] = recvMsg.msgId;
                        datrec[recvIndex++] = 0x00;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0x00;;
                        datrec[recvIndex++] = 0xFF;;
                        send_ack(recvMsg );
                        state = PROCESS;
                        break;

		case SEND_MESSAGE :
 			todo_send = recvMsg.byteRec ;
                        peername.can_addr.j1939.addr = temp_dat[10];
                        peername.can_addr.j1939.pgn =  temp_dat[5]<<16 | temp_dat[6]<<8 | temp_dat[7];

                        for (recvMsg.sendByte = 0 ; recvMsg.sendByte < recvMsg.len - 6; recvMsg.sendByte++)
                        {
                                recvMsg.sendData[recvMsg.sendByte] = temp_dat[11 + recvMsg.sendByte];
                        }

//                        if (valid_peername ) { //&& address claimed
				 recvMsg.sendByte -= 1;

 				ack = sendto(sock, recvMsg.sendData, recvMsg.sendByte, 0,(void *)&peername, sizeof(peername));
				if ( ack < 0) {
                        		perror("Send to fail \n");
	                                send_ack(recvMsg );
                    		}
				else{
                                    //    recvMsg.len = 0x0004;
                                    //    recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
                                    //    recvMsg.ackData[recvMsg.ackIndex++] = 0x01;
        	                    //   send_ack(recvMsg );
				}

				state = PROCESS;
			 break;

			 case	RP1210_Set_Message_Filtering:
                                filter_type = temp_dat[6];
				if(filter_type == 0x12){ //|J1587!J1939!CAN!
		                        for(filterCnt = 0; filterCnt < MAX_FILTER; filterCnt++)
                		        {
                                		j1939_filt[filterCnt].pgn = 0x00;
                                		j1939_filt[filterCnt].addr = 0xFF;
                        		}
                        		filter  = 0;
					FilterType = PASS_ALL;
                    			recvMsg.len = 0x0004;
                        		recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
                        		recvMsg.ackData[recvMsg.ackIndex++] = 0x11;
	                        	send_ack(recvMsg );
				}
				else if(filter_type == 0x02){//|J1587!J1939!CAN!
                                        for(filterCnt = 0; filterCnt < MAX_FILTER; filterCnt++)
                                        {
                                                j1939_filt[filterCnt].pgn = 0x00;
                                                j1939_filt[filterCnt].addr = 0xFF;
                                        }
                                        filter  = 0;
                                        FilterType = DISCARD_ALL;
                                        recvMsg.len = 0x0004;
                                        recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
                                        recvMsg.ackData[recvMsg.ackIndex++] = 0x01;
                                	send_ack(recvMsg );
				}
                        	state = PROCESS;
			break;

			case RP1210_Set_Message_Filtering_For_J1939:
			//printf("In set Filter\n");
                        FilterType = FILTER;
 
                        if(filter  == MAX_FILTER )
			{
                                //printf("MAX Filter HI LIMIT\n");
				state = PROCESS;
                                recvMsg.len = 0x0004;
                                recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
                                recvMsg.ackData[recvMsg.ackIndex++] = 0x01;
                                send_ack(recvMsg );
				break;
			}

			filter_type =  temp_dat[6];
			if(filter_type & 0x01)
			{
	                        J1939Filter.can_addr.j1939.pgn =  temp_dat[7] | temp_dat[8]<<8 | temp_dat[9]<<16;
                                if(j1939_filt[filter].pgn == 0x00){
	                                j1939_filt[filter].pgn = J1939Filter.can_addr.j1939.pgn;
	                                //printf("In set Filter Pgn :%05x \n", J1939Filter.can_addr.j1939.pgn);
				}
			}

                        if(filter_type & 0x04 )  //source address
                        {
                        	J1939Filter.can_addr.j1939.addr = temp_dat[11];
                                if(j1939_filt[filter].addr == 0xFF){
             		                    j1939_filt[filter].addr = J1939Filter.can_addr.j1939.addr;
				            //printf("In set Filter DA :%02x \n", J1939Filter.can_addr.j1939.addr);
                                }
			}
/*
                        if(filter_type & 0x08 )  //destination address
                        {
                                printf("In Set DA filtering \n");
                                J1939Filter.can_addr.j1939.addr = temp_dat[12];
                                printf("Filter DA:%02x: \n", J1939Filter.can_addr.j1939.addr);
                                if (J1939Filter.can_addr.j1939.addr <= 0xff)
                                {
                                       for(filterCount = 0; filterCount < MAX_FILTER; filterCount++){
                                                if(j1939_filt[filterCount].addr == 0xFF){
                                                        j1939_filt[filterCount].addr = J1939Filter.can_addr.j1939.addr;
//                                                        printf(" Set destination :%d \n",  j1939_filt[filterCount].addr);
                                                         //filter = filter + 1;
                                                        break;
                                                }
                                        }
                                }
                        }
*/

                        recvMsg.len = 0x0004;
                        recvMsg.ackData[recvMsg.ackIndex++] = recvMsg.msgId;
                        recvMsg.ackData[recvMsg.ackIndex++] = 0x01;
                        send_ack(recvMsg );
 			filter = filter + 1;
                        state = PROCESS;
                        break;

		case RP1210_Echo_Transmitted_Messages:
			printf("in RP1210_Echo_Transmitted_Messages \n");
			recvStart = 11 ;
			CMD_RP1210_Echo_Transmitted_Messages = 1;
                        state = PROCESS;
			break;

                default:
                        printf("Received the following:\n");
                        send_ack(recvMsg );
			recvMsg.byteRec = 0;
			state = PROCESS;
                        break;

		}
	}
}

/*Transmit Thread to PC*/

void *functionCount2()
{
	for(;;)
	{
		peernamelen = sizeof(peername);
		ret = recvfrom(sock,&datrec[16], 4000, 0,(void *)&peername, &peernamelen);//change 10 to eco byte
		recvIndex = 0;

               // tm = *localtime(&tdut.tv_sec);
		if(firstTime == 0 ){
	        	ioctl(sock, SIOCGSTAMP, &last_tv);
			firstTime = 1 ;
		}

		ioctl(sock, SIOCGSTAMP, &tv);

		if (ret < 0) {
			if (EINTR == errno) {
				if (verbose)
					fprintf(stderr, "-\t<interrupted>\n");
				continue;
			}
			error(1, errno, "recvfrom()");
			}
		tv.tv_sec = tv.tv_sec - last_tv.tv_sec;
        	Timestamp.i_Time = tv.tv_sec * 1000 +  tv.tv_usec/1000 ;

        	recLen.dataRec = ret + 12;

                datrec[recvIndex++] = 0xAE;
                datrec[recvIndex++] = 0x40;
                datrec[recvIndex++] = (recLen.dataRec & 0xFF00)>>8;//recLen.len[0] ;
                datrec[recvIndex++] = recLen.dataRec;//recLen.len[1] ;
                datrec[recvIndex++] = recvMsg.adMsgId++;;

		datrec[recvIndex++] = Timestamp.t_Time[3];
                datrec[recvIndex++] = Timestamp.t_Time[2];
                datrec[recvIndex++] = Timestamp.t_Time[1];
                datrec[recvIndex++] = Timestamp.t_Time[0];

                datrec[recvIndex++] = 0x00; //eco byte
                datrec[recvIndex++] = peername.can_addr.j1939.pgn >> 16;
                datrec[recvIndex++] = peername.can_addr.j1939.pgn >> 8;
                datrec[recvIndex++] = peername.can_addr.j1939.pgn;
                datrec[recvIndex++] = 0x06;
                datrec[recvIndex++] = peername.can_addr.j1939.addr;
                datrec[recvIndex++] = sockname.can_addr.j1939.addr;

		if( (ReceiveStart )&&( check_filter( peername,filter )) )
		{
			recvIndex = recvIndex + ret + 1;
			datrec[recvIndex] = 0x00;
                        sendto(connfd,datrec,recvIndex,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));

     			//printf("(Timestamp %03ld.%06ld) ", tv.tv_sec, tv.tv_usec);
	                //for (j = 0; j < recvIndex; ++j)
    	                //printf(" %02x", datrec[j]);
                	//printf("|  recvIndex + ret %d|\n", recvIndex);
		}
	}
}

