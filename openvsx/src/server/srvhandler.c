/** <!--
 *
 *  Copyright (C) 2014 OpenVCX openvcx@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like this software to be made available to you under an 
 *  alternate license please email openvcx@gmail.com for more information.
 *
 * -->
 */


#include "vsx_common.h"

#if defined(VSX_HAVE_SERVERMODE)

static int validatePwd(CLIENT_CONN_T *pConn,
                 enum HTTP_STATUS *pHttpStatus,
                 unsigned char **ppout, unsigned int *plenout,
                 int sendresp) {
  const char *parg;
  char tmp[128];
  int rc = 0;

  //
  // Check 'uipwd'
  //
  if(pConn->pCfg->uipwd &&
     (!(parg = conf_find_keyval(pConn->httpReq.uriPairs, "pass")) ||
      strcmp(pConn->pCfg->uipwd, parg))) {

    rc = -1;

    LOG(X_WARNING("Invalid password from %s:%d%s (%d char given)"),
         FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)),
         pConn->httpReq.puri, parg ? strlen(parg) : 0);

    if(pHttpStatus && ppout && plenout) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
      *ppout = (unsigned char *) HTTP_STATUS_STR_FORBIDDEN;
      *plenout = strlen((char *) *ppout);
    } else if(sendresp) {
      http_resp_send(&pConn->sd, &pConn->httpReq,
                          HTTP_STATUS_FORBIDDEN,
                          (unsigned char *) HTTP_STATUS_STR_FORBIDDEN,
                          strlen(HTTP_STATUS_STR_FORBIDDEN));
    }
  }

  return rc;
}

#endif // VSX_HAVE_SERVERMODE


