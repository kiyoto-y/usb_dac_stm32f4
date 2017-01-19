/***********************************************************************

                  -- USB-DAC for STM32F4 Discovery --

------------------------------------------------------------------------

	DAC driver(Cirrus Logic CS43L22).

	Copyright (c) 2016 Kiyoto.
	This file is licensed under the MIT License. See /LICENSE.TXT file.

***********************************************************************/

#include "main.h"
#include "stm32f4xx_hal.h"

#define CS43L22_CS_ID				(0x94)			/* I2Cﾊﾞｽ ﾃﾞﾊﾞｲｽID */

#define CS43L22_REG_ID				(0x01)			/* ﾚｼﾞｽﾀ:  */
#define CS43L22_REG_POW_CTL1		(0x02)			/* ﾚｼﾞｽﾀ:  */
#define CS43L22_REG_POW_CTL2		(0x04)			/* ﾚｼﾞｽﾀ:  */
#define CS43L22_REG_CLK_CTL			(0x05)			/* ﾚｼﾞｽﾀ:  */
#define CS43L22_REG_ITF_CTL1		(0x06)			/* ﾚｼﾞｽﾀ:  */

#define CS43L22_REG_PCM_VOL_A		(0x1A)			/* ﾚｼﾞｽﾀ:  */
#define CS43L22_REG_PCM_VOL_B		(0x1B)			/* ﾚｼﾞｽﾀ:  */
#define CS43L22_REG_MASTER_VOL_A	(0x20)			/* ﾚｼﾞｽﾀ:  */
#define CS43L22_REG_MASTER_VOL_B	(0x21)			/* ﾚｼﾞｽﾀ:  */

/* ﾒｲﾝ側で宣言しているI2C制御用ﾃﾞﾊﾞｲｽｲﾝｽﾀﾝｽ */
extern I2C_HandleTypeDef hi2c1;

/* debug */
uint8_t cs43l22_id;	/* I2C導通確認用 */

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	音量設定
	
	vol:	0.5dB単位、2の補数表現
*/
void cs43l22_set_vol(int vol){
	
	/* @memo byte化すると負数側の有効値域が正数範囲とかぶるが、負数からの続きとみなす */
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_MASTER_VOL_A, vol}, 2, -1);
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_MASTER_VOL_B, vol}, 2, -1);
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ﾐｭｰﾄ設定
	
	mute:	非零でﾐｭｰﾄ
*/
void cs43l22_set_mute(int mute){
	mute = mute ? 0x80 : 0x00;
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_PCM_VOL_A, mute}, 2, -1);
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_PCM_VOL_B, mute}, 2, -1);
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	DAC初期化
	
	freq:	(未使用)
	vol:	0.5dB単位、2の補数表現
*/
void cs43l22_init(int freq, int vol){
	
	HAL_I2C_Mem_Read(&hi2c1, CS43L22_CS_ID, CS43L22_REG_ID, I2C_MEMADD_SIZE_8BIT, &cs43l22_id, sizeof(cs43l22_id), -1);
	
	/* ﾊﾟﾜｰｵﾌ */
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_POW_CTL1, 0x01}, 2, -1);
	
	/* ﾃﾞｰﾀｼｰﾄ記載の初期化ｼｰｹﾝｽ */
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){0x00, 0x99}, 2, -1);
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){0x47, 0x80}, 2, -1);
	{
		uint8_t rd_data[1];
		HAL_I2C_Mem_Read(&hi2c1, CS43L22_CS_ID, 0x32, I2C_MEMADD_SIZE_8BIT, rd_data, sizeof(rd_data), -1);
		rd_data[0] |= 0x80;
		HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){0x32, rd_data[0]}, 2, -1);
		HAL_I2C_Mem_Read(&hi2c1, CS43L22_CS_ID, 0x32, I2C_MEMADD_SIZE_8BIT, rd_data, sizeof(rd_data), -1);
		rd_data[0] &= ~0x80;
		HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){0x32, rd_data[0]}, 2, -1);
	}
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){0x00, 0x00}, 2, -1);
	
	/* ﾊﾞｽｸﾛｯｸを自動判別に(Slaveで動かすので) */
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_CLK_CTL, 0x81}, 2, -1);
	
	/* ｽﾚｰﾌﾞﾓｰﾄﾞ、I2Sﾌｫｰﾏｯﾄ(DSPﾓｰﾄﾞは使わない) */  
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_ITF_CTL1, 0x04}, 2, -1);
	
	/* ﾍｯﾄﾞﾌｫﾝ出力側だけがLineOutに出ているので */
	/* →一応、初期値のままでも正常動作できる配線になっている為、特に触らない */
//	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_POW_CTL1, 0xAF}, 2, -1);
	
	cs43l22_set_vol(vol);
	cs43l22_set_mute(0);	/* ﾐｭｰﾄ解除 */
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ｱﾅﾛｸﾞ段動作開始
	
	freq:	(未使用)
	vol:	(未使用)
	
	このDACでは必要ないが、ｸﾛｯｸ変更に合わせてﾌｨﾙﾀの変更等がしたければここで。
*/
void cs43l22_start(int freq, int vol){
	
	/* @warn 呼出前にMCLKを出力開始させておくこと! */
	
	/* ﾐｭｰﾄｵﾌ */
//	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_PCM_VOL_A, 0x00}, 2, -1);
//	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_PCM_VOL_B, 0x00}, 2, -1);
	
	/* ﾊﾟﾜｰｵﾝ */
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_POW_CTL1, 0x9E}, 2, -1);

	HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_SET);
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ｱﾅﾛｸﾞ段動作停止
*/
void cs43l22_stop(){
	
	/* ﾐｭｰﾄｵﾝ */
//	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_PCM_VOL_A, 0x80}, 2, -1);
//	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_PCM_VOL_B, 0x80}, 2, -1);
	
	/* ﾊﾟﾜｰｵﾌ */
	HAL_I2C_Master_Transmit(&hi2c1, CS43L22_CS_ID, (uint8_t []){CS43L22_REG_POW_CTL1, 0x01}, 2, -1);
	
	/* 確実に100μs以上待つ(1ms未満の精度が不確かなので、2ms以上) */
	HAL_Delay(2);
	
	/* これ以降はいつでもMCLKを止めて良い */
	
	HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_RESET);
}

