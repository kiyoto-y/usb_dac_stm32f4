/**
  ******************************************************************************
  * @file    usbd_audio.c
  * @author  MCD Application Team
  * @version V2.4.2
  * @date    11-December-2015
  * @brief   This file provides the Audio core functions.
  *
  * @verbatim
  *      
  *          ===================================================================      
  *                                AUDIO Class  Description
  *          ===================================================================
  *           This driver manages the Audio Class 1.0 following the "USB Device Class Definition for
  *           Audio Devices V1.0 Mar 18, 98".
  *           This driver implements the following aspects of the specification:
  *             - Device descriptor management
  *             - Configuration descriptor management
  *             - Standard AC Interface Descriptor management
  *             - 1 Audio Streaming Interface (PCM, Stereo mode)
  *             - 1 Audio Streaming Endpoint
  *             - 1 Audio Terminal Input (2 channel)
  *             - 1 Audio Feedback Endpoint
  *             - Audio Class-Specific AC Interfaces
  *             - Audio Class-Specific AS Interfaces
  *             - AudioControl Requests: only SET_CUR and GET_CUR / MIN / MAX / RES requests are supported (for Mute, Volume)
  *             - Audio Feature Unit (Mute and volume control)
  *             - Audio Synchronization type: Asynchronous
  *             - Single fixed audio sampling rate (configurable in usbd_conf.h file)
  *          The current audio class version supports the following audio features:
  *             - Pulse Coded Modulation (PCM) format
  *             - sampling rate: 44.1KHz. 
  *             - Bit resolution: 32
  *             - Number of channels: 2
  *             - Volume control
  *             - Mute/Unmute capability
  *             - Asynchronous Endpoints 
  *          
  * @note     In HS mode and when the DMA is used, all variables and data structures
  *           dealing with the DMA during the transaction process should be 32-bit aligned.
  *           
  *      
  *  @endverbatim
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  * Copyright (c) 2016 Kiyoto.
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "usbd_audio.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"


/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_AUDIO 
  * @brief usbd core module
  * @{
  */ 

/** @defgroup USBD_AUDIO_Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USBD_AUDIO_Private_Defines
  * @{
  */ 

/**
  * @}
  */ 


/** @defgroup USBD_AUDIO_Private_Macros
  * @{
  */ 
#define AUDIO_SAMPLE_FREQ(frq)      (uint8_t)(frq), (uint8_t)((frq >> 8)), (uint8_t)((frq >> 16))

#define AUDIO_PACKET_SZE(val)          (uint8_t)((val) & 0xFF), \
                                       (uint8_t)(((val) >> 8) & 0xFF)
                                         
/**
  * @}
  */ 




/** @defgroup USBD_AUDIO_Private_FunctionPrototypes
  * @{
  */


static uint8_t  USBD_AUDIO_Init (USBD_HandleTypeDef *pdev, 
                               uint8_t cfgidx);

static uint8_t  USBD_AUDIO_DeInit (USBD_HandleTypeDef *pdev, 
                                 uint8_t cfgidx);

static uint8_t  USBD_AUDIO_Setup (USBD_HandleTypeDef *pdev, 
                                USBD_SetupReqTypedef *req);

static uint8_t  *USBD_AUDIO_GetCfgDesc (uint16_t *length);

static uint8_t  *USBD_AUDIO_GetDeviceQualifierDesc (uint16_t *length);

static uint8_t  USBD_AUDIO_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t  USBD_AUDIO_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t  USBD_AUDIO_EP0_RxReady (USBD_HandleTypeDef *pdev);

static uint8_t  USBD_AUDIO_EP0_TxReady (USBD_HandleTypeDef *pdev);

static uint8_t  USBD_AUDIO_SOF (USBD_HandleTypeDef *pdev);

