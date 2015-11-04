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
#if !defined(WIN32) || defined(__MINGW32__)
#include <signal.h>
#include <getopt.h>
#endif // WIN32
#include "vsxlib_int.h"

enum ACTION_TYPE {
  ACTION_TYPE_NONE =                     0x0000,
  ACTION_TYPE_INVALID =                  0x0001,
  ACTION_TYPE_EXTRACT_VIDEO =            0x0002,
  ACTION_TYPE_EXTRACT_AUDIO =            0x0004,
  ACTION_TYPE_CREATE_MP4 =               0x0008,
  ACTION_TYPE_DUMP_CONTAINER =           0x0010,
  ACTION_TYPE_ANALYZE_MEDIA =            0x0020,
  ACTION_TYPE_CAPTURE_LIST_INTERFACES =  0x0040,
  ACTION_TYPE_CAPTURE =                  0x0080,
  ACTION_TYPE_STREAM =                   0x0100,
  ACTION_TYPE_SERVER =                   0x0200,
  ACTION_TYPE_LICENSE_GEN =              0x0400,
  ACTION_TYPE_LICENSE_CHECK =            0x0800
};

#define ACTION_TYPE_CAPSTREAM_MASK       (ACTION_TYPE_CAPTURE | ACTION_TYPE_STREAM) 

#define TSLIVE_LISTEN_PORT_STR               HTTP_LISTEN_PORT_STR 
#define HTTPLIVE_LISTEN_PORT_STR             HTTP_LISTEN_PORT_STR 
#define MOOFLIVE_LISTEN_PORT_STR             HTTP_LISTEN_PORT_STR 
#define FLVLIVE_LISTEN_PORT_STR              HTTP_LISTEN_PORT_STR 
#define MKVLIVE_LISTEN_PORT_STR              HTTP_LISTEN_PORT_STR 
#define STATUS_LISTEN_PORT_STR               HTTP_LISTEN_PORT_STR 
#define CONFIG_LISTEN_PORT_STR               HTTP_LISTEN_PORT_STR 
#define PIP_LISTEN_PORT_STR                  HTTP_LISTEN_PORT_STR 

static VSXLIB_STREAM_PARAMS_T *g_streamParams;

#ifndef WIN32

#if defined(DBGMALLOC) 
extern void _print_alloc_stats(int verbose, FILE * fp);
#endif // DBGMALLOC

extern int g_proc_exit;
extern pthread_cond_t *g_proc_exit_cond;
void *g_plogProps;

static void sig_hdlr(int sig) {

  fprintf(stderr, "Caught signal:%u\n", sig);
  fflush(stderr);

  switch(sig) {
    case SIGINT:

      //
      // Can't call blocking function from pre-empting signal handler here
      // since vsxlib_close will block until inUse flag is unset
      //
      //vsxlib_close(g_streamParams);

      g_proc_exit = 1;
      if(g_proc_exit_cond) {
        pthread_cond_broadcast(g_proc_exit_cond);
      }

      break;
#if defined(DBGMALLOC)
    case SIGUSR1:
      _print_alloc_stats(1, stderr);
      return;
#endif // DBGMALLOC
  }

  signal(sig, SIG_DFL);
}

#endif // WIN32

#ifndef DISABLE_PCAP
#define VSX_DESCR_PCAP " (pcap)"
#else // DISABLE_PCAP
#define VSX_DESCR_PCAP  
#endif // DISABLE_PCAP

#ifdef VSX_HAVE_SSL
#define VSX_DESCR_SSL " (ssl)"
#else // VSX_HAVE_SSL
#define VSX_DESCR_SSL 
#endif // VSX_HAVE_SSL

#if defined(XCODE_STATIC)
#define VSX_XCODE_COMPILED " (non-redistributable)"
#else
#define VSX_XCODE_COMPILED
#endif 

#define VSX_DESCR VSX_XCODE_COMPILED VSX_DESCR_PCAP VSX_DESCR_SSL


static void banner(FILE *fp) {

  fprintf(stdout, "\n"VSX_BANNER(VSX_APPTYPE_RC) VSX_DESCR"\n", BUILD_INFO_NUM);
  fprintf(stdout, VSX_BANNER3"\n");

}

static void create_licenseinfo() {

#if defined(VSX_HAVE_LICENSE)
  char buf[1024];

  if((license_createpkt(buf, sizeof(buf))) < 0) {
    LOG(X_ERROR("Unable to create license"));
    return;
  }
  
  //license_dump_str(buf, strlen(buf), stdout);

  fprintf(stdout, "You may copy and paste this license request:\n");
  fprintf(stdout, "%s\n", buf);
#endif // VSX_HAVE_LICENSE

}

static void check_licenseinfo(VSXLIB_STREAM_PARAMS_T *pParams) {

#if defined(VSX_HAVE_LICENSE)
  LIC_INFO_T lic;

  memset(&lic, 0, sizeof(lic));

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, 0);

  vsxlib_checklicense(pParams, &lic, NULL);

#else // VSX_HAVE_LICENSE
  fprintf(stdout, "License: disabled\n");
#endif // VSX_HAVE_LICENSE

}

#if defined(VSX_HAVE_DEBUG)
//#include "test/test.c"
int runtests(const char *optarg);
#else // VSX_HAVE_DEBUG
int runtests(const char *optarg) {
  fprintf(stdout, "test not compiled\n");
  return -1;
}
#endif // VSX_HAVE_DEBUG


static void usage_analyze(int argc, const const char *argv[]) {

#if defined(VSX_HAVE_VIDANALYZE)
  fprintf(stdout,
      " ============================================================================\n"
      "   Analyze H.264 video bitstream.\n"
      " ============================================================================\n"
      "\n"
      " --analyze,-a\n"
      "   --in=[ input path ]  Supports [flv|h264|mp4]\n"
      "   --fps=[ frame rate ] (eg. --fps=29.97)\n"
      "\n");
#endif // VSX_HAVE_VIDANALYZE

}

/*
static void usage_server(int argc, const const char *argv[]) {

#if defined(VSX_HAVE_SERVERMODE)
  fprintf(stdout,
      " ============================================================================\n"
      "   Run in broadcast control mode and listen for control commands.\n"
      "   A Web user interface is available at the listen address.\n"
      " ============================================================================\n"
      "\n"
      " --broadcast,-e\n"
      "   --listen=[ address:port ] http server listen port\n"
      //"   --conf=[ configuration file path ] (default=%s)\n"
      "   --media=[ media library dir ] (default=.)\n"
      "   --home=[ home dir ] (default=.)\n"
      "   --dbdir=[ media database dir ] (default=[media dir]/%s)\n"
      "   --nodb        Do not write media data base content\n"
      "\n",
      //VSX_CONF_PATH,
      VSX_DB_PATH);
#endif // VSX_HAVE_SERVERMODE

}
*/

static void usage_capture(int argc, const const char *argv[]) {

#if defined(VSX_HAVE_CAPTURE)

  fprintf(stdout,
#ifndef DISABLE_PCAP
      " ============================================================================\n"
      "   Capture media via local interface, live pcap, or pcap file.\n"
      "   [%s]\n"
      " ============================================================================\n"
      "\n"
      " --capture,-c\n"
      "   --in=[ [ transport ]:// ] address:port | sdp file | net interface | pcap file ] \n"
#else //DISABLE_PCAP
      " ============================================================================\n"
      "   Capture media via local interface\n"
      "   [%s]\n"
      " ============================================================================\n"
      "\n"
      " --capture,-c\n"
      "   --in[ [ [ transport ]:// ] address:port | sdp file ]\n"
#endif // DISABLE_PCAP

      "\n"
      "     Transport types: rtp://  srtp:// dtls:// dtlssrtp:// udp:// http:// https://\n"
      "                      rtmp:// rtmps:// rtsp:// rtsps://\n"
      "\n     Some Examples:\n"
      "         --capture=\"udp://127.0.0.1:5004\" --filter=\"type=m2t\"\n"
      "         --capture=\"rtp://0.0.0.0:5004,5006\" --filter=\"type=native\"\n" 
      "         --capture=\[input path].sdp\n"
      "         --capture=\"rtsp[s]://[username:password@][remote-address]:[port]/[stream-name]\n"
      "         --capture=\"rtsp[s]://[username:password@][listener-address]:[port]\n"
      "         --capture=\"rtmp[s]://[remote-address]:[port]/[app]/[stream-name]\n"
      "         --capture=\"rtmp[s]://[username:password@][listener-address]:[port]\n"
      "         --capture=\"http[s]://[username:password@][remote-address]:[port]/flvlive\" --filter=\"type=flv\"\n"
      "         --capture=\"http[s]://[username:password@][remote-address]:[port]/httplive/out.m3u8\"\n"
      "         --capture=\"http[s]://[username:password@][remote-address]:[port]/tslive\"\n"
      "\n"
      "     To record the capture use stream output types such as:\n"
      "         '--out=[output.m2t]', '--flvrecord=[output.flv]', '--mkvrecord=[output.webm]'\n"
      "\n"
      "   --auth-basic  Allow HTTP/RTSP Basic authentication (disabled by default)\n"
      "                 If not enabled, HTTP/RTSP Digest authentication is preferred\n"
      "   --auth-basic-force  Force HTTP/RTSP Basic authentication instead of Digest\n"
      "   --dir=[ output directory ]\n"
      "   --filter=[ packet filter string ] contains one or more of the following:\n"
      "          dst=[dest ip], src=[src ip], ip=[ip],\n"
      "          dstport=[dest udp port], srcport=[src udp port], port=[udp port]\n"
      "          ssrc=[RTP ssrc eg. 0x01020304], pt=[RTP payload type],\n"
      "          type=[%s],\n"
      "          file=[output media file prefix],\n"
      "          clock=[clock Hz], channels=[1-8]\n"
      "          eg. --capture=\"rtp://5004\" --filter=\"pt=96, type=h264\"\n"
      "   --firxmit=[ 0 | 1 ] Controls sending RTCP FB FIR messages\n" 
      "          Refer to userguide for detailed command specifics.\n"
      "   --maxrtpdelay=[ ms to wait for missing RTP packet ] (default=%d ms)\n"
      "   --maxrtpaudiodelay=[ ms to wait for missing audio RTP packet ] (default=%d ms)\n"
      "   --maxrtpvideodelay=[ ms to wait for missing video RTP packet ] (default=%d ms)\n"
      "   --nackxmit=[ 0 | 1 ] Controls sending RTCP FB NACK messages\n" 
      "   --nortcpbye   Do not exit capture upon RTCP BYE\n"
      "   --overwrite   Overwrite output file if it exists\n"
      "   --pes         Extract each PES (used with mpeg2-ts capture only)\n"
#ifndef DISABLE_PCAP
      "   --promisc     Promiscuous capture\n"
#endif // DISABLE_PCAP
      "   --queue=[ packet queue slots ] (default=%d) used by m2t capture\n"
      "   --realtime    Download HTTP input in realtime\n"
      "   --rembkxmit=[ 0 | 1 ] Controls sending RTCP FB APP REMB messages\n" 
      "   --rembkxmitmaxrate=[ bitrate in Kbps ] Max permitted RTCP FB APP REMB bitrate\n" 
      "   --rembkxmitminrate=[ bitrate in Kbps ] Min permitted RTCP FB APP REMB bitrate\n" 
      //"   --rembkxmitforce=[ 0 | 1 ] Controls sending RTCP FB APP REMB messages\n" 
      "   --retry=[ active connect retry ] (default=0) Valid for http:// rtmp:// rtsp://\n"
      "          eg.  --retry=2  (twice)   --retry  (indefinately)\n"
      "   --rtcprr=[ RTCP RR interval sec ] (default=%.1f disabled)\n"
      "   --rtmpnosig   Disable RTMP Handshake signature\n"
      "   --rtmppageurl=[ RTMP Page URL String ] \n"
      "   --rtmpswfurl=[ RTMP SWF URL String ] \n"
      "   --rtpframedrop=[ RTP input loss frame drop policy ] (default=1)\n"
      "          0 - Do not drop frames with corruption from lost packets\n" 
      "          1 - (default) Drop frames with corruption at beginning\n" 
      "          2 - Drop frames with any corruption\n" 
      "   --rtsp-interleaved  Use RTSP TCP interleaved mode\n"
      "                 This becomes the default setting when connecting to RTSP-SSL listener [rtsps://]\n"
      "                 Use --rtsp-interleaved=0 to force RTSP UDP/RTP with RTSP-SSL\n"
      "   --vidq=[ video frame queue slots ] used by RTP video frame buffer\n"
      "   --audq=[ audio frame queue slots ] used by RTP audio frame buffer\n"
      "   --stunrespond=[ STUN password ]\n"
      "\n",
      CAP_FILTER_TRANS_DISP_STR,
      CAP_FILTER_TYPES_DISP_STR,
      CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS,
      CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS,
      CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS,
      PKTQUEUE_LEN_DEFAULT,
      0.0f);

#endif // VSX_HAVE_CAPTURE

}

static void usage_create(int argc, const const char *argv[]) {

#if defined(VSX_CREATE_CONTAINER)
  fprintf(stdout,
      " ============================================================================\n"
      "   Create or add to contents to mp4 container file.\n" 
      " ============================================================================\n"
      "\n"
      " --create,-r\n"
      "   --in=[ input path ]\n"
      "   --out=[ mp4 output path ]\n"
      "   --stts=[ mp4 stts input path ] Use dumped mp4 a/v sync info\n"
      "     Input format is autodetected.\n"
      "     AAC with ADTS header input options:\n"
      "   --fps=[ samples / frame ] (960 or 1024, default=1024)\n"
      "   --objtype=[ 0 - 46 ] Explicit AAC ESDS object type id\n"
      "     H.264 Annex B format input options:\n"
      "   --fps=[ frame rate ] (eg. --fps=29.97)\n"
      "\n");
#endif // VSX_CREATE_CONTAINER

}

static void usage_dump(int argc, const const char *argv[]) {

#if defined(VSX_DUMP_CONTAINER)
  fprintf(stdout,
      " ============================================================================\n"
      "   Dump media container info.\n"
      " ============================================================================\n"
      "\n"
      " --dump,-d\n"
      "   --in=[ input path ]\n"
      "   --stts=[ mp4 stts output path ] Dump mp4 a/v sync as text\n"
      "   --noseekfile   Do not read from .seek (m2t only) \n"
      "\n");
#endif // (VSX_DUMP_CONTAINER

}

static void usage_extract(int argc, const const char *argv[]) {

#if defined(VSX_EXTRACT_CONTAINER)
  fprintf(stdout,
      " ============================================================================\n"
      "   Extract raw media from [flv|mp4|m2t] container.\n" 
      " ============================================================================\n"
      "\n"
      " --extract,-x\n"
      "   --in=[ container file input path ]\n"
      "   --out=[ output path ]\n"
      "   --start=[ start offset seconds eg.  --start=1.5 ]\n"
      "   --duration=[ duration seconds eg.  --duration=5.3 ]\n"
      "   --audio       Audio contents only\n"
      "   --video       Video contents only\n"
      "\n");
#endif // VSX_EXTRACT_CONTAINER

}

static void usage_list(int argc, const const char *argv[]) {

#ifndef DISABLE_PCAP
  fprintf(stdout,
      " ============================================================================\n"
      "   List all pcap interface names and exit.\n"
      " ============================================================================\n"
      "\n"
      " --list,-l\n"
      "\n");
#endif // DISABLE_PCAP

}

