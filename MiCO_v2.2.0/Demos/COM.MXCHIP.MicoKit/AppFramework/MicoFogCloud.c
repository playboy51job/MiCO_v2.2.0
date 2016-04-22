/**
******************************************************************************
* @file    MicoFogCloud.c 
* @author  Eshen Wang
* @version V1.0.0
* @date    17-Mar-2015
* @brief   This file contains the implementations of cloud service interfaces 
*          for MICO.
operation
******************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
******************************************************************************
*/ 

#include "MICODefine.h"
#include "MICONotificationCenter.h"

#include "MicoFogCloud.h"
#include "fogcloud.h"
#include "FogCloudUtils.h"

#define fogcloud_log(M, ...) custom_log("MicoFogCloud", M, ##__VA_ARGS__)
#define fogcloud_log_trace() custom_log_trace("MicoFogCloud")


/*******************************************************************************
 *                                  DEFINES
 ******************************************************************************/
#ifdef ENABLE_FOGCLOUD_AUTO_ACTIVATE
  #define MAX_AUTO_ACTIVATE_RETRY_COUNTS    5
#endif

/*******************************************************************************
 *                                  VARIABLES
 ******************************************************************************/
//static mico_semaphore_t _wifi_station_on_sem = NULL;
mico_semaphore_t _fogcloud_connect_sem = NULL;


/*******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************/
extern OSStatus MicoStartFogCloudConfigServer ( mico_Context_t * const inContext );
extern void set_RF_LED_cloud_connected     ( mico_Context_t * const inContext );
extern void set_RF_LED_cloud_disconnected  ( mico_Context_t * const inContext );
extern void wait_for_wifi_info_delegate( mico_Context_t * const inContext );
extern void fogcloud_working_info_delegate( mico_Context_t * const inContext );
  
// override by user in user_main.c
WEAK OSStatus user_fogcloud_msg_handler(mico_Context_t* context, 
                            const char* topic, const unsigned int topicLen,
                            unsigned char *inBuf, unsigned int inBufLen)
{
  fogcloud_log("WARNING: user_fogcloud_msg_handler not override, use default, do nothing.");
  return kNoErr;
}

void fogNotify_WifiStatusHandler(WiFiEvent event, mico_Context_t * const inContext)
{
  fogcloud_log_trace();
  (void)inContext;
  switch (event) {
  case NOTIFY_STATION_UP:
    inContext->appStatus.isWifiConnected = true;
//    if(NULL == _wifi_station_on_sem){
//      mico_rtos_init_semaphore(&_wifi_station_on_sem, 1);
//    }
//    mico_rtos_set_semaphore(&_wifi_station_on_sem);
    break;
  case NOTIFY_STATION_DOWN:
    inContext->appStatus.isWifiConnected = false;
    break;
  case NOTIFY_AP_UP:
    break;
  case NOTIFY_AP_DOWN:
    break;
  default:
    break;
  }
  return;
}