static uint8_t  USBD_AUDIO_IsoINIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t  USBD_AUDIO_IsoOutIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t AUDIO_REQ_GetCurrent(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

static void AUDIO_REQ_SetCurrent(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

/**
  * @}
  */ 

/** @defgroup USBD_AUDIO_Private_Variables
  * @{
  */ 

USBD_ClassTypeDef  USBD_AUDIO = 
{
  USBD_AUDIO_Init,
  USBD_AUDIO_DeInit,
  USBD_AUDIO_Setup,
  USBD_AUDIO_EP0_TxReady,  
  USBD_AUDIO_EP0_RxReady,
  USBD_AUDIO_DataIn,
  USBD_AUDIO_DataOut,
  USBD_AUDIO_SOF,
  USBD_AUDIO_IsoINIncomplete,
  USBD_AUDIO_IsoOutIncomplete,      
  USBD_AUDIO_GetCfgDesc,
  USBD_AUDIO_GetCfgDesc, 
  USBD_AUDIO_GetCfgDesc,
  USBD_AUDIO_GetDeviceQualifierDesc,
};

/* USB AUDIO device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_AUDIO_CfgDesc[USB_AUDIO_CONFIG_DESC_SIZ] __ALIGN_END =
{
  /* Configuration 1 */
  0x09,                                 /* bLength */
  USB_DESC_TYPE_CONFIGURATION,          /* bDescriptorType */
  LOBYTE(USB_AUDIO_CONFIG_DESC_SIZ),    /* wTotalLength  109 bytes*/
  HIBYTE(USB_AUDIO_CONFIG_DESC_SIZ),      
  0x02,                                 /* bNumInterfaces */
  0x01,                                 /* bConfigurationValue */
  0x00,                                 /* iConfiguration */
  0xC0,                                 /* bmAttributes  BUS Powred*/
  0x32,                                 /* bMaxPower = 100 mA*/
  /* 09 byte*/
  
  /* USB Speaker Standard interface descriptor */
  AUDIO_INTERFACE_DESC_SIZE,            /* bLength */
  USB_DESC_TYPE_INTERFACE,              /* bDescriptorType */
  0x00,                                 /* bInterfaceNumber */
  0x00,                                 /* bAlternateSetting */
  0x00,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOCONTROL,          /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */
  /* 09 byte*/
  
  /* USB Speaker Class-specific AC Interface Descriptor */
  AUDIO_INTERFACE_DESC_SIZE,            /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_HEADER,                 /* bDescriptorSubtype */
  0x00,          /* 1.00 */             /* bcdADC */
  0x01,
  0x27,                                 /* wTotalLength = 39*/
  0x00,
  0x01,                                 /* bInCollection */
  0x01,                                 /* baInterfaceNr */
  /* 09 byte*/
  
  /* USB Speaker Input Terminal Descriptor */
  AUDIO_INPUT_TERMINAL_DESC_SIZE,       /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_INPUT_TERMINAL,         /* bDescriptorSubtype */
  0x01,                                 /* bTerminalID */
  0x01,                                 /* wTerminalType AUDIO_TERMINAL_USB_STREAMING   0x0101 */
  0x01,
  0x00,                                 /* bAssocTerminal */
  0x02,                                 /* bNrChannels */
  0x03,                                 /* wChannelConfig L-R */
  0x00,
  0x00,                                 /* iChannelNames */
  0x00,                                 /* iTerminal */
  /* 12 byte*/
  
  /* USB Speaker Audio Feature Unit Descriptor */
  0x0A,                                 /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_FEATURE_UNIT,           /* bDescriptorSubtype */
  AUDIO_OUT_STREAMING_CTRL,             /* bUnitID */
  0x01,                                 /* bSourceID */
  0x01,                                 /* bControlSize */
  AUDIO_CONTROL_MUTE | AUDIO_CONTROL_VOLUME, /* bmaControls(0): master ch */
  0,                                    /* bmaControls(1): L */
  0,                                    /* bmaControls(2): R */
  0x00,                                 /* iTerminal */
  /* 10 byte*/
  
  /*USB Speaker Output Terminal Descriptor */
  0x09,      /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_CONTROL_OUTPUT_TERMINAL,        /* bDescriptorSubtype */
  0x03,                                 /* bTerminalID */
  0x01,                                 /* wTerminalType  0x0301*/
  0x03,
  0x00,                                 /* bAssocTerminal */
  0x02,                                 /* bSourceID */
  0x00,                                 /* iTerminal */
  /* 09 byte*/
  
  /* USB Speaker Standard AS Interface Descriptor - Audio Streaming Zero Bandwith */
  /* Interface 1, Alternate Setting 0                                             */
  AUDIO_INTERFACE_DESC_SIZE,  /* bLength */
  USB_DESC_TYPE_INTERFACE,        /* bDescriptorType */
  0x01,                                 /* bInterfaceNumber */
  0x00,                                 /* bAlternateSetting */
  0x00,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOSTREAMING,        /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */
  /* 09 byte*/
  
  /* USB Speaker Standard AS Interface Descriptor - Audio Streaming Operational */
  /* Interface 1, Alternate Setting 1                                           */
  AUDIO_INTERFACE_DESC_SIZE,  /* bLength */
  USB_DESC_TYPE_INTERFACE,        /* bDescriptorType */
  0x01,                                 /* bInterfaceNumber */
  0x01,                                 /* bAlternateSetting */
  0x02,                                 /* bNumEndpoints */
  USB_DEVICE_CLASS_AUDIO,               /* bInterfaceClass */
  AUDIO_SUBCLASS_AUDIOSTREAMING,        /* bInterfaceSubClass */
  AUDIO_PROTOCOL_UNDEFINED,             /* bInterfaceProtocol */
  0x00,                                 /* iInterface */
  /* 09 byte*/
  
  /* USB Speaker Audio Streaming Interface Descriptor */
  AUDIO_STREAMING_INTERFACE_DESC_SIZE,  /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_STREAMING_GENERAL,              /* bDescriptorSubtype */
  0x01,                                 /* bTerminalLink */
  0x01,                                 /* bDelay */
  0x01,                                 /* wFormatTag AUDIO_FORMAT_PCM  0x0001*/
  0x00,
  /* 07 byte*/
  
  /* USB Speaker Audio Type I Format Interface Descriptor(なんでType3だったんじゃ…) */
  0x0B,                                 /* bLength */
  AUDIO_INTERFACE_DESCRIPTOR_TYPE,      /* bDescriptorType */
  AUDIO_STREAMING_FORMAT_TYPE,          /* bDescriptorSubtype */
  AUDIO_FORMAT_TYPE_I,                  /* bFormatType */ 
  0x02,                                 /* bNrChannels */
  0x04,                                 /* bSubFrameSize */
  32,                                   /* bBitResolution (32-bits per sample) */ 
  0x01,                                 /* bSamFreqType only one frequency supported */ 
  AUDIO_SAMPLE_FREQ(USBD_AUDIO_FREQ),         /* Audio sampling frequency coded on 3 bytes */
  /* 11 byte*/
  
  /* Endpoint 0x01 - Standard Descriptor */
  AUDIO_STANDARD_ENDPOINT_DESC_SIZE,    /* bLength */
  USB_DESC_TYPE_ENDPOINT,               /* bDescriptorType */
  AUDIO_OUT_EP,                         /* bEndpointAddress 1 out endpoint*/
  USBD_EP_TYPE_ISOC | 0x04,             /* bmAttributes(Isochronous +  Async) */
  AUDIO_PACKET_SZE(AUDIO_OUT_PACKET),   /* wMaxPacketSize in Bytes */
  0x01,                                 /* bInterval */
  0x00,                                 /* bRefresh */
  AUDIO_FEEDBACK_EP,                    /* bSynchAddress */
  /* 09 byte*/
  
  /* Endpoint - Audio Streaming Descriptor*/
  AUDIO_STREAMING_ENDPOINT_DESC_SIZE,   /* bLength */
  AUDIO_ENDPOINT_DESCRIPTOR_TYPE,       /* bDescriptorType */
  AUDIO_ENDPOINT_GENERAL,               /* bDescriptor */
  0x01,                                 /* bmAttributes(周波数変更可) */
  0x00,                                 /* bLockDelayUnits */
  0x00,                                 /* wLockDelay */
  0x00,
  /* 07 byte*/
  
  /* Endpoint 0x81(ﾌｨｰﾄﾞﾊﾞｯｸ用) - Standard Descriptor */
  AUDIO_STANDARD_ENDPOINT_DESC_SIZE,    /* bLength */
  USB_DESC_TYPE_ENDPOINT,               /* bDescriptorType */
  AUDIO_FEEDBACK_EP,                    /* bEndpointAddress 1 in endpoint*/
  USBD_EP_TYPE_ISOC,                    /* bmAttributes */
  3,                                    /* wMaxPacketSize in Bytes (usb_20.pdfの方に記載有り。feedbackは3ﾊﾞｲﾄで通知…と) */
  0,                                   
  0x01,                                 /* bInterval */
  0x05,                                 /* bRefresh(測定期間の指数部。2^0x05 = 32ms) */
  0x00,                                 /* bSynchAddress */
  /* 09 byte*/
} ;

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_AUDIO_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END=
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

/**
  * @}
  */ 

/** @defgroup USBD_AUDIO_Private_Functions
  * @{
  */ 

/**
  * @brief  AUDIO_UpdateFeedback
  *         ﾌｨｰﾄﾞﾊﾞｯｸ値更新
  * @param  pdev: device instance
  */
static void AUDIO_UpdateFeedback(USBD_HandleTypeDef *pdev){
	
	USBD_AUDIO_HandleTypeDef *haudio = (USBD_AUDIO_HandleTypeDef*)pdev->pClassData;
	
	extern signed int audio_buffer_getfeedback();
	
	/* ﾌｨｰﾄﾞﾊﾞｯｸ値更新 */
	int diff = audio_buffer_getfeedback();
	
	diff <<= 14;	/* 整数部のLSB揃え */
	diff /= 1000;	/* 1msあたりの値に変換 */
//	diff /= AUDIO_FEEDBACK_COUNT;	/* 次のﾌｨｰﾄﾞﾊﾞｯｸ値通知までの間に誤差を相殺させる */
	
	/* 最上位 8bit: 未使用 */
	/* 次の  10bit: 整数部 */
	/* 最下位14bit: 小数部 */
	haudio->feedback_val = diff + (((USBD_AUDIO_FREQ << 10) / 1000) << 4);
}

/**
  * @brief  USBD_AUDIO_Init
  *         Initialize the AUDIO interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_AUDIO_Init (USBD_HandleTypeDef *pdev, 
                               uint8_t cfgidx)
{
  USBD_AUDIO_HandleTypeDef   *haudio;
  
  /* Open EP OUT(ﾃﾞｰﾀｽﾄﾘｰﾑ) */
  USBD_LL_OpenEP(pdev, AUDIO_OUT_EP, USBD_EP_TYPE_ISOC, AUDIO_OUT_PACKET);
  
  /* Open EP IN(ﾌｨｰﾄﾞﾊﾞｯｸ用) */
  USBD_LL_OpenEP(pdev, AUDIO_FEEDBACK_EP, USBD_EP_TYPE_ISOC, 3);
  
  /* Allocate Audio structure */
  pdev->pClassData = USBD_malloc(sizeof (USBD_AUDIO_HandleTypeDef));
  
  if(pdev->pClassData == NULL)
  {
    return USBD_FAIL; 
  }
  else
  {
    haudio = (USBD_AUDIO_HandleTypeDef*) pdev->pClassData;
    haudio->alt_setting = 0;
    haudio->feedback_val = (USBD_AUDIO_FREQ << 14) / 1000;	/* @todo 周波数選択時に初期化 */
    
    haudio->mute = 0;
    haudio->vol = 0;
    
    /* Initialize the Audio output Hardware layer */
    /* @todo 周波数 / Alt変更時に再実行(optionで区別?) */
    if (((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->Init(USBD_AUDIO_FREQ, AUDIO_DEFAULT_VOLUME, 0) != USBD_OK)
    {
      return USBD_FAIL;
    }
    
	USBD_LL_Transmit(pdev, AUDIO_FEEDBACK_EP, &haudio->feedback_val, 3);
    /* Prepare Out endpoint to receive 1st packet */ 
    USBD_LL_PrepareReceive(pdev,
                           AUDIO_OUT_EP,
                           haudio->buffer,
                           AUDIO_OUT_PACKET);
  }
  return USBD_OK;
}

/**
  * @brief  USBD_AUDIO_Init
  *         DeInitialize the AUDIO layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_AUDIO_DeInit (USBD_HandleTypeDef *pdev, 
                                 uint8_t cfgidx)
{
  
  /* Close EP OUT */
  USBD_LL_CloseEP(pdev, AUDIO_OUT_EP);
  USBD_LL_CloseEP(pdev, AUDIO_FEEDBACK_EP);

  /* DeInit  physical Interface components */
  if(pdev->pClassData != NULL)
  {
   ((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->DeInit(0);
    USBD_free(pdev->pClassData);
    pdev->pClassData = NULL;
  }
  
  return USBD_OK;
}

/**
  * @brief  USBD_AUDIO_Setup
  *         Handle the AUDIO specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t  USBD_AUDIO_Setup (USBD_HandleTypeDef *pdev, 
                                USBD_SetupReqTypedef *req)
{
  USBD_AUDIO_HandleTypeDef   *haudio;
  uint16_t len;
  uint8_t *pbuf;
  uint8_t ret = USBD_OK;
  haudio = (USBD_AUDIO_HandleTypeDef*) pdev->pClassData;
  
  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :  
    switch (req->bRequest)
    {
    case AUDIO_REQ_GET_CUR:
    case AUDIO_REQ_GET_MIN:
    case AUDIO_REQ_GET_MAX:
    case AUDIO_REQ_GET_RES:
      if (AUDIO_REQ_GetCurrent(pdev, req) == USBD_OK){
		USBD_CtlSendData (pdev, haudio->control.data, req->wLength);
      }else{
        USBD_CtlError (pdev, req);
        ret = USBD_FAIL; 
      }
      break;
      
    case AUDIO_REQ_SET_CUR:
      AUDIO_REQ_SetCurrent(pdev, req);   
      break;
      
    default:
      USBD_CtlError (pdev, req);
      ret = USBD_FAIL; 
    }
    break;
    
  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR:      
      if( (req->wValue >> 8) == AUDIO_DESCRIPTOR_TYPE)
      {
        pbuf = USBD_AUDIO_CfgDesc + 18;
        len = MIN(USB_AUDIO_DESC_SIZ , req->wLength);
        
        
        USBD_CtlSendData (pdev, 
                          pbuf,
                          len);
      }
      break;
      
    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        (uint8_t *)&(haudio->alt_setting),
                        1);
      break;
      
    case USB_REQ_SET_INTERFACE :
      if ((uint8_t)(req->wValue) <= USBD_MAX_NUM_INTERFACES)
      {
        haudio->alt_setting = (uint8_t)(req->wValue);
        
        if (haudio->alt_setting == 0){
          /* 0帯域 … 停止を通知 */
          /* @memo I2Sを停止しない運用。呼ばない */
          //((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->AudioCmd(&haudio->buffer[0], 0, AUDIO_CMD_STOP);
        }else{
		  haudio->feedback_val = (USBD_AUDIO_FREQ << 14) / 1000;
		  
		  /* 開始を通知 */
		  /* @todo 周波数選択 */
          /* @memo I2Sを停止しない運用。あちらの初期化時に実行する為、呼ばない */
		  //((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->AudioCmd(&haudio->buffer[0], 0, AUDIO_CMD_START);
		}
      }
      else
      {
        /* Call the error management function (command will be nacked */
        USBD_CtlError (pdev, req);
      }
      break;      
      
    default:
      USBD_CtlError (pdev, req);
      ret = USBD_FAIL;     
    }
  }
  return ret;
}


