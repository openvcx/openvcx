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


#ifndef __CAPTURE_TCP_H__
#define __CAPTURE_TCP_H__

#include "capture.h"
#include "pkttypes.h"

int capture_decodeTcp(const struct ether_header *peth,
                              const struct ip *pip,
                              const struct tcphdr *ptcp,
                              const struct pcap_pkthdr *pkthdr,
                              COLLECT_STREAM_PKT_T *pkt);


#endif //__CAPTURE_RTP_H__
