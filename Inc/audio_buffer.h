/***********************************************************************

                  -- USB-DAC for STM32F4 Discovery --

------------------------------------------------------------------------

	Audio stream buffer

	Copyright (c) 2016 Kiyoto.
	This file is licensed under the MIT License. See /LICENSE.TXT file.

***********************************************************************/

#pragma once

#include "usbd_audio.h"

#define AUDIO_BUFFER_SIZE		(AUDIO_TOTAL_BUF_SIZE)		/* ﾌﾚｰﾑ長単位 */

void audio_buffer_init();
void audio_buffer_feed(unsigned char *buf, int size);
void audio_buffer_fill(int val, int size, int valid);
void audio_buffer_fill_next_ip(int val);
unsigned char *audio_buffer_getptr();
unsigned char *audio_buffer_read(int ofs, int size);
signed int audio_buffer_getfeedback();

/* ﾌｫｰﾏｯﾄﾊﾟﾗﾒﾀ */
#define AUDIO_BUFFER_FRAMESIZE		(8)	/* ﾊﾞｯﾌｧ上のﾌﾚｰﾑ長(32bitｽﾃﾚｵ固定) */

