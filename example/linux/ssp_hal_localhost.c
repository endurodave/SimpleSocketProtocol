// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h 
// using memory buffers to simulate communication between sockets for testing SSP
#include <errno.h>
#include<unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include<sys/socket.h>
#include<arpa/inet.h>

#include "ssp_opt.h"
#include "ssp_hal.h"
#include "ssp_osal.h"
#include "ssp_fault.h"

#define SEND_RETRY_MAX      2
#define SEND_RETRY_DELAY    5   // in mS

#define RECV_BUF_SIZE    1024 

#define SSP_LOCK_WAIT_DEFAULT   5000

static SSP_OSAL_HANDLE g_port_lock[SSP_MAX_PORTS];

static BOOL powerSave = TRUE;

#define LOCALHOST_PORT_ID	6001	               //Only one Port by now, but may be extended by simpy enum portId

const char *ip = "10.15.154.18";       // Use localhost

static INT g_sockfd[SSP_MAX_PORTS] = { -1 }; //If more Ports, more g_sockfd's needed

void SSPHAL_Init(SspPortId portId) {
   for (INT port = 0; port < SSP_MAX_PORTS; port++) {
      g_sockfd[port] = -1;
      g_port_lock[port] = SSPOSAL_LockCreate();
      ASSERT_TRUE(g_port_lock[port] != SSP_OSAL_INVALID_HANDLE_VALUE);
   }
}

void SSPHAL_Term(void) {
   for (INT port = 0; port < SSP_MAX_PORTS; port++) {
      SSPHAL_PortClose(port);
      SSPOSAL_LockDestroy(g_port_lock[port]);
      g_port_lock[port] = SSP_OSAL_INVALID_HANDLE_VALUE;
   }
}