#ifndef DISABLE_FOGCLOUD_OTA_CHECK
void fogcloud_ota_thread(void *arg)
{
  OSStatus err = kUnknownErr;
  MVDOTARequestData_t devOTARequestData;
  mico_Context_t *inContext = (mico_Context_t *)arg;
  
  fogcloud_log("OTA: check new firmware ...");
  memset((void*)&devOTARequestData, 0, sizeof(devOTARequestData));
  strncpy(devOTARequestData.loginId,
          inContext->flashContentInRam.appConfig.fogcloudConfig.loginId,
          MAX_SIZE_LOGIN_ID);
  strncpy(devOTARequestData.devPasswd,
          inContext->flashContentInRam.appConfig.fogcloudConfig.devPasswd,
          MAX_SIZE_DEV_PASSWD);
  strncpy(devOTARequestData.user_token,
          inContext->flashContentInRam.appConfig.fogcloudConfig.userToken,
          MAX_SIZE_USER_TOKEN);
  err = fogCloudDevFirmwareUpdate(inContext, devOTARequestData);
  if(kNoErr == err){
    if(inContext->appStatus.fogcloudStatus.RecvRomFileSize > 0){
      fogcloud_log("OTA: firmware download success, system will reboot && update...");
      // set bootloader to reboot && update app firmware
      mico_rtos_lock_mutex(&inContext->flashContentInRam_mutex);
      memset(&inContext->flashContentInRam.bootTable, 0, sizeof(boot_table_t));
      inContext->flashContentInRam.bootTable.length = inContext->appStatus.fogcloudStatus.RecvRomFileSize;
      inContext->flashContentInRam.bootTable.start_address = UPDATE_START_ADDRESS;
      inContext->flashContentInRam.bootTable.type = 'A';
      inContext->flashContentInRam.bootTable.upgrade_type = 'U';
      if(inContext->flashContentInRam.micoSystemConfig.configured != allConfigured)
        inContext->flashContentInRam.micoSystemConfig.easyLinkByPass = EASYLINK_SOFT_AP_BYPASS;
      MICOUpdateConfiguration(inContext);
      mico_rtos_unlock_mutex(&inContext->flashContentInRam_mutex);
      inContext->micoStatus.sys_state = eState_Software_Reset;
      if(inContext->micoStatus.sys_state_change_sem != NULL ){
        mico_rtos_set_semaphore(&inContext->micoStatus.sys_state_change_sem);
      }
      mico_thread_sleep(MICO_WAIT_FOREVER);
    }
    else{
      fogcloud_log("OTA: firmware is up-to-date!");
    }
  }
  else{
    fogcloud_log("OTA: firmware download failed, err=%d", err);
  }
  
  fogcloud_log("fogcloud_ota_thread exit err=%d.", err);
  mico_rtos_delete_thread(NULL);
  return;
}
#endif   // DISABLE_FOGCLOUD_OTA_CHECK

void fogcloud_main_thread(void *arg)
{
  OSStatus err = kUnknownErr;
  mico_Context_t *inContext = (mico_Context_t *)arg;
  
#ifdef ENABLE_FOGCLOUD_AUTO_ACTIVATE
  MVDActivateRequestData_t devDefaultActivateData;
  uint32_t auto_activate_retry_cnt = MAX_AUTO_ACTIVATE_RETRY_COUNTS;
#endif
  
  /* wait for station on */
  //fogcloud_log("MicoFogCloud start, wait for Wi-Fi...");
  //while(kNoErr != mico_rtos_get_semaphore(&_wifi_station_on_sem, MICO_WAIT_FOREVER));
  while(!inContext->appStatus.isWifiConnected){
    mico_thread_msleep(500);
  }
  
  /* start FogCloud service */
  err = fogCloudStart(inContext);
  require_noerr_action(err, exit, 
                       fogcloud_log("ERROR: MicoFogCloudCloudInterfaceStart failed!") );
  
  /* start configServer for fogcloud (server for activate/authorize/reset/ota cmd from user APP) */
  err = MicoStartFogCloudConfigServer( inContext);
  require_noerr_action(err, exit, 
                       fogcloud_log("ERROR: start FogCloud configServer failed!") );
  
 #ifdef ENABLE_FOGCLOUD_AUTO_ACTIVATE
  /* activate when wifi on */
  while(false == inContext->flashContentInRam.appConfig.fogcloudConfig.isActivated){
    if(0 == auto_activate_retry_cnt){
      fogcloud_log("device auto activate failed.");
      break;
    }
    // auto activate, using default login_id/dev_pass/user_token
    fogcloud_log("device activate start...");
    memset((void*)&devDefaultActivateData, 0, sizeof(devDefaultActivateData));
    strncpy(devDefaultActivateData.loginId,
            inContext->flashContentInRam.appConfig.fogcloudConfig.loginId,
            MAX_SIZE_LOGIN_ID);
    strncpy(devDefaultActivateData.devPasswd,
            inContext->flashContentInRam.appConfig.fogcloudConfig.devPasswd,
            MAX_SIZE_DEV_PASSWD);
    strncpy(devDefaultActivateData.user_token,
            inContext->micoStatus.mac,   // use MAC as default user_token
            MAX_SIZE_USER_TOKEN);
    err = fogCloudDevActivate(inContext, devDefaultActivateData);
    if(kNoErr == err){
      fogcloud_log("device activate success!");
    }
    else{
      auto_activate_retry_cnt--;
      fogcloud_log("device auto activate failed, err = %d, will retry in 3s ...", err);
    }
    mico_thread_sleep(3);
  }
  fogcloud_log("device is already activated.");
#endif  // ENABLE_FOGCLOUD_AUTO_ACTIVATE
  
#ifndef DISABLE_FOGCLOUD_OTA_CHECK
  /* OTA check just device activated */
  if( (!inContext->appStatus.noOTACheckOnSystemStart) && 
     (inContext->flashContentInRam.appConfig.fogcloudConfig.isActivated) ){
    // start ota thread
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "fogcloud_ota", 
                                  fogcloud_ota_thread, STACK_SIZE_FOGCLOUD_OTA_THREAD, 
                                  inContext);
    if(kNoErr != err){
      fogcloud_log("ERROR: start FogCloud OTA thread failed, err=%d.", err);
    }
  }
  inContext->appStatus.noOTACheckOnSystemStart = false;