static void usage_stream(int argc, const const char *argv[]) {

#if defined(VSX_HAVE_STREAMER)

  fprintf(stdout,
      " ============================================================================\n"
      "   Stream and transcode static and live media.\n"
      " ============================================================================\n"
      "\n"
      " --stream,-s\n"
      "\n     Some Examples:\n"
      "         --stream=\"udp://127.0.0.1:5004\" --rtsptransport=m2t\n" 
      "         --stream=\"rtp://127.0.0.1:5004,5006\" --rtsptransport=native\n" 
      "         --stream=\[output path].m2t\n"
      "         --stream=\"[dtls|dtls-srtp|rtp|srtp]://127.0.0.1:7000\" --rtp-mux --rtcp-mux --sdp=[output].sdp\n"
      "         --stream=\"rtsp[s]://[username:password@][remote-address]:[port]/[stream-name]\"\n"
      "         --stream=\"rtmp[s]://[username:password@][remote-address]:[port]/[app]/[stream-name]\"\n"
      "\n"
      "   --abrauto=[ 0 | 1 ] Enables Dynamically Adaptive Bitrate Control (default=0)\n"
      "   --auth-basic  Allow HTTP/RTSP Basic authentication (disabled by default)\n"
      "                 If not enabled, HTTP/RTSP Digest authentication is preferred\n"
      "   --auth-basic-force  Force HTTP/RTSP Basic authentication instead of Digest\n"
      "   --avsync=[ audio/video sync adjustment (+/- float sec) ]\n"
      "                 For output specific index use '--avsync1=', --avsync2=', '--avsync3='\n"
      "   --conf=[ configuration file path ] (default=%s)\n"
      "   --delay=[ audio/video output delay factor (float sec) ]\n"
      "   --duration=[ max stream duration seconds eg.  --duration=5.3 ]\n"
      "   --firaccept=[ 0 | 1 ] Controls local encoder IDR requests (default=1)\n" 
      "                 If enabled, inbound RTCP FB FIR messages will be respected.\n"
      "                 An IDR may also be requested upon a media connection request.\n"
      "   --framethin=[ 0 | 1 ] Controls frame thinning for flvlive/mkvlive/rtmp\n"
      "   --in=[ input media or description file path ]\n"
      //"     media container types [aac|h264|flv|mp4|m2t]\n"
#if defined(WIN32) && !defined(__MINGW32__)
      "   --interface=[ outbound network interface ] Use raw socket transmission\n"
      "                 Use --list to show available network interface names\n"
#endif // WIN32
      "   --loop        Loop media content\n"
      "   --localhost=[ Global IP address or hostname of this machine ]\n"
      "   --noaudio     Do not stream audio elementary stream\n" 
      "   --novideo     Do not stream video elementary stream\n" 
      "   --overwrite   overwrite output recording file if it already exists\n"
      "   --start=[ input file start offset seconds eg.  --start=1.5 ]\n"
      "   --statusmax=[ max ] Max HTTP status server sessions (default=0)\n"
      "   --streamstats=[ File path to dump output statistics (or stdout if omitted) ]\n"
      "   --token=[ Server Request Authorization Token Id ]\n"
      //"   --httpmax=[ max ] Max HTTP sessions (default=%d)\n"

      "   --xcode=[ transcode configuration options or transcoder config file path ]\n"
      "                 A detailed parameter list can be found in 'etc"DIR_DELIMETER_STR"xcode.conf'\n"
#if defined(LITE_VERSION)
      "\n   *** Transcoding is not enabled in "VSX_VERSION_LITE" ***\n"
#endif // LITE_VERSION

      "\n   Parameters affecting HTTP Dynamic Update / Configuration Server\n\n"
      "   --configsrv=[ address:port ] HTTP dynamic configuration update server listener\n"
      "                 (default=%s)\n"
      "                 Access the configuration update server at the URL: \n"
      "                 http[s]://[username:password@][listener address]:[port]%s\n"
      "   --configmax=[ max ] Max HTTP dynamic config sessions (default=0)\n"

      "\n   Parameters affecting MPEG-DASH / Fragmented MP4 stream output\n\n"
      "   --dash=[ address:port ] MPEG-DASH broadcast server listener\n"
      "                 (default=%s)\n"
      "                 Access the DASH mpd at the URL: \n"
      "                 http[s]://[username:password@][listener address]:[port]%s/%s\n"
      "   --dashdir=[ segment output dir ] Custom directory path (default=\"%s\")\n"
      "   --dashfileprefix=[ out file prefix ] Segment output name prefix (default=\"%s\")\n"
      "   --dashurlhost=[ URL media host prefix used in playlist ] (default=\"%s\")\n"
      "   --dashfragduration=[ fragment duration (float sec) ] (default=\"%.1f\" sec)\n"
      "   --dashmaxduration=[ max segment duration (float sec) ] (default=\"%.1f\" sec)\n"
      "   --dashminduration=[ min segment duration (float sec) ] (default=\"%.1f\" sec)\n"
      "   --dashmoof=[ Enable / disable DASH .mp4 fragment and .mpd creation ] (default=\"1\")\n"
      "   --dashts=[ Enable / disable DASH .ts fragment and .mpd creation ] (default=\"0\")\n"
      "                 default=\"1\" if httplive segmentor is enabled\n"
      "   --dashtsduration=[ .ts segment duration ] (default=\"%.1f\" sec)\n"
      "                 This option is the same as --httplivechunk \n"
      "   --dashuseinit=[ Enable / disable  mp4 initializer segment ] (default=\"%d\")\n"
      "   --dashmpdtype=[ number | time ] MPD SegmentTemplate media naming convention (default=\"%s\")\n"

      "\n   Parameters affecting DTLS/SSL stream output\n\n"
      "   --dtls        Use SSL/DTLS RTP output\n"
      "   --dtls-srtp   Use SRTP output with SSL/DTLS handshake generated keys\n"
      "   --dtlsserver  Use DTLS server keying mode\n"
      "   --dtlscert=[ SSL DTLS Certificate path ] (default="SSL_DTLS_CERT_PATH")\n"
      "   --dtlskey=[ SSL DTLS key path ] (default="SSL_DTLS_PRIVKEY_PATH")\n"

      "\n   Parameters affecting FLV stream output\n\n"
      "   --flvdelay=[ initial output buffering delay factor (default=%.1f sec) ]\n"
      "   --flvlive=[ address:port ] HTTP/FLV encapsulated broadcast server listener\n"
      "                 (default=%s)\n"
      "                 Access the live stream at the URL: \n"
      "                 http[s]://[username:password@][listener address]:[port]%s\n"
      "   --flvrecord=[ FLV output media file ] Record streaming output to FLV\n"
      "                 For output specific files use '--flvrecord1=', --flvrecord2=', etc.\n"

      "\n   Parameters affecting HTTPLive stream output\n\n"
      "   --httplive=[ address:port ]  HTTPLive broadcast server listener\n"
      "                 (default=%s)\n"
      "                 Access the live stream at the URL: \n"
      "                 http[s]://[username:password@][listener address]:[port]%s\n"
      "   --httplivebw=[ \"KBPS,KBPS\" ] Playlist published bitrate(s) CSV in Kb/s\n"
      "   --httplivechunk=[ chunk duration ] Segment chunk duration (default=\"%.1f\" sec)\n"
      "   --httplivedir=[ chunk output dir ] Custom chunk output directory\n"
      "   --httpliveprefix=[ out file prefix ] Chunk output prefix (default=\"%s\")\n"
      "   --httpliveurlhost=[ URL media host prefix used in playlist ] (default=\"%s\")\n"

      "\n   Parameters affecting Automatic Format Adaptation Server\n\n"
      "   --live=[ address:port ] HTTP broadcast server listener\n"
      "                 (default=%s)\n"
      "                 View the live stream using a web link at the URL:\n"
      "                 http[s]://[username:password@][listener address]:[port]%s\n"
      //"   --livemax=[ max ] Max HTTP sessions (default=%d)\n"

      "\n   Parameters affecting Matroska / WebM stream output\n\n"
      "   --mkvlive=[ address:port ] HTTP/Matroska / WebM encapsulated broadcast server listener\n"
      "                 (default=%s)\n"
      "                 Access the live stream at the URL:\n"
      "                 http[s]://[username:password@][listener address]:[port]%s\n"
      "   --mkvdelay=[ Matroska / WebM initial output buffering delay factor ] (default=%.1f sec)\n"
      "   --mkvrecord=[ MKV output media file ] Record streaming output to Matroska\n"
      "                 For output specific files use '--mkvrecord1=', --mkvrecord2=', etc.\n"
      //"   --mkvavsync=[ MKV packetization audio/video sync adjustment (+/- float sec) ]\n"
      //"   --mkvrecordavsync=[ MKV recording packetization audio/video sync adjustment (+/- float sec) ]\n"

      "\n   Parameters affecting MPEG2-TS stream output\n\n"
      "   --tslive=[ address:port ] HTTP/MPEG2-TS encapsulated broadcast server listener\n"
      "                 (default=%s)\n"
      "                 Access the live stream at the URL:\n"
      "                 http[s]://[username:password@][listener address]:[port]%s\n"

      "\n   Parameters affecting UDP / RTP stream output\n\n"
      "   --mtu=[ packet payload MTU ] (default=%d)\n"
      "   --noseqhdrs   Do not bundle video start sequence parameters with output\n"
      "   --out=[ <dtls|dtls-srtp|rtp|srtp|udp>://<destination host>:<port> | <local output file> ]\n"
      "                 This is synonomous with '--stream'.\n"
#if defined(VSX_HAVE_CAPTURE)
      "                 Use with '--capture,-c' to stream live content.\n"
#endif // VSX_HAVE_CAPTURE
      "                 For multiple output use '--out2=', --out3='\n"
      "\n   Parameters affecting RTP stream output\n\n"
      "   --rtcpavsync=[ RTCP Sender Report ntp / ts adjustment (+/- float sec) ]\n"
      "   --rtcpports=[ RTCP output ports ]  Enables non-default RTCP port(s)\n"
      "                 (eg., \"--out=rtp://a.b.c.d:2000,2002\" --rtcpports=2001,2003\"\n"
      "                 For multiple output (eg., '--out2') use '--rtcpports2=', --rtcpports3='\n"
      "   --rtcp-mux    Send RTCP over the RTP port(s). Akin to:  --rtcpports=<video port>,<audio port>\n"
      "   --rtcpsr=[ RTCP Sender Report interval sec ] (default=%.1f), Use 0 to disable SR\n"
      "   --rtpclock=[\"v=Hz,a=Hz\"] RTP clock rate in Hz for video and/or audio output stream \n"
      "                 (eg., \"v=24000,a=48000\", or \"24000,48000\"), or \",48000\")\n"
      //"   --rtpmax=[ max ] Max UDP/RTP output sessions (default=%d)\n"
      "   --rtpmaxptime=[ Audio duration in ms to aggregate into a single RTP packet ]\n"
      "   --rtp-mux    Send RTP video and audio over the same port. Akin to:  --out=rtp://[remote-address]:7000,7000\n"
      "   --rtppayloadtype=[\"v=PT,a=PT\"] RTP payload type for video and/or audio output stream\n"
      "                 (eg., \"v=97,a=96\", or \"97,96\"), or \",96\")\n"
      "   --rtppktzmode=[ codec specific RTP packetization mode ]\n"
      "   --rtpretransmit=[ 0 | 1 ]  Enable RTP retransmission upon receiving RTCP NACK\n"
      "   --rtpssrc=[\"v=SSRC,a=SSRC\"] RTP SSRC for video and/or audio output stream \n"
      "                 (eg., \"v=0x01020304,a=16909061\", or \"0x01020304,0x01020305\"), or \",0x01020305\")\n"
      "   --rtptransport=[ m2t | native ] protocol specific transport (default=m2t)\n"
      "       m2t: "STREAM_RTP_DESCRIPTION_MP2TS", rtp: media specifc encapsulation\n"
      "   --rtpusebindport=[ 0 | 1 ]  Use RTP input capture port(s) as outbound source port\n"
      "   --sdp=[ output Session Description Protocol file path ]\n"
      "   --srtp        Use SRTP output\n"
      "   --srtpkey=[ Base64 encoded SRTP SDES key(s).  Unique a/v keys can be delimited by ',' ]\n"
      "                 For multiple output (eg., '--out2') use '--srtpkey2=', --srtpkey3='\n"

      "\n   Parameters affecting RTMP stream output\n\n"
      "   --rtmp=[ address:port ] RTMP broadcast server listener\n"
      "                 (default=%s)\n"
      "                 Access the live stream at the URL: \n"
      "                 rtmp[s]://[listener address]:[port]\n"
      "   --rtmpnosig   Disable RTMP Handshake signature (when using --stream=rtmp[s]://[remote-address]/)\n"
      "   --rtmppageurl=[ RTMP Page URL String ] (when using --stream=rtmp[s]://[[remote-address]/\n"
      "   --rtmpswfurl=[ RTMP SWF URL String ] (when using --stream=rtmp[s]://[remote-address]/)\n"
      "   --rtmpt[ 0 | 1 ]   Explicitly enable or disable RTMP Tunneling server\n"
      "\n   Parameters affecting RTSP stream output\n\n"
      "   --rtsp=[ address:port ] RTSP broadcast server listener\n"
      "                 (default=%s)\n"
      "                 Access the live stream at the URL:\n"
      "                 rtsp[s]://[username:password@][listener address]:[port]\n"
      "   --rtsp-interleaved  Force RTSP TCP interleaved mode output media streams\n"
      "                 Use --rtsp-interleaved=0 to force RTSP UDP/RTP output media streams \n"
      "   --rtsp-srtp   Use SRTP for RTSP media streams\n"
      "                 This becomes the default setting for any RTSP-SSL listener [rtsps://]\n"
      "                 Use --rtsp-srtp=0 to force RTSP UDP/RTP with RTSP-SSL control channel\n"
      "\n   Parameters affecting STUN \n\n"
      "   --stunrequest Initiate STUN binding requests\n"
      "   --stunrequestuser=[ STUN binding request username(s).  Unique a/v values can be delimited by ','  ]\n"
      "   --stunrequestpass=[ STUN binding request password(s).  Unique a/v values can be delimited by ','  ]\n"
      "   --stunrequestrealm=[ STUN binding request realm(s).  Unique a/v values can be delimited by ','  ]\n"

#if defined(VSX_HAVE_TURN)
      "\n   Parameters affecting TURN\n\n"
      "   --turnpass =[ TURN server auth password ]\n"
      "   --turnrealm=[ TURN server auth realm ]\n"
      "   --turnserver=[ TURN server address:port ] If port is ommitted, the default value is %d\n"
      "   --turnsdp=[ output Session Description Protocol file containing allocated TURN relay ports ]\n"
      "   --turnuser =[ TURN server auth username ]\n"
#endif // (VSX_HAVE_TURN)

      "\n"

      ,VSX_CONF_PATH
      ,CONFIG_LISTEN_PORT_STR
      ,VSX_CONFIG_URL
      ,MOOFLIVE_LISTEN_PORT_STR
      ,VSX_DASH_URL
      ,DASH_MPD_DEFAULT_NAME
      ,VSX_MOOF_PATH
      ,DASH_DEFAULT_NAME_PRFX
      ,""
      ,MOOF_TRAF_MAX_DURATION_SEC_DEFAULT
      ,MOOF_MP4_MAX_DURATION_SEC_DEFAULT
      ,MOOF_MP4_MIN_DURATION_SEC_DEFAULT
      ,HTTPLIVE_DURATION_DEFAULT
      ,MOOF_USE_INITMP4_DEFAULT
      ,DASH_MPD_TYPE_DEFAULT_STR

      ,FLVLIVE_DELAY_DEFAULT
      ,FLVLIVE_LISTEN_PORT_STR
      ,VSX_FLVLIVE_URL

      ,HTTPLIVE_LISTEN_PORT_STR
      ,VSX_HTTPLIVE_URL
      ,HTTPLIVE_DURATION_DEFAULT
      ,HTTPLIVE_TS_NAME_PRFX
      ,""
      ,HTTP_LISTEN_PORT_STR
      ,VSX_LIVE_URL

      ,MKVLIVE_LISTEN_PORT_STR
      ,VSX_MKVLIVE_URL
      ,MKVLIVE_DELAY_DEFAULT

      ,TSLIVE_LISTEN_PORT_STR
      ,VSX_TSLIVE_URL

      ,STREAM_RTP_MTU_DEFAULT
      ,STREAM_RTCP_SR_INTERVAL_SEC
      ,RTMP_LISTEN_PORT_STR
      ,RTSP_LISTEN_PORT_STR
#if defined(VSX_HAVE_TURN)
      ,DEFAULT_PORT_TURN
#endif // (VSX_HAVE_TURN)
      );

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
  fprintf(stdout,
      "   Parameters controlling Picture In Picutre (PIP)\n"
#if defined(LITE_VERSION)
      "\n   *** Picture In Picture (PIP) is not enabled in "VSX_VERSION_LITE" ***\n"
#endif // LITE_VERSION
      "\n"
      "   --pip=[ input stream (file path) ] Enable PIP Input Stream\n"
      "   --pipalphamax=[ 0 ... 255 ] max transparency value of PIP \n"
      "   --pipalphamin=[ 0 ... 255 ] minimum transparency of PIP (overrides input alpha)\n"
      "   --pipbefore   Insert PIP before scaling output picture\n"
      "   --pipconf=[ PIP motion configuration file ] Refer to 'etc"DIR_DELIMETER_STR"pip.conf'\n"
      "   --piphttp=[ address:port ] PIP control interface server listener (default=%s)\n"
      "                 Add / remove PIP(s) via the URL:\n"
      "                 http[s]://[username:password@][listener address]:[port]%s\n"
      "   --piphttpmax=[ max ] Max HTTP PIP control connections (default=0)\n"
      "   --pipxcode=[ PIP resize options ] key=value options list similar to '--xcode='\n"
      "   --pipx=[ x placement offset ] \n"
      "   --pipxright=[ x placement offset from right edge ] \n"
      "   --pipy=[ y placement offset ] \n"
      "   --pipybottom =[ y placement offset from bottom edge ] \n"
      "   --pipzorder=[ zorder +/- value ] \n"
     "\n"
      ,PIP_LISTEN_PORT_STR
      ,VSX_PIP_URL);
#endif // XCODE_HAVE_PIP

#endif // VSX_HAVE_STREAMER

}

