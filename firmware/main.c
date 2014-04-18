/*
 * Author: Minux
 * Author: Xiangfu Liu <xiangfu@openmobilefree.net>
 * Bitcoin:	1CanaaniJzgps8EV6Sfmpb7T8RutpaeyFn
 *
 * This is free and unencumbered software released into the public domain.
 * For details see the UNLICENSE file at the root of the source tree.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "minilibc.h"
#include "system_config.h"
#include "defines.h"
#include "io.h"
#include "intr.h"
#include "uart.h"
#include "miner.h"
#include "sha256.h"
#include "alink.h"
#include "twipwm.h"
#include "shifter.h"
#include "timer.h"
#include "spi.h"
#include "protocol.h"
#include "crc.h"
#include "be200.h"
#include "be200_test.h"
#include "miner.h"
#include "hexdump.c"
#include "utils.h"


#define IDLE_TIME	5	/* Seconds */
static uint8_t g_pkg[HRTO_P_COUNT];
static uint8_t g_act[HRTO_P_COUNT];
//static int g_module_id = 0;	/* Default ID is 0 */
static int g_new_stratum = 0;
static int g_local_work = 0;
static int g_hw_work = 0;

static uint32_t g_nonce2_offset = 0;
static uint32_t g_nonce2_range = 0xffffffff;

#define RET_RINGBUFFER_SIZE_RX 16
#define RET_RINGBUFFER_MASK_RX (RET_RINGBUFFER_SIZE_RX-1)
//static uint8_t ret_buf[RET_RINGBUFFER_SIZE_RX][HRTO_P_DATA_LEN];
static volatile unsigned int ret_produce = 0;
static volatile unsigned int ret_consume = 0;

static struct chip_status miner_status[CHIP_NUMBER];
#define BE200_RET_RINGBUFFER_SIZE_RX 256
#define BE200_RET_RINGBUFFER_MASK_RX (BE200_RET_RINGBUFFER_SIZE_RX-1)
static struct be200_result be200_result_buff[BE200_RET_RINGBUFFER_SIZE_RX];
static volatile unsigned int be200_ret_produce = 0;
static volatile unsigned int be200_ret_consume = 0;

static uint8_t freq_read(uint8_t idx) {
	return 0;
}

static void freq_write(uint8_t idx, uint8_t multi) {
	be200_set_pll(idx, multi);
	delay(1);
	return;
}

static void encode_pkg(uint8_t *p, int type, uint8_t *buf, unsigned int len, int idx, int cnt)
{
	uint32_t tmp;
	uint16_t crc;
	uint8_t *data;

	memset(p, 0, HRTO_P_COUNT);

	p[0] = HRTO_H1;
	p[1] = HRTO_H2;

	p[2] = type;
	p[3] = idx;
	p[4] = cnt;

	data = p + 5;
//	memcpy(data + 28, &g_module_id, 4); /* Attach the module_id at end */

	switch(type) {
	case HRTO_P_ACKDETECT:
	case HRTO_P_NONCE:
	case HRTO_P_TEST_RET:
	case HRTO_P_GET_FREQ:
		memcpy(data, buf, len);
		break;
	case HRTO_P_STATUS:
		tmp = read_temp0() << 16 | read_temp1();
		memcpy(data + 0, &tmp, 4);

		tmp = read_fan0() << 16 | read_fan1();
		memcpy(data + 4, &tmp, 4);

//		tmp = get_asic_freq();
//		memcpy(data + 8, &tmp, 4);
//		tmp = get_voltage();
//		memcpy(data + 12, &tmp, 4);

		memcpy(data + 8,  &g_local_work, 4);
		memcpy(data + 12, &g_hw_work,    4);

//		tmp = read_power_good();
//		memcpy(data + 24, &tmp, 4);
		break;
	default:
		break;
	}

	crc = crc16(data, HRTO_P_DATA_LEN);
	p[HRTO_P_COUNT - 2] = crc & 0x00ff;
	p[HRTO_P_COUNT - 1] = (crc & 0xff00) >> 8;
}

void send_pkg(int type, uint8_t *buf, unsigned int len, uint8_t idx, uint8_t cnt)
{
//	debug32("Send: %d\n", type);
	encode_pkg(g_act, type, buf, len, idx, cnt);
	uart_nwrite((char *)g_act, HRTO_P_COUNT);
}