#endif  // DISABLE_FOGCLOUD_OTA_CHECK
  
  while(1){
    mico_thread_sleep(1);
    if(inContext->appStatus.fogcloudStatus.isOTAInProgress){
      continue;  // ota is in progress, the oled && system led will be holding
    }
    
    if(inContext->appStatus.fogcloudStatus.isCloudConnected){
      set_RF_LED_cloud_connected(inContext);  // toggle LED
    }
    else{
      set_RF_LED_cloud_disconnected(inContext);  // stop LED blink
    }
  }
  
exit:
  fogcloud_log("fogcloud_main_thread exit err=%d.", err);
  mico_rtos_delete_thread(NULL);
  return;
}


/*******************************************************************************
 *                        FogCloud  interfaces init
 ******************************************************************************/
// reset default value
void MicoFogCloudRestoreDefault(mico_Context_t* const context)
{
  // reset all MicoFogCloud config params
  memset((void*)&(context->flashContentInRam.appConfig.fogcloudConfig), 
         0, sizeof(fogcloud_config_t));
  
  context->flashContentInRam.appConfig.fogcloudConfig.isActivated = false;
  sprintf(context->flashContentInRam.appConfig.fogcloudConfig.deviceId, DEFAULT_DEVICE_ID);
  sprintf(context->flashContentInRam.appConfig.fogcloudConfig.masterDeviceKey, DEFAULT_DEVICE_KEY);
  sprintf(context->flashContentInRam.appConfig.fogcloudConfig.romVersion, DEFAULT_ROM_VERSION);
  
  sprintf(context->flashContentInRam.appConfig.fogcloudConfig.loginId, DEFAULT_LOGIN_ID);
  sprintf(context->flashContentInRam.appConfig.fogcloudConfig.devPasswd, DEFAULT_DEV_PASSWD);
  sprintf(context->flashContentInRam.appConfig.fogcloudConfig.userToken, context->micoStatus.mac);
}