static void usage_conference(int argc, const const char *argv[]) {

#if defined(VSX_HAVE_STREAMER) && defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0) && \
     defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  fprintf(stdout,
      " ============================================================================\n"
      "   Video Conferencing.\n"
#if defined(LITE_VERSION)
      "\n   *** Conferencing is not enabled in "VSX_VERSION_LITE" ***\n\n"
#endif // LITE_VERSION
      " ============================================================================\n"
      "\n"
      "   Parameters controlling video conferencing\n"
      "\n"
      "   --conference  Enable conferencing mode\n"
      "             Conferencing mode has no main overlay input video source and depends on\n"
      "             the HTTP PIP control interface to add participants\n"
      "   --in=[ overlay video or still image path ]  Supported image type is [png|bmp]\n"
      "             Use background video or still image as main video overlay\n"
      "   --layout=[ p2pgrid | grid | vad ] Participant layout type (default=p2pgrid)\n"
      "   --mixer   Enable audio mixer (Enabled by default for conferencing mode)\n"
      "   --mixeragc=[ 1 | 0 ] Enable or disable mixer Acoustic Gain Control (default=1)\n"
      "   --mixerdenoise=[ 1 | 0 ] Enable or disable mixer Denoise filter (default=1)\n"
      "   --mixervad=[ 1 | 0 ] Enable or disable mixer Voice Activity Detection (default=1)\n"
      "   --piphttp=[ address:port ] PIP control interface server listener (default=%s)\n"
      "                 Add / remove participants via the URL: http://[address]:[port]/pip\n"
      "   --piphttpmax=[ max ] Max simultaneous HTTP PIP control connections (default=0)\n"
      "\n",
      PIP_LISTEN_PORT_STR);

#endif // (VSX_HAVE_STREAMER) && (XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

}

static void usage_general(int argc, const const char *argv[]) {

  fprintf(stdout,
      " ============================================================================\n"
      "   General options.\n"
      " ============================================================================\n"
      "\n"
      " --conf=[ configuration file path ] (default=%s)\n"

#if defined(VSX_HAVE_LICENSE)
      " --license=[ license file path ] (default=%s)\n"
      " --liccheck      Print information about the installed license\n"
      " --licgen        Create a license request\n"
#endif // VSX_HAVE_LICENSE

      " --log=[ log file output path ]\n"
      "                 If no path is given the default is '%s'\n"
      " --logfilemaxsize=[ log file max size in bytes ] (default=%dKB)\n"
      " --pid=[ pid output file path ] Write PID to specified file path\n"
      " --verbose=[ level ],-v,-vvv  Increase log verbosity (default=%d)\n"
      "\n"
      //"   Show this help\n"
      //"\n"
      //" --help,-h:\n\n"
          , 
      VSX_CONF_PATH, 

#if defined(VSX_HAVE_LICENSE)
      LICENSE_FILEPATH_DEFAULT, 
#endif // VSX_HAVE_LICENSE

      DEFAULT_VSX_LOGPATH,
      LOGGER_MAX_FILE_SZ/1024,
      VSX_VERBOSITY_NORMAL);
}

static int usage(int argc, const char *argv[], const char *arg, int do_banner) {
  int handled = 1;

  if(do_banner) {
    banner(stdout);
  }

  if(arg) {

    fprintf(stdout, "\n");

    if(!strcasecmp(arg, "analyze")) {
      usage_analyze(argc, argv);
    //} else if(!strcasecmp(arg, "server") || !strcasecmp(arg, "broadcast")) {
    //  usage_server(argc, argv);
    } else if(!strcasecmp(arg, "capture")) {
      usage_capture(argc, argv);
#if defined(VSX_HAVE_STREAMER) && defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0) && \
     defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
    } else if(!strcasecmp(arg, "conference")) {
      usage_conference(argc, argv);
#endif // (VSX_HAVE_STREAMER) && (XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
    } else if(!strcasecmp(arg, "create")) {
      usage_create(argc, argv);
    } else if(!strcasecmp(arg, "dump")) {
      usage_dump(argc, argv);
    } else if(!strcasecmp(arg, "extract")) {
      usage_extract(argc, argv);
    } else if(!strcasecmp(arg, "general")) {
      usage_general(argc, argv);
    } else if(!strcasecmp(arg, "list")) {
      usage_list(argc, argv);
    } else if(!strcasecmp(arg, "stream")) {
      usage_stream(argc, argv);
    } else {
      handled = 0;
    }
    if(handled) {
      return -1;
    }
  }

  fprintf(stdout, "\nUsage: "VSX_BINARY" < mode > [ options ]\n" "\n");

  fprintf(stdout, "Please specify a mode to show specific help\n\n");
  fprintf(stdout, " -h analyze  ( --analyze )\n");
  fprintf(stdout, "     Show usage for media bitstream analysis.\n\n");
  //fprintf(stdout, " -h broadcast  ( --broadcast )\n");
  //fprintf(stdout, "     Show usage for broadcast control mode.\n\n");
  fprintf(stdout, " -h capture  ( --capture )\n");
  fprintf(stdout, "     Show usage for media capture.\n\n");
  fprintf(stdout, " -h conference ( --conference )\n");
  fprintf(stdout, "     Show usage for video conferencing.\n\n");
  fprintf(stdout, " -h create  ( --create )\n");
  fprintf(stdout, "     Show usage for container file creation.\n\n");
  fprintf(stdout, " -h dump  ( --dump )\n");
  fprintf(stdout, "     Show usage for dumping container file contents.\n\n");
  fprintf(stdout, " -h extract  ( --extract )\n");
  fprintf(stdout, "     Show usage for extracting raw media from container file.\n\n");
  fprintf(stdout, " -h stream  ( --stream )\n");
  fprintf(stdout, "     Show usage for streaming media output\n\n");
  fprintf(stdout, " -h general\n");
  fprintf(stdout, "     Show usage for general options\n\n");
  //fprintf(stdout, "For detailed command line usage and help refer to doc/VSX_userguide.pdf\n\n");

  return -1;
}


#if 1

