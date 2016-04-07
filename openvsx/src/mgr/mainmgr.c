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

#include "mgr/srvmgr.h"

enum ACTION_TYPE {
  ACTION_TYPE_NONE =                     0x0000,
};


#define MGR_LISTEN_PORT                   8080
#define MGR_LISTEN_PORT_STR              "8080" 


#ifndef WIN32

#if defined(DBGMALLOC) 
extern void _print_alloc_stats(int verbose, FILE * fp);
#endif // DBGMALLOC

//
// System globals
//
int g_proc_exit;
int g_verbosity;
//int g_usehttplog = 0;
const char *g_http_log_path;
TIME_VAL g_debug_ts;
int g_debug_flags = 0;
const char *g_client_ssl_method;
unsigned int g_thread_stack_size;
pthread_cond_t *g_proc_exit_cond;

#if defined(VSX_HAVE_SSL)
pthread_mutex_t g_ssl_mtx;
#endif // VSX_HAVE_SSL




enum XCODE_CFG_ENABLED xcode_enabled(int printerror) {
  return 0;
}


static void sig_hdlr(int sig) {

  fprintf(stderr, "Caught signal:%u\n", sig);
  fflush(stderr);

  switch(sig) {
    case SIGINT:
      //fprintf(stderr, "set g_proc_exit %d 0x%x\n\n\n\n\n\n\n", g_proc_exit, &g_proc_exit);
      g_proc_exit = 1;

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



static void banner(FILE *fp) {

  fprintf(stdout, "\n"VSX_BANNER_MGR(VSX_APPTYPE_RC) "\n", BUILD_INFO_NUM);
  fprintf(stdout, VSX_BANNER3"\n");

}

static int usage(int argc, char *argv[]) {

  fprintf(stdout, "usage: %s < mode > [ options ]\n"
      "\n"
      " --conf=[ configuration file path ] (default=%s)\n"
      " --dbdir=[ media database dir ] (default=[media dir]/%s)\n"
      " --home=[ home dir ] (default=.)\n"
      " --listen=[ address:port ] http server listen port\n"
      " --log=[ log file output path ]\n"
      "             Set to 'stdout' or 'stderr' for console output\n"
      " --media=[ media library dir ] (default=.)\n"
      " --nodb      Do not write media data base content\n"
      " --disablerootlist Disable listing contents at root of media dir\n"
      " --verbose=[ level ],-v,-vvv  Increase log verbosity (default=%d)\n"
      "\n"
      "   Show this help\n"
      " --help,-h:\n\n"
          , argv[0], 
            MGR_CONF_PATH,
            VSX_DB_PATH,
            VSX_VERBOSITY_NORMAL);

  return -1;
}

#if 1

enum CMD_OPT {
  CMD_OPT_CONF = 256,     // do not mix with ascii chars
  CMD_OPT_LISTEN,
  CMD_OPT_NODB,
  CMD_OPT_DISABLE_ROOT_DIRLIST,
  CMD_OPT_DIR,
  CMD_OPT_DEBUG,
  CMD_OPT_DEBUG_AUTH,
  CMD_OPT_DEBUG_DASH,
  CMD_OPT_DEBUG_HTTP,
  CMD_OPT_DEBUG_LIVE,
  CMD_OPT_DEBUG_MEM,
  CMD_OPT_DEBUG_METAFILE,
  CMD_OPT_DEBUG_MGR,
  CMD_OPT_DEBUG_PROXY,
  CMD_OPT_DEBUG_NET,
  CMD_OPT_DEBUG_SSL,
  CMD_OPT_DEBUG_RTMP,
  CMD_OPT_DEBUG_RTSP,
  CMD_OPT_HOME,
  CMD_OPT_HTTPLOG,
  CMD_OPT_DBDIR,
  CMD_OPT_LOGTIME,
  CMD_OPT_LOGPATH,
  CMD_OPT_LBNODESFILE,
};

int main(int argc, char *argv[]) {
  int ch;
  enum ACTION_TYPE action = ACTION_TYPE_NONE;
  int rc;
  int logoutputflags = 0;
  char curdir[VSX_MAX_PATH_LEN];
  SRV_MGR_PARAMS_T params;
  unsigned int idxListen = 0;
  static const char *cmdopts ="-hv";

  g_verbosity = VSX_VERBOSITY_NORMAL;

  //
  // Enable 'access_log'
  //
  //g_usehttplog = 1;

  struct option longopt[] = {
                 { "help",        no_argument,             NULL, 'h' },
                 { "h",           no_argument,             NULL, 'h' },
                 { "conf",        required_argument,       NULL, CMD_OPT_CONF },
                 { "dbdir",       required_argument,       NULL, CMD_OPT_DBDIR },
                 { "debug",       required_argument,       NULL, CMD_OPT_DEBUG },
                 { "debug-auth",  optional_argument,       NULL, CMD_OPT_DEBUG_AUTH },
                 { "debug-dash",  optional_argument,       NULL, CMD_OPT_DEBUG_DASH},
                 { "debug-http",  optional_argument,       NULL, CMD_OPT_DEBUG_HTTP },
                 { "debug-live",  optional_argument,       NULL, CMD_OPT_DEBUG_LIVE },
                 { "debug-mem",   optional_argument,       NULL, CMD_OPT_DEBUG_MEM},
                 { "debug-metafile", optional_argument,    NULL, CMD_OPT_DEBUG_METAFILE },
                 { "debug-mgr",   optional_argument,       NULL, CMD_OPT_DEBUG_MGR },
                 { "debug-proxy", optional_argument,       NULL, CMD_OPT_DEBUG_PROXY },
                 { "debug-net",   optional_argument,       NULL, CMD_OPT_DEBUG_NET },
                 { "debug-ssl",   optional_argument,       NULL, CMD_OPT_DEBUG_SSL },
                 { "debug-rtmp",  optional_argument,       NULL, CMD_OPT_DEBUG_RTMP },
                 { "debug-rtsp",  optional_argument,       NULL, CMD_OPT_DEBUG_RTSP },
                 { "home",        required_argument,       NULL, CMD_OPT_HOME },
                 { "httplog",     optional_argument,       NULL, CMD_OPT_HTTPLOG },
                 { "listen",      required_argument,       NULL, CMD_OPT_LISTEN},
                 //{ "logtime",     no_argument,             NULL, CMD_OPT_LOGTIME },
                 { "logfile",     required_argument,       NULL, CMD_OPT_LOGPATH },
                 { "load-balancer",required_argument,      NULL, CMD_OPT_LBNODESFILE },
                 { "lb-config",   required_argument,       NULL, CMD_OPT_LBNODESFILE },
                 { "log",         required_argument,       NULL, CMD_OPT_LOGPATH },
                 { "media",       required_argument,       NULL, CMD_OPT_DIR },
                 { "nodb",        no_argument,             NULL, CMD_OPT_NODB },
                 { "disablerootlist", no_argument,         NULL, CMD_OPT_DISABLE_ROOT_DIRLIST },
                 { "verbose",     optional_argument,       NULL, 'v' },
#if defined(WIN32) 
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

  memset(&params, 0, sizeof(params));
  
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
      case 'v':
        if(optarg && (rc = atoi(optarg)) >= 0) {
          g_verbosity = rc;
        } else {
          g_verbosity++;
        }
        break;
      case CMD_OPT_CONF:
        params.confpath = optarg;
        break;
      case CMD_OPT_DBDIR:
        params.dbdir = optarg;
        break;
      case CMD_OPT_DIR:
        params.mediadir = optarg;
        break;
      case CMD_OPT_DEBUG:
        if(!strcasecmp("auth", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_AUTH;
        } else if(!strcasecmp("dash", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_DASH;
        } else if(!strcasecmp("http", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_HTTP;
        } else if(!strcasecmp("live", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_LIVE;
        } else if(!strcasecmp("mem", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_MEM;
        } else if(!strcasecmp("mgr", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_MGR;
        } else if(!strcasecmp("proxy", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_PROXY;
        } else if(!strcasecmp("net", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_NET;
        } else if(!strcasecmp("metafile", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_METAFILE;
        } else if(!strcasecmp("rtmp", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_RTMP;
        } else if(!strcasecmp("rtsp", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_RTSP;
        } else if(!strcasecmp("ssl", optarg)) {
          g_debug_flags |= VSX_DEBUG_FLAG_SSL;
        }
        break;
      case CMD_OPT_DEBUG_AUTH:
        g_debug_flags |= VSX_DEBUG_FLAG_AUTH;
        break;
      case CMD_OPT_DEBUG_DASH:
        g_debug_flags |= VSX_DEBUG_FLAG_DASH;
        break;
      case CMD_OPT_DEBUG_HTTP:
        g_debug_flags |= VSX_DEBUG_FLAG_HTTP;
        break;
      case CMD_OPT_DEBUG_LIVE:
        g_debug_flags |= VSX_DEBUG_FLAG_LIVE;
        break;
      case CMD_OPT_DEBUG_MGR:
        g_debug_flags |= VSX_DEBUG_FLAG_MGR;
        break;
      case CMD_OPT_DEBUG_MEM:
        g_debug_flags |= VSX_DEBUG_FLAG_MEM;
        break;
      case CMD_OPT_DEBUG_PROXY:
        g_debug_flags |= VSX_DEBUG_FLAG_PROXY;
        break;
      case CMD_OPT_DEBUG_NET:
        g_debug_flags |= VSX_DEBUG_FLAG_NET;
        break;
      case CMD_OPT_DEBUG_METAFILE:
        g_debug_flags |= VSX_DEBUG_FLAG_METAFILE;
        break;
      case CMD_OPT_DEBUG_SSL:
        g_debug_flags |= VSX_DEBUG_FLAG_SSL;
        break;
      case CMD_OPT_DEBUG_RTMP:
        g_debug_flags |= VSX_DEBUG_FLAG_RTMP;
        break;
      case CMD_OPT_DEBUG_RTSP:
        g_debug_flags |= VSX_DEBUG_FLAG_RTSP;
        break;
      case CMD_OPT_HOME:
        params.homedir = optarg;
        break;
      case CMD_OPT_HTTPLOG:
        params.httpaccesslogfile = optarg ? optarg : DEFAULT_HTTPACCESS_LOGPATH;;
        break;
      case CMD_OPT_LISTEN:
        if(idxListen < sizeof(params.listenaddr) / sizeof(params.listenaddr[0])) {
          params.listenaddr[idxListen] = optarg;
        }
        idxListen++;
        break;
      //case CMD_OPT_LOGTIME:
      //  logoutputflags = LOG_OUTPUT_PRINT_DATE_STDERR;
      //  break;
      case CMD_OPT_LOGPATH:
        params.logfile = optarg;
        break;
      case CMD_OPT_LBNODESFILE:
        params.lbnodesfile = optarg;
        break;
      case CMD_OPT_NODB:
        params.nodb = 1;
        break;
      case CMD_OPT_DISABLE_ROOT_DIRLIST:
        params.disable_root_dirlist = 1;
        break;
#if defined(WIN32) && !defined(__MINGW32__)
      case CMD_OPT_INSTALL:
        logger_SetLevel(S_INFO);
        logger_AddStderr(S_INFO, 0);
        return service_install(argv[0]);
        break;
      case CMD_OPT_UNINSTALL:
        logger_SetLevel(S_INFO);
        logger_AddStderr(S_INFO, 0);
        return service_uninstall();
        break;
#endif // WIN32
      case '?':
        banner(stdout);
          fprintf(stderr, "Unrecognized option '%s'\n", 
                  argv[optind - 1 + (optarg ? 1 : 0)]);
        fprintf(stderr, "run: '%s -h' to list all available options\n", argv[0]);
        return -1;
      default:
        banner(stdout);
        return usage(argc, argv);
    }

  }

  g_proc_exit = 0;
  srandom((unsigned int) time(NULL));

  if(g_verbosity > VSX_VERBOSITY_NORMAL) {
    logger_SetLevel(S_DEBUG);
    logger_AddStderr(S_DEBUG, logoutputflags);
  } else {
    logger_SetLevel(S_INFO);
    logger_AddStderr(S_INFO, logoutputflags);
  }

  if(action != ACTION_TYPE_NONE) {
    banner(stdout);
  }

  switch(action) {

    case ACTION_TYPE_NONE: 

      if((rc = mediadb_getdirlen(argv[0])) >= 0 && rc < sizeof(curdir)) {
        strncpy(curdir, argv[0], sizeof(curdir) - 1);
        curdir[rc] = '\0';
        params.curdir = curdir;
      }

      srvmgr_start(&params);
      break;

    default:
      rc = usage(argc, argv);
      break;
  }

  LOG(X_DEBUG(VSX_APPNAME" exiting"));


  return 0;
}

#endif // 1