OSStatus MicoStartFogCloudService(mico_Context_t* const inContext)
{
  OSStatus err = kUnknownErr;
  
  //init MicoFogCloud status
  inContext->appStatus.fogcloudStatus.isCloudConnected = false;
  inContext->appStatus.fogcloudStatus.RecvRomFileSize = 0;
  inContext->appStatus.fogcloudStatus.isActivated = inContext->flashContentInRam.appConfig.fogcloudConfig.isActivated;
  inContext->appStatus.fogcloudStatus.isOTAInProgress = false;
  
  //init fogcloud service interface
  err = fogCloudInit(inContext);
  require_noerr_action(err, exit, 
                       fogcloud_log("ERROR: FogCloud interface init failed!") );
  
  // add wifi notify && semaphore
//  if(NULL == _wifi_station_on_sem){
//    err = mico_rtos_init_semaphore(&_wifi_station_on_sem, 1);
//    require_noerr_action(err, exit, 
//                         fogcloud_log("ERROR: mico_rtos_init_semaphore (_wifi_station_on_sem) failed!") );
//  }
  err = MICOAddNotification( mico_notify_WIFI_STATUS_CHANGED, (void *)fogNotify_WifiStatusHandler );
  require_noerr_action(err, exit, 
                       fogcloud_log("ERROR: MICOAddNotification (mico_notify_WIFI_STATUS_CHANGED) failed!") );
  
  // init fogcloud connect semaphore
  if(NULL == _fogcloud_connect_sem){
    err = mico_rtos_init_semaphore(&_fogcloud_connect_sem, 1);
    require_noerr_action(err, exit, 
                         fogcloud_log("ERROR: mico_rtos_init_semaphore (_fogcloud_connect_sem) failed!") );
  }
  
  // start MicoFogCloud main thread (dev reset && ota check, then start fogcloud service)
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "fogcloud_main", 
                                fogcloud_main_thread, STACK_SIZE_FOGCLOUD_MAIN_THREAD, 
                                inContext );
  
exit:
  return err;
}


/*******************************************************************************
*                            MicoFogCloud get state
*******************************************************************************/
// cloud connect state
bool MicoFogCloudIsConnect(mico_Context_t* const context)
{
  if(NULL == context){
    return false;
  }
  return context->appStatus.fogcloudStatus.isCloudConnected;
}

// device activate state
bool MicoFogCloudIsActivated(mico_Context_t* const context)
{
  if(NULL == context){
    return false;
  }
  return context->flashContentInRam.appConfig.fogcloudConfig.isActivated;
}

void mico_fogcloud_waitfor_connect(mico_Context_t* const context, uint32_t timeout_ms){
  if(NULL == _fogcloud_connect_sem){
    mico_rtos_init_semaphore(&_fogcloud_connect_sem, 1);
  }
  while(kNoErr != mico_rtos_get_semaphore(&_fogcloud_connect_sem, timeout_ms));
}


/*******************************************************************************
*                           FogCloud control interfaces
******************************************************************************/
//activate
OSStatus MicoFogCloudActivate(mico_Context_t* const context, 
                              MVDActivateRequestData_t activateData)
{
  OSStatus err = kUnknownErr;
  
  if(context->flashContentInRam.appConfig.fogcloudConfig.isActivated){
    // already activated, just do authorize
    err = fogCloudDevAuthorize(context, activateData);
    require_noerr_action(err, exit, 
                         fogcloud_log("ERROR: device authorize failed! err=%d", err) );
  }
  else {
    // activate
    err = fogCloudDevActivate(context, activateData);
    require_noerr_action(err, exit, 
                         fogcloud_log("ERROR: device activate failed! err=%d", err) );
  }
  return kNoErr;
  
exit:
  return err;
}

//authorize
OSStatus MicoFogCloudAuthorize(mico_Context_t* const context,
                               MVDAuthorizeRequestData_t authorizeData)
{
  OSStatus err = kUnknownErr;
  mico_Context_t *inContext = context;
  
  if(context->flashContentInRam.appConfig.fogcloudConfig.isActivated){
    err = fogCloudDevAuthorize(inContext, authorizeData);
    require_noerr_action(err, exit, 
                         fogcloud_log("ERROR: device authorize failed! err=%d", err) );
  }
  else{
    fogcloud_log("ERROR: device not activate!");
    err = kStateErr;
  }
  
exit:
  return err;
}

//OTA
OSStatus MicoFogCloudFirmwareUpdate(mico_Context_t* const context,
                                    MVDOTARequestData_t OTAData)
{
  OSStatus err = kUnknownErr;
  mico_Context_t *inContext = context;
  
  err = fogCloudDevFirmwareUpdate(inContext, OTAData);
  require_noerr_action(err, exit, 
                       fogcloud_log("ERROR: Firmware Update error! err=%d", err) );
  return kNoErr;
  
exit:
  return err;
}