static void be200_polling(struct mm_work *mw)
{
	uint8_t buf[HRTO_P_DATA_LEN];
	struct be200_result *data;
	uint32_t tmp;

	if (be200_ret_consume == be200_ret_produce) {
		send_pkg(HRTO_P_STATUS, NULL, 0, 1, 1);

		g_local_work = 0;
		g_hw_work = 0;
		return;
	}
	
	data = &be200_result_buff[be200_ret_consume];
	be200_ret_consume = (be200_ret_consume + 1) & BE200_RET_RINGBUFFER_MASK_RX;
	
	memset(buf, 0, sizeof(buf));
	tmp = data->idx;
	memcpy(buf +  0, (uint8_t *)&tmp, 4);
	tmp = mw->pool_no;
	memcpy(buf +  4, (uint8_t *)&tmp, 4);
	tmp = data->nonce2;
	memcpy(buf +  8, (uint8_t *)&tmp, 4);
	tmp = data->nonce;
	memcpy(buf + 12, (uint8_t *)&tmp, 4);
	// job_id
	memcpy(buf + 16, mw->job_id, 4);
	
	debug32("polling, miner: %02x, pool_no: %02x, nonce2: %08x, nonce: %08x\n",
			data->idx, mw->pool_no, data->nonce2, data->nonce);

	send_pkg(HRTO_P_NONCE, buf, HRTO_P_DATA_LEN, 1, 1);
	return;
}

//static void polling()
//{
//	uint8_t *data;
//
//	if (ret_consume == ret_produce) {
//		send_pkg(HRTO_P_STATUS, NULL, 0, 1, 1);
//
//		g_local_work = 0;
//		g_hw_work = 0;
//		return;
//	}
//
//	data = ret_buf[ret_consume];
//	ret_consume = (ret_consume + 1) & RET_RINGBUFFER_MASK_RX;
//	send_pkg(HRTO_P_NONCE, data, HRTO_P_DATA_LEN - 4, 1, 1);
//	return;
//}

static void freq_polling() {
	uint8_t freq_arr[HRTO_P_DATA_LEN];
	int i, j, n;
	n = HRTO_DEFAULT_MINERS / HRTO_P_DATA_LEN;
	
	for (i = 0; i <= n; i++) {
		memset(freq_arr, 0, sizeof(freq_arr));
		if (i == n) {
			j = HRTO_DEFAULT_MINERS % HRTO_P_DATA_LEN - 1;
		} else {
			j = HRTO_P_DATA_LEN - 1;
		}
		for (; j >= 0; j--) {
			freq_arr[j] = freq_read((uint16_t)(i * HRTO_P_DATA_LEN + j));
		}
		send_pkg(HRTO_P_GET_FREQ, freq_arr, HRTO_P_DATA_LEN, i, n);
	}
}