static int write_pid(const char *path) {
  FILE_HANDLE fp;
  int pid;

  if((fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    return -1;
  }

  pid = getpid();

  fileops_Write(fp, "%d\n", pid);

  fileops_Close(fp);

  return pid;
}

const char *getnextarg(int argc, char *argv[]) {
  if(optarg) {
    return optarg;
  } else if(optind < argc) {
    return argv[optind];
  } else {
    return NULL;
  }
}

enum CMD_OPT {
  CMD_OPT_AUDIO = 256,     // do not mix with ascii chars
  CMD_OPT_VIDEO,
  CMD_OPT_AUTH_TYPE_ALLOW_BASIC,
  CMD_OPT_AUTH_TYPE_FORCE_BASIC,
  CMD_OPT_ABRAUTO,
  CMD_OPT_ABRSELF,
  CMD_OPT_FRAMETHIN,
  CMD_OPT_NODB,
  CMD_OPT_CONF,
  CMD_OPT_DIR,
  CMD_OPT_DBDIR,
  CMD_OPT_DURATION,
  CMD_OPT_TRANSPORT,
  CMD_OPT_FILTER,
  CMD_OPT_FPS,
  CMD_OPT_INPUT,
  CMD_OPT_CONFERENCEDRIVER,
  CMD_OPT_MIXER,
  CMD_OPT_MIXERVAD,
  CMD_OPT_MIXERSELF,
  CMD_OPT_MIXERAGC,
  CMD_OPT_MIXERDENOISE,
  CMD_OPT_PIPLAYOUT,
  CMD_OPT_PIPINPUT,
  CMD_OPT_PIPX,
  CMD_OPT_PIPXRIGHT,
  CMD_OPT_PIPY,
  CMD_OPT_PIPYBOTTOM,
  CMD_OPT_PIPALPHAMAX_MIN1,
  CMD_OPT_PIPALPHAMIN_MIN1,
  CMD_OPT_PIPBEFORE,
  CMD_OPT_PIPCONF,
  CMD_OPT_PIPMAX,
  CMD_OPT_PIPXCODE,
  //CMD_OPT_PIPCLOSEONEND,
  CMD_OPT_PIPHTTPMAX,
  CMD_OPT_PIPHTTPPORT,
  CMD_OPT_PIPZORDER,
  CMD_OPT_PIPNOAUDIO,
  CMD_OPT_PIPNOVIDEO,
  CMD_OPT_LISTEN,
  CMD_OPT_LICGEN,
  CMD_OPT_LICCHECK,
  CMD_OPT_LICFILE,
  CMD_OPT_OUTPUT,
  CMD_OPT_OUTPUT0,
  CMD_OPT_OUTPUT1,
  CMD_OPT_OUTPUT2,
  CMD_OPT_OUTPUT3,
  CMD_OPT_RTPMUX,
  CMD_OPT_RTCPMUX,
  CMD_OPT_RTCPDISABLE,
  CMD_OPT_RTCPIGNOREBYE,
  CMD_OPT_RTCPPORTS,
  CMD_OPT_RTCPPORTS0,
  CMD_OPT_RTCPPORTS1,
  CMD_OPT_RTCPPORTS2,
  CMD_OPT_RTCPPORTS3,
  CMD_OPT_HOME,
  CMD_OPT_LOOP,
  CMD_OPT_LOCALHOST,
  CMD_OPT_PROMISC,
  CMD_OPT_NOTFROMSEEK,
  CMD_OPT_OBJID,
  CMD_OPT_OVERWRITE,
  CMD_OPT_PES,
  CMD_OPT_NOSEQHDRS,
  CMD_OPT_QUEUE,
  CMD_OPT_QUEUEAUD,
  CMD_OPT_QUEUEVID,
  CMD_OPT_SDPOUT,
  CMD_OPT_STARTOFFSET,
  CMD_OPT_STTS,
  CMD_OPT_INSTALL,
  CMD_OPT_UNINSTALL,
  CMD_OPT_RAW,
  CMD_OPT_MTU,
  CMD_OPT_AUDIOAGGREGATEMS,
  CMD_OPT_RTP_PT,
  CMD_OPT_RTP_PKTZ_MODE,
  CMD_OPT_RTP_SSRC,
  CMD_OPT_RTP_CLOCK,
  CMD_OPT_RTP_BINDPORT,
  CMD_OPT_RTPMAX,
  CMD_OPT_FIR_XMIT,
  CMD_OPT_FIR_ACCEPT,
  CMD_OPT_NACK_RTPRETRANSMIT,
  CMD_OPT_NACK_XMIT,
  CMD_OPT_APPREMB_XMIT,
  CMD_OPT_APPREMB_XMITMAXRATE,
  CMD_OPT_APPREMB_XMITMINRATE,
  CMD_OPT_APPREMB_XMITFORCE,
  CMD_OPT_XCODE,
  CMD_OPT_LIVEPORT,
  CMD_OPT_LIVEMAX,
  CMD_OPT_LIVEPWD,
  CMD_OPT_HTTPMAX,
  CMD_OPT_HTTPLIVEMAX,
  CMD_OPT_RTMPLIVEADDRPORT,
  CMD_OPT_RTMPLIVEMAX,
  CMD_OPT_FLVLIVEADDRPORT,
  CMD_OPT_FLVLIVEMAX,
  CMD_OPT_FLVLIVEDELAY,
  CMD_OPT_FLVRECORD,
  CMD_OPT_FLVRECORD0,
  CMD_OPT_FLVRECORD1,
  CMD_OPT_FLVRECORD2,
  CMD_OPT_FLVRECORD3,
  //CMD_OPT_MOOFRECORD,
  CMD_OPT_MOOFLIVEPORT,
  CMD_OPT_MOOFLIVEMAX,
  CMD_OPT_MOOFSEGMOOF,
  CMD_OPT_MOOFSEGTS,
  CMD_OPT_MOOFLIVEDIR,
  CMD_OPT_MOOFLIVEFILEPREFIX,
  CMD_OPT_MOOFLIVEURLPREFIX,
  CMD_OPT_MOOFMP4MINDURATION,
  CMD_OPT_MOOFMP4MAXDURATION,
  CMD_OPT_MOOFTRAFMAXDURATION,
  CMD_OPT_DASHTSSEGDURATION,
  CMD_OPT_DASHMPDTYPE,
  CMD_OPT_MOOFUSEINIT,
  CMD_OPT_MOOFNODELETE,
  CMD_OPT_MOOFSYNCSEGMENTS,
  CMD_OPT_MKVLIVEDELAY,
  CMD_OPT_MKVLIVEADDRPORT,
  CMD_OPT_MKVLIVEMAX,
  CMD_OPT_MKVRECORD,
  CMD_OPT_MKVRECORD0,
  CMD_OPT_MKVRECORD1,
  CMD_OPT_MKVRECORD2,
  CMD_OPT_MKVRECORD3,
  CMD_OPT_RTSPLIVEADDRPORT,
  CMD_OPT_RTSPLIVEMAX,
  CMD_OPT_RTSPINTERLEAVED,
  CMD_OPT_RTSPSRTP,
  CMD_OPT_TSLIVEPORT,
  CMD_OPT_TSLIVEMAX,
  CMD_OPT_HTTPLIVEPORT,
  CMD_OPT_HTTPLIVESEGDURATION,
  CMD_OPT_HTTPLIVEFILEPREFIX,
  CMD_OPT_HTTPLIVEURLPREFIX,
  CMD_OPT_HTTPLIVEDIR,
  CMD_OPT_HTTPLIVEBITRATES,
  CMD_OPT_RTCPSR,
  CMD_OPT_RTCPRR,
  CMD_OPT_RTPFRAMEDROPPOLICY,
  CMD_OPT_CONNECTRETRY,
  CMD_OPT_AVTYPE,
  CMD_OPT_AVOFFSETRTCP,
  CMD_OPT_AVOFFSET,
  CMD_OPT_AVOFFSET0,
  CMD_OPT_AVOFFSET1,
  CMD_OPT_AVOFFSET2,
  CMD_OPT_AVOFFSET3,
  CMD_OPT_AVOFFSETPKTZMKV,
  CMD_OPT_AVOFFSETPKTZMKV_RECORD,
  CMD_OPT_NOAUDIO,
  CMD_OPT_NOVIDEO,
  CMD_OPT_RTP_CAPDELAY,
  CMD_OPT_RTP_CAPDELAY_VID,
  CMD_OPT_RTP_CAPDELAY_AUD,
  CMD_OPT_STREAM_PKTZ_MODE,
  CMD_OPT_STREAM_DELAY,
  CMD_OPT_RTMP_NOSIG,
  CMD_OPT_RTMP_PAGEURL,
  CMD_OPT_RTMP_SWFURL,
  CMD_OPT_RTMP_DORTMPT,
  CMD_OPT_STATUSMAX,
  CMD_OPT_STATUSPORT,
  CMD_OPT_CONFIGMAX,
  CMD_OPT_CONFIGPORT,
  CMD_OPT_CAPREALTIME,
  //CMD_OPT_LOGTIME,
  CMD_OPT_LOGPATH,
  CMD_OPT_LOGMAXSIZE,
  CMD_OPT_HTTPLOG,
  CMD_OPT_ENABLE_SYMLINK,
  CMD_OPT_STREAMSTATSFILE,
  CMD_OPT_SRTPKEY,
  CMD_OPT_SRTPKEY0,
  CMD_OPT_SRTPKEY1,
  CMD_OPT_SRTPKEY2,
  CMD_OPT_SRTPKEY3,
  CMD_OPT_SRTP,
  CMD_OPT_DTLSSRTP,
  //CMD_OPT_DTLSCLIENT,
  CMD_OPT_DTLSSERVER,
  //CMD_OPT_DTLSSERVERKEY,
  //CMD_OPT_DTLSXMITSERVERKEY,
  //CMD_OPT_DTLSCAPSERVERKEY,
  CMD_OPT_DTLS,
  CMD_OPT_DTLSCERTPATH,
  CMD_OPT_DTLSPRIVKEYPATH,
  CMD_OPT_STUNRESP,
  CMD_OPT_STUNREQ,
  CMD_OPT_STUNREQUSER,
  CMD_OPT_STUNREQPASS,
  CMD_OPT_STUNREQREALM,
  CMD_OPT_TURNSERVER,
  CMD_OPT_TURNPEER,
  CMD_OPT_TURNUSER,
  CMD_OPT_TURNPASS,
  CMD_OPT_TURNREALM,
  CMD_OPT_TURNSDPOUTPATH,
  CMD_OPT_TURNPOLICY,
  CMD_OPT_TOKENID,
  CMD_OPT_OUTQ_PREALLOC,
  CMD_OPT_DEBUG,
  CMD_OPT_DEBUG_CODEC,
  CMD_OPT_DEBUG_AUTH,
  CMD_OPT_DEBUG_HTTP,
  CMD_OPT_DEBUG_MKV,
  CMD_OPT_DEBUG_DASH,
  CMD_OPT_DEBUG_INFRAME,
  CMD_OPT_DEBUG_LIVE,
  CMD_OPT_DEBUG_MEM,
  CMD_OPT_DEBUG_NET,
  CMD_OPT_DEBUG_OUTFMT,
  CMD_OPT_DEBUG_REMB,
  CMD_OPT_DEBUG_RTP,
  CMD_OPT_DEBUG_RTCP,
  CMD_OPT_DEBUG_RTSP,
  CMD_OPT_DEBUG_RTMP,
  CMD_OPT_DEBUG_RTMPT,
  CMD_OPT_DEBUG_STREAMAV,
  CMD_OPT_DEBUG_SSL,
  CMD_OPT_DEBUG_SRTP,
  CMD_OPT_DEBUG_DTLS,
  CMD_OPT_DEBUG_XCODE,
  CMD_OPT_TEST,
};


int main(int argc, char *argv[]) {
  char curdir[VSX_MAX_PATH_LEN];
  //char pathbufs[2][VSX_MAX_PATH_LEN];
  //const char *arg_duration = NULL;
  int ch, i;
  unsigned int idx;
  int loggerLevel = S_INFO;
  VSXLIB_STREAM_PARAMS_T streamParams;
  const char *arg_audio = NULL;
  const char *arg_video = NULL;
  const char *arg_nodb = NULL;
  const char *arg_dbdir = NULL;
  const char *arg_fps = NULL;
  const char *arg_home = NULL;
  const char *arg_notfromseek = NULL;
  const char *arg_objid = NULL;
  const char *arg_stts = NULL;
  const char *arg_pidfile = NULL;
  int have_arg_stream = 0;
  int have_arg_stts = 0;
  int have_arg_input = 0;
  unsigned int idxFilters = 0;
  unsigned int idxInputs = 0;
  const char *prevHttpAddr = NULL;
  unsigned int idxPipInputs = 0;
  unsigned int idxTslive = 0;
  unsigned int idxFlvlive = 0;
  unsigned int idxMkvlive = 0;
  unsigned int idxHttplive = 0;
  unsigned int idxMooflive = 0;
  unsigned int idxLive = 0;
  unsigned int idxStatus = 0;
  unsigned int idxConfig = 0;
  unsigned int idxPip = 0;
  unsigned int idxRtmplive = 0;
  unsigned int idxRtsplive = 0;
  enum ACTION_TYPE action = ACTION_TYPE_NONE;
  int logoutputflags = 0;
  int rc;
  static const char *cmdopts ="-hacdelrsvxp:";

  struct option longopt[] = {
                 { "abrauto",     optional_argument,       NULL, CMD_OPT_ABRAUTO },
                 { "abrself",     optional_argument,       NULL, CMD_OPT_ABRSELF }, // experimental
                 { "abr",         optional_argument,       NULL, CMD_OPT_ABRAUTO },
                 { "analyze",     optional_argument,       NULL, 'a' },
                 { "audio",       no_argument,             NULL, CMD_OPT_AUDIO },
                 { "audq",        required_argument,       NULL, CMD_OPT_QUEUEAUD },
                 { "auth-basic",  optional_argument,       NULL, CMD_OPT_AUTH_TYPE_ALLOW_BASIC },
                 { "auth-basic-force", optional_argument,  NULL, CMD_OPT_AUTH_TYPE_FORCE_BASIC },
                 { "avtype",      required_argument,       NULL, CMD_OPT_AVTYPE },
                 { "avsync",      required_argument,       NULL, CMD_OPT_AVOFFSET },
                 { "avsync1",     required_argument,       NULL, CMD_OPT_AVOFFSET0 },
                 { "avsync2",     required_argument,       NULL, CMD_OPT_AVOFFSET1 },
                 { "avsync3",     required_argument,       NULL, CMD_OPT_AVOFFSET2 },
                 { "avsync4",     required_argument,       NULL, CMD_OPT_AVOFFSET3 },
                 { "mkvavsync",   required_argument,       NULL, CMD_OPT_AVOFFSETPKTZMKV },
                 { "mkvrecordavsync", required_argument,   NULL, CMD_OPT_AVOFFSETPKTZMKV_RECORD },
                 { "capture",     optional_argument,       NULL, 'c' },
                 { "config",      optional_argument,       NULL, CMD_OPT_CONFIGPORT },
                 { "configsrv",   optional_argument,       NULL, CMD_OPT_CONFIGPORT },
                 { "configmax",   required_argument,       NULL, CMD_OPT_CONFIGMAX },
                 { "create",      optional_argument,       NULL, 'r' },
                 { "conf",        optional_argument,       NULL, CMD_OPT_CONF },
                 { "dir",         required_argument,       NULL, CMD_OPT_DIR },
                 { "dbdir",       required_argument,       NULL, CMD_OPT_DBDIR },
                 { "delay",       required_argument,       NULL, CMD_OPT_STREAM_DELAY},
                 { "debug",       required_argument,       NULL, CMD_OPT_DEBUG },
                 { "debug-auth",  optional_argument,       NULL, CMD_OPT_DEBUG_AUTH },
                 { "debug-http",  optional_argument,       NULL, CMD_OPT_DEBUG_HTTP },
                 { "debug-mkv",   optional_argument,       NULL, CMD_OPT_DEBUG_MKV },
                 { "debug-codec", optional_argument,       NULL, CMD_OPT_DEBUG_CODEC },
                 { "debug-dash",  optional_argument,       NULL, CMD_OPT_DEBUG_DASH },
                 { "debug-live",  optional_argument,       NULL, CMD_OPT_DEBUG_LIVE  },
                 { "debug-inframe", optional_argument,     NULL, CMD_OPT_DEBUG_INFRAME },
                 { "debug-mem",   optional_argument,       NULL, CMD_OPT_DEBUG_MEM },
                 { "debug-net",   optional_argument,       NULL, CMD_OPT_DEBUG_NET  },
                 { "debug-outfmt",optional_argument,       NULL, CMD_OPT_DEBUG_OUTFMT },
                 { "debug-remb",  optional_argument,       NULL, CMD_OPT_DEBUG_REMB },
                 { "debug-rtsp ", optional_argument,       NULL, CMD_OPT_DEBUG_RTSP },
                 { "debug-rtp",   optional_argument,       NULL, CMD_OPT_DEBUG_RTP },
                 { "debug-rtcp",  optional_argument,       NULL, CMD_OPT_DEBUG_RTCP },
                 { "debug-rtmp",  optional_argument,       NULL, CMD_OPT_DEBUG_RTMP },
                 { "debug-rtmpt", optional_argument,       NULL, CMD_OPT_DEBUG_RTMPT },
                 { "debug-streamav", optional_argument,    NULL, CMD_OPT_DEBUG_STREAMAV },
                 { "debug-ssl",   optional_argument,       NULL, CMD_OPT_DEBUG_SSL },
                 { "debug-srtp",  optional_argument,       NULL, CMD_OPT_DEBUG_SRTP },
                 { "debug-dtls",  optional_argument,       NULL, CMD_OPT_DEBUG_DTLS},
                 { "debug-xcode", optional_argument,       NULL, CMD_OPT_DEBUG_XCODE },
                 { "dump",        optional_argument,       NULL, 'd' },
                 { "duration",    required_argument,       NULL, CMD_OPT_DURATION },
                 { "filter",      required_argument,       NULL, CMD_OPT_FILTER },
                 { "flvdelay",    required_argument,       NULL, CMD_OPT_FLVLIVEDELAY },
                 { "flvlive",     optional_argument,       NULL, CMD_OPT_FLVLIVEADDRPORT },
                 { "flvlivemax",  required_argument,       NULL, CMD_OPT_FLVLIVEMAX },
                 { "flvrecord",   required_argument,       NULL, CMD_OPT_FLVRECORD },
                 { "flvrecord1",  required_argument,       NULL, CMD_OPT_FLVRECORD0 },
                 { "flvrecord2",  required_argument,       NULL, CMD_OPT_FLVRECORD1 },
                 { "flvrecord3",  required_argument,       NULL, CMD_OPT_FLVRECORD2 },
                 { "flvrecord4",  required_argument,       NULL, CMD_OPT_FLVRECORD3 },
                 { "fps",         required_argument,       NULL, CMD_OPT_FPS },
                 { "framethin",   optional_argument,       NULL, CMD_OPT_FRAMETHIN },
                 { "in",          required_argument,       NULL, CMD_OPT_INPUT },
                 { "input",       required_argument,       NULL, CMD_OPT_INPUT },
                 { "help",        optional_argument,       NULL, 'h' },
                 { "h",           optional_argument,       NULL, 'h' },
                 { "home",        required_argument,       NULL, CMD_OPT_HOME },
                 { "list",        no_argument,             NULL, 'l' },
                 { "listen",      required_argument,       NULL, CMD_OPT_LISTEN },
                 { "live",        optional_argument,       NULL, CMD_OPT_LIVEPORT },
                 { "livemax",     required_argument,       NULL, CMD_OPT_LIVEMAX },
                 { "livepwd",     required_argument,       NULL, CMD_OPT_LIVEPWD },
                 { "httpmax",     required_argument,       NULL, CMD_OPT_HTTPMAX },
                 //{ "logtime",     no_argument,             NULL, CMD_OPT_LOGTIME },
                 { "logfile",     optional_argument,       NULL, CMD_OPT_LOGPATH },
                 { "log",         optional_argument,       NULL, CMD_OPT_LOGPATH },
                 { "httplog",     optional_argument,       NULL, CMD_OPT_HTTPLOG },
                 { "logfilemaxsize", required_argument,    NULL, CMD_OPT_LOGMAXSIZE },
                 { "loop",        no_argument,             NULL, CMD_OPT_LOOP },
                 { "localhost",   required_argument,       NULL, CMD_OPT_LOCALHOST },
                 { "licgen",      no_argument,             NULL, CMD_OPT_LICGEN },
                 { "liccheck",    no_argument,             NULL, CMD_OPT_LICCHECK },
                 { "license",     required_argument,       NULL, CMD_OPT_LICFILE },
                 { "maxrtpdelay", required_argument,       NULL, CMD_OPT_RTP_CAPDELAY},
                 { "maxrtpvideodelay", required_argument,  NULL, CMD_OPT_RTP_CAPDELAY_VID },
                 { "maxrtpaudiodelay", required_argument,  NULL, CMD_OPT_RTP_CAPDELAY_AUD },
                 { "mkvdelay",    required_argument,       NULL, CMD_OPT_MKVLIVEDELAY },
                 { "webmdelay",   required_argument,       NULL, CMD_OPT_MKVLIVEDELAY },
                 { "mkvlive",     optional_argument,       NULL, CMD_OPT_MKVLIVEADDRPORT },
                 { "mkvlivemax",  required_argument,       NULL, CMD_OPT_MKVLIVEMAX },
                 { "mkvrecord",   required_argument,       NULL, CMD_OPT_MKVRECORD },
                 { "mkvrecord1",  required_argument,       NULL, CMD_OPT_MKVRECORD0 },
                 { "mkvrecord2",  required_argument,       NULL, CMD_OPT_MKVRECORD1 },
                 { "mkvrecord3",  required_argument,       NULL, CMD_OPT_MKVRECORD2 },
                 { "mkvrecord4",  required_argument,       NULL, CMD_OPT_MKVRECORD3 },
                 { "webmlive",    optional_argument,       NULL, CMD_OPT_MKVLIVEADDRPORT },
                 { "webmlivemax", required_argument,       NULL, CMD_OPT_MKVLIVEMAX },
                 //{ "dashrecord",  required_argument,       NULL, CMD_OPT_MOOFRECORD },
                 { "dash",        optional_argument,       NULL, CMD_OPT_MOOFLIVEPORT },
                 { "dashmax",     required_argument,       NULL, CMD_OPT_MOOFLIVEMAX },
                 { "dashmoof",    optional_argument,       NULL, CMD_OPT_MOOFSEGMOOF },
                 { "dashts",      optional_argument,       NULL, CMD_OPT_MOOFSEGTS },
                 { "dashtsduration", optional_argument,    NULL, CMD_OPT_DASHTSSEGDURATION},
                 { "dashdir",     optional_argument,       NULL, CMD_OPT_MOOFLIVEDIR },
                 { "dashfileprefix", required_argument,    NULL, CMD_OPT_MOOFLIVEFILEPREFIX },
                 { "dashurlhost", required_argument,       NULL, CMD_OPT_MOOFLIVEURLPREFIX },
                 //{ "dashbw",  required_argument,         NULL, CMD_OPT_MOOFLIVEBITRATES },
                 { "dashminduration", required_argument,   NULL, CMD_OPT_MOOFMP4MINDURATION },
                 { "dashmaxduration", required_argument,   NULL, CMD_OPT_MOOFMP4MAXDURATION },
                 { "dashfragduration", required_argument,  NULL, CMD_OPT_MOOFTRAFMAXDURATION },
                 { "dashfragmentduration", required_argument,  NULL, CMD_OPT_MOOFTRAFMAXDURATION },
                 { "dashuseinit", optional_argument,       NULL, CMD_OPT_MOOFUSEINIT},
                 { "dashnodelete", optional_argument,      NULL, CMD_OPT_MOOFNODELETE },
                 { "dashsyncsegments", optional_argument,  NULL, CMD_OPT_MOOFSYNCSEGMENTS},
                 { "dashmpdtype", required_argument,       NULL, CMD_OPT_DASHMPDTYPE },

                 { "media",       required_argument,       NULL, CMD_OPT_DIR },
                 { "mtu",         required_argument,       NULL, CMD_OPT_MTU },
                 { "rtpmaxptime", required_argument,       NULL, CMD_OPT_AUDIOAGGREGATEMS },
                 { "rtpmax",      required_argument,       NULL, CMD_OPT_RTPMAX },
                 { "rtppayloadtype", required_argument,    NULL, CMD_OPT_RTP_PT },
                 { "rtppktzmode", required_argument,       NULL, CMD_OPT_RTP_PKTZ_MODE},
                 { "rtpssrc",     required_argument,       NULL, CMD_OPT_RTP_SSRC },
                 { "rtpclock",    required_argument,       NULL, CMD_OPT_RTP_CLOCK},
                 { "rtpusebindport", optional_argument,    NULL, CMD_OPT_RTP_BINDPORT},
                 { "rtpretransmit", optional_argument,     NULL, CMD_OPT_NACK_RTPRETRANSMIT },
                 { "nodb",        no_argument,             NULL, CMD_OPT_NODB },
                 { "noseqhdrs",   no_argument,             NULL, CMD_OPT_NOSEQHDRS },
                 { "noseekfile",  no_argument,             NULL, CMD_OPT_NOTFROMSEEK },
                 { "novid",       no_argument,             NULL, CMD_OPT_NOVIDEO },
                 { "novideo",     no_argument,             NULL, CMD_OPT_NOVIDEO },
                 { "noaud",       no_argument,             NULL, CMD_OPT_NOAUDIO },
                 { "noaudio",     no_argument,             NULL, CMD_OPT_NOAUDIO },
                 { "objtype",     required_argument,       NULL, CMD_OPT_OBJID },
                 { "out",         required_argument,       NULL, CMD_OPT_OUTPUT },
                 { "output",      required_argument,       NULL, CMD_OPT_OUTPUT },
                 { "out1",        required_argument,       NULL, CMD_OPT_OUTPUT0 },
                 { "out2",        required_argument,       NULL, CMD_OPT_OUTPUT1 },
                 { "out3",        required_argument,       NULL, CMD_OPT_OUTPUT2 },
                 { "out4",        required_argument,       NULL, CMD_OPT_OUTPUT3 },
                 { "rtpmux",      no_argument      ,       NULL, CMD_OPT_RTPMUX },
                 { "rtp-mux",     no_argument      ,       NULL, CMD_OPT_RTPMUX },
                 { "rtcpdisable", no_argument      ,       NULL, CMD_OPT_RTCPDISABLE },
                 { "nortcpbye",   no_argument      ,       NULL, CMD_OPT_RTCPIGNOREBYE },
                 { "rtcpmux",     no_argument      ,       NULL, CMD_OPT_RTCPMUX },
                 { "rtcp-mux",    no_argument      ,       NULL, CMD_OPT_RTCPMUX },
                 { "rtcpports",   required_argument,       NULL, CMD_OPT_RTCPPORTS },
                 { "rtcpports1",  required_argument,       NULL, CMD_OPT_RTCPPORTS0 },
                 { "rtcpports2",  required_argument,       NULL, CMD_OPT_RTCPPORTS1 },
                 { "rtcpports3",  required_argument,       NULL, CMD_OPT_RTCPPORTS2 },
                 { "rtcpports4",  required_argument,       NULL, CMD_OPT_RTCPPORTS3 },
                 { "overwrite",   no_argument,             NULL, CMD_OPT_OVERWRITE },
                 { "packmode",    required_argument,       NULL, CMD_OPT_STREAM_PKTZ_MODE },
                 { "pes",         optional_argument,       NULL, CMD_OPT_PES },
                 { "pid",         required_argument,       NULL, 'p'},
#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
                 { "conference",  no_argument,             NULL, CMD_OPT_CONFERENCEDRIVER },
                 { "mixer",       optional_argument,       NULL, CMD_OPT_MIXER },
                 { "layout",      required_argument,       NULL, CMD_OPT_PIPLAYOUT },
                 { "mixervad",    optional_argument,       NULL, CMD_OPT_MIXERVAD },
                 { "mixerself",   optional_argument,       NULL, CMD_OPT_MIXERSELF },
                 { "mixeragc",    optional_argument,       NULL, CMD_OPT_MIXERAGC },
                 { "mixerdenoise",optional_argument,       NULL, CMD_OPT_MIXERDENOISE },
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
                 { "pip",         required_argument,       NULL, CMD_OPT_PIPINPUT },
                 { "pipx",        required_argument,       NULL, CMD_OPT_PIPX },
                 { "pipxleft",    required_argument,       NULL, CMD_OPT_PIPX },
                 { "pipxright",   required_argument,       NULL, CMD_OPT_PIPXRIGHT },
                 { "pipy",        required_argument,       NULL, CMD_OPT_PIPY },
                 { "pipytop",     required_argument,       NULL, CMD_OPT_PIPY },
                 { "pipybottom",  required_argument,       NULL, CMD_OPT_PIPYBOTTOM },
                 { "pipbefore",   no_argument,             NULL, CMD_OPT_PIPBEFORE},
                 //{ "pipclose",    no_argument,             NULL, CMD_OPT_PIPCLOSEONEND},
                 { "pipalphamax", required_argument,       NULL, CMD_OPT_PIPALPHAMAX_MIN1},
                 { "pipalphamin", required_argument,       NULL, CMD_OPT_PIPALPHAMIN_MIN1},
                 { "piphttpmax",  optional_argument,       NULL, CMD_OPT_PIPHTTPMAX},
                 { "piphttp",     optional_argument,       NULL, CMD_OPT_PIPHTTPPORT},
                 { "pipxcode",    optional_argument,       NULL, CMD_OPT_PIPXCODE},
                 { "pipconf",     required_argument,       NULL, CMD_OPT_PIPCONF},
                 { "pipmax",      required_argument,       NULL, CMD_OPT_PIPMAX},
                 { "pipzorder",   required_argument,       NULL, CMD_OPT_PIPZORDER },
                 { "pipnoaud",    no_argument,             NULL, CMD_OPT_PIPNOAUDIO },
                 { "pipnoaudio",  no_argument,             NULL, CMD_OPT_PIPNOAUDIO },
                 { "pipnovid",    no_argument,             NULL, CMD_OPT_PIPNOVIDEO },
                 { "pipnovideo",  no_argument,             NULL, CMD_OPT_PIPNOVIDEO },
#endif // XCODE_HAVE_PIP
                 { "promisc",     no_argument,             NULL, CMD_OPT_PROMISC },
                 { "queue",       required_argument,       NULL, CMD_OPT_QUEUE },
#if defined(WIN32) && !defined(__MINGW32__)
                 { "raw",         required_argument,       NULL, CMD_OPT_RAW },
#endif // WIN32
                 { "retry",       optional_argument,       NULL, CMD_OPT_CONNECTRETRY },
                 { "realtime",    optional_argument,       NULL, CMD_OPT_CAPREALTIME },
                 { "rtcpavsync",  required_argument,       NULL, CMD_OPT_AVOFFSETRTCP},
                 { "rtcpsr",      required_argument,       NULL, CMD_OPT_RTCPSR },
                 { "rtcprr",      optional_argument,       NULL, CMD_OPT_RTCPRR },
                 { "rtpframedrop",required_argument,       NULL, CMD_OPT_RTPFRAMEDROPPOLICY },

                 { "nackxmit",    optional_argument,       NULL, CMD_OPT_NACK_XMIT },
                 { "rembxmit",    optional_argument,       NULL, CMD_OPT_APPREMB_XMIT },
                 { "rembxmitmaxrate", required_argument,   NULL, CMD_OPT_APPREMB_XMITMAXRATE },
                 { "rembxmitminrate", required_argument,   NULL, CMD_OPT_APPREMB_XMITMINRATE },
                 { "rembxmitforce",   optional_argument,   NULL, CMD_OPT_APPREMB_XMITFORCE },

                 { "firxmit",     optional_argument,       NULL, CMD_OPT_FIR_XMIT },
                 { "firaccept",   optional_argument,       NULL, CMD_OPT_FIR_ACCEPT },

                 { "rtmp",        optional_argument,       NULL, CMD_OPT_RTMPLIVEADDRPORT },
                 { "rtmpmax",     required_argument,       NULL, CMD_OPT_RTMPLIVEMAX },
                 { "rtmpnosig",   no_argument,             NULL, CMD_OPT_RTMP_NOSIG },
                 { "rtmppageurl", required_argument,       NULL, CMD_OPT_RTMP_PAGEURL },
                 { "rtmpswfurl",  required_argument,       NULL, CMD_OPT_RTMP_SWFURL },
                 { "rtmpt",       optional_argument,       NULL, CMD_OPT_RTMP_DORTMPT },
                 { "rtsp",        optional_argument,       NULL, CMD_OPT_RTSPLIVEADDRPORT },
                 { "rtspmax",     required_argument,       NULL, CMD_OPT_RTSPLIVEMAX },
                 { "rtsp-interleaved", optional_argument,  NULL, CMD_OPT_RTSPINTERLEAVED },
                 { "rtsp-srtp",   optional_argument,       NULL, CMD_OPT_RTSPSRTP },
                 { "httplive",    optional_argument,       NULL, CMD_OPT_HTTPLIVEPORT },
                 { "httplivemax", required_argument,       NULL, CMD_OPT_HTTPLIVEMAX },
                 { "httplivedir", optional_argument,       NULL, CMD_OPT_HTTPLIVEDIR },
                 { "httplivechunk", optional_argument,     NULL, CMD_OPT_HTTPLIVESEGDURATION },
                 { "httpliveprefix", required_argument,    NULL, CMD_OPT_HTTPLIVEFILEPREFIX },
                 { "httplivefileprefix", required_argument,NULL, CMD_OPT_HTTPLIVEFILEPREFIX },
                 { "httpliveurlhost", required_argument,   NULL, CMD_OPT_HTTPLIVEURLPREFIX },
                 { "httplivebw",  required_argument,       NULL, CMD_OPT_HTTPLIVEBITRATES },
                 { "broadcast",   optional_argument,       NULL, 'e' },
                 { "sdp",         required_argument,       NULL, CMD_OPT_SDPOUT },
                 { "srtp",        optional_argument,       NULL, CMD_OPT_SRTP },
                 //{ "sdes",        optional_argument,       NULL, CMD_OPT_SRTP },
                 { "srtpkey",     required_argument,       NULL, CMD_OPT_SRTPKEY },
                 { "srtpkey1",    required_argument,       NULL, CMD_OPT_SRTPKEY0 },
                 { "srtpkey2",    required_argument,       NULL, CMD_OPT_SRTPKEY1 },
                 { "srtpkey3",    required_argument,       NULL, CMD_OPT_SRTPKEY2 },
                 { "srtpkey4",    required_argument,       NULL, CMD_OPT_SRTPKEY3 },
                 //{ "stream",      optional_argument,       NULL, 's' },
                 { "stream",      optional_argument,       NULL, CMD_OPT_OUTPUT },
                 { "stream1",     required_argument,       NULL, CMD_OPT_OUTPUT0 },
                 { "stream2",     required_argument,       NULL, CMD_OPT_OUTPUT1 },
                 { "stream3",     required_argument,       NULL, CMD_OPT_OUTPUT2 },
                 { "stream4",     required_argument,       NULL, CMD_OPT_OUTPUT3 },
                 { "symlink",     optional_argument,       NULL, CMD_OPT_ENABLE_SYMLINK },
                 //{ "dtlssrtp",    optional_argument,       NULL, CMD_OPT_DTLSSRTP },
                 { "dtls-srtp",   optional_argument,       NULL, CMD_OPT_DTLSSRTP },
                 { "dtls",        optional_argument,       NULL, CMD_OPT_DTLS },
                 //{ "dtlsclient",  optional_argument,       NULL, CMD_OPT_DTLSCLIENT },
                 { "dtlsserver",  optional_argument,       NULL, CMD_OPT_DTLSSERVER },
                 //{ "dtlsserverkey", optional_argument,    NULL, CMD_OPT_DTLSSERVERKEY },
                 //{ "dtlsinputserverkey", optional_argument, NULL, CMD_OPT_DTLSCAPSERVERKEY },
                 //{ "dtlsoutputserverkey", optional_argument, NULL, CMD_OPT_DTLSXMITSERVERKEY },
                 { "dtlscert",    required_argument,       NULL, CMD_OPT_DTLSCERTPATH },
                 { "dtlskey",     required_argument,       NULL, CMD_OPT_DTLSPRIVKEYPATH },
                 { "stunrespond", optional_argument,       NULL, CMD_OPT_STUNRESP },
                 { "stunrequest", optional_argument,       NULL, CMD_OPT_STUNREQ },
                 { "stunrequestuser", required_argument,   NULL, CMD_OPT_STUNREQUSER },
                 { "stunrequestpass", required_argument,   NULL, CMD_OPT_STUNREQPASS },
                 { "stunrequestrealm", required_argument,  NULL, CMD_OPT_STUNREQREALM },

                 { "turnserver",  required_argument,       NULL, CMD_OPT_TURNSERVER },
                 { "turnpeer",    required_argument,       NULL, CMD_OPT_TURNPEER },
                 { "turnuser",    required_argument,       NULL, CMD_OPT_TURNUSER },
                 { "turnpass",    required_argument,       NULL, CMD_OPT_TURNPASS },
                 { "turnrealm",   required_argument,       NULL, CMD_OPT_TURNREALM },
                 { "turnsdp",     required_argument,       NULL, CMD_OPT_TURNSDPOUTPATH },
                 { "turnpolicy",  optional_argument,       NULL, CMD_OPT_TURNPOLICY},

                 { "start",       required_argument,       NULL, CMD_OPT_STARTOFFSET },
                 { "streamstats", optional_argument,       NULL, CMD_OPT_STREAMSTATSFILE },
                 { "status",      optional_argument,       NULL, CMD_OPT_STATUSPORT },
                 { "statusmax",   required_argument,       NULL, CMD_OPT_STATUSMAX },
                 { "stts",        optional_argument,       NULL, CMD_OPT_STTS },
                 { "test",        optional_argument,       NULL, CMD_OPT_TEST },
                 { "token",       required_argument,       NULL, CMD_OPT_TOKENID },
                 { "prealloc",    optional_argument,       NULL, CMD_OPT_OUTQ_PREALLOC },
                 { "tslive",      optional_argument,       NULL, CMD_OPT_TSLIVEPORT },
                 { "tslivemax",   required_argument,       NULL, CMD_OPT_TSLIVEMAX },
                 { "transport",   required_argument,       NULL, CMD_OPT_TRANSPORT },
                 { "rtptransport",required_argument,       NULL, CMD_OPT_TRANSPORT },
                 { "verbose",     optional_argument,       NULL, 'v' },
                 { "video",       no_argument,             NULL, CMD_OPT_VIDEO },
                 { "vidq",        required_argument,       NULL, CMD_OPT_QUEUEVID },
                 { "extract",     optional_argument,       NULL, 'x' },
                 { "xcode",       required_argument,       NULL, CMD_OPT_XCODE },
#if defined(CFG_HAVE_WIN32_SERVICE) && defined(WIN32) && !defined(__MINGW32__)
                 { "install",     no_argument,             NULL, CMD_OPT_INSTALL },
                 { "uninstall",   no_argument,             NULL, CMD_OPT_UNINSTALL },
#endif // WIN32
                 { NULL,          0,                       NULL,  0  }
                      };

#ifdef WIN32

  WSADATA wsaData;
  WSAStartup(MAKEWORD(2,2), &wsaData);

#else

  signal(SIGINT, sig_hdlr);
  signal(SIGUSR1, sig_hdlr);

#endif // WIN32

  //
  // Setup default parameters
  //
  vsxlib_open(&streamParams);
  g_streamParams = &streamParams;

#ifdef __APPLE__
    while(optind < argc) {
        if((ch = getopt_long(argc, argv, cmdopts, longopt, NULL)) == -1) {
        ch = 1;
        optarg = argv[optind];
        optind++;
      }
#else
  while( (ch = getopt_long(argc, argv, cmdopts, longopt, NULL)) != -1 ) {
#endif // __APPLE__

    switch(ch) {
      //case 1:
       //  = optarg;
      //  break;
      case 'a':
        action = ACTION_TYPE_ANALYZE_MEDIA;
        if(optarg) {
          streamParams.inputs[0] = optarg;
        }
        break;
      case 'c':
        action |= (ACTION_TYPE_CAPTURE & ACTION_TYPE_CAPSTREAM_MASK);
        if(optarg) {
          if(idxInputs < 2) {
            streamParams.inputs[idxInputs++] = optarg;
          }
        }
        break;
      case 'd':
        action = ACTION_TYPE_DUMP_CONTAINER;
        if(optarg) {
          streamParams.inputs[0] = optarg;
        }
        break;
      //case 'e':
      //  action = ACTION_TYPE_SERVER;
      //  if(optarg) {
      //    streamParams.inputs[0] = optarg;
      //  }
      //  break;
      case 'l':
        action = ACTION_TYPE_CAPTURE_LIST_INTERFACES;
        break;
      case 'p':
        // dummy argument to allow passing unique instance id
        arg_pidfile = optarg;
        break;
      case 'r':
        action = ACTION_TYPE_CREATE_MP4;
        if(optarg) {
          streamParams.inputs[0] = optarg;
        }
        break;
      //case 's':
      //  action |= (ACTION_TYPE_STREAM & ACTION_TYPE_CAPSTREAM_MASK);
      //  if(optarg) {
      //    streamParams.outputs[0] = optarg;
      //  }
      //  break;
      case 'x':
        action = (ACTION_TYPE_EXTRACT_VIDEO | ACTION_TYPE_EXTRACT_AUDIO);
        if(optarg) {
          streamParams.inputs[0] = optarg;
        }
        break;
      case 'v':
        if(optarg && (rc = atoi(optarg)) >= 0) {
          streamParams.verbosity = rc;
        } else {
          streamParams.verbosity++;
        }
        break;
      case CMD_OPT_AUDIO:
        arg_audio = "";
        break;
      case CMD_OPT_AUTH_TYPE_ALLOW_BASIC:
          streamParams.httpauthtype = HTTP_AUTH_TYPE_ALLOW_BASIC;
        break;
      case CMD_OPT_AUTH_TYPE_FORCE_BASIC:
          streamParams.httpauthtype = HTTP_AUTH_TYPE_FORCE_BASIC;
        break;
      case CMD_OPT_AVTYPE:
        if(optarg) {
            streamParams.avReaderType = atoi(optarg);
        }
        break;
      case CMD_OPT_AVOFFSETPKTZMKV:
        if(optarg) {
          streamParams.faoffset_mkvpktz = atof(optarg); 
        }
        break;
      case CMD_OPT_AVOFFSETPKTZMKV_RECORD:
        if(optarg) {
          streamParams.faoffset_mkvpktz_record = atof(optarg); 
        }
        break;
      case CMD_OPT_AVOFFSET:
        if(optarg) {
          for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
            streamParams.haveavoffsets[idx] = 1;
            streamParams.favoffsets[idx] = atof(optarg); 
          }
        }
        break;
      case CMD_OPT_AVOFFSET0:
        if(optarg) {
          streamParams.haveavoffsets[0] = 2;
          streamParams.favoffsets[0] = atof(optarg); 
        }
        break;
      case CMD_OPT_AVOFFSET1:
        if(optarg && 1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.haveavoffsets[1] = 2;
          streamParams.favoffsets[1] = atof(optarg); 
        }
        break;
      case CMD_OPT_AVOFFSET2:
        if(optarg && 2 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.haveavoffsets[2] = 2;
          streamParams.favoffsets[2] = atof(optarg); 
        }
        break;
      case CMD_OPT_AVOFFSET3:
        if(optarg && 3 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.haveavoffsets[3] = 2;
          streamParams.favoffsets[3] = atof(optarg); 
        }
        break;
      case CMD_OPT_AVOFFSETRTCP:
        if(optarg) {
          streamParams.haveavoffsetrtcp = 1;
          streamParams.favoffsetrtcp = atof(optarg); 
        }
        break;
      case CMD_OPT_CAPREALTIME:
        streamParams.caprealtime = 1;
        break;
      case CMD_OPT_CONNECTRETRY:
        if(optarg) {
          if((streamParams.connectretrycntminone = atoi(optarg)) > 0) {
            streamParams.connectretrycntminone++;
          }
        } else {
          streamParams.connectretrycntminone = 1;
        }
        break;
      case CMD_OPT_RTP_CAPDELAY:
        i = atoi(optarg);
        streamParams.capture_max_rtpplayoutviddelayms = i;
        streamParams.capture_max_rtpplayoutauddelayms = i; 
        break;
      case CMD_OPT_RTP_CAPDELAY_AUD:
        streamParams.capture_max_rtpplayoutauddelayms = atoi(optarg);
        break;
      case CMD_OPT_RTP_CAPDELAY_VID:
        streamParams.capture_max_rtpplayoutviddelayms = atoi(optarg);
        break;
      case CMD_OPT_CONF:
        if(optarg) {
          streamParams.confpath = optarg;
        } else {
          streamParams.confpath = VSX_CONF_PATH;
        }
        break;
      case CMD_OPT_CONFIGMAX:
        streamParams.configmax = atoi(optarg);
        break;
      case CMD_OPT_CONFIGPORT:
        if(idxConfig < sizeof(streamParams.configaddr) / sizeof(streamParams.configaddr[0])) {
          if(optarg) {
            streamParams.configaddr[idxConfig] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.configaddr[idxConfig] = prevHttpAddr ? prevHttpAddr : CONFIG_LISTEN_PORT_STR;
          }
        }
        idxConfig++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_DBDIR:
        arg_dbdir = optarg;
        break;
      case CMD_OPT_DIR:
        streamParams.dir = optarg;
        break;
      case CMD_OPT_DURATION:
        //arg_duration = optarg;
        streamParams.strdurationmax = optarg; 
        break;
      case CMD_OPT_FILTER:
        if(idxFilters < 2) {
          streamParams.strfilters[idxFilters++] = optarg;
        }
        break;
      case CMD_OPT_FLVLIVEDELAY:
        streamParams.flvlivedelay = atof(optarg);
        break;
      case CMD_OPT_FLVLIVEMAX:
        streamParams.flvlivemax = atoi(optarg);
        break;
      case CMD_OPT_FLVLIVEADDRPORT:
        if(idxFlvlive < sizeof(streamParams.flvliveaddr) / sizeof(streamParams.flvliveaddr[0])) {
          if(optarg) {
            streamParams.flvliveaddr[idxFlvlive] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.flvliveaddr[idxFlvlive] = prevHttpAddr ? prevHttpAddr : FLVLIVE_LISTEN_PORT_STR;
          }
        }
        idxFlvlive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_FLVRECORD:
        for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
          streamParams.flvrecord[idx] = optarg;
        }
        have_arg_stream = 1;
        break;
      case CMD_OPT_FLVRECORD0:
        streamParams.flvrecord[0] = optarg;
        have_arg_stream = 1;
        break;
      case CMD_OPT_FLVRECORD1:
        if(1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.flvrecord[1] = optarg;
          have_arg_stream = 1;
        }
        break;
      case CMD_OPT_FLVRECORD2:
        if(2 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.flvrecord[2] = optarg;
          have_arg_stream = 1;
        }
        break;
      case CMD_OPT_FLVRECORD3:
        if(3 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.flvrecord[3] = optarg;
          have_arg_stream = 1;
        } 
      case CMD_OPT_FPS:
        arg_fps = optarg;
        break;
      case CMD_OPT_HOME:
        arg_home = optarg;
        break;
      case CMD_OPT_HTTPLIVEMAX:
        streamParams.httplivemax = atoi(optarg);
        break;
      case CMD_OPT_HTTPLIVEPORT:
        if(idxHttplive < sizeof(streamParams.httpliveaddr) / sizeof(streamParams.httpliveaddr[0])) {
          if(optarg) {
            streamParams.httpliveaddr[idxHttplive] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.httpliveaddr[idxHttplive] = prevHttpAddr ? prevHttpAddr : HTTPLIVE_LISTEN_PORT_STR;
          }
        }
        idxHttplive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_DASHTSSEGDURATION:
        streamParams.dash_ts_segments = 1;
        //
        // Fall through to set the segment duration
        //
      case CMD_OPT_HTTPLIVESEGDURATION:
        if(optarg) {
          streamParams.httplive_chunkduration = atof(optarg);
        } else {
          // Invalid value will be re-set to default
          streamParams.httplive_chunkduration = -1;
        }
        break;
      case CMD_OPT_HTTPLIVEFILEPREFIX:
          streamParams.httplivefileprefix = optarg;
        break;
      case CMD_OPT_HTTPLIVEURLPREFIX:
          streamParams.httpliveuriprefix = optarg;
        break;
      case CMD_OPT_HTTPLIVEDIR:
          streamParams.httplivedir = optarg;
        break;
      case CMD_OPT_HTTPLIVEBITRATES:
        streamParams.httplivebitrates = optarg;
        break;
      case CMD_OPT_INPUT:
        have_arg_input = 1;
      case CMD_OPT_LISTEN:
        if(idxInputs < 2) {
          streamParams.inputs[idxInputs] = optarg;
          idxInputs++;
        }
        break;
      case CMD_OPT_LICGEN:
        action = ACTION_TYPE_LICENSE_GEN;
        break;
      case CMD_OPT_LICCHECK:
        action = ACTION_TYPE_LICENSE_CHECK;
        break;
      case CMD_OPT_LICFILE:
        streamParams.licfilepath = optarg;
        break;
      case CMD_OPT_LIVEPORT:
        if(idxLive < sizeof(streamParams.liveaddr) / sizeof(streamParams.liveaddr[0])) {
          if(optarg) {
            streamParams.liveaddr[idxLive] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.liveaddr[idxLive] = prevHttpAddr ? prevHttpAddr : HTTP_LISTEN_PORT_STR;
          }
        }
        idxLive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_LIVEMAX:
        streamParams.livemax = atoi(optarg);
        break;
      case CMD_OPT_LIVEPWD:
        streamParams.livepwd = optarg;
        break;
      case CMD_OPT_HTTPMAX:
        streamParams.httpmax = atoi(optarg);
        break;
      case CMD_OPT_RTPMAX:
        streamParams.rtplivemax = atoi(optarg);
        break;
      //case CMD_OPT_LOGTIME:
      //  logoutputflags = LOG_OUTPUT_PRINT_DATE_STDERR;
      //  break;
      case CMD_OPT_LOGPATH:
        streamParams.logfile = optarg ? optarg : DEFAULT_VSX_LOGPATH;
        break;
      case CMD_OPT_LOGMAXSIZE:
        streamParams.logmaxsz = (unsigned int) strutil_read_numeric(optarg, 0, 0, 0);
        break;
      case CMD_OPT_HTTPLOG:
        streamParams.httpaccesslogfile = optarg ? optarg : DEFAULT_HTTPACCESS_LOGPATH;
        break;
      case CMD_OPT_LOOP:
        streamParams.loop = 1;
        break;
      case CMD_OPT_LOCALHOST:
        streamParams.strlocalhost = optarg;
        break;
      case CMD_OPT_MKVLIVEDELAY:
        streamParams.mkvlivedelay = atof(optarg);
        break;
      case CMD_OPT_MKVLIVEMAX:
        streamParams.mkvlivemax = atoi(optarg);
        break;
      case CMD_OPT_MKVLIVEADDRPORT:
        if(idxMkvlive < sizeof(streamParams.mkvliveaddr) / sizeof(streamParams.mkvliveaddr[0])) {
          if(optarg) {
            streamParams.mkvliveaddr[idxMkvlive] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.mkvliveaddr[idxMkvlive] = prevHttpAddr ? prevHttpAddr : MKVLIVE_LISTEN_PORT_STR;
          }
        }
        idxMkvlive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_MKVRECORD:
        for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
          streamParams.mkvrecord[idx] = optarg;
        }
        have_arg_stream = 1;
        break;
      case CMD_OPT_MKVRECORD0:
        streamParams.mkvrecord[0] = optarg;
        have_arg_stream = 1;
        break;
      case CMD_OPT_MKVRECORD1:
        if(1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.mkvrecord[1] = optarg;
          have_arg_stream = 1;
        }
        break;
      case CMD_OPT_MKVRECORD2:
        if(2 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.mkvrecord[2] = optarg;
          have_arg_stream = 1;
        }
        break;
      case CMD_OPT_MKVRECORD3:
        if(3 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.mkvrecord[3] = optarg;
          have_arg_stream = 1;
        } 
      //case CMD_OPT_MOOFRECORD:
      //  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      //    streamParams.moofrecord[idx] = optarg;
      //  }
      //  have_arg_stream = 1;
      //  break;
      case CMD_OPT_MOOFLIVEDIR:
        streamParams.moofdir = optarg;
        break;
      case CMD_OPT_MOOFLIVEFILEPREFIX:
        streamParams.mooffileprefix = optarg;
        break;

      case CMD_OPT_MOOFMP4MINDURATION:
        if(optarg) {
          streamParams.moofMp4MinDurationSec = atof(optarg);
          if(MOOF_USE_RAP_DEFAULT && streamParams.moofMp4MinDurationSec > 0) {
            // Ensure moof packetizer RAP is on, to propagate the min keyframe duration setting
            streamParams.moofUseRAP = 1;
          }
        }
        break;
      case CMD_OPT_MOOFMP4MAXDURATION:
        if(optarg) {
          streamParams.moofMp4MaxDurationSec = atof(optarg);
        }
        break;
      case CMD_OPT_MOOFTRAFMAXDURATION:
        if(optarg) {
          streamParams.moofTrafMaxDurationSec = atof(optarg);
        }
        break;
      case CMD_OPT_MOOFUSEINIT:
        if(optarg) {
          streamParams.moofUseInitMp4 = atoi(optarg);
        } else {
          streamParams.moofUseInitMp4 = 1;
        }
        break;
      case CMD_OPT_MOOFNODELETE:
        if(optarg) {
          streamParams.moof_nodelete = atoi(optarg);
        } else {
          streamParams.moof_nodelete = 1;
        }
        break;
      case CMD_OPT_MOOFSYNCSEGMENTS:
        if(optarg) {
          streamParams.moofSyncSegments = atoi(optarg);
        } else {
          streamParams.moofSyncSegments = 1;
        }
        break;
      case CMD_OPT_DASHMPDTYPE:
        if(optarg) {
          streamParams.dash_mpd_type = dash_mpd_str_to_type(optarg);
        }
        break;
      case CMD_OPT_MOOFLIVEURLPREFIX:
        streamParams.moofuriprefix = optarg;
        break;
      case CMD_OPT_MOOFLIVEMAX:
        streamParams.dashlivemax = atoi(optarg);
        break;
      case CMD_OPT_MOOFSEGMOOF:
        if(optarg) {
          streamParams.dash_moof_segments = atoi(optarg);
        } else {
          streamParams.dash_moof_segments = 1;
        }
        //have_arg_dash_moof = 1;
        break;
      case CMD_OPT_MOOFSEGTS:
        if(optarg) {
          streamParams.dash_ts_segments = atoi(optarg);
        } else {
          streamParams.dash_ts_segments = 1;
        }
        //have_arg_dash_ts = 1;
        break;
      case CMD_OPT_MOOFLIVEPORT:
        if(idxMooflive < sizeof(streamParams.dashliveaddr) / sizeof(streamParams.dashliveaddr[0])) {
          if(optarg) {
            streamParams.dashliveaddr[idxMooflive] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.dashliveaddr[idxMooflive] = prevHttpAddr ? prevHttpAddr : MOOFLIVE_LISTEN_PORT_STR;
          }

          //for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
          //  //streamParams.moofrecord[idx] = streamParams.dashliveaddr[idxMooflive];
          //  streamParams.moofrecord[idx] = "1";
          //}

        }
        if(streamParams.dash_moof_segments == -1) {
          streamParams.dash_moof_segments = 1;
        }
        idxMooflive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_MTU:
        if(optarg) { 
          streamParams.mtu = atoi(optarg);
        }
        break;
      case CMD_OPT_AUDIOAGGREGATEMS:
        if(optarg) { 
          streamParams.audioAggregatePktsMs = atoi(optarg);
        }
        break;
      case CMD_OPT_RTP_PT:
        streamParams.output_rtpptstr = optarg;
        break;
      case CMD_OPT_RTP_PKTZ_MODE:
        if(optarg) {
          streamParams.rtpPktzMode = atoi(optarg);
        }
        break;
      case CMD_OPT_RTP_SSRC:
        streamParams.output_rtpssrcsstr = optarg;
        break;
      case CMD_OPT_RTP_CLOCK:
        streamParams.output_rtpclockstr = optarg;
        break;
      case CMD_OPT_RTP_BINDPORT:
        streamParams.rtp_useCaptureSrcPort = optarg ? atoi(optarg) : 1;
        break;
      case CMD_OPT_NODB:
        arg_nodb = "";
        break;
      case CMD_OPT_NOAUDIO:
        streamParams.noaud = 1;
        break;
      case CMD_OPT_NOVIDEO:
        streamParams.novid = 1;
        break;
      case CMD_OPT_NOSEQHDRS:
        streamParams.noIncludeSeqHdrs = 1;
        break;
      case CMD_OPT_NOTFROMSEEK:
        arg_notfromseek = "";
        break;
      case CMD_OPT_OBJID:
        arg_objid = optarg;
        break;
      case 's':
      case CMD_OPT_OUTPUT:
        //action |= (ACTION_TYPE_STREAM & ACTION_TYPE_CAPSTREAM_MASK);
        if(optarg) {
          streamParams.outputs[0] = optarg;
        }
        have_arg_stream = 1;
        break;
      case CMD_OPT_OUTPUT0:
        streamParams.outputs[0] = optarg;
        have_arg_stream = 1;
        break;
      case CMD_OPT_OUTPUT1:
        if(1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.outputs[1] = optarg;
          have_arg_stream = 1;
        }
        break;
      case CMD_OPT_OUTPUT2:
        if(2 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.outputs[2] = optarg;
          have_arg_stream = 1;
        }
        break;
      case CMD_OPT_OUTPUT3:
        if(3 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.outputs[3] = optarg;
          have_arg_stream = 1;
        }
        break;
      case CMD_OPT_RTPMUX:
        streamParams.streamflags |= VSX_STREAMFLAGS_RTPMUX;
        break;
      case CMD_OPT_RTCPMUX:
        streamParams.streamflags |= VSX_STREAMFLAGS_RTCPMUX;
        break;
      case CMD_OPT_RTCPDISABLE:
        streamParams.streamflags |= VSX_STREAMFLAGS_RTCPDISABLE;
        break;
      case CMD_OPT_RTCPIGNOREBYE:
        streamParams.streamflags |= VSX_STREAMFLAGS_RTCPIGNOREBYE;
        break;
      case CMD_OPT_RTCPPORTS:
      case CMD_OPT_RTCPPORTS0:
        streamParams.rtcpPorts[0] = optarg;
        break;
      case CMD_OPT_RTCPPORTS1:
        if(1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.rtcpPorts[1] = optarg;
        }
        break;
      case CMD_OPT_RTCPPORTS2:
        if(1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.rtcpPorts[2] = optarg;
        }
        break;
      case CMD_OPT_RTCPPORTS3:
        if(1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.rtcpPorts[3] = optarg;
        }
        break;
      case CMD_OPT_OVERWRITE:
        //arg_overwrite = "";
        streamParams.overwritefile = 1;
        break;
      case CMD_OPT_STREAM_PKTZ_MODE:
        break;
      case CMD_OPT_CONFERENCEDRIVER:
        streamParams.mixerCfg.conferenceInputDriver = 1;
        break;
      case CMD_OPT_MIXER:
        if(optarg) {
          streamParams.mixerCfg.active = atoi(optarg);
        } else {
          streamParams.mixerCfg.active = 1;
        }
        break;
      case CMD_OPT_PIPLAYOUT:
        streamParams.mixerCfg.layoutType = pip_str2layout(optarg);
        break;
      case CMD_OPT_MIXERSELF:
        streamParams.mixerCfg.includeSelfChannel= optarg ? atoi(optarg) : 1;
        break;
      case CMD_OPT_MIXERVAD:
        if(optarg) {
          streamParams.mixerCfg.vad = atoi(optarg);
        } else {
          streamParams.mixerCfg.vad = 1;
        }
        break;
      case CMD_OPT_MIXERAGC:
        if(optarg) {
          streamParams.mixerCfg.agc = atoi(optarg);
        } else {
          streamParams.mixerCfg.agc = 1;
        }
      case CMD_OPT_MIXERDENOISE:
        if(optarg) {
          streamParams.mixerCfg.denoise = atoi(optarg);
        } else {
          streamParams.mixerCfg.denoise = 1;
        }
      case CMD_OPT_PIPINPUT:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].input = optarg;
        }
        break;
      case CMD_OPT_PIPMAX:
        if(optarg) {
          streamParams.pipMax = atoi(optarg);
        }
        break;
      case CMD_OPT_PIPCONF:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].cfgfile = optarg;
        }
        break;
      case CMD_OPT_PIPHTTPMAX:
        if(optarg) {
          streamParams.piphttpmax = atoi(optarg);
        } else {
          streamParams.piphttpmax = 1;
        }
        break;
      case CMD_OPT_PIPHTTPPORT:
        if(idxPip < sizeof(streamParams.pipaddr) / sizeof(streamParams.pipaddr[0])) {
          if(optarg) {
            streamParams.pipaddr[idxPip] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.pipaddr[idxPip] = prevHttpAddr ? prevHttpAddr : HTTP_LISTEN_PORT_STR;
          }
        }
        idxPip++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_PIPALPHAMIN_MIN1:
        if(optarg && idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].alphamin_min1 = atoi(optarg) + 1;
        }
        break;
      case CMD_OPT_PIPALPHAMAX_MIN1:
        if(optarg && idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].alphamax_min1 = atoi(optarg) + 1;
        }
        break;
      case CMD_OPT_PIPBEFORE:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].flags |= PIP_FLAGS_INSERT_BEFORE_SCALE; 
        }
        break;
      //case CMD_OPT_PIPCLOSEONEND:
      //  if(idxPipInputs < 1) {
      //    streamParams.pipCfg[idxPipInputs].flags |= PIP_FLAGS_CLOSE_ON_END; 
      //  }
      //  break;
      case CMD_OPT_PIPXRIGHT:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].flags |= PIP_FLAGS_POSITION_RIGHT; 
        }
      case CMD_OPT_PIPX:
        if(optarg && idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].posx = atoi(optarg);
        }
        break;
      case CMD_OPT_PIPYBOTTOM:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].flags |= PIP_FLAGS_POSITION_BOTTOM; 
        }
      case CMD_OPT_PIPY:
        if(optarg && idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].posy = atoi(optarg);
        }
        break;
      case CMD_OPT_PIPXCODE:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].strxcode = optarg;
        }
        break;
      case CMD_OPT_PIPZORDER:
        if(optarg && idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].zorder = atoi(optarg);
        }
        break;
      case CMD_OPT_PIPNOAUDIO:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].noaud = 1;
        }
        break;
      case CMD_OPT_PIPNOVIDEO:
        if(idxPipInputs < 1) {
          streamParams.pipCfg[idxPipInputs].novid = 1;
        }
        break;
      case CMD_OPT_PES:
        if((streamParams.extractpes = optarg ? atoi(optarg) : 1)) {
          streamParams.extractpes = BOOL_ENABLED_OVERRIDE;
        } else {
          streamParams.extractpes = BOOL_DISABLED_OVERRIDE;
        }
        break;
      case CMD_OPT_PROMISC:
        streamParams.promisc = 1;
        break;
      case CMD_OPT_QUEUE:
        streamParams.pktqslots = atoi(optarg);
        break;
      case CMD_OPT_QUEUEAUD:
        streamParams.fraudqslots = atoi(optarg);
        break;
      case CMD_OPT_QUEUEVID:
        streamParams.frvidqslots = atoi(optarg);
        break;
      case CMD_OPT_RAW:
        if(optarg) {
          streamParams.rawxmit = 1;
#if defined(WIN32) && !defined(__MINGW32__)
          pktgen_SetInterface(optarg);
#endif // WIN32
        }
        break;
      case CMD_OPT_RTCPRR:
        streamParams.frtcp_rr_intervalsec = optarg ? atof(optarg) : STREAM_RTCP_RR_INTERVAL_SEC;
        break;
      case CMD_OPT_RTCPSR:
        if(optarg) {
          streamParams.frtcp_sr_intervalsec = atof(optarg);
        }
        break;
      case CMD_OPT_RTPFRAMEDROPPOLICY:
        if(optarg) {
          streamParams.capture_rtp_frame_drop_policy = atoi(optarg);
        }
        break;
      case CMD_OPT_NACK_RTPRETRANSMIT:
        streamParams.nackRtpRetransmitVideo = optarg ? atoi(optarg) : 1;
        break;
      case CMD_OPT_NACK_XMIT:
        i = 1;
        if(optarg) {
          i = atoi(optarg);
        }
        if(!optarg || i) {
          streamParams.nackRtcpSendVideo = BOOL_ENABLED_OVERRIDE;
        } else {
          streamParams.nackRtcpSendVideo = BOOL_DISABLED_OVERRIDE;
        }
        break;
      case CMD_OPT_APPREMB_XMITMAXRATE:
        streamParams.apprembRtcpSendVideoMaxRateBps =  (unsigned int) strutil_read_numeric(optarg, 0, 1000, 0);
        break;
      case CMD_OPT_APPREMB_XMITMINRATE:
        streamParams.apprembRtcpSendVideoMinRateBps =  (unsigned int) strutil_read_numeric(optarg, 0, 1000, 0);
        break;
      case CMD_OPT_APPREMB_XMIT:
        i = 1;
        if(optarg) {
          i = atoi(optarg);
        }
        if(!optarg || i) {
          streamParams.apprembRtcpSendVideo = BOOL_ENABLED_OVERRIDE;
        } else {
          streamParams.apprembRtcpSendVideo = BOOL_DISABLED_OVERRIDE;
        }
        break;
      case CMD_OPT_APPREMB_XMITFORCE:
        i = 1;
        if(optarg) {
          i = atoi(optarg);
        }
        if(!optarg || i) {
          streamParams.apprembRtcpSendVideoForceAdjustment = BOOL_ENABLED_OVERRIDE;
        } else {
          streamParams.apprembRtcpSendVideoForceAdjustment = BOOL_DISABLED_OVERRIDE;
        }
        break;
      case CMD_OPT_FIR_XMIT:
        i = 0;
        if(optarg) {
          i = atoi(optarg);
        }

        if(!optarg || i) {
          streamParams.firCfg.fir_send_from_local = BOOL_ENABLED_OVERRIDE;
          streamParams.firCfg.fir_send_from_decoder = BOOL_ENABLED_OVERRIDE;
          streamParams.firCfg.fir_send_from_capture = BOOL_ENABLED_OVERRIDE;
        } else {
          streamParams.firCfg.fir_send_from_local = BOOL_DISABLED_OVERRIDE;
          streamParams.firCfg.fir_send_from_remote = BOOL_DISABLED_OVERRIDE;
          streamParams.firCfg.fir_send_from_decoder = BOOL_DISABLED_OVERRIDE;
          streamParams.firCfg.fir_send_from_capture = BOOL_DISABLED_OVERRIDE;
        }
        break;

      case CMD_OPT_FIR_ACCEPT:
        i = 0;
        if(optarg) {
          i = atoi(optarg);
        }
        if(!optarg || i) {
          streamParams.firCfg.fir_recv_via_rtcp = BOOL_ENABLED_OVERRIDE;
          streamParams.firCfg.fir_recv_from_connect = BOOL_ENABLED_OVERRIDE;
          streamParams.firCfg.fir_recv_from_remote = BOOL_ENABLED_OVERRIDE;
        } else if(i == 0) {
          streamParams.firCfg.fir_recv_via_rtcp = BOOL_DISABLED_OVERRIDE;
          streamParams.firCfg.fir_recv_from_connect = BOOL_DISABLED_OVERRIDE;
          streamParams.firCfg.fir_recv_from_remote = BOOL_DISABLED_OVERRIDE;
        }
        break;

      case CMD_OPT_RTMPLIVEADDRPORT:
        if(idxRtmplive < sizeof(streamParams.rtmpliveaddr) / sizeof(streamParams.rtmpliveaddr[0])) {
          if(optarg) {
            streamParams.rtmpliveaddr[idxRtmplive] = optarg;
          } else {
            streamParams.rtmpliveaddr[idxRtmplive] = RTMP_LISTEN_PORT_STR;
          }
        }
        idxRtmplive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_RTMPLIVEMAX:
        streamParams.rtmplivemax = atoi(optarg);
        break;
      case CMD_OPT_RTSPLIVEADDRPORT:
        if(idxRtsplive < sizeof(streamParams.rtspliveaddr) / sizeof(streamParams.rtspliveaddr[0])) {
          if(optarg) {
            streamParams.rtspliveaddr[idxRtsplive] = optarg;
          } else {
            streamParams.rtspliveaddr[idxRtsplive] = RTSP_LISTEN_PORT_STR;
          }
        }
        idxRtsplive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_RTSPLIVEMAX:
        streamParams.rtsplivemax = atoi(optarg);
        break;
      case CMD_OPT_RTSPINTERLEAVED:
        if(!optarg || (i = atoi(optarg)) != 0) {
          streamParams.rtsptranstype = RTSP_TRANSPORT_TYPE_INTERLEAVED;
        } else {
          streamParams.rtsptranstype = RTSP_TRANSPORT_TYPE_UDP;
        }
        break;
      case CMD_OPT_RTSPSRTP:
        if(!optarg || (i = atoi(optarg)) != 0) {
          streamParams.rtspsecuritytype = RTSP_TRANSPORT_SECURITY_TYPE_SRTP;
        } else {
          streamParams.rtspsecuritytype = RTSP_TRANSPORT_SECURITY_TYPE_RTP;
        }
        break;
      case CMD_OPT_RTMP_NOSIG:
        streamParams.rtmpfp9 = !RTMP_CLIENT_FP9;
        break;
      case CMD_OPT_RTMP_PAGEURL:
        streamParams.rtmppageurl = optarg;
        break;
      case CMD_OPT_RTMP_SWFURL:
        streamParams.rtmpswfurl = optarg;
        break;
      case CMD_OPT_RTMP_DORTMPT:
        streamParams.rtmpdotunnel = (!optarg || atoi(optarg)) != 0 ? BOOL_ENABLED_OVERRIDE : BOOL_DISABLED_OVERRIDE;
        break;
      case CMD_OPT_SDPOUT:
        streamParams.sdpoutpath = optarg;
        break;
      case CMD_OPT_SRTPKEY:
      case CMD_OPT_SRTPKEY0:
        streamParams.srtpCfgs[0].keysBase64 = optarg;
        break;
      case CMD_OPT_SRTPKEY1:
        if(1 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.srtpCfgs[1].keysBase64 = optarg;
        }
        break;
      case CMD_OPT_SRTPKEY2:
        if(2 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.srtpCfgs[2].keysBase64 = optarg;
        }
        break;
      case CMD_OPT_SRTPKEY3:
        if(3 < IXCODE_VIDEO_OUT_MAX) {
          streamParams.srtpCfgs[3].keysBase64 = optarg;
        }
        break;
      case CMD_OPT_SRTP:
        streamParams.srtpCfgs[0].srtp = optarg ? atoi(optarg) : 1;
        break;
      case CMD_OPT_ENABLE_SYMLINK:
        streamParams.enable_symlink = (optarg && atoi(optarg) == 0) ? BOOL_DISABLED_OVERRIDE : BOOL_ENABLED_OVERRIDE;
        break;
      case CMD_OPT_DTLSSRTP:
        streamParams.srtpCfgs[0].srtp = streamParams.srtpCfgs[0].dtls = (optarg ? atoi(optarg) : 1);
        break;
      case CMD_OPT_DTLS:
        streamParams.srtpCfgs[0].dtls = optarg ? atoi(optarg) : 1;
        break;
      //case CMD_OPT_DTLSCLIENT:
      //  streamParams.srtpCfgs[0].dtls_handshake_server = optarg ? atoi(optarg) : 0;
      //  break;
      case CMD_OPT_DTLSSERVER:
        streamParams.srtpCfgs[0].dtls_handshake_server = optarg ? atoi(optarg) : 1;
        break;