//reset device info on cloud
OSStatus MicoFogCloudResetCloudDevInfo(mico_Context_t* const context,
                                       MVDResetRequestData_t devResetData)
{
  OSStatus err = kUnknownErr;
  mico_Context_t *inContext = context;
  
  err = fogCloudResetCloudDevInfo(inContext, devResetData);
  require_noerr_action(err, exit, 
                       fogcloud_log("ERROR: reset device info on cloud error! err=%d", err) );
  return kNoErr;
  
exit:
  return err;
}

//get state of the device( e.g. isActivate/isConnected)
OSStatus MicoFogCloudGetState(mico_Context_t* const context,
                              MVDGetStateRequestData_t getStateRequestData,
                              void* outDevState)
{
  //OSStatus err = kUnknownErr;
  mico_Context_t *inContext = context;
  json_object* report = (json_object*)outDevState;
  
  uint16_t login_id_cmp_len = 0;
  uint16_t dev_passwd_cmp_len = 0;
  
  if((NULL == context) || (NULL == outDevState)){
    return kParamErr;
  }
  
  // login_id/dev_passwd ok ?
  login_id_cmp_len = strlen(inContext->flashContentInRam.appConfig.fogcloudConfig.loginId) > strlen(getStateRequestData.loginId) ?
                     strlen(inContext->flashContentInRam.appConfig.fogcloudConfig.loginId) : strlen(getStateRequestData.loginId);
  dev_passwd_cmp_len = strlen(inContext->flashContentInRam.appConfig.fogcloudConfig.devPasswd) > strlen(getStateRequestData.devPasswd) ?
                       strlen(inContext->flashContentInRam.appConfig.fogcloudConfig.devPasswd) : strlen(getStateRequestData.devPasswd);
 
  if((0 != strncmp(context->flashContentInRam.appConfig.fogcloudConfig.loginId, 
                   getStateRequestData.loginId, login_id_cmp_len)) ||
     (0 != strncmp(context->flashContentInRam.appConfig.fogcloudConfig.devPasswd, 
                   getStateRequestData.devPasswd, dev_passwd_cmp_len))){
    fogcloud_log("ERROR: MVDGetState: loginId/devPasswd mismatch!");
    return kMismatchErr;
  }
  
  json_object_object_add(report, "isActivated",
                         json_object_new_boolean(inContext->flashContentInRam.appConfig.fogcloudConfig.isActivated)); 
  json_object_object_add(report, "isConnected",
                         json_object_new_boolean(inContext->appStatus.fogcloudStatus.isCloudConnected));
  json_object_object_add(report, "version",
                         json_object_new_string(inContext->flashContentInRam.appConfig.fogcloudConfig.romVersion));
  
  return kNoErr;
}


/*******************************************************************************
*                       MicoFogCloud message send interface
******************************************************************************/
// MCU => Cloud
// if topic is NULL, send to default topic: device_id/out,
// else send to sub-channel: device_id/out/<topic>
OSStatus MicoFogCloudMsgSend(mico_Context_t* const context, const char* topic, 
                                   unsigned char *inBuf, unsigned int inBufLen)
{
  fogcloud_log_trace();
  OSStatus err = kUnknownErr;
  
  err = fogCloudSendtoChannel(topic, inBuf, inBufLen);  // transfer raw data
  require_noerr_action( err, exit, fogcloud_log("ERROR: send to cloud error! err=%d", err) );
  return kNoErr;
  
exit:
  return err;
}


/*******************************************************************************
*                      MicoFogCloud message exchange protocol
******************************************************************************/
// handle cloud msg here, for example: send to USART or echo to cloud
OSStatus MicoFogCloudCloudMsgProcess(mico_Context_t* context, 
                                     const char* topic, const unsigned int topicLen,
                                     unsigned char *inBuf, unsigned int inBufLen)
{
  fogcloud_log_trace();
  OSStatus err = kUnknownErr;
  
  err = user_fogcloud_msg_handler(context, topic, topicLen, inBuf, inBufLen);
  
  return err;
}
