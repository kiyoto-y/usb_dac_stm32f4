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

/* @memo ﾊﾞｯﾌｧ上のﾌﾚｰﾑ長は8ﾊﾞｲﾄ…32bitｽﾃﾚｵ…固定 */

/* ﾊﾞｯﾌｧとは別に、ﾊﾞｯﾌｧへのﾃﾞｰﾀ流入出数を管理するｶｳﾝﾀ */
volatile signed int audio_buffer_count = 0;			/* in/out両方で更新する値 */
volatile signed int audio_buffer_count_fixed = 0;	/* out側だけで更新する値(out更新時に上記値をｺﾋﾟｰ)。ﾊﾞｯﾌｧ残数のﾌｨｰﾄﾞﾊﾞｯｸ通知算出用 */
volatile signed int audio_buffer_count_valid = 0;	/* audio_buffer_count同様だが、audio_buffer_fill_next_ip()での投入分が反映されない。その他の方法を用いて投入すると、あちら側と同値に修正される */


/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	初期化
*/
void audio_buffer_init(){
	
	int size = sizeof(audio_buffer.data);
	int *p = (int *)audio_buffer.data;
	
	audio_buffer.wp = 0;
	audio_buffer_count = 0;
	audio_buffer_count_valid = 0;
	
	/* ｲﾝｼﾞｹｰﾀ更新 */
	(void)audio_buffer_read(0, 0);
	
	/* 初回のﾌｨｰﾄﾞﾊﾞｯｸ値通知対策(再生が始まるまでは制御目標通りであることにする) */
	audio_buffer_count_fixed = (AUDIO_BUFFER_SIZE / 4);
	
	/* @memo ﾊﾞｯﾌｧの初期化は行わない。実際の再生開始までに呼出元でﾃﾞｰﾀ補充を行うこと */
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
	audio_buffer_count_valid = audio_buffer_count;	/* 同期 */
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ﾊﾞｯﾌｧﾌｨﾙ
	
	valid != 0: 供給ﾃﾞｰﾀを有効ﾃﾞｰﾀにしたい場合
	valid == 0: 供給ﾃﾞｰﾀを無効ﾃﾞｰﾀにしたい場合(DMA停止時の零ﾌｨﾙを想定)
	
	@warning sizeは必ずﾌﾚｰﾑ長の整数倍。0は不可
*/
void audio_buffer_fill(int val, int size, int valid){
	
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
	if (valid) audio_buffer_count_valid = audio_buffer_count;	/* 同期 */
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	次のﾊﾞｯﾌｧ中間地点 / 末尾までを指定ﾊﾞｲﾄで埋める
	
	中間 / 末尾 == DMA転送で割り込みがかかる地点を想定。
	
	@warning sizeは必ずﾌﾚｰﾑ長の整数倍。0は不可
	@warning audio_buffer_count_validは更新されない(DMA停止時のｵｰﾊﾞﾗﾝｴﾘｱを予めﾌｨﾙしておく目的の為)
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
	ﾊﾞｯﾌｧの先頭ｱﾄﾞﾚｽ取得
*/
unsigned char *audio_buffer_getptr(){
	return &(audio_buffer.data[0]);
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ﾊﾞｯﾌｧからの読み出し(指定ｵﾌｾｯﾄのﾊﾞｯﾌｧｱﾄﾞﾚｽ取得)
	
	@memo ﾌｨｰﾄﾞﾊﾞｯｸ値算出用の残量計が更新される
	      このため、DMA転送する場合も転送完了ｲﾍﾞﾝﾄ毎に呼び出すこと
	      また、読みだすｻｲｽﾞを必ず申告すること
	
	@warning sizeは必ずﾌﾚｰﾑ長の整数倍。0許容
*/
unsigned char *audio_buffer_read(int ofs, int size){
	
	if (ofs < sizeof(audio_buffer.data)){
		
		audio_buffer_count -= size;
		audio_buffer_count_valid -= size;
		
#if 0
		/* ｵｰﾊﾞ / ｱﾝﾀﾞｰﾌﾛｰ対策 */
		/* つまり、現象が発生した際は、周囲のﾃﾞｰﾀを破壊してでも早期収束を図る */
		while (audio_buffer_count >= (signed int)AUDIO_BUFFER_SIZE){
//			HAL_GPIO_TogglePin(GPIOD, LD5_Pin);
			audio_buffer_count -= AUDIO_BUFFER_SIZE;
		}
		while (audio_buffer_count < 0){
//			HAL_GPIO_TogglePin(GPIOD, LD4_Pin);
			audio_buffer_count += AUDIO_BUFFER_SIZE;
		}
#endif
		/* ｵｰﾊﾞ / ｱﾝﾀﾞｰﾗﾝﾁｪｯｸ */
		HAL_GPIO_WritePin(GPIOD, LD4_Pin, (audio_buffer_count < 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOD, LD5_Pin, (audio_buffer_count > (signed int)AUDIO_BUFFER_SIZE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		
		audio_buffer_count_fixed = audio_buffer_count;
		
		/* ﾊﾞｯﾌｧ残数を表示(ﾌｨｰﾄﾞﾊﾞｯｸによって左右均等に点くような状態が好ましい) */
//		HAL_GPIO_WritePin(GPIOD, LD4_Pin, (audio_buffer_count_fixed <= (signed int)(AUDIO_BUFFER_SIZE / 4)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
//		HAL_GPIO_WritePin(GPIOD, LD5_Pin, (audio_buffer_count_fixed >= (signed int)(AUDIO_BUFFER_SIZE / 4)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		
		return &(audio_buffer.data[ofs]);
		
	}else{
		return (void *)0;
	}
}

/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
/*
	ﾊﾞｯﾌｧへの供給ｻｲｽﾞを調整する為のﾌｨｰﾄﾞﾊﾞｯｸ値を取得
	
	値は、制御目標となるﾊﾞｯﾌｧ残量との差[ｻﾝﾌﾟﾙ数単位]。
	目標値 > 残 → 正(つまり、ﾊﾞｯﾌｧ残を増やしたい)
	目標値 < 残 → 負(つまり、ﾊﾞｯﾌｧ残を減らしたい)
*/
signed int audio_buffer_getfeedback(){
	
	/* @memo 制御目標は、DMA半ﾊﾞｯﾌｧ転送割り込み後にﾃﾞｰﾀ残数が1/4ﾊﾞｯﾌｧ分ある状態 */
	/* (buf残 > (AUDIO_BUFFER_SIZE / 4)) → 要求量が小さくなる方向 */
	/* (buf残 < (AUDIO_BUFFER_SIZE / 4)) → 要求量が大きくなる方向 */
	return ((signed int)(AUDIO_BUFFER_SIZE / 4) - audio_buffer_count_fixed) / AUDIO_BUFFER_FRAMESIZE;
}
