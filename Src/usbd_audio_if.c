/**
  ******************************************************************************
  * @file           : usbd_audio_if.c
  * @brief          : Generic media access Layer.
  ******************************************************************************
  *
  * Copyright (c) 2016 STMicroelectronics International N.V. 
  * Copyright (c) 2016 Kiyoto.
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "usbd_audio_if.h"
/* USER CODE BEGIN INCLUDE */

/* DAC制御用 */
#include "cs43l22.h"

/* ﾊﾞｯﾌｧ扱い */
#include "audio_buffer.h"

/* USER CODE END INCLUDE */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */

/** @defgroup USBD_AUDIO 
  * @brief usbd core module
  * @{
  */ 

/** @defgroup USBD_AUDIO_Private_TypesDefinitions
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_TYPES */
/* USER CODE END PRIVATE_TYPES */ 
/**
  * @}
  */ 

/** @defgroup USBD_AUDIO_Private_Defines
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */
  
/**
  * @}
  */ 

/** @defgroup USBD_AUDIO_Private_Macros
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_MACRO */
/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */ 

/** @defgroup USBD_AUDIO_IF_Private_Variables
  * @{
  */
/* USER CODE BEGIN PRIVATE_VARIABLES */
/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */ 
  
/** @defgroup USBD_AUDIO_IF_Exported_Variables
  * @{
  */ 
  extern USBD_HandleTypeDef hUsbDeviceFS;
/* USER CODE BEGIN EXPORTED_VARIABLES */

extern I2S_HandleTypeDef hi2s3;

/* ﾃﾞｰﾀ供給制御(USB-Audio側からのﾃﾞｰﾀ供給を受けるかどうか) */
volatile int feed_enable = 0;

/* I2S側の再生制御用 */
volatile int i2s_is_playing;
volatile int8_t i2s_cmd_queue[4];	/* 停止/開始要求の実行ｷｭｰ(開始→停止 / 停止→開始等の操作を高速で行った場合に、ｴｯｼﾞ方向を正しく処理できるように) */
#define I2S_OP_INIT				(0x01)		/* 初期化要求 */
#define I2S_OP_FEED				(0x02)		/* ﾃﾞｰﾀ供給通知(初回から数えて半ﾊﾞｯﾌｧ分以上埋まったらDMAを始動する) */
#define I2S_OP_STOP_REQ			(0x03)		/* 停止要求(初段) */
#define I2S_OP_STOP_SYN			(0x04)		/* ↑を受けて、再生ﾊﾞｯﾌｧの残りを再生し終わるまで待機(半ﾊﾞｯﾌｧ分) */
#define I2S_OP_STOP_DMA			(0x05)		/* ↑を受けて、実際にDMAを止めるよう要求 */
volatile int i2s_cmd_queue_wp;
volatile int i2s_cmd_queue_rp;

/* 音量制御系の処理(ﾒｲﾝﾙｰﾁﾝ側で遅延実行) */
volatile int vol_ctrl_req = 0;
volatile int vol_ctrl_value = 0;
volatile int mute_ctrl_req = 0;
volatile int mute_ctrl_value = 0;

/* for debug */
static volatile int error_code;

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */ 
  
/** @defgroup USBD_AUDIO_Private_FunctionPrototypes
  * @{
  */
static int8_t  AUDIO_Init_FS         (uint32_t  AudioFreq, int16_t Volume, uint32_t options);
static int8_t  AUDIO_DeInit_FS       (uint32_t options);
static int8_t  AUDIO_AudioCmd_FS     (uint8_t* pbuf, uint32_t size, uint8_t cmd);
static int8_t  AUDIO_VolumeCtl_FS    (int16_t vol);
static int8_t  AUDIO_MuteCtl_FS      (uint8_t cmd);
static int8_t  AUDIO_PeriodicTC_FS   (uint8_t cmd);
static int8_t  AUDIO_GetState_FS     (void);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */
/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */ 
  
