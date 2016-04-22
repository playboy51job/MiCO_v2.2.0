/**
******************************************************************************
* @file    os_mutex.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   MiCO RTOS thread control demo.
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

#include "MiCO.h" 

#define os_thread_log(M, ...) custom_log("OS", M, ##__VA_ARGS__)

int g_sold=0;//sold tickets
int g_tickets=1000;//remain tickets
mico_mutex_t  mutex;

void run(void *arg)
{
  char *name=(char*)arg;
  while(1)
  {
       mico_rtos_lock_mutex(&mutex);
       if(g_tickets<=0)
       {
            os_thread_log("thread name=%s die now",name);
            mico_rtos_unlock_mutex(&mutex);
            mico_rtos_delete_thread(NULL);
            break;
       } 
       
       g_sold++;
       g_tickets--;
       os_thread_log("name=%s,g_sold=%d,g_tickets=%d",name,g_sold,g_tickets);
       mico_rtos_unlock_mutex(&mutex);

  } 
}

int application_start( void )
{
  OSStatus err = kNoErr;
  mico_thread_t handle1;
  mico_thread_t handle2;
  /* Create a new thread */
  char *p_name1="t1";
  char *p_name2="t2";
  mico_rtos_init_mutex(&mutex);
  err = mico_rtos_create_thread(&handle1, MICO_APPLICATION_PRIORITY, "t1", run, 0x800, (void*)p_name1);
  err = mico_rtos_create_thread(&handle2, MICO_APPLICATION_PRIORITY, "t2", run, 0x800, (void*)p_name2);
  mico_rtos_thread_join(&handle1);
  mico_rtos_thread_join(&handle2);
  mico_rtos_deinit_mutex(&mutex );
  os_thread_log( "t1 t2 exit now" );
  return kNoErr;  
}