/**
  * @brief  USBD_AUDIO_GetCfgDesc 
  *         return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_AUDIO_GetCfgDesc (uint16_t *length)
{
  *length = sizeof (USBD_AUDIO_CfgDesc);
  return USBD_AUDIO_CfgDesc;
}

/**
  * @brief  USBD_AUDIO_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_AUDIO_DataIn (USBD_HandleTypeDef *pdev, 
                              uint8_t epnum)
{
	USBD_AUDIO_HandleTypeDef *haudio = (USBD_AUDIO_HandleTypeDef*)pdev->pClassData;
	
#if 1
	if (epnum == (AUDIO_FEEDBACK_EP & 0x7F))
	{
//		HAL_GPIO_TogglePin(GPIOD, LD3_Pin);
		AUDIO_UpdateFeedback(pdev);
		USBD_LL_Transmit(pdev, AUDIO_FEEDBACK_EP, &haudio->feedback_val, 3);
	}
#endif
	return USBD_OK;
}

/**
  * @brief  USBD_AUDIO_EP0_RxReady
  *         handle EP0 Rx Ready event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t  USBD_AUDIO_EP0_RxReady (USBD_HandleTypeDef *pdev)
{
  USBD_AUDIO_HandleTypeDef   *haudio;
  haudio = (USBD_AUDIO_HandleTypeDef*) pdev->pClassData;
  
  if (haudio->control.cmd == AUDIO_REQ_SET_CUR)
  {/* In this driver, to simplify code, only SET_CUR request is managed */

    if (haudio->control.unit == AUDIO_OUT_STREAMING_CTRL)
    {
      switch (haudio->control.sel){
		case AUDIO_CONTROL_MUTE:{
			((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->MuteCtl(haudio->mute = haudio->control.data[0]);
			break;
		}
		case AUDIO_CONTROL_VOLUME:{
			((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->VolumeCtl(haudio->vol = ((int16_t *)(haudio->control.data))[0]);
			break;
		}
	  }
      haudio->control.cmd = 0;
      haudio->control.len = 0;
    }
  } 

  return USBD_OK;
}
/**
  * @brief  USBD_AUDIO_EP0_TxReady
  *         handle EP0 TRx Ready event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t  USBD_AUDIO_EP0_TxReady (USBD_HandleTypeDef *pdev)
{
  /* Only OUT control data are processed */
  return USBD_OK;
}
/**
  * @brief  USBD_AUDIO_SOF
  *         handle SOF event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t  USBD_AUDIO_SOF (USBD_HandleTypeDef *pdev)
{
	return USBD_OK;
}

/**
  * @brief  USBD_AUDIO_SOF
  *         handle SOF event
  * @param  pdev: device instance
  * @retval status
  */
void  USBD_AUDIO_Sync (USBD_HandleTypeDef *pdev, AUDIO_OffsetTypeDef offset)
{
}

/**
  * @brief  USBD_AUDIO_IsoINIncomplete
  *         handle data ISO IN Incomplete event
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_AUDIO_IsoINIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
	USBD_AUDIO_HandleTypeDef *haudio = (USBD_AUDIO_HandleTypeDef*)pdev->pClassData;
	
//	if (epnum == (AUDIO_FEEDBACK_EP & 0x7F))	/* 適当な値が来るので無視 */
	{
		extern HAL_StatusTypeDef HAL_PCD_EP_CancelTransmit(PCD_HandleTypeDef *hpcd, uint8_t ep_addr);
		
		/* 転送失敗 → Isochronous転送のEven / Odd設定が違う可能性 → 再度上書き */
		HAL_PCD_EP_CancelTransmit(pdev->pData, AUDIO_FEEDBACK_EP);
		AUDIO_UpdateFeedback(pdev);
		USBD_LL_Transmit(pdev, AUDIO_FEEDBACK_EP, &haudio->feedback_val, 3);
		
//		HAL_GPIO_TogglePin(GPIOD, LD3_Pin);
	}
	
	return USBD_OK;
}
/**
  * @brief  USBD_AUDIO_IsoOutIncomplete
  *         handle data ISO OUT Incomplete event
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_AUDIO_IsoOutIncomplete (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
	USBD_AUDIO_HandleTypeDef *haudio = (USBD_AUDIO_HandleTypeDef*)pdev->pClassData;
	
//	if (epnum == AUDIO_OUT_EP)	/* 適当な値が来るので無視 */
	{
		extern HAL_StatusTypeDef HAL_PCD_EP_CancelReceive(PCD_HandleTypeDef *hpcd, uint8_t ep_addr);
		
		/* 転送失敗 → Isochronous転送のEven / Odd設定が違う可能性 → 再度上書き */
		HAL_PCD_EP_CancelReceive(pdev->pData, AUDIO_OUT_EP);
		USBD_LL_PrepareReceive(pdev, AUDIO_OUT_EP, &haudio->buffer[0], AUDIO_OUT_PACKET);
		
		HAL_GPIO_TogglePin(GPIOD, LD6_Pin);
		
		/* このﾌﾚｰﾑで供給される予定だった分を零埋め */
		/* @memo WASAPIの一時停止中など、正常動作時の再生中でもこのｲﾍﾞﾝﾄが発生する場面がある */
		/* @memo このｲﾍﾞﾝﾄ発生時は通知ﾀｲﾐﾝｸﾞが乱れている可能性がある為、通常よりもﾌｨｰﾄﾞﾊﾞｯｸ値の反映を増やして早期収束を図る */
		extern signed int audio_buffer_getfeedback();
		((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->AudioCmd(&haudio->buffer[0], ((USBD_AUDIO_FREQ / 1000) + (audio_buffer_getfeedback() / AUDIO_FEEDBACK_COUNT)) * 8 , AUDIO_CMD_MISSING);
	}
	return USBD_OK;
}
/**
  * @brief  USBD_AUDIO_DataOut
  *         handle data OUT Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t  USBD_AUDIO_DataOut (USBD_HandleTypeDef *pdev, 
                              uint8_t epnum)
{
  USBD_AUDIO_HandleTypeDef   *haudio;
  haudio = (USBD_AUDIO_HandleTypeDef*) pdev->pClassData;
  
  if (epnum == AUDIO_OUT_EP)
  {
	int rx_len = USBD_GetRxCount(pdev, AUDIO_OUT_EP);
	
    ((USBD_AUDIO_ItfTypeDef *)pdev->pUserData)->AudioCmd(&haudio->buffer[0], rx_len, AUDIO_CMD_PLAY);
    
    /* Prepare Out endpoint to receive next audio packet */
    USBD_LL_PrepareReceive(pdev,
                           AUDIO_OUT_EP,
                           &haudio->buffer[0], 
                           AUDIO_OUT_PACKET);
	
//	HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_SET);
//	HAL_GPIO_TogglePin(GPIOD, LD3_Pin);
  }
  
  return USBD_OK;
}