USBD_AUDIO_ItfTypeDef USBD_AUDIO_fops_FS = 
{
  AUDIO_Init_FS,
  AUDIO_DeInit_FS,
  AUDIO_AudioCmd_FS,
  AUDIO_VolumeCtl_FS,
  AUDIO_MuteCtl_FS,
  AUDIO_PeriodicTC_FS,
  AUDIO_GetState_FS,
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  AUDIO_Init_FS
  *         Initializes the AUDIO media low layer over USB FS IP
  * @param  AudioFreq: Audio frequency used to play the audio stream.
  * @param  Volume: Initial volume level (8.8形式、2の補数)
  * @param  options: Reserved for future use 
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_Init_FS(uint32_t  AudioFreq, int16_t Volume, uint32_t options)
{ 
  /* USER CODE BEGIN 0 */
	
	cs43l22_init(AudioFreq, Volume, 1/* 初回はﾐｭｰﾄｵﾝ */);

	audio_buffer_init();
	
	feed_enable = 0;
	i2s_is_playing = 0;
	i2s_cmd_queue_wp = 0;
	i2s_cmd_queue_rp = 0;
	((int *)(&i2s_cmd_queue))[0] = 0;
	vol_ctrl_value = Volume;
	mute_ctrl_value = 0;
	
	/* @memo I2Sを常に動作させておく運用。USB-Audioがｱｸﾃｨﾌﾞな間は常に動作させておく */
	AUDIO_AudioCmd_FS(NULL, 0, AUDIO_CMD_START);
	
	return (USBD_OK);
  /* USER CODE END 0 */
}

/**
  * @brief  AUDIO_DeInit_FS
  *         DeInitializes the AUDIO media low layer
  * @param  options: Reserved for future use
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_DeInit_FS(uint32_t options)
{
  /* USER CODE BEGIN 1 */ 
	if (i2s_is_playing){
		AUDIO_AudioCmd_FS(NULL, 0, AUDIO_CMD_STOP);
	}
	return (USBD_OK);
  /* USER CODE END 1 */
}

/**
  * @brief  AUDIO_AudioCmd_FS
  *         Handles AUDIO command.
  * @param  pbuf: Pointer to buffer of data to be sent
  * @param  size: Number of data to be sent (in bytes)
  * @param  cmd: Command opcode
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_AudioCmd_FS (uint8_t* pbuf, uint32_t size, uint8_t cmd)
{
  /* USER CODE BEGIN 2 */
  
  /* @memo ﾊﾞｯﾌｧの更新は原則割り込みﾊﾝﾄﾞﾗｻｲﾄﾞで行うこと(USB受信ﾃﾞｰﾀをこのｺﾝﾃｷｽﾄでしか供給できない為) */
  
//	HAL_GPIO_TogglePin(GPIOD, LD6_Pin);
	switch(cmd){
		case AUDIO_CMD_START:{
			
			i2s_cmd_queue[i2s_cmd_queue_wp] = I2S_OP_INIT;
			i2s_cmd_queue_wp = (i2s_cmd_queue_wp + 1) & 0x03;
			
			if (i2s_cmd_queue[i2s_cmd_queue_rp/* @warning */] == I2S_OP_STOP_SYN){
				/* 再生停止の為の同期中。つまり、再生停止→開始が高速で行われたﾊﾟﾀｰﾝ */
				/* DMAを止めずにそのまま続けて再生する */
				i2s_cmd_queue_rp/* @warning */++;
			}
			break;
		}
		case AUDIO_CMD_PLAY:{
			
			if (feed_enable){
				/* @memo 転送がﾄﾗﾌﾞｯた場合等、不正なｻｲｽﾞで来る可能性を考慮 */
				if (!(size & 0x07) && (size != 0)) audio_buffer_feed(pbuf, size);
			
				/* ﾃﾞｰﾀが来たことを通知 */
				i2s_cmd_queue[i2s_cmd_queue_wp] = I2S_OP_FEED;
				i2s_cmd_queue_wp = (i2s_cmd_queue_wp + 1) & 0x03;
			}
			break;
		}
		case AUDIO_CMD_STOP:{
			
			i2s_cmd_queue[i2s_cmd_queue_wp] = I2S_OP_STOP_REQ;
			i2s_cmd_queue_wp = (i2s_cmd_queue_wp + 1) & 0x03;
			
			/* @memo DMA停止はDMAの半ﾊﾞｯﾌｧ割り込みｴｯｼﾞで行う */
			/*       それまでの間、AUDIO_CMD_MISSINGにて零ﾌｨﾙﾃﾞｰﾀが供給されることを期待している(DMA停止までに不正ﾃﾞｰﾀが出ないよう配慮) */
			break;
		}
		case AUDIO_CMD_MISSING:{
			
			if (feed_enable){
				/* ﾎｽﾄ側からｽﾄﾘｰﾑが来なかった。零埋めで妥協 */
				/* 停止動作を行っている最中の場合は、無効ﾃﾞｰﾀとして計上する(再生ﾃﾞｰﾀの残数に反映させない。DMA停止までの0ﾌｨﾙﾃﾞｰﾀ) */
				int valid = ((i2s_cmd_queue_wp == i2s_cmd_queue_rp) || (i2s_cmd_queue[i2s_cmd_queue_rp] != I2S_OP_STOP_SYN && i2s_cmd_queue[i2s_cmd_queue_rp] != I2S_OP_STOP_DMA));
				audio_buffer_fill(0, size, valid);
				
				/* ﾃﾞｰﾀが来たことを通知 */
				/* @memo 停止処理中のｷｭｰ上書きを防ぐ為、通知を飛ばすのは有効ﾃﾞｰﾀ時のみ */
				if (valid){
					i2s_cmd_queue[i2s_cmd_queue_wp] = I2S_OP_FEED;
					i2s_cmd_queue_wp = (i2s_cmd_queue_wp + 1) & 0x03;
				}
			}
			break;
		}
	}
	
	return (USBD_OK);
  /* USER CODE END 2 */
  
}

