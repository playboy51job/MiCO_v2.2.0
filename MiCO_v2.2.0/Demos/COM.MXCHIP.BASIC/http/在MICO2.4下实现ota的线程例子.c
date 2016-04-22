
#include "MICO.h"
#include "ota.h"
#include "HTTPUtils.h"
#include "SocketUtils.h"
#include "StringUtils.h"
#include "CheckSumUtils.h"

#define http_client_log(M, ...) custom_log("HTTP", M, ##__VA_ARGS__)
//自定义的OTA缓冲区常数
#define UPDATE_START_ADDRESS 0x40000
#define UPDATE_END_ADDRESS   0x40000+0x70000
#define MICO_FLASH_FOR_UPDATE MICO_PARTITION_OTA_TEMP

void http_client_thread( void *inContext );
static OSStatus parseStreamReboot(int fd, HTTPHeader_t* inHeader, void * inUserContext);
static OSStatus onReceivedData(struct _HTTPHeader_t * httpHeader, uint32_t pos, uint8_t * data, size_t len, void * userContext);

//在局域网下测试用的环境常数
static char *ota_url = "demo.bin";
static char *http_server = "192.168.31.224";
static int http_port = 80;
static mico_mutex_t http_mutex;
static mico_Context_t *inContext;

typedef struct _configContext_t{
  uint32_t offset;
  bool     isFlashLocked;
  CRC16_Context crc16_contex;
} configContext_t;

const char http_format[]=
{
"GET /%s HTTP/1.1\r\n\
Host: %s\r\n\
Cache-Control: no-cache\r\n\r\n"
};

void http_client_thread( void *inContext )
{
  OSStatus err;
  int client_fd = -1;
  fd_set readfds;
  char ipstr[16];
  struct sockaddr_t addr;
  struct timeval_t t;
  char *httpRequest = NULL;
  HTTPHeader_t *httpHeader = NULL;
  configContext_t httpContext = {0, false};
  
  httpHeader = HTTPHeaderCreateWithCallback(512,onReceivedData, NULL, &httpContext);
  require_action( httpHeader, exit, err = kNoMemoryErr );
  HTTPHeaderClear( httpHeader );

  http_client_log("Free memory %d bytes", MicoGetMemoryInfo()->free_memory) ; 
  
  while(1) {
   err = gethostbyname(http_server, (uint8_t *)ipstr, 16);
   require_noerr(err, ReConnWithDelay);
   http_client_log("server address: host:%s, ip: %s", http_server, ipstr);
   break;

 ReConnWithDelay:
   mico_thread_sleep(1);
  }
   
  client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  
  addr.s_ip = inet_addr(ipstr);
  addr.s_port = http_port;
  
  err = connect(client_fd, &addr, sizeof(addr)); 
  require_noerr_action(err, exit, http_client_log("connect http server failed"));

  http_client_log("connect http server success, fd %d", client_fd);
  
  httpRequest = malloc(256);
  sprintf(httpRequest, http_format, ota_url, http_server);// 
  http_client_log("http string=%s",httpRequest);
  err = SocketSend( client_fd, (uint8_t *)httpRequest, strlen(httpRequest) );
  require_noerr( err, exit );
  free(httpRequest);
  
  t.tv_sec = 10;
  t.tv_usec = 0;
 
  mico_rtos_init_mutex(&http_mutex);//初始化互斥锁
  while(1){
     FD_ZERO(&readfds);
     FD_SET(client_fd, &readfds);
     select(1, &readfds, NULL, NULL, &t);
     if(FD_ISSET(client_fd,&readfds))
     {
        err = SocketReadHTTPHeader( client_fd, httpHeader );
        switch ( err )
        {
          case kNoErr:
            err = SocketReadHTTPBody( client_fd, httpHeader );// 
            err = parseStreamReboot( client_fd, httpHeader, &httpContext);
            require_noerr(err, exit);
           // Reuse HTTPHeader
           HTTPHeaderClear( httpHeader );
           break;
        case EWOULDBLOCK:
            // NO-OP, keep reading
           break;
        case kNoSpaceErr:
        case kConnectionErr:
        default:
          http_client_log("ERROR: HTTP Header parse internal error: %d", err);
          goto exit;
      }
    }
  }

exit:
  http_client_log("Exit: Client exit with err = %d, fd:%d", err, client_fd);
  SocketClose(&client_fd);
  if(httpHeader) {
    HTTPHeaderClear( httpHeader );
    free(httpHeader);
  }
  mico_rtos_delete_thread(NULL);
  return;
}

