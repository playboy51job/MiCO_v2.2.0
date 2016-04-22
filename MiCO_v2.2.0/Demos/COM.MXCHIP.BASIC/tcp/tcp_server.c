/**
******************************************************************************
* @file    tcp_server.c 
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
#include "MICOCli.h"

#define tcp_server_log(M, ...) custom_log("TCP", M, ##__VA_ARGS__)

#define SERVER_PORT 20000 /*set up a tcp server,port at 20000*/
//static char *ap_ssid = "Xiaomi.Router";
//static char *ap_key  = "stm32f215";
static char *ap_ssid = "Voip";
static char *ap_key  = "1234567890";
static mico_semaphore_t wait_sem;

void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
  tcp_server_log("Wlan Connection Err %d", err);
}

void micoNotify_WifiStatusHandler(WiFiEvent event, void* const inContext)
{
  switch (event) {
  case NOTIFY_STATION_UP:
    tcp_server_log("connected wlan success");
    mico_rtos_set_semaphore(&wait_sem);/*release semaphore*/
    break;
  case NOTIFY_STATION_DOWN:
    tcp_server_log("Station down");
    break;
  }
}

static void connect_ap( void )
{  
  network_InitTypeDef_adv_st wNetConfigAdv;
  memset(&wNetConfigAdv, 0x0, sizeof(network_InitTypeDef_adv_st));
  strcpy((char*)wNetConfigAdv.ap_info.ssid, ap_ssid);/*ap ssid*/
  strcpy((char*)wNetConfigAdv.key, ap_key);/*ap password*/
  wNetConfigAdv.key_len = strlen(ap_key);
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;
  wNetConfigAdv.ap_info.channel = 0; /*Auto*/
  wNetConfigAdv.dhcpMode = DHCP_Client;/*auto config IP*/
  wNetConfigAdv.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfigAdv);
}
void tcp_client_thread(void *inContext)
{
  int fd=(int)inContext;
  OSStatus err;
  int len=0;
  fd_set rset;/*result set*/
  fd_set set;/*concerned set*/
  char *buf=(char*)malloc(1024);
  require_action(buf, EXIT_CLIENT_THRED, err = kNoMemoryErr);/*alloc failed*/
  FD_ZERO(&set);
  FD_SET(fd, &set);
  while(1)
  {
    rset=set;
    select(fd+1, &rset, NULL, NULL, NULL);
    if (FD_ISSET( fd, &rset )) /*one client has data*/
    {
      memset(buf,0,1024);
      len=read(fd,buf,1024);
      if( len <= 0)
      {
        close(fd);
        FD_CLR(fd,&set);
        break;
      }
      tcp_server_log("tcp read from client len=%d,buf=%s", len, buf);
      write(fd,buf,len);
      tcp_server_log("tcp write back to client len=%d", len);
    }
  }
EXIT_CLIENT_THRED:
  mico_rtos_delete_thread(NULL);
  free(buf);
}

/*create server socket*/
void tcp_server_thread(void *inContext)
{
  OSStatus err;
  struct sockaddr_t server_addr,remote_addr;
  socklen_t    sockaddr_t_size;
  char  ip_address[16]={0};
  int tcp_listen_fd ,tcp_new_fd;
  tcp_listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  tcp_server_log("tcp_listen_fd=%d",tcp_listen_fd);
  tcp_server_log("tcp_listen_fd add=%x",&tcp_listen_fd);
  memset(&server_addr,0,sizeof(server_addr));
  server_addr.s_ip =  INADDR_ANY;/*local ip address*/
  server_addr.s_port = SERVER_PORT;/*20000*/
  err = bind(tcp_listen_fd, &server_addr, sizeof(server_addr));
  require_noerr( err, EXIT_SERVER_THRED );
  err = listen(tcp_listen_fd, 0);
  require_noerr( err, EXIT_SERVER_THRED );
  while(1)
  {
    sockaddr_t_size = sizeof(remote_addr);
    /*waiting for client to connect*/
    tcp_new_fd = accept(tcp_listen_fd, &remote_addr, &sockaddr_t_size);
    memset(ip_address,0,sizeof(ip_address));
    inet_ntoa(ip_address, remote_addr.s_ip );
    tcp_server_log("new client ip: %s port: %d connected", ip_address, remote_addr.s_port);
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, NULL, tcp_client_thread, 0x800, (void*)tcp_new_fd );
  }
EXIT_SERVER_THRED:
  mico_rtos_delete_thread(NULL);
}
int application_start( void )
{
  OSStatus err = kNoErr;
  IPStatusTypedef para;
  tcp_server_log("tcp_server demo");
  MicoInit();/*important,init tcpip,rf driver and so on...*/
  MicoCliInit();
  /*The notification message for the registered WiFi status change*/
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler );
  require_noerr( err, EXIT ); 
  
  err = MICOAddNotification( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler );
  require_noerr( err, EXIT );
  
  err = mico_rtos_init_semaphore(&wait_sem, 1);
  require_noerr( err, EXIT ); 
  
  connect_ap( );
  tcp_server_log("connecting to %s...", ap_ssid);
  mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);/*wifi connected successful*/
  
  /*you can get ip status after device connected wifi*/
  micoWlanGetIPStatus(&para, Station);
  tcp_server_log("Server established at ip: %s port: %d",para.ip, SERVER_PORT);
  
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "TCP_server", tcp_server_thread, 0x800, NULL );
  require_noerr_action( err, EXIT, tcp_server_log("ERROR: Unable to start the tcp server thread.") );
EXIT:
  tcp_server_log("ERROR, err: %d", err);
  return err;
}

