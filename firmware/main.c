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

#include "hexdump.c"

#define IDLE_TIME	5	/* Seconds */
static uint8_t g_pkg[AVA2_P_COUNT];
static uint8_t g_act[AVA2_P_COUNT];
static int g_module_id = 0;	/* Default ID is 0 */
static int g_new_stratum = 0;
static int g_local_work = 0;
static int g_hw_work = 0;

static uint32_t g_nonce2_offset = 0;
static uint32_t g_nonce2_range = 0xffffffff;

#define RET_RINGBUFFER_SIZE_RX 16
#define RET_RINGBUFFER_MASK_RX (RET_RINGBUFFER_SIZE_RX-1)
static uint8_t ret_buf[RET_RINGBUFFER_SIZE_RX][AVA2_P_DATA_LEN];
static volatile unsigned int ret_produce = 0;
static volatile unsigned int ret_consume = 0;

/*static uint8_t test_data[BUFFER_SIZE] = {
	0x81, 0x85, 0x65, 0x8c, 0x24, 0x66, 0x21, 0x1d, 
    0xe2, 0x65, 0xd0, 0xf8, 0xbf, 0xa6, 0x6e, 0xc9, 
    0xda, 0xff, 0xe2, 0x8b, 0xe3, 0xb3, 0x66, 0xe4,
    0x43, 0xf3, 0x1f, 0x12, 0xf7, 0x3f, 0xe9, 0xe3, 
    0x26, 0xd8, 0x6e, 0x20, 0x50, 0x0a, 0x9a, 0x4b, 
    0x1a, 0x08, 0xfd, 0x2f
};*/
static uint8_t test_data[BUFFER_SIZE];


void prepare()
{
    int8_t i;
	uint8_t cf=1;
	for(i=BUFFER_SIZE -1; i>=0; i--)
	{
		if(cf)
		{
	        test_data[i]++;
			if(test_data[i] == 0x00)
				cf = 1;
			else
				cf = 0;
		}
	}
	/*for(i=0;i<BUFFER_SIZE;i++)
	{
		uart1_write(test_data[i]);
	}*/

	//uart1_write(test_data[0]);
}

void delay(unsigned int ms)
{
	unsigned int i;

	while (ms && ms--) {
		for (i = 0; i < CPU_FREQUENCY / 1000 / 5; i++)
			__asm__ __volatile__("nop");
	}
}

static void encode_pkg(uint8_t *p, int type, uint8_t *buf, unsigned int len)
{
	uint32_t tmp;
	uint16_t crc;
	uint8_t *data;

	memset(p, 0, AVA2_P_COUNT);

	p[0] = AVA2_H1;
	p[1] = AVA2_H2;

	p[2] = type;
	p[3] = 1;
	p[4] = 1;

	data = p + 5;
	memcpy(data + 28, &g_module_id, 4); /* Attach the module_id at end */

	switch(type) {
	case AVA2_P_ACKDETECT:
	case AVA2_P_NONCE:
	case AVA2_P_TEST_RET:
		memcpy(data, buf, len);
		break;
	case AVA2_P_STATUS:
		tmp = read_temp0() << 16 | read_temp1();
		memcpy(data + 0, &tmp, 4);

		tmp = read_fan0() << 16 | read_fan1();
		memcpy(data + 4, &tmp, 4);

		tmp = get_asic_freq();
		memcpy(data + 8, &tmp, 4);
		tmp = get_voltage();
		memcpy(data + 12, &tmp, 4);

		memcpy(data + 16, &g_local_work, 4);
		memcpy(data + 20, &g_hw_work, 4);

		tmp = read_power_good();
		memcpy(data + 24, &tmp, 4);
		break;
	default:
		break;
	}

	crc = crc16(data, AVA2_P_DATA_LEN);
	p[AVA2_P_COUNT - 2] = crc & 0x00ff;
	p[AVA2_P_COUNT - 1] = (crc & 0xff00) >> 8;
}

void send_pkg(int type, uint8_t *buf, unsigned int len)
{
	debug32("Send: %d\n", type);
	encode_pkg(g_act, type, buf, len);
	uart_nwrite((char *)g_act, AVA2_P_COUNT);
}

static void polling()
{
	uint8_t *data;

	if (ret_consume == ret_produce) {
		send_pkg(AVA2_P_STATUS, NULL, 0);

		g_local_work = 0;
		g_hw_work = 0;
		return;
	}

	data = ret_buf[ret_consume];
	ret_consume = (ret_consume + 1) & RET_RINGBUFFER_MASK_RX;
	send_pkg(AVA2_P_NONCE, data, AVA2_P_DATA_LEN - 4);
	return;
}