static OSStatus onReceivedData(struct _HTTPHeader_t * inHeader, uint32_t inPos, uint8_t * inData, size_t inLen, void * inUserContext )
{
  OSStatus err = kNoErr;
  const char *value;
  size_t valueSize;
  configContext_t *context = (configContext_t *)inUserContext;
  
  //http_client_log("download data coming");
  if(inLen == 0)
  {
    http_client_log("inLen=%d",inLen);
    return err;
  }
  mico_logic_partition_t* ota_partition = MicoFlashGetInfo( MICO_PARTITION_OTA_TEMP );
  err = HTTPGetHeaderField( inHeader->buf, inHeader->len, "Content-Type", NULL, NULL, &value, &valueSize, NULL );
  
  if(err == kNoErr && strnicmpx( value, valueSize, kMIMEType_Stream ) == 0){
    http_client_log("OTA data %d, %d to %x", inPos, inLen, context->offset); 
    if(inPos == 0)
    {
       context->offset = 0x0;
       CRC16_Init( &context->crc16_contex );
       mico_rtos_lock_mutex(&http_mutex); // the Flash content need to be written, no other write is possible
       context->isFlashLocked = true;
       err = MicoFlashErase( MICO_PARTITION_OTA_TEMP, 0x0, ota_partition->partition_length);
       require_noerr(err, flashErrExit);
       err = MicoFlashWrite( MICO_PARTITION_OTA_TEMP, &context->offset, (uint8_t *)inData, inLen);
       require_noerr(err, flashErrExit);
       CRC16_Update( &context->crc16_contex, inData, inLen);
    }
    else
    {
       err = MicoFlashWrite( MICO_PARTITION_OTA_TEMP, &context->offset, (uint8_t *)inData, inLen);
       require_noerr(err, flashErrExit);
       CRC16_Update( &context->crc16_contex, inData, inLen);
    }
  }
  else{
    return kUnsupportedErr;
  }

  if(err!=kNoErr)  http_client_log("onReceivedData");
  return err;
  
flashErrExit:
  http_client_log("err==%d!!!!!!",err);
  mico_rtos_unlock_mutex(&http_mutex);
  return err;
}


OSStatus parseStreamReboot(int fd, HTTPHeader_t* inHeader, void * inUserContext)
{
  uint16_t crc;
  OSStatus err = kUnknownErr;
  const char *value;
  size_t valueSize;
  //configContext_t *context = (configContext_t *)inUserContext;
  configContext_t *http_context = (configContext_t *)inHeader->userContext;
  err = HTTPGetHeaderField( inHeader->buf, inHeader->len, "Content-Type", NULL, NULL, &value, &valueSize, NULL );
  mico_logic_partition_t* ota_partition = MicoFlashGetInfo( MICO_PARTITION_OTA_TEMP );
  if(err == kNoErr && strnicmpx( value, valueSize, kMIMEType_Stream ) == 0){
    if(inHeader->contentLength > 0){
      
      http_client_log("Receive OTA data!");
      mico_rtos_unlock_mutex(&http_mutex);
      CRC16_Final( &http_context->crc16_contex, &crc);
      inContext= mico_system_context_get( );
      memset(&inContext->flashContentInRam.bootTable, 0, sizeof(boot_table_t));
      inContext->flashContentInRam.bootTable.length = inHeader->contentLength;
      inContext->flashContentInRam.bootTable.start_address = ota_partition->partition_start_addr;
      inContext->flashContentInRam.bootTable.type = 'A';
      inContext->flashContentInRam.bootTable.upgrade_type = 'U';
      inContext->flashContentInRam.bootTable.crc = crc;
      if( inContext->flashContentInRam.micoSystemConfig.configured != allConfigured )
        inContext->flashContentInRam.micoSystemConfig.easyLinkByPass = EASYLINK_SOFT_AP_BYPASS;
      
      mico_system_context_update( mico_system_context_get( ));//更新固件
      mico_system_power_perform( inContext, eState_Software_Reset );
      mico_thread_sleep( MICO_WAIT_FOREVER );
    }
  
    goto exit;
  }
  else{
    return kNotFoundErr;
  };

 exit:
  if(inHeader->persistent == false)  //Return an err to close socket and exit the current thread
    err = kConnectionErr;

  return err;

}