BOOL SSPHAL_PortOpen(SspPortId portId) {
   ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

   if (SSPHAL_PortIsOpen(portId) == TRUE)
       return TRUE;

   const int y = 1;

   SSPOSAL_LockGet(g_port_lock[portId], SSP_LOCK_WAIT_DEFAULT);

   struct sockaddr_in serverAddr;

   printf("Create udp socket\n");
   if ((g_sockfd[portId] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
      goto error;
   }
   // Configure settings of the server address struct
   // Address family = Internet
   serverAddr.sin_family = AF_INET;

   //Set port number, using htons function to use proper byte order
   serverAddr.sin_port = htons(LOCALHOST_PORT_ID);

   //Set IP address to localhost
   serverAddr.sin_addr.s_addr = inet_addr(ip);

   //setsockopt(g_sockfd[portId], SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));

	//Bind the address struct to the socket
	if(bind(g_sockfd[portId], (struct sockaddr*) &serverAddr, sizeof(serverAddr))<0)
	{
		printf("SSPHAL_PortOpen failed:%d,(%s)\n", errno, strerror(errno));
		goto error;
	}

   ASSERT_TRUE(g_sockfd[portId] != -1);

   SSPOSAL_LockPut(g_port_lock[portId]);

   return TRUE;

error:
   SSPHAL_PortClose(portId);
   return FALSE;
}

void SSPHAL_PortClose(SspPortId portId) {

   ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

   SSPOSAL_LockGet(g_port_lock[portId], SSP_LOCK_WAIT_DEFAULT);

   if (g_sockfd[portId] > 0)
   {
      close(g_sockfd[portId]);
      g_sockfd[portId] = -1;
   }

   SSPOSAL_LockPut(g_port_lock[portId]);
}

BOOL SSPHAL_PortIsOpen(SspPortId portId) {
   BOOL isOpen;
   SSPOSAL_LockGet(g_port_lock[portId], SSP_LOCK_WAIT_DEFAULT);

   if (g_sockfd[portId] == -1)
       isOpen = FALSE;
   else
       isOpen = TRUE;

   SSPOSAL_LockPut(g_port_lock[portId]);
   return isOpen;
}

BOOL SSPHAL_PortSend(SspPortId portId, const char *buf, UINT16 bytesToSend) {

   if (NULL == buf || 0 == bytesToSend)
      return FALSE;

   BOOL success = TRUE;
   INT32 bytesSent = 0;

   SSPOSAL_LockGet(g_port_lock[portId], SSP_LOCK_WAIT_DEFAULT);

   if (g_sockfd[portId] != -1)
   {
	  struct sockaddr_in addr;

	  memset(&addr, '\0', sizeof(addr));

	  addr.sin_family = AF_INET;
	  addr.sin_port = htons(LOCALHOST_PORT_ID+1);
	  addr.sin_addr.s_addr = inet_addr(ip);

      if ((bytesSent=sendto(g_sockfd[portId], buf, bytesToSend, 0, (struct sockaddr*) &addr,
            sizeof(addr))) <= 0) {
    	  printf("send failed:%d,(%s)\n", errno, strerror(errno));
         success = FALSE;
      }
      ASSERT_TRUE(bytesToSend == bytesSent);
   }


   SSPOSAL_LockPut(g_port_lock[portId]);
   return success ? TRUE : FALSE;
}

BOOL SSPHAL_PortRecv(SspPortId portId, char *buf, UINT16 *bytesRead,
      UINT16 maxLen, UINT16 timeout) {
   if (NULL == buf || NULL == bytesRead)
      return FALSE;

   if (maxLen <= 0)
       return FALSE;

   INT32 byReadTemp = 0;

   SSPOSAL_LockGet(g_port_lock[portId], SSP_LOCK_WAIT_DEFAULT);

   struct timeval tv;
   struct sockaddr_in server_addr;
   int server_struct_length = sizeof(server_addr);
   tv.tv_sec = 0;
   tv.tv_usec = (timeout * 1000) % 1000000;    //Calc nanos


//   struct timespec now, when;
//   timespec_get(&now, TIME_UTC);
//
//   UINT32 when_tv_sec, when_tv_nsec;
//
//   when_tv_sec = timeout / 1000;            //Calc secs
//   when_tv_nsec = (timeout * 1000) % 1000000;    //Calc nanos
//
//   when.tv_sec = now.tv_sec + when_tv_sec;
//   when.tv_nsec = now.tv_nsec + when_tv_nsec;

   *bytesRead = 0;

   if (setsockopt(g_sockfd[portId], SOL_SOCKET, SO_RCVTIMEO, &tv,
         sizeof(tv)) < 0) {
	  printf("setsockopt failed:%d,(%s)\n", errno, strerror(errno));
      goto error;
   }


   //Receive a reply from the client
//   if ((byReadTemp = recv(g_sockfd[portId], buf, maxLen, 0)) <= 0) {
//      goto error;
//   }
   if((byReadTemp = recvfrom(g_sockfd[portId], buf, maxLen, 0,
        (struct sockaddr*)&server_addr, &server_struct_length)) <= 0){
            goto error;
   }

   //Not expected
   if(byReadTemp>65535)
	   byReadTemp = 65535;

   *bytesRead = (UINT16)byReadTemp;	//CAST ok

   SSPOSAL_LockPut(g_port_lock[portId]);
   return TRUE;


error:
    SSPOSAL_LockPut(g_port_lock[portId]);
    return FALSE;

}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId) {
   /* Clients register with SSP to receive asynchronous callbacks
    * using the SSP_Listen() API. The API accepts a callback function pointer
    * and a socket ID. When a packet successfully arrives on the specified socket,
    * the client callback function is called.
    * A single receive buffer exists separate from the sending buffers.
    * Once the client notification callback occurs, the receive buffer is free
    * to be used for the next incoming packet. Therefore, if a listener callback
    * needs to retain the incoming data it must be copied to another application
    * defined location.
    */
   fd_set rfd;
   FD_ZERO(&rfd);
   FD_SET(g_sockfd[portId], &rfd);

   struct timeval timeout;
   timeout.tv_sec = 0;
   timeout.tv_usec = 0;

   int ret = select(g_sockfd[portId] + 1, &rfd, NULL, NULL, &timeout);

   if ((ret < 0) || (ret == 0))
	  return TRUE;
   else
   {
	  if(FD_ISSET(g_sockfd[portId], &rfd))
		 return FALSE;
	  else
		 return TRUE;
   }
}

void SSPHAL_PortFlush(SspPortId portId) {
   //TODO
}

void SSPHAL_PowerSave(BOOL enable) {
   powerSave = enable;

   if (FALSE == powerSave) {
      /// @TODO: Do something when power savings disabled as necessary
   }
}

BOOL SSPHAL_IsPowerSave(void) {
   return powerSave;
}

