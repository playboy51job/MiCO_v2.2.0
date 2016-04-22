/**
******************************************************************************
* @file    tcp_client.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   First MiCO application to say hello world!
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2014 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy 
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights 
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************
*/

#include "MICO.h"
#include "SocketUtils.h"
#include "MICONotificationCenter.h"
#include "MicoWlan.h"

#define tcp_client_log(M, ...) custom_log("TCP", M, ##__VA_ARGS__)


static char *ap_ssid = "Xiaomi.Router";
static char *ap_key  = "stm32f215";
static char tcp_remote_ip[16] = "192.168.31.232";/*remote ip address*/
static int tcp_remote_port = 20000;/*remote port*/
static mico_semaphore_t wait_sem;

void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
  tcp_client_log("Wlan Connection Err %d", err);
}
void micoNotify_WifiStatusHandler(WiFiEvent event, void* const inContext)
{
  switch (event) {
      case NOTIFY_STATION_UP:
      tcp_client_log("connected wlan success");
      /*release semaphore*/
      mico_rtos_set_semaphore(&wait_sem);
      break;
  case NOTIFY_STATION_DOWN:
      tcp_client_log("Station down");
      break;
  }
}

/*connect wifi*/
static void connect_ap( void )
{  
  network_InitTypeDef_adv_st wNetConfigAdv;
  memset(&wNetConfigAdv, 0x0, sizeof(network_InitTypeDef_adv_st));
  strcpy((char*)wNetConfigAdv.ap_info.ssid, ap_ssid);/*ap's ssid*/
  strcpy((char*)wNetConfigAdv.key, ap_key);/*ap's key*/
  wNetConfigAdv.key_len = strlen(ap_key);
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;
  wNetConfigAdv.ap_info.channel = 0; /*Auto*/
  wNetConfigAdv.dhcpMode = DHCP_Client;
  wNetConfigAdv.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfigAdv);
}

/*when client connected wlan success,create socket*/
void tcp_client_thread(void *inContext)
{
  OSStatus err;
  struct sockaddr_t addr={0};
  struct timeval_t timeout;
  fd_set rset;/*result set*/
  fd_set set;/*concerned set*/
  int tcp_fd = -1 , len, nfound, maxfd;
  char *buf=(char*)malloc(1024);
  require_action(buf, EXIT_THREAD, err = kNoMemoryErr);/*alloc failed*/
  
  tcp_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  
  addr.s_ip = inet_addr(tcp_remote_ip);
  addr.s_port = tcp_remote_port;
  tcp_client_log("Connecting to server: ip=%s  port=%d!", tcp_remote_ip,tcp_remote_port);
  err = connect(tcp_fd, &addr, sizeof(addr));/*err=0,success*/
  require_noerr(err, EXIT_THREAD);
  
  FD_ZERO(&set);
  FD_SET(tcp_fd, &set);
  maxfd=tcp_fd + 1;
  while(1)
  {
      rset=set;
      timeout.tv_sec = 2;
      timeout.tv_usec = 0;
      nfound=select(maxfd, &rset, NULL, NULL, &timeout);
      if(nfound==0)/*nothing happened,printf dot...*/
      {
        printf(".");
        fflush(stdout);
        continue;
      }
      /*recv wlan data*/
      if (FD_ISSET( tcp_fd, &rset )) /*read what,write back*/
      {
          memset(buf, 0, 1024);
         len=read(tcp_fd,buf,1024);
         if( len <= 0)
         {
             tcp_client_log("socket disconnected");
             break;/*error*/
         }
         tcp_client_log("tcp read from server,len=%d,buf=%s", len, buf);
         len=write(tcp_fd,buf,len);
         tcp_client_log("tcp write back to server len=%d", len);
      }
  }
EXIT_THREAD:
    mico_rtos_delete_thread(NULL);
    free(buf);
    close(tcp_fd);
}
int application_start( void )
{
  OSStatus err = kNoErr;
  tcp_client_log("tcp_client demo");
  MicoInit( );/*important,init tcpip,rf driver and so on...*/
  
   /*The notification message for the registered WiFi status change*/
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler );
  require_noerr( err, EXIT ); 
  
  err = MICOAddNotification( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler );
  require_noerr( err, EXIT );
  
  err = mico_rtos_init_semaphore(&wait_sem, 1);
  require_noerr( err, EXIT ); 
  
  connect_ap( );/*connect wifi*/
  tcp_client_log("connecting to %s...", ap_ssid);
  mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);/*wait forever until connected wifi*/
    
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "TCP_client", tcp_client_thread, 0x800, NULL );
  require_noerr_action( err, EXIT, tcp_client_log("ERROR: Unable to start the tcp client thread.") );
  
  return err;

EXIT:
  tcp_client_log("ERROR, err: %d", err);
  return err;
}