static int decode_pkg(uint8_t *p, struct mm_work *mw)
{
	unsigned int expected_crc;
	unsigned int actual_crc;
	int idx, cnt, i;
	uint32_t tmp;

	uint8_t *data = p + 5;

	idx = p[3];
	cnt = p[4];

//	debug32("Decode: %d %d/%d\n", p[2], idx, cnt);

	expected_crc = (p[HRTO_P_COUNT - 1] & 0xff) |
		((p[HRTO_P_COUNT - 2] & 0xff) << 8);

	actual_crc = crc16(data, HRTO_P_DATA_LEN);
	if(expected_crc != actual_crc) {
		debug32("PKG: CRC failed (W %08x, R %08x)\n",
			expected_crc, actual_crc);
		return 1;
	}

//	timer_set(0, IDLE_TIME);
	switch (p[2]) {
	case HRTO_P_DETECT:
		g_new_stratum = 0;
//		alink_flush_fifo();
		break;
	case HRTO_P_STATIC:
		g_new_stratum = 0;
//		alink_flush_fifo();
		memcpy(&mw->coinbase_len, data, 4);
		memcpy(&mw->nonce2_offset, data + 4, 4);
		memcpy(&mw->nonce2_size, data + 8, 4);
		memcpy(&mw->merkle_offset, data + 12, 4);
		memcpy(&mw->nmerkles, data + 16, 4);
		memcpy(&mw->diff, data + 20, 4);
		memcpy(&mw->pool_no, data + 24, 4);
		debug32("D: (%d):  %d, %d, %d, %d, %d, %d, %d\n",
			g_new_stratum,
			mw->coinbase_len,
			mw->nonce2_offset,
			mw->nonce2_size,
			mw->merkle_offset,
			mw->nmerkles,
			mw->diff,
			mw->pool_no);
		break;
	case HRTO_P_JOB_ID:
		memcpy(mw->job_id, data, 4);
		break;
	case HRTO_P_COINBASE:
		if (idx == 1)
			memset(mw->coinbase, 0, sizeof(mw->coinbase));
		memcpy(mw->coinbase + (idx - 1) * HRTO_P_DATA_LEN, data, HRTO_P_DATA_LEN);
		break;
	case HRTO_P_MERKLES:
		memcpy(mw->merkles[idx - 1], data, HRTO_P_DATA_LEN);
		break;
	case HRTO_P_HEADER:
		memcpy(mw->header + (idx - 1) * HRTO_P_DATA_LEN, data, HRTO_P_DATA_LEN);
		break;
	case HRTO_P_POLLING:
//		memcpy(&tmp, data + 28, 4);
//		debug32("ID: %d-%d\n", g_module_id, tmp);
//		if (g_module_id == tmp)
//			polling();
		
//		polling();
		be200_polling(mw);

//		memcpy(&tmp, data + 24, 4);
//		if (tmp) {
//			memcpy(&tmp, data, 4);
//			adjust_fan(tmp);
//			memcpy(&tmp, data + 4, 4);
//			set_voltage(tmp);
//			memcpy(&tmp, data + 8, 4);
//			set_asic_freq(tmp);
//
//			alink_flush_fifo();
//		}
		break;
	case HRTO_P_REQUIRE:
		break;
	case HRTO_P_SET:
		memcpy(&tmp, data, 4);
		adjust_fan(tmp);
//		memcpy(&tmp, data + 4, 4);
//		set_voltage(tmp);
//		memcpy(&tmp, data + 8, 4);
//		set_asic_freq(tmp);

		memcpy(&g_nonce2_offset, data + 12, 4);
		memcpy(&g_nonce2_range, data + 16, 4);

//		mw->nonce2 = g_nonce2_offset + (g_nonce2_range / AVA2_DEFAULT_MODULES) * g_module_id;
		mw->nonce2 = g_nonce2_offset + g_nonce2_range;
//		alink_flush_fifo();

		g_new_stratum = 1;
		break;
	case HRTO_P_TARGET:
		memcpy(mw->target, data, HRTO_P_DATA_LEN);
		break;
	case HRTO_P_SET_FREQ:
		if (cnt != 3) { break; }
			if (idx == cnt - 1) {
				i = HRTO_DEFAULT_MINERS % HRTO_P_DATA_LEN - 1;
			} else {
				i = HRTO_P_DATA_LEN - 1;
			}
		for (; i >= 0; i--) {
			freq_write((uint16_t)(idx * HRTO_P_DATA_LEN + i), *(data+i));
		}
		break;
	case HRTO_P_GET_FREQ:
		freq_polling();
		break;
	case HRTO_P_TEST:
//		memcpy(&tmp, data + 28, 4);
//		if (g_module_id == tmp) {
//			set_voltage(0x8a00);
//			led(1);
//			alink_asic_test();	/* Test ASIC */
//			led(0);
//			set_voltage(0x8f00);
//		}
		break;
	default:
		break;
	}

	return 0;
}

uint32_t be200_send_work(uint8_t idx, struct work *w)
{
	if (!be200_check_idle(idx))
		return 0;
	
	be200_input_task(idx, w->data);
	be200_start(idx);
	
	miner_status[idx].nonce2 = w->nonce2;
//	memcpy(&miner_status[idx].nonce2, &w->nonce2, 4);
//	memcpy(&miner_status[idx].job_id, &w->task_id+4, 4);
	return 1;
}

uint32_t be200_read_result(struct mm_work *mw)
{
	uint8_t idx;
	uint8_t ready;
	uint8_t nonce_mask;
	uint32_t res;
	int32_t nonce_check;
	struct be200_result * data;
	
//	for (idx = 0; idx < CHIP_NUMBER; idx++) {
	for (idx = 16; idx < 32; idx++) {
		ready = be200_get_done(idx, &nonce_mask);
		if(ready == 0)
			continue;
		
		be200_get_result(idx, nonce_mask, &res);
//		delay(2);
//		be200_dump_register(idx);
		
		g_local_work++;

		/* check the validation of the nonce*/
		//nonce_check = test_nonce(mw, ret);
		//TODO: For the test, return value of test_nonce always be NONCE_DIFF
		nonce_check = NONCE_DIFF;
		
		if (nonce_check == NONCE_HW) {
			g_hw_work++;
		}
		if (nonce_check == NONCE_DIFF) {
			/* put the valid nonce into be200 ring buffer */
			data = &be200_result_buff[be200_ret_produce];
			be200_ret_produce = (be200_ret_produce + 1) & BE200_RET_RINGBUFFER_MASK_RX;

			data->idx = idx;
			data->nonce2  = miner_status[idx].nonce2;
			data->nonce   = res;
//			data->pool_no = mw->pool_no;
//			data->job_id  = miner_status[idx].job_id;
			
			debug32("be200_read_result, g_local_work: %d, miner: %d, pool_no: %02x, nonce2: %08x, nonce: %08x\n",
					g_local_work, data->idx, mw->pool_no, data->nonce2, data->nonce);
			
		}
	}
	return 0;
}