/*
      case CMD_OPT_DTLSXMITSERVERKEY:
        streamParams.srtpCfgs[0].dtls_xmit_serverkey = optarg ? atoi(optarg) : 1;
        if(streamParams.srtpCfgs[0].dtls_xmit_serverkey) {
          streamParams.srtpCfgs[0].dtls = streamParams.srtpCfgs[0].dtls_xmit_serverkey;
        }
        break;
      case CMD_OPT_DTLSCAPSERVERKEY:
        streamParams.srtpCfgs[0].dtls_capture_serverkey = optarg ? atoi(optarg) : 1;
        if(streamParams.srtpCfgs[0].dtls_capture_serverkey) {
          streamParams.srtpCfgs[0].dtls = 1;
        }
        break;
      case CMD_OPT_DTLSSERVERKEY:
        streamParams.srtpCfgs[0].dtls_capture_serverkey = 
        streamParams.srtpCfgs[0].dtls_xmit_serverkey = optarg ? atoi(optarg) : 1;
        if(streamParams.srtpCfgs[0].dtls_capture_serverkey) {
          streamParams.srtpCfgs[0].dtls = 1;
        }
        break;
*/
      case CMD_OPT_DTLSCERTPATH:
        streamParams.srtpCfgs[0].dtlscertpath = optarg;
        break;
      case CMD_OPT_DTLSPRIVKEYPATH:
        streamParams.srtpCfgs[0].dtlsprivkeypath = optarg;
        break;
      case CMD_OPT_STUNRESP:
        streamParams.stunResponderCfg.bindingResponse = STUN_POLICY_ENABLED;
        if(optarg) {
          streamParams.stunResponderCfg.respPass = optarg;
        } else {
          streamParams.stunResponderCfg.respUseSDPIceufrag = 1;
        }
        break;
      case CMD_OPT_STUNREQ:
        if(optarg) {
          streamParams.stunRequestorCfg.bindingRequest = atoi(optarg);
        } else {
          streamParams.stunRequestorCfg.bindingRequest = STUN_POLICY_ENABLED;
        }
        break;
      case CMD_OPT_STUNREQUSER:
        if(streamParams.stunRequestorCfg.bindingRequest == STUN_POLICY_NONE) {
          streamParams.stunRequestorCfg.bindingRequest = STUN_POLICY_XMIT_IF_RCVD;
        }
        streamParams.stunRequestorCfg.reqUsername = optarg;
        break;
      case CMD_OPT_STUNREQPASS:
        if(streamParams.stunRequestorCfg.bindingRequest == STUN_POLICY_NONE) {
          streamParams.stunRequestorCfg.bindingRequest = STUN_POLICY_XMIT_IF_RCVD;
        }
        streamParams.stunRequestorCfg.reqPass = optarg;
        break;
      case CMD_OPT_STUNREQREALM:
        if(streamParams.stunRequestorCfg.bindingRequest == STUN_POLICY_NONE) {
          streamParams.stunRequestorCfg.bindingRequest = STUN_POLICY_XMIT_IF_RCVD;
        }
        streamParams.stunRequestorCfg.reqRealm = optarg;
        break;
      case CMD_OPT_TURNSERVER:
        streamParams.turnCfg.turnServer = optarg;
        break;
      case CMD_OPT_TURNUSER:
        streamParams.turnCfg.turnUsername = optarg;
        break;
      case CMD_OPT_TURNPASS:
        streamParams.turnCfg.turnPass = optarg;
        break;
      case CMD_OPT_TURNREALM:
        streamParams.turnCfg.turnRealm = optarg;
        break;
      case CMD_OPT_TURNPEER:
        streamParams.turnCfg.turnPeerRelay = optarg;
        break;
      case CMD_OPT_TURNSDPOUTPATH:
        streamParams.turnCfg.turnSdpOutPath = optarg;
        break;
      case CMD_OPT_TURNPOLICY:
        if(optarg) {
          streamParams.turnCfg.turnPolicy = atoi(optarg);
        } else {
          streamParams.turnCfg.turnPolicy = TURN_POLICY_MANDATORY;
        }
        break;
      case CMD_OPT_STARTOFFSET:
        streamParams.strstartoffset = optarg;
        break;
      case CMD_OPT_STREAMSTATSFILE:
        streamParams.statfilepath = optarg ? optarg : "stdout";
        break;
      case CMD_OPT_ABRSELF:
        streamParams.abrSelf = optarg ? atoi(optarg) : 1;
        break;
      case CMD_OPT_ABRAUTO:
        streamParams.abrAuto = optarg ? atoi(optarg) : 1;
        break;
      case CMD_OPT_FRAMETHIN:
        streamParams.frameThin = optarg ? atoi(optarg) : 1;
        break;
      case CMD_OPT_STATUSMAX:
        streamParams.statusmax = atoi(optarg);
        if(idxStatus == 0 && prevHttpAddr) {
          streamParams.statusaddr[idxStatus] = prevHttpAddr;
        }
        break;
      case CMD_OPT_STATUSPORT:
        if(idxStatus < sizeof(streamParams.statusaddr) / sizeof(streamParams.statusaddr[0])) {
          if(optarg) {
            streamParams.statusaddr[idxStatus] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.statusaddr[idxStatus] = prevHttpAddr ? prevHttpAddr : STATUS_LISTEN_PORT_STR;
          }
        }
        idxStatus++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_STREAM_DELAY:
        streamParams.favdelay = atof(optarg);
        break;
      case CMD_OPT_STTS:
        arg_stts = optarg;
        have_arg_stts = 1;
        break;
      case CMD_OPT_TOKENID:
        streamParams.tokenid = optarg;
        break;
      case CMD_OPT_OUTQ_PREALLOC:
        streamParams.outq_prealloc = (optarg && atoi(optarg) <= 0) ? BOOL_DISABLED_OVERRIDE : BOOL_ENABLED_OVERRIDE;
        break;
      case CMD_OPT_TSLIVEPORT:
        if(idxTslive < sizeof(streamParams.tsliveaddr) / sizeof(streamParams.tsliveaddr[0])) {
          if(optarg) {
            streamParams.tsliveaddr[idxTslive] = optarg;
            prevHttpAddr = optarg;
          } else {
            streamParams.tsliveaddr[idxTslive] = prevHttpAddr ? prevHttpAddr : TSLIVE_LISTEN_PORT_STR; 
          }
        }
        idxTslive++;
        have_arg_stream = 1;
        break;
      case CMD_OPT_TSLIVEMAX:
        streamParams.tslivemax = atoi(optarg);
        have_arg_stream = 1;
        break;
      case CMD_OPT_TRANSPORT:
        streamParams.transproto = optarg;
        break;
      case CMD_OPT_VIDEO:
        arg_video = "";
        break;
      case CMD_OPT_XCODE:
        streamParams.strxcode = optarg;
        have_arg_stream = 1;
        break;
      case CMD_OPT_TEST:
        return runtests(optarg);
        break;
      case CMD_OPT_DEBUG:
        if(optarg) {
          if(!strcasecmp("auth", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_AUTH;
          } else if(!strcasecmp("codec", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_CODEC;
          } else if(!strcasecmp("http", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_HTTP;
          } else if(!strcasecmp("remb", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_REMB;
          } else if(!strcasecmp("mem", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_MEM;
          } else if(!strcasecmp("mkv", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_MKV;
          } else if(!strcasecmp("dash", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_DASH;
          } else if(!strcasecmp("live", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_LIVE;
          } else if(!strcasecmp("outfmt", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_OUTFMT;
          } else if(!strcasecmp("rtp", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_RTP;
          } else if(!strcasecmp("rtcp", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_RTCP;
          } else if(!strcasecmp("rtmp", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_RTMP;
          } else if(!strcasecmp("rtmpt", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_RTMPT;
          } else if(!strcasecmp("rtsp", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_RTSP;
            g_debug_flags |= VSX_DEBUG_FLAG_HTTP;
          } else if(!strcasecmp("streamav", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_STREAMAV;
          } else if(!strcasecmp("ssl", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_SSL;
          } else if(!strcasecmp("srtp", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_SRTP;
          } else if(!strcasecmp("xcode", optarg)) {  
            g_debug_flags |= VSX_DEBUG_FLAG_XCODE;
          }
        }
        break;
      case CMD_OPT_DEBUG_AUTH:
        g_debug_flags |= VSX_DEBUG_FLAG_AUTH;
        break;
      case CMD_OPT_DEBUG_HTTP:
        g_debug_flags |= VSX_DEBUG_FLAG_HTTP;
        break;
      case CMD_OPT_DEBUG_MKV:
        g_debug_flags |= VSX_DEBUG_FLAG_MKV;
        break;
      case CMD_OPT_DEBUG_CODEC:
        g_debug_flags |= VSX_DEBUG_FLAG_CODEC;
        break;
      case CMD_OPT_DEBUG_DASH:
        g_debug_flags |= VSX_DEBUG_FLAG_DASH;
        break;
      case CMD_OPT_DEBUG_LIVE:
        g_debug_flags |= VSX_DEBUG_FLAG_LIVE;
        break;
      case CMD_OPT_DEBUG_INFRAME:
        g_debug_flags |= VSX_DEBUG_FLAG_INFRAME;
        break;
      case CMD_OPT_DEBUG_MEM:
        g_debug_flags |= VSX_DEBUG_FLAG_MEM;
        break;
      case CMD_OPT_DEBUG_OUTFMT:
        g_debug_flags |= VSX_DEBUG_FLAG_OUTFMT;
        break;
      case CMD_OPT_DEBUG_REMB:
        g_debug_flags |= VSX_DEBUG_FLAG_REMB;
        break;
      case CMD_OPT_DEBUG_RTP:
        g_debug_flags |= VSX_DEBUG_FLAG_RTP;
        break;
      case CMD_OPT_DEBUG_RTCP:
        g_debug_flags |= VSX_DEBUG_FLAG_RTCP;
        break;
      case CMD_OPT_DEBUG_RTSP:
        g_debug_flags |= VSX_DEBUG_FLAG_RTSP;
        g_debug_flags |= VSX_DEBUG_FLAG_HTTP;
        break;
      case CMD_OPT_DEBUG_RTMP:
        g_debug_flags |= VSX_DEBUG_FLAG_RTMP;
        break;
      case CMD_OPT_DEBUG_RTMPT:
        g_debug_flags |= VSX_DEBUG_FLAG_RTMPT;
        break;
      case CMD_OPT_DEBUG_NET:
        g_debug_flags |= VSX_DEBUG_FLAG_NET;
        break;
      case CMD_OPT_DEBUG_STREAMAV:
        g_debug_flags |= VSX_DEBUG_FLAG_STREAMAV;
        break;
      case CMD_OPT_DEBUG_SSL:
        g_debug_flags |= VSX_DEBUG_FLAG_SSL;
        break;
      case CMD_OPT_DEBUG_SRTP:
        g_debug_flags |= VSX_DEBUG_FLAG_SRTP;
        break;
      case CMD_OPT_DEBUG_DTLS:
        g_debug_flags |= VSX_DEBUG_FLAG_DTLS;
        break;
      case CMD_OPT_DEBUG_XCODE:
        g_debug_flags |= VSX_DEBUG_FLAG_XCODE;
        break;

#if defined(CFG_HAVE_WIN32_SERVICE) && defined(WIN32) && !defined(__MINGW32__)
      case CMD_OPT_INSTALL:
        logger_SetLevel(S_INFO);
        logger_AddStderr(S_INFO, logoutputflags);
        return service_install(argv[0]);
        break;
      case CMD_OPT_UNINSTALL:
        logger_SetLevel(S_INFO);
        logger_AddStderr(S_INFO, logoutputflags);
        return service_uninstall();
        break;
#endif // WIN32
      case '?':

        banner(stdout);

        //if(optind < argc) {
          fprintf(stderr, "Unrecognized option '%s'\n", argv[optind - 1 + (optarg ? 1 : 0)]);
          //fprintf(stderr, "Unrecognized option '%s'\n", argv[optind - 1]);
        //} else {
        //  fprintf(stderr, "Invalid command line argument\n");
        //}
        fprintf(stderr, "run: '"VSX_BINARY" -h' to list all available options\n");
        return -1;
      case 'h':
        return usage(argc, (const char **) argv, getnextarg(argc, argv), 1);
      default:
        return usage(argc, (const char **) argv, NULL, 1);
    }

  }

  if(streamParams.verbosity >= VSX_VERBOSITY_NORMAL) {
    loggerLevel = MIN( (streamParams.verbosity + S_WARNING), S_DEBUG_VVERBOSE);
  } else {
    loggerLevel = S_WARNING;
  }

  logger_SetLevel(loggerLevel);
  logger_AddStderr(loggerLevel, logoutputflags);

  if(action == ACTION_TYPE_LICENSE_GEN) {
    create_licenseinfo();
    return 0;
  } else if(action == ACTION_TYPE_LICENSE_CHECK) {
    check_licenseinfo(&streamParams);
    return 0;
  }

  if(action != ACTION_TYPE_DUMP_CONTAINER && !have_arg_stts) {
    banner(stdout);
  }


#if defined(WIN32) && !defined(__MINGW32__)
  if(streamParams.rawxmit) {
     //
    // Perform packet generation engine initialization
    //
    if(pktgen_Init() != 0) {
      return -1;
    }
    //
    // Perform mac address lookup table initialization
    //
    if(maclookup_Init(ACTQOS_MAC_EXPIRE_SEC) != 0) {
      pktgen_Close();
      return -1;
    }
  }
#endif // WIN32

  if(arg_fps) {
    streamParams.fps = atof(arg_fps);
  }

  if(action == ACTION_TYPE_NONE && have_arg_stream) {
    //action |= (ACTION_TYPE_STREAM & ACTION_TYPE_CAPSTREAM_MASK);
    action |= ACTION_TYPE_STREAM;
  } else if(action == ACTION_TYPE_NONE && have_arg_input) {
    // Avoid defaulting to broadcastctrl w/ incomplete command line
    action = ACTION_TYPE_INVALID;
  } else if(action == ACTION_TYPE_CAPTURE && have_arg_stream) {
    action |= ACTION_TYPE_STREAM;
  }

  if(arg_pidfile) {
    if((ch = write_pid(arg_pidfile)) >= 0) {
      LOG(X_DEBUG("Wrote pid %d to %s"), ch, arg_pidfile);
    }
  }

  switch(action) {

#if defined(VSX_EXTRACT_CONTAINER)

    case ACTION_TYPE_EXTRACT_AUDIO: 
    case ACTION_TYPE_EXTRACT_VIDEO:
    case (ACTION_TYPE_EXTRACT_VIDEO | ACTION_TYPE_EXTRACT_AUDIO):

      logger_SetLevel(S_DEBUG);

      //if(!(arg_a && arg_v)) {
      if(!(arg_audio && arg_video)) {
        if(arg_audio) {
          action &= ~ACTION_TYPE_EXTRACT_VIDEO;
        }
        if(arg_video) {
          action &= ~ACTION_TYPE_EXTRACT_AUDIO;
        }
      }
      rc = vsxlib_extractMedia((action & ACTION_TYPE_EXTRACT_VIDEO ? 1 : 0), 
                               (action & ACTION_TYPE_EXTRACT_AUDIO ? 1 : 0),
            streamParams.inputs[0], streamParams.outputs[0], streamParams.strstartoffset, 
            streamParams.strdurationmax);

      break;
#endif // VSX_EXTRACT_CONTAINER

#if defined(VSX_CREATE_CONTAINER)

    case ACTION_TYPE_CREATE_MP4:
      logger_SetLevel(S_DEBUG);
      rc = vsxlib_createMp4(streamParams.inputs[0], streamParams.outputs[0], 
                            (arg_fps ? atof(arg_fps) : 0.0), arg_stts,
                            arg_objid ? atoi(arg_objid) + 1 : 0);
      break;

#endif // VSX_CREATE_CONTAINER

#if defined(VSX_DUMP_CONTAINER)

    case ACTION_TYPE_DUMP_CONTAINER:
      logger_SetLevel(S_DEBUG);
      rc = vsxlib_dumpContainer(streamParams.inputs[0], streamParams.verbosity, 
                      (have_arg_stts ? (arg_stts ? arg_stts : "") : NULL), 
                                arg_notfromseek ? 0 : 1);
      break;

#endif // VSX_DUMP_CONTAINER

#if defined(VSX_HAVE_VIDANALYZE)

    case ACTION_TYPE_ANALYZE_MEDIA:

      g_verbosity =  streamParams.verbosity;
      logger_SetLevel(S_DEBUG);

      rc = vsxlib_analyzeH264(streamParams.inputs[0], (arg_fps ? atof(arg_fps) : 0.0), 
                               streamParams.verbosity);
      break;

#endif // VSX_HAVE_VIDANALYZE

#ifndef DISABLE_PCAP

    case ACTION_TYPE_CAPTURE_LIST_INTERFACES:
      rc = pkt_ListInterfaces(stdout);
      break;

#endif // DISABLE_PCAP

#if defined(VSX_HAVE_CAPTURE)

    case ACTION_TYPE_CAPTURE:
      rc = vsxlib_captureNet(&streamParams);
      break;

#endif // VSX_HAVE_CAPTURE

#if defined(VSX_HAVE_STREAMER)
    case ACTION_TYPE_STREAM:
    case (ACTION_TYPE_STREAM | ACTION_TYPE_CAPTURE):

      if((action & ACTION_TYPE_CAPTURE)) {
        streamParams.islive = 1;
      } else {
        streamParams.islive = 0;
      }
      rc = vsxlib_stream(&streamParams);
      break;

#endif // VSX_HAVE_STREAMER

#if defined(WIN32)
    //
    // On windows, default to '--broadcast' mode
    //
    case 0: 
#endif // WIN32

#if defined(VSX_HAVE_SERVERMODE)

#if defined(WIN32)
      LOG(X_INFO("Defaulting to '--broadcast'.  Use --help (-h) to show usage"));
#endif // WIN32

    case ACTION_TYPE_SERVER:

      if((rc = mediadb_getdirlen(argv[0])) >= 0 && rc < sizeof(curdir)) {
        strncpy(curdir, argv[0], sizeof(curdir) - 1);
        curdir[rc] = '\0'; 
      }
      if((rc = srv_start_conf(&streamParams, arg_home, arg_dbdir, 
                              arg_nodb ? 0 : 1, curdir)) == -2 &&
         action == 0) {
        fprintf(stdout, "\n");

        usage(argc, (const char **) argv, NULL, 1);
      } 

      if(rc != 0) {
        //
        // Give the user the chance to see any error message since its likely
        // that the program was started via a double-click, opening command prompt
        //
        for(ch = 10; ch > 0; ch--) {
          fprintf(stdout, ".");
          fflush(stdout);
          usleep(500000);
        }
        fprintf(stdout, "\n");
      }
      break;

#endif // VSX_HAVE_SERVERMODE

    case ACTION_TYPE_INVALID:
      fprintf(stdout, "\nNo operation specified!  To view help: "VSX_BINARY" -h stream\n");
    default:
      rc = usage(argc, (const char **) argv, NULL, 0);
      break;
  }

  LOG(X_DEBUG(VSX_APPNAME" exiting"));

  if(arg_pidfile) {
     fileops_DeleteFile(arg_pidfile);
  }

  vsxlib_close(&streamParams);

  return 0;
}

#endif // 1

