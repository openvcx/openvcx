#ifndef __SERVER_LIMITS_H__
#define __SERVER_LIMITS_H__



//
// Maximum unique (http|https://address:port) listeners 
//
#define SRV_LISTENER_MAX          4 
#define SRV_LISTENER_MAX_HTTP     SRV_LISTENER_MAX 
#define SRV_LISTENER_MAX_RTMP     SRV_LISTENER_MAX 
#define SRV_LISTENER_MAX_RTSP     SRV_LISTENER_MAX 




#endif // __SERVER_LIMITS_H__
