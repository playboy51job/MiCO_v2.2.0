/**
  ******************************************************************************
  * @file    user_config.h 
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    17-Mar-2015
  * @brief   This file contains user config for app.
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

#ifndef __USER_CONFIG_H_
#define __USER_CONFIG_H_


/*******************************************************************************
 *                              APP INFO
 ******************************************************************************/
#define APP_INFO                           "MicoKit3288 Demo based on MICO OS"

#define FIRMWARE_REVISION                  "MK3288@1506101455"
#define SERIAL_NUMBER                      "20150610"
#define PROTOCOL                           "com.mxchip.micokit"

/*User provided configurations*/
#define CONFIGURATION_VERSION               0x00000001 // if default configuration is changed, update this number
   
/* product type */
#define PRODUCT_ID                         "d64f517c"
#define PRODUCT_KEY                        "e935ef56-1d03-4432-9524-8d4a691a26ec"
#define DEFAULT_ROM_VERSION                FIRMWARE_REVISION
#define DEFAULT_DEVICE_NAME                "MiCOKit-3288"  // device name upload to cloud
#define DEFAULT_MANUFACTURER               "MXCHIP"       // device manufacturer


/*******************************************************************************
 *                             CONNECTING
 ******************************************************************************/
/* Wi-Fi configuration mode */
#define MICO_CONFIG_MODE                   CONFIG_MODE_EASYLINK_WITH_SOFTAP

/* MICO cloud service type */
#define MICO_CLOUD_TYPE                    CLOUD_FOGCLOUD

// disalbe FogCloud OTA check when system start
//#define DISABLE_FOGCLOUD_OTA_CHECK


/*******************************************************************************
 *                             RESOURCES
 ******************************************************************************/
#define STACK_SIZE_USER_MAIN_THREAD         0x800
#define STACK_SIZE_NOTIFY_THREAD            0x800
#define MICO_PROPERTIES_NOTIFY_INTERVAL_MS  1000
   
   
#endif  // __USER_CONFIG_H_
