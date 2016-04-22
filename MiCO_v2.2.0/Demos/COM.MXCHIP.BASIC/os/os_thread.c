/**
******************************************************************************
* @file    os_thread.c 
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

void run1(void *arg)
{
  int i=0;
  while(1)
  {
       i++;
       os_thread_log("thread1 running,i=%d",i);
       mico_thread_sleep (1);
  }
}
void run2(void *arg)
{
  int j=0;
  while(1)
  {
       j++;
       os_thread_log("thread2 running,j=%d",j);
       mico_thread_sleep (1);
  }
}


int application_start( void )
{
  OSStatus err = kNoErr;
  mico_thread_t handle1;
  mico_thread_t handle2;
  /* Create new thread */
  err = mico_rtos_create_thread(&handle1, MICO_APPLICATION_PRIORITY, "t1", run1, 500, NULL);
  err = mico_rtos_create_thread(&handle2, MICO_APPLICATION_PRIORITY, "t2", run2, 500, NULL);
  mico_rtos_thread_join(&handle1);
  mico_rtos_thread_join(&handle2);
  os_thread_log( "t1 t2 exit now" );
  return kNoErr;  
}


