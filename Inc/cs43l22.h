/***********************************************************************

                  -- USB-DAC for STM32F4 Discovery --

------------------------------------------------------------------------

	DAC driver(Cirrus Logic CS43L22).

	Copyright (c) 2016 Kiyoto.
	This file is licensed under the MIT License. See /LICENSE.TXT file.

***********************************************************************/

#pragma once

void cs43l22_set_vol(int vol);
void cs43l22_init(int freq, int vol);
void cs43l22_start(int freq, int vol);
void cs43l22_stop();
