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


#ifndef __SERVER_H___
#define __SERVER_H___


#if defined(WIN32)
#include "unixcompat.h"
#include "pthread_compat.h"
#else
#include "wincompat.h"
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#endif // WIN32

#include "vsxconfig.h"

#include "srvlimits.h"
#include "srvconf.h"
#include "srvauth.h"
#include "srvhttp.h"
#include "srvrtsp_handler.h"
#include "srvrtsp.h"
#include "srvrtmp.h"
#include "srvflv.h"
#include "srvmkv.h"
#include "srvmoof.h"
#include "srvdevtype.h"
#include "srvmediadb.h"
#include "srvsession.h"
#include "srvfidx.h"
#include "srvprop.h"
#include "srvdirlist.h"
#include "srvcmd.h"
#include "srvctrl.h"
#include "srvfiles.h"
#include "srvlistener.h"
#include "srvlistenstart.h"




#endif // __SERVER_H___