static int read_result(struct mm_work *mw, struct result *ret)
{
	/*
	uint8_t *data;
	int nonce;

	if (alink_rxbuf_empty())
		return 0;

	alink_read_result(ret);
	g_local_work++;

	nonce = test_nonce(mw, ret);
	if (nonce == NONCE_HW) {
		g_hw_work++;
		return 1;
	}

	if (nonce == NONCE_DIFF) {
		data = ret_buf[ret_produce];
		ret_produce = (ret_produce + 1) & RET_RINGBUFFER_MASK_RX;

		memcpy(data, (uint8_t *)ret, 20);
		memcpy(data + 20, mw->job_id, 4); 
	}
	*/
	return 1;
}

static int get_pkg(struct mm_work *mw)
{
	static char pre_last, last;
	static int start = 0, count = 2;

	while (1) {
		if (!uart_read_nonblock() && !start)
			break;

		pre_last = last;
		last = uart_read();

		if (start)
			g_pkg[count++] = last;

		if (count == HRTO_P_COUNT) {
			pre_last = last = 0;

			start = 0;
			count = 2;

			if (decode_pkg(g_pkg, mw)) {
#ifdef CFG_ENABLE_ACK
				send_pkg(HRTO_P_NAK, NULL, 0, 1, 1);
#endif
				return 1;
			} else {
				/* Here we send back PKG if necessary */
#ifdef CFG_ENABLE_ACK
				send_pkg(HRTO_P_ACK, NULL, 0, 1, 1);
#endif
				switch (g_pkg[2]/* pkg type */) {
				case HRTO_P_DETECT:
//					memcpy(&tmp, g_pkg + 5 + 28, 4);
//					if (g_module_id == tmp)
//						send_pkg(HRTO_P_ACKDETECT, (uint8_t *)MM_VERSION, MM_VERSION_LEN);
					send_pkg(HRTO_P_ACKDETECT, (uint8_t *)MM_VERSION, MM_VERSION_LEN, 1, 1);
					break;
				case HRTO_P_REQUIRE:
//					memcpy(&tmp, g_pkg + 5 + 28, 4);
//					if (g_module_id == tmp)
//						send_pkg(HRTO_P_STATUS, NULL, 0);
					send_pkg(HRTO_P_STATUS, NULL, 0, 1, 1);
					break;
				default:
					break;
						
				}
			}
		}

		if (pre_last == HRTO_H1 && last == HRTO_H2 && !start) {
			g_pkg[0] = pre_last;
			g_pkg[1] = last;
			start = 1;
			count = 2;
		}
	}

	return 0;
}

int main1(int argv, char **argc)
{
	struct mm_work mm_work;
	struct work work;
	struct result result;
	uint8_t idx;

	led(1);
	adjust_fan(0);		/* Set the fan to 100% */
//	alink_flush_fifo();

	wdg_init(1);
	wdg_feed((CPU_FREQUENCY / 1000) * 2); /* Configure the wdg to ~2 second, or it will reset FPGA */

	irq_setmask(0);
	irq_enable(1);

//	g_module_id = read_module_id();

	uart_init();
	uart1_init();
	debug32("MM-%s\n", MM_VERSION);
	led(0);

	timer_set(0, IDLE_TIME);
	g_new_stratum = 0;

//	alink_asic_idle();
	adjust_fan(0x1ff);
	set_voltage(0x8f00);

	while (1) {
		get_pkg(&mm_work);

		wdg_feed((CPU_FREQUENCY / 1000) * 2);
		if (!timer_read(0) && g_new_stratum) {
			g_new_stratum = 0;
//			alink_asic_idle();
			adjust_fan(0x1ff);
//			set_voltage(0x8f00);
		}

		if (!g_new_stratum)
			continue;

		if (alink_txbuf_count() < (24 * 5)) {	
			get_pkg(&mm_work);
			if (!g_new_stratum)
				continue;

			for(idx=0;idx<CHIP_NUMBER;idx++)
			{
				miner_gen_nonce2_work(&mm_work, mm_work.nonce2, &work);
				mm_work.nonce2++;
				miner_init_work(&mm_work, &work);
				alink_send_work(&work);
			}
		}

		while (read_result(&mm_work, &result)) {
			get_pkg(&mm_work);
			if (!g_new_stratum)
				break;
		}
		
	}
//	be200_polling();
	return 0;
}