void srv_cmd_proc(void *pfuncarg) {
  CLIENT_CONN_T *pConn = (CLIENT_CONN_T *) pfuncarg;
  int rc = 0;
  //int forbidden = 0;
  char buf[1024];
  char authbuf[AUTH_BUF_SZ];
  char tmp[128];
  const char *pauthbuf = NULL;
  struct stat st;
  char *p;
  unsigned int urilen;
  //SESSION_DESCR_T *pSession = NULL;
  HTTP_STATUS_T httpStatus = HTTP_STATUS_OK;
#if defined(VSX_HAVE_SERVERMODE)
  int rcout;
  char path[VSX_MAX_PATH_LEN];
  unsigned int lenout = 0;
#endif // VSX_HAVE_SERVERMODE
  enum URL_CAPABILITY capability = pConn->pListenCfg->urlCapabilities;

  LOG(X_DEBUG("Handling %sconnection on port %d from %s:%d"), 
              ((pConn->sd.netsocket.flags & NETIO_FLAG_SSL_TLS) ? "SSL " : ""),
              ntohs(INET_PORT(pConn->pListenCfg->sa)),
              FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

  do {

    VSX_DEBUG_HTTP( LOG(X_DEBUGV("HTTP - srv_cmd_proc calling http_req_readparse...")))

    //
    // Get the HTTP request
    // 
    if((rc = http_req_readparse(&pConn->sd, &pConn->httpReq, HTTP_REQUEST_TIMEOUT_SEC, 1)) <= 0) {
      if(rc < 0 && NETIOSOCK_FD(pConn->sd.netsocket) != INVALID_SOCKET) {
        //fprintf(stderr, "SENDING BAD REQUEST>..baad rc:%d\n", rc);
        rc = http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_BADREQUEST, 1, NULL, NULL);
      }

      break;
    }

    VSX_DEBUG_HTTP( LOG(X_DEBUG("HTTP - srv_cmd_proc method: '%s', rc:%d, URI: %s%s"), 
           pConn->httpReq.method, rc, pConn->httpReq.puri, http_req_dump_uri(&pConn->httpReq, buf, sizeof(buf))));


/*
    if(!(phdrcookie = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.reqPairs, "Cookie"))) {
      if((pSession = session_add(pConn->pSessionCache,
                                 &pConn->sd.sain.sin_addr)) == NULL) {
        LOG(X_WARNING("Unable to create new HTTP session"));
      } else {
        snprintf(pConn->httpReq.cookie, sizeof(pConn->httpReq.cookie),
           "id=%s; Max-Age=60;", pSession->cookie); 
      }
    } else {
      pSession = session_find(pConn->pSessionCache, phdrcookie);
    }

fprintf(stderr, "COOKIE: '%s' 0x%x\n", phdrcookie, pSession);
*/

#if defined(VSX_HAVE_SERVERMODE)

    //fprintf(stderr, "REQUEST URI: '%s' method:'%s' from %s:%d\n", pConn->httpReq.puri, pConn->httpReq.method, inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port));

    p = pConn->httpReq.puri;
    while(*p == '/') {
      p++;
    }
    while(*p != '\0' && *p != '/' && *p != '?' && *p != '&') {
      p++;
    }

    urilen = p - pConn->httpReq.puri;
    httpStatus = HTTP_STATUS_OK;
    rc = 0;
    pauthbuf = NULL;

    if((pauthbuf = srv_check_authorized(pConn->pListenCfg->pAuthStore, &pConn->httpReq, authbuf))) {

      httpStatus = HTTP_STATUS_UNAUTHORIZED;

    //
    // Handle VSX_ROOT_URL   /
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_ROOT_URL, 2)) {

      // Handle form submission
      if(!strcasecmp(pConn->httpReq.method, "POST")) {

        if(capability & URL_CAP_POST) {
          if((rc = srv_ctrl_submitdata(pConn)) < 0) {
            rc = http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_SERVERERROR, 0, NULL, NULL);
            break;
          }
        } else {
          httpStatus = HTTP_STATUS_FORBIDDEN;
        }

      } else if(strcmp(pConn->httpReq.method, HTTP_METHOD_GET) && 
                strcmp(pConn->httpReq.method, HTTP_METHOD_HEAD)) {
        httpStatus = HTTP_STATUS_METHODNOTALLOWED;
        rc = -1;
      }

      if(rc >= 0 && httpStatus == HTTP_STATUS_OK) {
        //
        // '/' root level access is only available via vsxmgr
        //
        if(capability & URL_CAP_ROOTHTML &&
           (rc = validatePwd(pConn, NULL, NULL, NULL, 0)) == 0) {

          mediadb_prepend_dir(pConn->pCfg->pMediaDb->homeDir, 
                              VSX_INDEX_HTML, path, sizeof(path));

          if(fileops_stat(path, &st) == 0) {

            rc = http_resp_sendfile(pConn, &httpStatus, path, CONTENT_TYPE_TEXTHTML, HTTP_ETAG_AUTO);

          } else { 

            httpStatus = HTTP_STATUS_NOTFOUND;
            LOG(X_ERROR("Unable to read index file from '%s'"), path);
            rc = snprintf(buf, sizeof(buf), "Unable to read %s.  "
                      "Ensure 'home=' is set to the root path", path);
            rc = http_resp_send(&pConn->sd, &pConn->httpReq, 
                         httpStatus, (unsigned char *) buf, rc);
          }

          break;

        } else {
          httpStatus = HTTP_STATUS_FORBIDDEN;
        }
      }

    }

    if(httpStatus != HTTP_STATUS_OK) {
    //
    // From here on, only valid GET or HEAD is allowed
    //
    } else if(strcmp(pConn->httpReq.method, HTTP_METHOD_GET) && 
              strcmp(pConn->httpReq.method, HTTP_METHOD_HEAD)) {

      httpStatus = HTTP_STATUS_METHODNOTALLOWED;
    
    } else 

    //
    // Handle VSX_LIST_URL   /list
    //
    if(!strncasecmp(pConn->httpReq.puri, VSX_LIST_URL, strlen(VSX_LIST_URL) + 1)) {

      if((capability & URL_CAP_LIST) &&
         (rc = validatePwd(pConn, NULL, NULL, NULL, 0)) == 0) {
        rc = srv_ctrl_listmedia(pConn, &httpStatus);

      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }
#if 0
    //
    // Handle VSX_PROP_URL    /prop //TODO: deprecated
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_PROP_URL, strlen(VSX_PROP_URL) + 1)) {

      if((capability & URL_CAP_PROP) &&
         (rc = validatePwd(pConn, NULL, NULL, NULL, 0)) == 0) {

        rc = srv_ctrl_prop(pConn);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }
#endif // 0
    //
    // Handle VSX_IMG_URL  /img 
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_IMG_URL, strlen(VSX_IMG_URL) )) {

      if(capability & URL_CAP_IMG) {
        rc = srv_ctrl_loadimg(pConn, &httpStatus);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_TMN_URL   /tmn (should only be used by vsxmgr)
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_TMN_URL, strlen(VSX_TMN_URL) )) {

      if(capability & URL_CAP_TMN) {
        rc = srv_ctrl_loadtmn(pConn);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_MEDIA_URL   /media (should only be used by vsxmgr)
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_MEDIA_URL, strlen(VSX_MEDIA_URL) + 1)) {

      if((capability & URL_CAP_FILEMEDIA) &&
         (rc = validatePwd(pConn, NULL, NULL, NULL, 0)) == 0) {
        rc = srv_ctrl_loadmedia(pConn, NULL);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    } else  

#endif // VSX_HAVE_SERVERMODE

    //
    // Handle VSX_TSLIVE_URL   /tslive
    //
    // Do not compare strlen + 1 (NULL char) because '/tslive[/2]' can be used to select
    // output specific stream
    //
    if(!strncasecmp(pConn->httpReq.puri, VSX_TSLIVE_URL, MAX(urilen, strlen(VSX_TSLIVE_URL)))) {

      if(capability & URL_CAP_TSLIVE) {
        rc = srv_ctrl_tslive(pConn, &httpStatus);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_HTTPLIVE_URL   /httplive
    // http://tools.ietf.org/html/draft-pantos-http-live-streaming-04
    //
    // Do not compare strlen + 1 (NULL char) because '/httplive[/2]' can be used to select
    // output specific stream
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_HTTPLIVE_URL, MAX(urilen, strlen(VSX_HTTPLIVE_URL)))) {

      if(capability & (URL_CAP_TSHTTPLIVE | URL_CAP_LIVE)) {
        rc = srv_ctrl_httplive(pConn, VSX_HTTPLIVE_URL, NULL, NULL, &httpStatus);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_DASH_URL   /dash
    //
    // Do not compare strlen + 1 (NULL char) because '/dash[/2]' can be used to select
    // output specific stream
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_DASH_URL, MAX(urilen, strlen(VSX_DASH_URL)))) {

      if(capability & (URL_CAP_MOOFLIVE | URL_CAP_LIVE)) {
        rc = srv_ctrl_mooflive(pConn, VSX_DASH_URL, NULL, NULL, &httpStatus);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_FLVLIVE_URL   /flvlive
    //
    // Do not compare strlen + 1 (NULL char) because '/flvlive[/2]' can be used to select
    // output specific stream
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_FLVLIVE_URL, MAX(urilen, strlen(VSX_FLVLIVE_URL)))) {

      if(capability & URL_CAP_FLVLIVE) {
        rc = srv_ctrl_flvlive(pConn);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_MKVLIVE_URL   /mkvlive
    //
    // Do not compare strlen + 1 (NULL char) because '/mkvlive[/2]' can be used to select
    // output specific stream
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_MKVLIVE_URL, MAX(urilen, strlen(VSX_MKVLIVE_URL)))) {

      if(capability & URL_CAP_MKVLIVE) {
        rc = srv_ctrl_mkvlive(pConn);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_LIVE_URL   /live
    //
    // Do not compare strlen + 1 (NULL char) because '/live[/2]' can be used to select
    // output specific stream
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_LIVE_URL, MAX(urilen, strlen(VSX_LIVE_URL))) ||
          !strncasecmp(pConn->httpReq.puri, VSX_FLV_URL, MAX(urilen, strlen(VSX_FLV_URL))) ||
          !strncasecmp(pConn->httpReq.puri, VSX_RTSP_URL, MAX(urilen, strlen(VSX_RTSP_URL))) ||
          !strncasecmp(pConn->httpReq.puri, VSX_RTMP_URL, MAX(urilen, strlen(VSX_RTMP_URL))) ||
          !strncasecmp(pConn->httpReq.puri, VSX_RSRC_URL, MAX(urilen, strlen(VSX_RSRC_URL))) ||
          !strncasecmp(pConn->httpReq.puri, VSX_WEBM_URL, MAX(urilen, strlen(VSX_WEBM_URL))) ||
          !strncasecmp(pConn->httpReq.puri, VSX_MKV_URL, MAX(urilen, strlen(VSX_MKV_URL)))) {

      if(capability & URL_CAP_LIVE) {
        rc = srv_ctrl_live(pConn, &httpStatus, NULL);
      } else {
        LOG(X_ERROR("%s requires the Automatic Format Adaptation Server.  Use '--live' "), pConn->httpReq.puri);
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_STATUS_URL   /status
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_STATUS_URL, strlen(VSX_STATUS_URL) + 1)) {

      if(capability & URL_CAP_STATUS) {
        rc = srv_ctrl_status(pConn);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    //
    // Handle VSX_PIP_URL   /pip
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_PIP_URL, strlen(VSX_PIP_URL) + 1)) {

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
      if(capability & URL_CAP_PIP) {
        rc = srv_ctrl_pip(pConn);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }
#else // XCODE_HAVE_PIP
      httpStatus = HTTP_STATUS_FORBIDDEN;
#endif // XCODE_HAVE_PIP

    //
    // Handle VSX_CONFIG_URL   /config
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_CONFIG_URL, strlen(VSX_CONFIG_URL) + 1)) {

      if(capability & URL_CAP_CONFIG) {
        rc = srv_ctrl_config(pConn);
      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

#if defined(VSX_HAVE_SERVERMODE)

    //
    // Handle VSX_CMD_URL   /cmd
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_CMD_URL, strlen(VSX_CMD_URL) + 1)) {

      if((capability & URL_CAP_CMD)  &&
           (rc = validatePwd(pConn, NULL, NULL, NULL, 0)) == 0) {

        lenout = sizeof(buf);
        srv_ctrl_oncommand(pConn, &httpStatus, (unsigned char *) buf, &lenout);

        if((rcout = http_resp_send(&pConn->sd, &pConn->httpReq, 
                                 httpStatus, (unsigned char *) buf, lenout)) < 0) {
         rc = rcout;
        }

      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

#endif // VSX_HAVE_SERVERMODE

    //
    // Handle VSX_CROSSDOMAIN_XML_URL /crossdomain.xml
    //
    } else if(!strncasecmp(pConn->httpReq.puri, VSX_CROSSDOMAIN_XML_URL, strlen(VSX_CROSSDOMAIN_XML_URL) + 1)) {

      if(capability & URL_CAP_LIVE) {

        if(mediadb_prepend_dir(pConn->pCfg->pMediaDb->homeDir, VSX_HTML_PATH, buf, sizeof(buf)) >= 0 && 
           mediadb_prepend_dir(buf, VSX_CROSSDOMAIN_XML_URL, path, sizeof(path)) >= 0 && 
          fileops_stat(path, &st) == 0) {
            rc = http_resp_sendfile(pConn, &httpStatus, path, CONTENT_TYPE_TEXTHTML, HTTP_ETAG_AUTO);
        } else {
          httpStatus = HTTP_STATUS_NOTFOUND;
        }

      } else {
        httpStatus = HTTP_STATUS_FORBIDDEN;
      }

    } else {
      httpStatus = HTTP_STATUS_NOTFOUND;
    }

/*
    //
    // 404 not found for anything not matching VSX_CMD_URL  /cmd
    //
    if(httpStatus == HTTP_STATUS_NOTFOUND) {

      mediadb_prepend_dir2(pConn->pCfg->pMediaDb->homeDir,
                           VSX_HTML_PATH, pConn->httpReq.puri, buf, sizeof(buf));

      //TODO: shouldn't just send back the file here if it was deemed HTTP_STATUS_NOTFOUND
      if(fileops_stat(buf, &st) == 0 && !(st.st_mode & S_IFDIR)) {
        fprintf(stderr, "\n\nWAS 404... converting to path '%s'\n", buf);
        //rc = http_resp_sendfile(pConn, &httpStatus, buf, CONTENT_TYPE_TEXTHTML, HTTP_ETAG_AUTO);
      } else if(capability & URL_CAP_CMD) {
        LOG(X_DEBUG("HTTP 404 '%s'"), pConn->httpReq.puri);
        rc = http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 1, NULL);
        break;
      } else {
        //
        // Prevent exposing 404 for remote file discovery
        //
        //httpStatus = HTTP_STATUS_FORBIDDEN;
      }
    } 
*/

    //fprintf(stderr, "HTTP END OF LOOP - RC:%d, status:%d, uri:'%s'\n", rc, httpStatus, pConn->httpReq.puri); 

    if(httpStatus != HTTP_STATUS_OK) {
      LOG(X_WARNING("HTTP %s :%d%s %s from %s:%d"), pConn->httpReq.method, 
           ntohs(INET_PORT(pConn->pListenCfg->sa)), pConn->httpReq.puri, 
           http_lookup_statuscode(httpStatus), 
           FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));

      rc = http_resp_error(&pConn->sd, &pConn->httpReq, httpStatus, 1, NULL, pauthbuf);
      break;
    }

    //
    // Respect Connection: keep-aliive
    //
    if(pConn->httpReq.connType != HTTP_CONN_TYPE_KEEPALIVE) {
      break;
    }

  } while(rc >= 0);

  netio_closesocket(&pConn->sd.netsocket);

  LOG(X_DEBUG("HTTP%s connection ended on port %d from %s:%d"), 
          (pConn->sd.netsocket.flags & NETIO_FLAG_SSL_TLS) ? "S" : "", 
          ntohs(INET_PORT(pConn->pListenCfg->sa)),
          FORMAT_NETADDR(pConn->sd.sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pConn->sd.sa)));
}