/**
  * @brief  AUDIO_VolumeCtl_FS   
  *         Controls AUDIO Volume.
  * @param  vol: volume level (8.8形式、2の補数)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_VolumeCtl_FS (int16_t vol)
{
  /* USER CODE BEGIN 3 */ 
	
	/* ﾒｲﾝﾙｰﾁﾝ側で遅延実行 */
	/* ここでやると通信が遅すぎてUSB側のIso転送とかをとりこぼす可能性があるので… */
	vol_ctrl_value = vol;
	vol_ctrl_req = 1;
	
	return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  AUDIO_MuteCtl_FS
  *         Controls AUDIO Mute.   
  * @param  cmd: command opcode
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_MuteCtl_FS (uint8_t cmd)
{
  /* USER CODE BEGIN 4 */
	
	/* ﾒｲﾝﾙｰﾁﾝ側で遅延実行 */
	/* ここでやると通信が遅すぎてUSB側のIso転送とかをとりこぼす可能性があるので… */
	mute_ctrl_value = cmd;
	mute_ctrl_req = 1;
	
	return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  AUDIO_PeriodicT_FS              
  * @param  cmd: Command opcode
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_PeriodicTC_FS (uint8_t cmd)
{
  /* USER CODE BEGIN 5 */ 
	return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  AUDIO_GetState_FS
  *         Gets AUDIO State.  
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_GetState_FS (void)
{
  /* USER CODE BEGIN 6 */ 
	return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  Manages the DMA full Transfer complete event.
  * @param  None
  * @retval None
  */
void TransferComplete_CallBack_FS(void)
{
  /* USER CODE BEGIN 7 */ 
	
	/* ﾊﾞｯﾌｧ残量更新 */
	(void)audio_buffer_read(0, (AUDIO_BUFFER_SIZE / 2));
	
	if ((i2s_cmd_queue_wp != i2s_cmd_queue_rp) && i2s_cmd_queue[i2s_cmd_queue_rp] == I2S_OP_STOP_SYN){
		/* ﾊﾞｯﾌｧ内の残ﾃﾞｰﾀも極力再生できるように、半ﾊﾞｯﾌｧ割り込みｴｯｼﾞで停止を受付 */
		/* @warning 停止処理が完了するまで再生開始を割り込ませたくない。このため、rp位置への上書き扱いで進める */
		extern volatile signed int audio_buffer_count_valid;
		
		/* 有効ﾃﾞｰﾀの消化が終わったことを確認したらDMA停止に進む */
		if (audio_buffer_count_valid <= 0){
			i2s_cmd_queue[i2s_cmd_queue_rp/* @warning */] = I2S_OP_STOP_DMA;
		}
	}
	
  /* USER CODE END 7 */
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @param  None
  * @retval None
  */
void HalfTransfer_CallBack_FS(void)
{ 
  /* USER CODE BEGIN 8 */
	
	/* 中身は同じ */
	TransferComplete_CallBack_FS();
	
  /* USER CODE END 8 */
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @brief  Mainloop event.
  * @param  None
  * @retval None
  */
void AUDIO_main(){
	
	extern volatile signed int audio_buffer_count;
	
	/**
	  * @brief  I2S動作開始処理.
	  * @param  None
	  * @retval None
	  */
	void start_i2s(){
		/* 表示反映(@memo ｱﾝﾌﾟの電源制御兼用も考慮: ちょっと早めに行う) */
		HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_SET);
		
		/* I2S開始 */
		/* @memo MCLK出力もこのﾀｲﾐﾝｸﾞで開始される */
		error_code = HAL_I2S_Transmit_DMA(&hi2s3, (short *)audio_buffer_getptr(), (AUDIO_TOTAL_BUF_SIZE / 4)/* ｻﾌﾞﾌﾚｰﾑ単位なので */);
		
		/* DACﾐｭｰﾄ解除 */
		cs43l22_start(0, vol_ctrl_value >> 7, mute_ctrl_value);
		
		i2s_is_playing = 1;
	}
	
	if (i2s_cmd_queue_wp != i2s_cmd_queue_rp){
		
		if (i2s_cmd_queue[i2s_cmd_queue_rp] == I2S_OP_INIT){
			
			feed_enable = 1;
			i2s_cmd_queue_rp = (i2s_cmd_queue_rp + 1) & 0x03;
		
		}else if (i2s_cmd_queue[i2s_cmd_queue_rp] == I2S_OP_FEED){
			
			if (!i2s_is_playing){
				/* まだI2Sを始動していない状態 */
				if (audio_buffer_count >= (AUDIO_BUFFER_SIZE / 4)){
					/* ﾊﾞｯﾌｧが1/4埋まった。ここでI2S始動 */
					start_i2s();
				}
			}
			
			i2s_cmd_queue_rp = (i2s_cmd_queue_rp + 1) & 0x03;
			
		}else if (i2s_cmd_queue[i2s_cmd_queue_rp] == I2S_OP_STOP_REQ){
			
			if (!i2s_is_playing){
				/* 半ﾊﾞｯﾌｧ長よりも短いﾃﾞｰﾀだけが再生されたﾊﾟﾀｰﾝ */
				/* 一瞬だけ再生させる */
				start_i2s();
			}
			
			/* 今再生ﾊﾞｯﾌｧに乗っている分の再生が終わるまで待つ */
			/* @warning 停止処理が完了するまで再生開始を割り込ませたくない。このため、rp位置への上書き扱いで処理を進める */
			i2s_cmd_queue[i2s_cmd_queue_rp/* @warning */] = I2S_OP_STOP_SYN;
			
		}else if (i2s_cmd_queue[i2s_cmd_queue_rp] == I2S_OP_STOP_DMA){
			
			/* 表示反映(@memo ｱﾝﾌﾟの電源制御兼用も考慮: ちょっと早めに行う) */
			HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_RESET);
			
			/* 先にDACﾐｭｰﾄ */
			cs43l22_stop();
			
			/* DMA停止 */
			/* @warning MCLK出力もこのﾀｲﾐﾝｸﾞで停止する */
			HAL_I2S_DMAStop(&hi2s3);
			
			feed_enable = 0;
			i2s_is_playing = 0;
			audio_buffer_init();
			
			/* ここで初めてﾊﾞｯﾌｧ進行 */
			i2s_cmd_queue_rp = (i2s_cmd_queue_rp + 1) & 0x03;
		}
	}
	if (vol_ctrl_req){
		
		vol_ctrl_req = 0;
		
		/* 音量制御 */
		cs43l22_set_vol(vol_ctrl_value >> 7);
		
	}
	if (mute_ctrl_req){
		
		mute_ctrl_req = 0;
		
		/* ﾐｭｰﾄ制御 */
		cs43l22_set_mute(mute_ctrl_value);
		
	}
	
}

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */ 

/**
  * @}
  */  
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