int main(int argv,char * * argc)
{
	uint32_t ret;
	struct mm_work mm_work;
	struct work work;
	uint16_t idx;
//	struct result result;
	
//	uint8_t test_buf[44] = {
//		/* 32 */
//		0x37, 0xc9, 0x86, 0x58, 0xc9, 0x16, 0xd9, 0x12, 0x89, 0xd7, 0xdd, 0x17, 0xc8, 0x90, 0xac, 0x13,
//		0xde, 0x93, 0x6f, 0x0f, 0x63, 0x64, 0x6c, 0x96, 0x37, 0x5d, 0xb1, 0xb8, 0x41, 0xc5, 0x58, 0xc4,
//		/* 12 */
//		0x6f, 0x64, 0x53, 0xb0, 0x53, 0x46, 0xcc, 0xf7, 0x19, 0x00, 0xb3, 0xaa
//	}; /* nonce: 4f727c52, be200: d1abf843 */
//	uint8_t test_buf[44] = {
//		/* 32 */
//		0xc4, 0x58, 0xc5, 0x41, 0xb8, 0xb1, 0x5d, 0x37, 0x96, 0x6c, 0x64, 0x63, 0x0f, 0x6f, 0x93, 0xde,
//		0x13, 0xac, 0x90, 0xc8, 0x17, 0xdd, 0xd7, 0x89, 0x12, 0xd9, 0x16, 0xc9, 0x58, 0x86, 0xc9, 0x37,
//		/* 12 */
//		0xaa, 0xb3, 0x00, 0x19, 0xf7, 0xcc, 0x46, 0x53, 0xb0, 0x53, 0x64, 0x6f
//	}; /* nonce: 4f727c52 */
	
//	uint8_t test_buf[44] = {
//		/* 32 */
//		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//		/* 12 */
//		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x69
//	}; /* nonce: 017d0c44 */
	
	uint8_t test_buf[44] ={
		0xf9 ,0xca ,0xc7 ,0x7f ,0x2a ,0x00 ,0xf1 ,0x64 ,0xb2 ,0x27 ,0xce ,0xf9 ,0x83 ,0x4b ,0xe8 ,0x73 ,0x41 ,0xf4 ,0x57 ,0x3e ,0x13 ,0xcc ,0xdb ,0x3b ,0x12 ,0x31 ,0x8e ,0x65 ,0x54 ,0xa9 ,0x83 ,0x5a ,0x25 ,0x51 ,0x28 ,0xa1 ,0x52 ,0x90 ,0x4b ,0xce ,0x19 ,0x07 ,0x0b ,0xfb};
	// No. 0
	
//	uint8_t test_buf[44] = {
//		/* 32 bytes */
//		0xae, 0x73, 0x8e, 0xa1, 0x55, 0x01, 0x59, 0x9e, 0x46, 0x9b, 0x3e, 0x0b, 0x3c, 0xb1, 0x7b, 0x42,
//		0x31, 0x78, 0xa2, 0x51, 0xca, 0xe1, 0x3d, 0xe6, 0x7f, 0x0b, 0x40, 0xaa, 0x0d, 0x76, 0xd3, 0x5a,
//		/* 12 bytes */
//		0xb7, 0x31, 0x47, 0x5e, 0x53, 0x46, 0xcc, 0x9d, 0x19, 0x00, 0xb3, 0xaa
//	}; /* nonce: 5d1a2de2, be200: c16635a3 */
	

	
	irq_setmask(0);
	irq_enable(1);
	
	uart_init();
	uart1_init();
	
	adjust_fan(1200);
	
//	uint8_t c = 0x55;
	//uart1_write(0x55);
	
	for (idx = 0; idx < 80; idx++) {
		freq_write(idx, BE200_DEFAULT_FREQ);  // (X + 1) / 2
	}
	
	while(1) {
//		get_pkg(&mm_work);
//		
//		if (!g_new_stratum)
//			continue;
		
//		for (idx = 0; idx < CHIP_NUMBER; idx++) {
		for (idx = 16; idx < 32; idx++) {
			if (!be200_check_idle(idx)) {
				continue;
			}
//			mm_work.nonce2++;
//			miner_gen_nonce2_work(&mm_work, mm_work.nonce2, &work);
			
			// test 44 bytes
			memcpy(work.data, test_buf, 44);
			
			ret = be200_send_work(idx, &work);
//			hexdump(work.data, 44);
			
//			be200_dump_register(idx);
			
//			debug32("be200_send_work, ret: %d, idx: %d\n", ret, idx);
		}
		be200_read_result(&mm_work);
		
//		delay(1000);
	}
	return 0;
}
