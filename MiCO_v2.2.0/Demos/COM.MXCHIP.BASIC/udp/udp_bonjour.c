/**
******************************************************************************
* @file    udp_bonjour.c 
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

#include "MicoDefine.h"
#include "platform_config.h"
#include "MICONotificationCenter.h"
#include "MDNSUtils.h"
#include "StringUtils.h"

#define udp_bonjour_log(M, ...) custom_log("UDP", M, ##__VA_ARGS__)

static char *ap_ssid = "Xiaomi.Router";
static char *ap_key  = "stm32f215";
static mico_semaphore_t wait_sem;

void micoNotify_ConnectFailedHandler(OSStatus err, void* const inContext)
{
  udp_bonjour_log("Wlan Connection Err %d", err);
}
void micoNotify_WifiStatusHandler(WiFiEvent status, void* const inContext)
{
  switch (status) {
    case NOTIFY_STATION_UP:
      udp_bonjour_log("Station up");
      mico_rtos_set_semaphore(&wait_sem);
      break;
    case NOTIFY_STATION_DOWN:
      udp_bonjour_log("Station down");
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

void my_bonjour_server( WiFi_Interface interface )
{
  char *temp_txt= (char*)malloc(500);
  char *temp_txt2;
  IPStatusTypedef para;
  
  bonjour_init_t init={0};
  micoWlanGetIPStatus(&para, Station);

  udp_bonjour_log("mac is %s,ip=%s",para.mac,para.ip);
  init.service_name = BONJOUR_SERVICE;/*"_easylink._tcp.local."*/

  /*format*/
  /*   name#xxxxxx.local.  */
  snprintf( temp_txt, 100, "%s#%c%c%c%c%c%c.local.", BONJOURNANE, 
                                                     para.mac[6],   para.mac[7], \
                                                     para.mac[8],   para.mac[9], \
                                                     para.mac[10],  para.mac[11]  );
  init.host_name = (char*)__strdup(temp_txt);

  /*   name#xxxxxx.   */
  snprintf( temp_txt, 100, "%s#%c%c%c%c%c%c",        BONJOURNANE, 
                                                     para.mac[6],   para.mac[7], \
                                                     para.mac[8],   para.mac[9], \
                                                     para.mac[10],  para.mac[11]   );
  init.instance_name = (char*)__strdup(temp_txt);

  init.service_port = LOCAL_PORT;
  init.interface = interface;

  //take some data
  /*for example,modify here*/
  temp_txt2 = __strdup_trans_dot("MyTestMac");
  //temp_txt2 = __strdup_trans_dot(para.mac);

  sprintf(temp_txt, "MAC=%s.", temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(FIRMWARE_REVISION);
  sprintf(temp_txt, "%sFirmware Rev=%s.", temp_txt, temp_txt2);
  free(temp_txt2);
  
  temp_txt2 = __strdup_trans_dot(HARDWARE_REVISION);
  sprintf(temp_txt, "%sHardware Rev=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(MicoGetVer());
  sprintf(temp_txt, "%sMICO OS Rev=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(MODEL);
  sprintf(temp_txt, "%sModel=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(PROTOCOL);
  sprintf(temp_txt, "%sProtocol=%s.", temp_txt, temp_txt2);
  free(temp_txt2);

  temp_txt2 = __strdup_trans_dot(MANUFACTURER);
  sprintf(temp_txt, "%sManufacturer=%s.", temp_txt, temp_txt2);
  free(temp_txt2);
  
  /*printf*/
  udp_bonjour_log("TXT RECORD=%s",temp_txt);
  
  /*
  TXT RECORD=MAC=c89346918152.
  Firmware Rev=MICO_BASE_1_0.
  Hardware Rev=MK3288_1.
  MICO OS Rev=10880002/.032.
  Model=MiCOKit-3288.
  Protocol=com/.mxchip/.basic.
  Manufacturer=MXCHIP Inc/..
  */
  /*take some data in txt_record*/
  init.txt_record = (char*)__strdup(temp_txt);

  bonjour_service_init(init);

  /*free memeoy*/
  free(init.host_name);
  free(init.instance_name);
  free(init.txt_record);

  /*mDNS+DNS-sd*/
  start_bonjour_service( );
 
  if(temp_txt) 
    free(temp_txt);
}

int application_start( void )
{
  OSStatus err = kNoErr;
  IPStatusTypedef para;
  udp_bonjour_log("udp bonjour demo");
  MicoInit( );
  
   /*The notification message for the registered WiFi status change*/
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler );
  require_noerr( err, exit ); 
  
  err = MICOAddNotification( mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler );
  require_noerr( err, exit );
  
  err = mico_rtos_init_semaphore(&wait_sem, 1);
  require_noerr( err, exit ); 
  
  connect_ap( );
  udp_bonjour_log("connecting to %s...", ap_ssid);
  
  /*waiting for wifi connected successful*/
  mico_rtos_get_semaphore(&wait_sem, MICO_WAIT_FOREVER);
  /*registered bonjour server*/
  my_bonjour_server( Station );
  return err;

exit:
  udp_bonjour_log("ERROR, err: %d", err);
  return err;
}

