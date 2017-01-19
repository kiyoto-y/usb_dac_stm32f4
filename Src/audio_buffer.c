/***********************************************************************

                  -- USB-DAC for STM32F4 Discovery --

------------------------------------------------------------------------

	Audio stream buffer

	Copyright (c) 2016 Kiyoto.
	This file is licensed under the MIT License. See /LICENSE.TXT file.

***********************************************************************/

#include "audio_buffer.h"

/* ｵｰﾃﾞｨｵﾊﾞｯﾌｧ */
struct {
	unsigned char data[AUDIO_BUFFER_SIZE];
	unsigned int wp;
} audio_buffer __attribute__ ((aligned(128)));

/* ﾌｫｰﾏｯﾄﾊﾟﾗﾒﾀ(@memo ﾊﾞｯﾌｧ上のﾌﾚｰﾑ長は8ﾊﾞｲﾄ…32bitｽﾃﾚｵ…固定) */
volatile int audio_buffer_srate = 0;		/* ｻﾝﾌﾟﾘﾝｸﾞﾚｰﾄ */

/* ﾊﾞｯﾌｧとは別に、ﾊﾞｯﾌｧへのﾃﾞｰﾀ流入出数を管理するｶｳﾝﾀ */
volatile signed int audio_buffer_count = 0;			/* in/out両方で更新する値 */
volatile signed int audio_buffer_count_fixed = 0;	/* out側だけで更新する値(out更新時に上記値をｺﾋﾟｰ) */

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	初期化
*/
void audio_buffer_init(){
	audio_buffer.wp = 0;
	audio_buffer_count = 0;
	audio_buffer_count_fixed = 0;
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ﾊﾞｯﾌｧへﾃﾞｰﾀｽﾄﾘｰﾑを供給する
	
	@warning sizeは必ずﾌﾚｰﾑ長の整数倍。0は不可
*/
void audio_buffer_feed(unsigned char *buf, int size){
	
	/* -O0向けの最適化 */
	struct {
		unsigned short val[4];
	} *in_data = (void *)buf, *out_data = (void *)(&(audio_buffer.data[audio_buffer.wp]));
	
	int i = audio_buffer.wp;
	const int size_org = size;
	
	/* 基本的に割禁にしない運用なので、先に更新 */
	/* 但し、本来この部分だけはｶﾞｰﾄﾞすべき */
	//__disable_irq();
	audio_buffer.wp = (audio_buffer.wp + size_org) % sizeof(audio_buffer.data);
	//__enable_irq();
	
	/* 上位ﾜｰﾄﾞ→下位ﾜｰﾄﾞの順に送信する必要があるので、ここで並べ替え */
	/* (でもﾜｰﾄﾞ内はﾘﾄﾙｴﾝﾃﾞｨｱﾝ…) */
	do{
		/* I2S L */
		out_data->val[0] = in_data->val[1];
		out_data->val[1] = in_data->val[0];
		/* I2S R */
		out_data->val[2] = in_data->val[3];
		out_data->val[3] = in_data->val[2];
		
		in_data++;
		
		i += 8;
		i = (i >= sizeof(audio_buffer.data)) ? 0 : i;
		
		out_data = (void *)(&(audio_buffer.data[i]));
		
	}while (size -= 8);
	
	audio_buffer_count += size_org;
	
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ﾊﾞｯﾌｧﾌｨﾙ
	
	@warning sizeは必ずﾌﾚｰﾑ長の整数倍。0は不可
*/
void audio_buffer_fill(int val, int size){
	
	int i = audio_buffer.wp;
	const int size_org = size;
	
	/* 基本的に割禁にしない運用なので、先に更新 */
	/* 但し、本来この部分だけはｶﾞｰﾄﾞすべき */
	//__disable_irq();
	audio_buffer.wp = (audio_buffer.wp + size_org) % sizeof(audio_buffer.data);
	//__enable_irq();
	
	do{
		*((int *)(&(audio_buffer.data[i]))) = val;
		i += 4;
		i = (i >= sizeof(audio_buffer.data)) ? 0 : i;
	}while (size -= 4);
	
	audio_buffer_count += size_org;
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	次のﾊﾞｯﾌｧ中間地点 / 末尾までを指定ﾊﾞｲﾄで埋める
	
	中間 / 末尾 == DMA転送で割り込みがかかる地点を想定。
	
	@warning sizeは必ずﾌﾚｰﾑ長の整数倍。0は不可
*/
void audio_buffer_fill_next_ip(int val){
	
	int i = audio_buffer.wp;
	int size = (sizeof(audio_buffer.data) / 2) - (i % (sizeof(audio_buffer.data) / 2));
	const int size_org = size;
	
	/* 基本的に割禁にしない運用なので、先に更新 */
	/* 但し、本来この部分だけはｶﾞｰﾄﾞすべき */
	//__disable_irq();
	audio_buffer.wp = (audio_buffer.wp + size_org) % sizeof(audio_buffer.data);
	//__enable_irq();
	
	/* 次の割り込み地点までﾌｨﾙ */
	do{
		*((int *)(&(audio_buffer.data[i]))) = val;
		i += 4;
		i = (i >= sizeof(audio_buffer.data)) ? 0 : i;
	}while (size -= 4);
	
	audio_buffer_count += size_org;
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ﾊﾞｯﾌｧからの読み出し(指定ｵﾌｾｯﾄのﾊﾞｯﾌｧｱﾄﾞﾚｽ取得)
	
	@memo ﾌｨｰﾄﾞﾊﾞｯｸ値算出用の残量計が更新される
	      このため、DMA転送する場合も転送完了ｲﾍﾞﾝﾄ毎に呼び出すこと
	      また、読みだすｻｲｽﾞを必ず申告すること
	
	@warning sizeは必ずﾌﾚｰﾑ長の整数倍。0許容
*/
unsigned char *audio_buffer_getptr(int ofs, int size){
	
	if (ofs < sizeof(audio_buffer.data)){
		
		audio_buffer_count -= size;
		
		/* ｵｰﾊﾞ / ｱﾝﾀﾞｰﾌﾛｰ対策 */
		while (audio_buffer_count >= (signed int)AUDIO_BUFFER_SIZE){
//			HAL_GPIO_TogglePin(GPIOD, LD5_Pin);
			audio_buffer_count -= AUDIO_BUFFER_SIZE;
		}
		while (audio_buffer_count < 0){
//			HAL_GPIO_TogglePin(GPIOD, LD4_Pin);
			audio_buffer_count += AUDIO_BUFFER_SIZE;
		}
		audio_buffer_count_fixed = audio_buffer_count;
		
		/* ﾊﾞｯﾌｧ残数を表示(ﾌｨｰﾄﾞﾊﾞｯｸによって左右均等に点くような状態が好ましい) */
		HAL_GPIO_WritePin(GPIOD, LD4_Pin, (audio_buffer_count_fixed <= (signed int)(AUDIO_BUFFER_SIZE / 2)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, LD5_Pin, (audio_buffer_count_fixed >= (signed int)(AUDIO_BUFFER_SIZE / 2)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		
		return &(audio_buffer.data[ofs]);
		
	}else{
		return (void *)0;
	}
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	現在のﾌｨｰﾄﾞﾊﾞｯｸ値を取得
	
	ﾊﾞｯﾌｧ残量から算出されたﾌｨｰﾄﾞﾊﾞｯｸ値を返す。
	ﾌｫｰﾏｯﾄはUSB 2.0規格にあるFull speed endpoint用の値(10.14形式)。
*/
unsigned int audio_buffer_getfeedback(){
	
	int diff;
	
	/* (buf残 > (AUDIO_BUFFER_SIZE / 2)) → 要求量が小さくなる方向 */
	/* (buf残 < (AUDIO_BUFFER_SIZE / 2)) → 要求量が大きくなる方向 */
	diff = ((signed int)(AUDIO_BUFFER_SIZE / 2) - audio_buffer_count_fixed);
	diff /= 8;						/* ﾊﾞｲﾄ単位→ｻﾝﾌﾟﾙ数単位に */
	diff <<= 14;					/* 整数部のLSB揃え */
	diff /= AUDIO_BUFFER_FDB_RATE;	/* 1ms辺りの値に変換 */
	
	/* 24bit LE MSB詰めにして返す(整数部が最上位18bitで、最上位8bitは送信されない。つまり小数部は14bit) */
	return diff + (((audio_buffer_srate << 10) / 1000) << 4);
}
