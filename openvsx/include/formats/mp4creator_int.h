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


#ifndef __MP4_CREATOR_INT_H__
#define __MP4_CREATOR_INT_H__

BOX_T *mp4_create_box_generic(const BOX_HANDLER_T *pHandlerChain, uint32_t type);
BOX_FTYP_T *mp4_create_startheader(const BOX_HANDLER_T *pHandlerChain,
                                   uint32_t name,
                                   uint32_t major,
                                   uint32_t compat1, 
                                   uint32_t compat2,
                                   uint32_t compat3,
                                   uint32_t compat4);
BOX_MVHD_T *mp4_create_box_mvhd(const BOX_HANDLER_T *pHandlerChain, uint64_t duration,
                            uint64_t timescale, uint32_t next_trak_id,
                            uint64_t create_tm, uint64_t mod_tm);
BOX_TKHD_T *mp4_create_box_tkhd(const BOX_HANDLER_T *pHandlerChain, uint64_t duration,
                            uint32_t id, uint32_t pic_width, uint32_t pic_height,
                            uint64_t create_tm, uint64_t mod_tm, uint16_t volume);
BOX_MDHD_T *mp4_create_box_mdhd(const BOX_HANDLER_T *pHandlerChain,
                                       uint64_t duration, uint64_t timescale);
BOX_HDLR_T *mp4_create_box_hdlr(const BOX_HANDLER_T *pHandlerChain, uint32_t type, const char *name);
BOX_VMHD_T *mp4_create_box_vmhd(const BOX_HANDLER_T *pHandlerChain);
BOX_SMHD_T *mp4_create_box_smhd(const BOX_HANDLER_T *pHandlerChain);
BOX_DREF_T *mp4_create_box_dref(const BOX_HANDLER_T *pHandlerChain, BOX_DREF_ENTRY_T *pEntry);
BOX_DREF_ENTRY_T *mp4_create_box_dref_url(const BOX_HANDLER_T *pHandlerChain, const char *url);
BOX_STSD_T *mp4_create_box_stsd(const BOX_HANDLER_T *pHandlerChain);
BOX_AVC1_T *mp4_create_box_avc1(const BOX_HANDLER_T *pHandlerChain, uint16_t drefIdx,
                                   uint32_t pic_width, uint32_t pic_height);
BOX_AVCC_T *mp4_create_box_avcC(const BOX_HANDLER_T *pHandlerChain,
                            const unsigned char *psps, unsigned int len_sps,
                            const unsigned char *ppps, unsigned int len_pps);
BOX_MP4A_T *mp4_create_box_mp4a(const BOX_HANDLER_T *pHandlerChain, const ESDS_DECODER_CFG_T *pSp);
BOX_STSC_T *mp4_create_box_stsc(const BOX_HANDLER_T *pHandlerChain,
                           uint32_t numEntries, BOX_STSC_ENTRY_T *pEntries);
BOX_STCO_T *mp4_create_box_stco(const BOX_HANDLER_T *pHandlerChain, uint32_t numEntries, uint32_t *pEntries);
BOX_STSZ_T *mp4_create_box_stsz(const BOX_HANDLER_T *pHandlerChain, uint32_t numEntries, uint32_t *pEntries);
BOX_STSS_T *mp4_create_box_stss(const BOX_HANDLER_T *pHandlerChain, uint32_t numEntries, uint32_t *pEntries);
BOX_ESDS_T *mp4_create_box_esds(const BOX_HANDLER_T *pHandlerChain,
                            uint8_t objtypeid,
                            uint8_t streamtype,
                            uint32_t avgbitrate,
                            uint32_t maxbitrate,
                            uint32_t largestframe,
                            BOX_ESDS_DESCR_DECODERSPECIFIC_T *pSp);
int mp4_create_box_esds_decoderspecific(BOX_ESDS_DESCR_DECODERSPECIFIC_T *pDecoderSp,
                                        const ESDS_DECODER_CFG_T *pSp);


#endif // ___MP4_CREATOR_INT_H__
