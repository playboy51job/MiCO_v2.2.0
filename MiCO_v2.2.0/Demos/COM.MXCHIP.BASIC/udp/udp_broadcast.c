/**
******************************************************************************
* @file    udp_broadcast.c 
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
#include "MICONotificationCenter.h"

#define udp_broadcast_log(M, ...) custom_log("UDP", M, ##__VA_ARGS__)


static int   udp_port = 20000;
static char *ap_ssid = "Xiaomi.Router";
static char *ap_key  = "stm32f215";
static mico_semaphore_t wait_sem;

void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
  udp_broadcast_log("Wlan Connection Err %d", err);
}

void micoNotify_WifiStatusHandler(WiFiEvent event, void* const inContext)
{
  switch (event) {
      case NOTIFY_STATION_UP:
        udp_broadcast_log("connected wlan success");
        mico_rtos_set_semaphore(&wait_sem);//wifi连接成功了，下面可以创建socket线程了
        break;
      case NOTIFY_STATION_DOWN:
        udp_broadcast_log("Station down");
        break;
  }
}


static void connect_ap( void )
{  
  network_InitTypeDef_adv_st wNetConfigAdv={0};
  strcpy((char*)wNetConfigAdv.ap_info.ssid, ap_ssid);/*ap ssid*/
  strcpy((char*)wNetConfigAdv.key, ap_key);/*ap password*/
  wNetConfigAdv.key_len = strlen(ap_key);
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;
  wNetConfigAdv.ap_info.channel = 0; /*Auto*/
  wNetConfigAdv.dhcpMode = DHCP_Client;
  wNetConfigAdv.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfigAdv);
}

/*create udp socket*/
void udp_broadcast_thread(void *inContext)
{
  OSStatus err;
  struct sockaddr_t my_addr={0};
  struct sockaddr_t to_addr={0};
  int udp_fd = -1 ;
  int opt = 1;
  int len = 0;
  /*Establish a UDP port to receive any data sent to this port*/
  udp_fd = socket( AF_INET, SOCK_DGRM, IPPROTO_UDP );
  
  /*allow broadcast*/
  //setsockopt(udp_fd,SOL_SOCKET,SO_BROADCAST,&opt,sizeof(opt));
  
  //my_addr.s_ip = INADDR_ANY;/*broadcast ip address*/
  //my_addr.s_port = udp_port;/*20000*/
  //err = bind(udp_fd, &my_addr, sizeof(my_addr));
  

  require_noerr( err, EXIT_THREAD );
  udp_broadcast_log("Start UDP broadcast mode, Open UDP port %d", udp_port);
  while(1)
  {
      char *data="broadcast data";
      to_addr.s_ip = INADDR_BROADCAST;/*broadcast ip address*/
      to_addr.s_port = udp_port;/*20000*/
      /*the receiver should bind at port=20000*/
      len=sendto(udp_fd, data, strlen(data), 0, &to_addr, sizeof(struct sockaddr_t));
      udp_broadcast_log("broadcast every 2s,len=%d",len);
      mico_thread_sleep(2);
  }
  
EXIT_THREAD:
  mico_rtos_delete_thread(NULL);
}

int application_start( void )
{
  OSStatus err = kNoErr;
  IPStatusTypedef para;
  MicoInit( );
   /*The notification message for the registered WiFi status change*/
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler );
  require_noerr( err, EXIT ); 
  err = MICOAddNotification( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler );
  require_noerr( err, EXIT );
  err = mico_rtos_init_semaphore(&wait_sem, 1);
  require_noerr( err, EXIT ); 
  connect_ap( );
  udp_broadcast_log("connecting to %s...", ap_ssid);
  
  /*wait forever until wifi connected successful*/
  mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);
  
  micoWlanGetIPStatus(&para, Station);
  udp_broadcast_log("udp server ip: %s,port=%d", para.ip, udp_port);
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "udp_broadcast", udp_broadcast_thread, 0x800, NULL );
  require_noerr_action( err, EXIT, udp_broadcast_log("ERROR: Unable to start the UDP echo thread.") );
  
  return err;
EXIT:
  udp_broadcast_log("ERROR, err: %d", err);
  return err;
}

