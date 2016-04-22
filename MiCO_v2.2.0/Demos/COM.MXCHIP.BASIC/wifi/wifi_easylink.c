/**
******************************************************************************
* @file    wifi_easylink.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   easylink demo£¨get ssid and key from app£©
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
#include "StringUtils.h"
#include "MICONotificationCenter.h"

#define wifi_easylink_log(M, ...) custom_log("WIFI", M, ##__VA_ARGS__)

/*need to get ssid and key from app*/
static char ap_ssid[64], ap_key[32];
char ipstr[16]={0};
char *domain="www.baidu.com";
static mico_semaphore_t      easylink_sem;
static mico_semaphore_t      wifi_sem;
bool isEasyLinkSuccess = false;


void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
  wifi_easylink_log("Wlan Connection Err %d", err);
}
void micoNotify_WifiStatusHandler(WiFiEvent status, void* const inContext)
{
  switch (status) {
    case NOTIFY_STATION_UP:
      wifi_easylink_log("Station up,wifi is connected now");
      mico_rtos_set_semaphore(&wifi_sem);
      break;
    case NOTIFY_STATION_DOWN:
      wifi_easylink_log("Station down,wifi is not connected");
      break;
  }
}


void EasyLinkNotify_EasyLinkCompleteHandler(network_InitTypeDef_st *nwkpara, const int inContext)
{
  OSStatus err;
  wifi_easylink_log("EasyLink return");
  require_action(nwkpara, exit, err = kTimeoutErr);/*error*/
  strcpy(ap_ssid, nwkpara->wifi_ssid);/*get ssid and key*/
  strcpy(ap_key, nwkpara->wifi_key);
  wifi_easylink_log("Get SSID: %s, Key: %s", nwkpara->wifi_ssid, nwkpara->wifi_key);
  isEasyLinkSuccess=true;/*successfully get ssid and key*/
  return;
  
exit:
  wifi_easylink_log("ERROR, err: %d", err);
  mico_rtos_set_semaphore(&easylink_sem);
}

void EasyLinkNotify_EasyLinkGetExtraDataHandler(int datalen, char* data, void* inContext)
{
     wifi_easylink_log("get extra data=%s",data);
     mico_rtos_set_semaphore(&easylink_sem);
}

static void connect_ap( void )
{  
  network_InitTypeDef_adv_st wNetConfigAdv={0};
  strcpy((char*)wNetConfigAdv.ap_info.ssid, ap_ssid);
  strcpy((char*)wNetConfigAdv.key, ap_key);
  wNetConfigAdv.key_len = strlen(ap_key);
  wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;
  wNetConfigAdv.ap_info.channel = 0; /*Auto*/
  wNetConfigAdv.dhcpMode = DHCP_Client;
  wNetConfigAdv.wifi_retry_interval = 100;
  micoWlanStartAdv(&wNetConfigAdv);
  wifi_easylink_log("connecting to %s...", wNetConfigAdv.ap_info.ssid);
}

void easylink_thread(void *inContext)
{
  micoWlanStartEasyLinkPlus(20);/*ready for app to send data to config ssid and key*/
  wifi_easylink_log("Start Easylink configuration,app start to configure now");
  mico_rtos_get_semaphore(&easylink_sem,MICO_WAIT_FOREVER);
  if(isEasyLinkSuccess==true)
  {
      wifi_easylink_log("easylink sucess");
      connect_ap();
      mico_rtos_get_semaphore(&wifi_sem,MICO_WAIT_FOREVER);
      gethostbyname(domain, (uint8_t *)ipstr, 16);
      wifi_easylink_log("www.baidu.com, ip=%s",ipstr);
  }
  else
  {
      wifi_easylink_log("easylink failed,let app start easylink mode");
  }
   mico_rtos_delete_thread(NULL);
}

int application_start( void )
{
  OSStatus err = kNoErr;
  MicoInit( );
  
  /*The notification message for the registered WiFi status change*/
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler );
  require_noerr( err, exit ); 
  
  err = MICOAddNotification( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler );
  require_noerr( err, exit );

  err = MICOAddNotification( mico_notify_EASYLINK_WPS_COMPLETED, (void *)EasyLinkNotify_EasyLinkCompleteHandler );
  require_noerr(err, exit);
  
  err = MICOAddNotification( mico_notify_EASYLINK_GET_EXTRA_DATA, (void *)EasyLinkNotify_EasyLinkGetExtraDataHandler );
  require_noerr(err, exit);
  
  /*Start the EasyLink thread*/
  mico_rtos_init_semaphore(&easylink_sem, 1);/*wait easylink*/
  mico_rtos_init_semaphore(&wifi_sem,1);/*wait wifi to be connect*/
  
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "EASYLINK", easylink_thread, 0x800, NULL );
  require_noerr_action( err, exit, wifi_easylink_log("ERROR: Unable to start the EasyLink thread.") );
  
  return err;

exit:
  wifi_easylink_log("ERROR, err: %d", err);
  return err;
}

