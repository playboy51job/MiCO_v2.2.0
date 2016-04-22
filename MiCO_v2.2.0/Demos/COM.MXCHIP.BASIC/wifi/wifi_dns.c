/**
******************************************************************************
* @file    wifi_dns.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   Get the IP address from a host name.(DNS)
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

#define wifi_dns_log(M, ...) custom_log("WIFI", M, ##__VA_ARGS__)

static char *domain = "www.baidu.com";
static char *ap_ssid = "Xiaomi.Router";
static char *ap_key  = "stm32f215";
static mico_semaphore_t wait_sem;

void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
  wifi_dns_log("Wlan Connection Err %d", err);
}
void micoNotify_WifiStatusHandler(WiFiEvent status, void* const inContext)
{
  switch (status) {
  case NOTIFY_STATION_UP:
    wifi_dns_log("Station up");
    mico_rtos_set_semaphore(&wait_sem);
    break;
  case NOTIFY_STATION_DOWN:
    wifi_dns_log("Station down");
    break;
  }
}

static void connect_ap( void )
{  
  network_InitTypeDef_adv_st wNetConfigAdv;
  memset(&wNetConfigAdv, 0x0, sizeof(network_InitTypeDef_adv_st));
  
  strcpy((char*)wNetConfigAdv.ap_info.ssid, ap_ssid);
  strcpy((char*)wNetConfigAdv.key, ap_key);
  wNetConfigAdv.key_len = strlen(ap_key);
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;
  wNetConfigAdv.ap_info.channel = 0; //Auto
  wNetConfigAdv.dhcpMode = DHCP_Client;
  wNetConfigAdv.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfigAdv);
}

int application_start( void )
{
  OSStatus err = kNoErr;
  char ipstr[16];
  
  MicoInit( );
  mico_rtos_init_semaphore(&wait_sem,1);
  
  /*The notification message for the registered WiFi status change*/
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler );
  require_noerr( err, exit ); 
  
  err = MICOAddNotification( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler );
  require_noerr( err, exit );
  
  connect_ap( );
  wifi_dns_log("connecting to %s...", ap_ssid);
  mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);
  wifi_dns_log("wifi connected successful");
  err = gethostbyname(domain, (uint8_t *)ipstr, 16);
  require_noerr(err, exit);
  wifi_dns_log("%s ip address is %s",domain, ipstr);
  mico_rtos_deinit_semaphore(&wait_sem);
  
exit:  
  return err;
}