static int decode_pkg(uint8_t *p, struct mm_work *mw)
{
	unsigned int expected_crc;
	unsigned int actual_crc;
	int idx, cnt;
	uint32_t tmp;

	uint8_t *data = p + 5;

	idx = p[3];
	cnt = p[4];

	debug32("Decode: %d %d/%d\n", p[2], idx, cnt);

	expected_crc = (p[AVA2_P_COUNT - 1] & 0xff) |
		((p[AVA2_P_COUNT - 2] & 0xff) << 8);

	actual_crc = crc16(data, AVA2_P_DATA_LEN);
	if(expected_crc != actual_crc) {
		debug32("PKG: CRC failed (W %08x, R %08x)\n",
			expected_crc, actual_crc);
		return 1;
	}

	timer_set(0, IDLE_TIME);
	switch (p[2]) {
	case AVA2_P_DETECT:
		g_new_stratum = 0;
		alink_flush_fifo();
		break;
	case AVA2_P_STATIC:
		g_new_stratum = 0;
		alink_flush_fifo();
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
	case AVA2_P_JOB_ID:
		memcpy(mw->job_id, data, 4);
		hexdump(mw->job_id, 4);
		break;
	case AVA2_P_COINBASE:
		if (idx == 1)
			memset(mw->coinbase, 0, sizeof(mw->coinbase));
		memcpy(mw->coinbase + (idx - 1) * AVA2_P_DATA_LEN, data, AVA2_P_DATA_LEN);
		break;
	case AVA2_P_MERKLES:
		memcpy(mw->merkles[idx - 1], data, AVA2_P_DATA_LEN);
		break;
	case AVA2_P_HEADER:
		memcpy(mw->header + (idx - 1) * AVA2_P_DATA_LEN, data, AVA2_P_DATA_LEN);
		break;
	case AVA2_P_POLLING:
		memcpy(&tmp, data + 28, 4);
		debug32("ID: %d-%d\n", g_module_id, tmp);
		if (g_module_id == tmp)
			polling();

		memcpy(&tmp, data + 24, 4);
		if (tmp) {
			memcpy(&tmp, data, 4);
			adjust_fan(tmp);
			memcpy(&tmp, data + 4, 4);
			set_voltage(tmp);
			memcpy(&tmp, data + 8, 4);
			set_asic_freq(tmp);

			alink_flush_fifo();
		}
		break;
	case AVA2_P_REQUIRE:
		break;
	case AVA2_P_SET:
		memcpy(&tmp, data, 4);
		adjust_fan(tmp);
		memcpy(&tmp, data + 4, 4);
		set_voltage(tmp);
		memcpy(&tmp, data + 8, 4);
		set_asic_freq(tmp);

		memcpy(&g_nonce2_offset, data + 12, 4);
		memcpy(&g_nonce2_range, data + 16, 4);

		mw->nonce2 = g_nonce2_offset + (g_nonce2_range / AVA2_DEFAULT_MODULES) * g_module_id;
		alink_flush_fifo();

		g_new_stratum = 1;
		break;
	case AVA2_P_TARGET:
		memcpy(mw->target, data, AVA2_P_DATA_LEN);
		break;
	case AVA2_P_TEST:
		memcpy(&tmp, data + 28, 4);
		if (g_module_id == tmp) {
			set_voltage(0x8a00);
			led(1);
			alink_asic_test();	/* Test ASIC */
			led(0);
			set_voltage(0x8f00);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int read_result(struct mm_work *mw, struct result *ret)
{
	uint8_t *data;
	int nonce;

	if (alink_rxbuf_empty())
		return 0;

#ifdef DEBUG
	alink_buf_status();
#endif

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
		memcpy(data + 20, mw->job_id, 4); /* Attach the job_id */
	}

	return 1;
}

static int get_pkg(struct mm_work *mw)
{
	static char pre_last, last;
	static int start = 0, count = 2;
	int tmp;

	while (1) {
		if (!uart_read_nonblock() && !start)
			break;

		pre_last = last;
		last = uart_read();

		if (start)
			g_pkg[count++] = last;

		if (count == AVA2_P_COUNT) {
			pre_last = last = 0;

			start = 0;
			count = 2;

			if (decode_pkg(g_pkg, mw)) {
#ifdef CFG_ENABLE_ACK
				send_pkg(AVA2_P_NAK, NULL, 0);
#endif
				return 1;
			} else {
				/* Here we send back PKG if necessary */
#ifdef CFG_ENABLE_ACK
				send_pkg(AVA2_P_ACK, NULL, 0);
#endif
				switch (g_pkg[2]) {
				case AVA2_P_DETECT:
					memcpy(&tmp, g_pkg + 5 + 28, 4);
					if (g_module_id == tmp)
						send_pkg(AVA2_P_ACKDETECT, (uint8_t *)MM_VERSION, MM_VERSION_LEN);
					break;
				case AVA2_P_REQUIRE:
					memcpy(&tmp, g_pkg + 5 + 28, 4);
					if (g_module_id == tmp)
						send_pkg(AVA2_P_STATUS, NULL, 0);
					break;
				default:
					break;
				}
			}
		}

		if (pre_last == AVA2_H1 && last == AVA2_H2 && !start) {
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

	led(1);
	adjust_fan(0);		/* Set the fan to 100% */
	alink_flush_fifo();

	wdg_init(1);
	wdg_feed((CPU_FREQUENCY / 1000) * 2); /* Configure the wdg to ~2 second, or it will reset FPGA */

	irq_setmask(0);
	irq_enable(1);

	g_module_id = read_module_id();

	uart_init();
	uart1_init();
	debug32("%d:MM-%s\n", g_module_id, MM_VERSION);
	led(0);

	timer_set(0, IDLE_TIME);
	g_new_stratum = 0;

	alink_asic_idle();
	adjust_fan(0x1ff);
	set_voltage(0x8f00);

	while (1) {
		uart1_write((char)0x55);
		get_pkg(&mm_work);

		wdg_feed((CPU_FREQUENCY / 1000) * 2);
		if (!timer_read(0) && g_new_stratum) {
			g_new_stratum = 0;
			alink_asic_idle();
			adjust_fan(0x1ff);
			set_voltage(0x8f00);
		}

		if (!g_new_stratum)
			continue;

		if (alink_txbuf_count() < (24 * 5)) {
			miner_gen_nonce2_work(&mm_work, mm_work.nonce2, &work);
			get_pkg(&mm_work);
			if (!g_new_stratum)
				continue;

			mm_work.nonce2++;
			miner_init_work(&mm_work, &work);
			alink_send_work(&work);
		}

		while (read_result(&mm_work, &result)) {
			get_pkg(&mm_work);
			if (!g_new_stratum)
				break;
		}
	}

	return 0;
}

void be200_test(const uint16_t idx)
{
    uint8_t ready;
	uint8_t nonce_mask;
	uint32_t res;
    be200_reset(idx);
	delay(100);
	//be200_cmd_wr(0x0001,BE200_REG_PLL,0x9D);
	delay(100);
	while(1)
	{
		if( be200_check_idle(idx))
		{
			prepare();
		    be200_input_task(idx,test_data);
			be200_start(idx);
		}

		while(!be200_check_idle(idx))
		{
		    ready = be200_get_done(idx,&nonce_mask);
			if(ready > 0)
			{
			    be200_get_result(idx,nonce_mask,&res);
				be200_dump_register(idx);
				be200_clear(idx);
				//return;
			}
		}
		//uart1_write(0xE0);
	}
}


int main(int argv,char * * argc)
{
	//uint8_t mask;
	irq_setmask(0);
	irq_enable(1);
	uart_init();
	uart1_init();
	
	
	//spi_select(0x01);

	uint16_t idx = 0x0001;
	uint8_t cmd;
	uint8_t c;
	while(1)
	{	
		cmd = uart1_read();
		switch(cmd){
		case 0x01://full test
			be200_test(idx);
			break;
		case 0x02://check status
			uart1_writeb(be200_cmd_ck(idx));
			break;
		case 0x03://start enumrate
			be200_start(idx);
			break;
		case 0x04://set pll
			be200_cmd_wr(idx,BE200_REG_PLL,0x1D);
			//delay(100);
			be200_cmd_wr(idx,BE200_REG_PLL,0x1D);
			//delay(100);
			be200_cmd_wr(idx,BE200_REG_PLL,0x9D);
			//delay(100);
			c = be200_cmd_rd(idx,BE200_REG_PLL);
			uart1_writeb(c);
			break;
		case 0x05://soft reset
			be200_cmd_rst(idx);
			break;
		case 0x06:// read register 45
			uart1_writeb(be200_cmd_rd(idx,45));;
			break;
		case 0x07://write register 22
			be200_cmd_wr(idx,22,0x55);
			break;
		case 0x08://
			uart1_writeb(be200_cmd_rd(idx,22));;
			break;
		case 0x09:
			be200_clear(idx);
			break;
		case 0x0A:
			be200_dump_register(idx);
			break;
		case 0x0B:
			prepare();
			break;
		case 0x0C:
			be200_input_task(idx,test_data);
			break;
			
		}
	}

	
	return 0;
}