/**
  * @brief  AUDIO_Req_GetCurrent
  *         Handles the GET_CUR / MIN / MAX /RES Audio control request.
  * @param  pdev: instance
  * @param  req: setup class request
  * @retval status
  */
static uint8_t AUDIO_REQ_GetCurrent(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{  
	USBD_AUDIO_HandleTypeDef   *haudio;
	haudio = (USBD_AUDIO_HandleTypeDef*) pdev->pClassData;

	memset(haudio->control.data, 0, 64);

	/* 対象別動作 */
	if (((req->bmRequest & 0x0F) == 0x01)/* && ((req->wIndex >> 8) == AUDIO_OUT_STREAMING_CTRL)*/){
		/* ｵｰﾃﾞｨｵｺﾝﾄﾛｰﾙ側 */
		/* Feature unit control */
		switch (req->wValue >> 8){
			case AUDIO_CONTROL_MUTE:{
				((unsigned int *)(haudio->control.data))[0] = 
					(req->bRequest == AUDIO_REQ_GET_CUR) ? haudio->mute:
					(req->bRequest == AUDIO_REQ_GET_MIN) ? 0:
					(req->bRequest == AUDIO_REQ_GET_MAX) ? 1:
				/*	(req->bRequest == AUDIO_REQ_GET_RES) ? */ 1;
				break;
			}
			case AUDIO_CONTROL_VOLUME:{
				((unsigned int *)(haudio->control.data))[0] = 
					(req->bRequest == AUDIO_REQ_GET_CUR) ? haudio->vol:
					(req->bRequest == AUDIO_REQ_GET_MIN) ? -102 * 0x100:	/* -102dB */
					(req->bRequest == AUDIO_REQ_GET_MAX) ? 0x0000:			/* -0dB */
				/*	(req->bRequest == AUDIO_REQ_GET_RES) ? */ 0x80;			/* 0.5dB単位 */
				break;
			}
		}
		return USBD_OK;
		
	}else if (((req->bmRequest & 0x0F) == 0x02)/* && (req->wIndex == AUDIO_OUT_EP) */){
		/* Endpoint control */
		switch (req->wValue){
			case AUDIO_CONTROL_SAMPLING_FREQ:{
				((unsigned int *)(haudio->control.data))[0] = 
					(req->bRequest == AUDIO_REQ_GET_CUR) ? USBD_AUDIO_FREQ:
					(req->bRequest == AUDIO_REQ_GET_MIN) ? USBD_AUDIO_FREQ:
					(req->bRequest == AUDIO_REQ_GET_MAX) ? USBD_AUDIO_FREQ:
				/*	(req->bRequest == AUDIO_REQ_GET_RES) ? */ 1;
				return USBD_OK;
			}
		}
	}
	
	return USBD_FAIL;
}

/**
  * @brief  AUDIO_Req_SetCurrent
  *         Handles the SET_CUR Audio control request.
  * @param  pdev: instance
  * @param  req: setup class request
  * @retval status
  */
static void AUDIO_REQ_SetCurrent(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{ 
  USBD_AUDIO_HandleTypeDef   *haudio;
  haudio = (USBD_AUDIO_HandleTypeDef*) pdev->pClassData;
  
  if (req->wLength)
  {
    /* Prepare the reception of the buffer over EP0 */
    USBD_CtlPrepareRx (pdev,
                       haudio->control.data,                                  
                       req->wLength);    
    
    haudio->control.cmd = AUDIO_REQ_SET_CUR;     /* Set the request value */
    haudio->control.len = req->wLength;          /* Set the request data length */
    haudio->control.sel = HIBYTE(req->wValue);   /* Set the request control selector */
    haudio->control.unit = HIBYTE(req->wIndex);  /* Set the request target unit */
  }
}


/**
* @brief  DeviceQualifierDescriptor 
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
static uint8_t  *USBD_AUDIO_GetDeviceQualifierDesc (uint16_t *length)
{
  *length = sizeof (USBD_AUDIO_DeviceQualifierDesc);
  return USBD_AUDIO_DeviceQualifierDesc;
}

/**
* @brief  USBD_AUDIO_RegisterInterface
* @param  fops: Audio interface callback
* @retval status
*/
uint8_t  USBD_AUDIO_RegisterInterface  (USBD_HandleTypeDef   *pdev, 
                                        USBD_AUDIO_ItfTypeDef *fops)
{
  if(fops != NULL)
  {
    pdev->pUserData= fops;
  }
  return 0;
}
/**
  * @}
  */ 


/**
  * @}
  */ 


/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
