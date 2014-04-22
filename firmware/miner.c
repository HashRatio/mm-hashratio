/*
 * Author: Xiangfu Liu <xiangfu@openmobilefree.net>
 * Bitcoin:	1CanaaniJzgps8EV6Sfmpb7T8RutpaeyFn
 *
 * This is free and unencumbered software released into the public domain.
 * For details see the UNLICENSE file at the root of the source tree.
 */

#include <stdbool.h>
#include <stdint.h>

#include "minilibc.h"
#include "system_config.h"
#include "defines.h"
#include "io.h"
#include "uart.h"
#include "miner.h"
#include "sha256.h"
#include "alink.h"
#include "twipwm.h"

static uint32_t g_asic_freq = ASIC_FREQUENCY;

static inline void flip32(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;
	int i;

	for (i = 0; i < 8; i++)
		dest[i] = bswap_32(src[i]);
}

static inline void flip64(void *dest_p, const uint8_t *src_p)
{
	uint32_t *dest = dest_p;
	int i;

	for (i = 0; i < 16; i++) {
		dest[i] = src_p[i * 4 + 0] | src_p[i * 4 + 1] << 8 |
			src_p[i * 4 + 2] << 16 | src_p[i * 4 + 3] << 24;
	}
}

static inline void flip80(void *dest_p, const void *src_p)
{
	uint32_t *dest = dest_p;
	const uint32_t *src = src_p;
	int i;

	for (i = 0; i < 20; i++)
		dest[i] = bswap_32(src[i]);
}

static void calc_midstate(struct mm_work *mw, struct work *work)
{
	unsigned char data[64];
	uint32_t *data32 = (uint32_t *)data;

	flip64(data32, mw->header);

	sha256_init();
	sha256_update(data, 64);
	sha256_final(work->data);

	/* FIXME: LM32 will crash if I direct use
	 * flip64(work->data, work->data);
	 * Should be flip32 ??
	 */
	memcpy(data, work->data, 32);
	flip32(work->data, data);
	
	memcpy(work->data + 32, mw->header + 64, 12);
}

void set_asic_freq(uint32_t value)
{
	if (g_asic_freq == value)
		return;

	g_asic_freq = value;
}

uint32_t get_asic_freq()
{
	return g_asic_freq;
}

//void miner_init_work(struct mm_work *mw, struct work *work)
//{
//	work->nonce2 = mw->nonce2;
//	
//	work->job_id = mw->job_id;
//	memcpy(work->task_id, (uint8_t *)(&work->nonce2), 4);
//	memcpy(work->task_id + 4, (uint8_t *)(&mw->job_id), 4);
//}

//static void rev(unsigned char *s, size_t l)
//{
//	size_t i, j;
//	unsigned char t;
//
//	for (i = 0, j = l - 1; i < j; i++, j--) {
//		t = s[i];
//		s[i] = s[j];
//		s[j] = t;
//	}
//}

///* Total: 4W + 19W = 23W
// * TaskID_H:1, TASKID_L:1, STEP:1, TIMEOUT:1,
// * CLK_CFG:2, a2, Midsate:8, e0, e1, e2, a0, a1, Data:3
// */
//static void calc_prepare(struct work *work, uint8_t *buf)
//{
//	uint32_t precalc[6];
//	sha256_precalc(buf, buf + 32, 12, (uint8_t *)precalc);
//	memcpy(work->a0, precalc + 0, 4);
//	memcpy(work->a1, precalc + 1, 4);
//	memcpy(work->a2, precalc + 2, 4);
//	memcpy(work->e0, precalc + 3, 4);
//	memcpy(work->e1, precalc + 4, 4);
//	memcpy(work->e2, precalc + 5, 4);
//}

void miner_gen_nonce2_work(struct mm_work *mw, uint32_t nonce2, struct work *work)
{
	uint8_t merkle_root[32], merkle_sha[64];
//	uint8_t work_t[44];
	uint32_t *data32, *swap32, tmp32;
	int i;
	
//	uint8_t test_buf[] = {
//		0x00, 0x00, 0x00, 0x02
//		, 0x0f, 0x60, 0xba, 0x73, 0xa6, 0x71, 0x40, 0xbb, 0x5e, 0x12, 0x72, 0xd8, 0x09, 0xfe, 0x7b, 0x92, 0x67, 0x39, 0xfe, 0xb8, 0x41, 0x37, 0xe1, 0xee, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00
//		, 0xdd, 0x33, 0x33, 0x6a, 0xc4, 0xfc, 0x66, 0x8f, 0x93, 0x0a, 0x09, 0xb2, 0xcd, 0x83, 0x17, 0xe9, 0xce, 0xc5, 0xb7, 0xc3, 0x96, 0x48, 0xf7, 0x87, 0xbb, 0xe0, 0x2e, 0xe4,
//		0x95, 0x44, 0xed, 0xde, 0x52, 0x90, 0x8b, 0x99
//		, 0x19, 0x07, 0x0b, 0xfb
//	};

	tmp32 = bswap_32(nonce2);
	memcpy(mw->coinbase + mw->nonce2_offset, (uint8_t *)(&tmp32), sizeof(uint32_t));
	work->nonce2 = nonce2;

	dsha256(mw->coinbase, mw->coinbase_len, merkle_root);
	memcpy(merkle_sha, merkle_root, 32);
	for (i = 0; i < mw->nmerkles; i++) {
		memcpy(merkle_sha + 32, mw->merkles[i], 32);
		dsha256(merkle_sha, 64, merkle_root);
		memcpy(merkle_sha, merkle_root, 32);
	}
	data32 = (uint32_t *)merkle_sha;
	swap32 = (uint32_t *)merkle_root;
	flip32(swap32, data32);

	memcpy(mw->header + mw->merkle_offset, merkle_root, 32);
	memcpy(work->header, mw->header, 128);

//	debug32("Work: nonce2 %08x\n", work->nonce2);
//	memcpy(mw->header, test_buf, sizeof(test_buf));
	
	calc_midstate(mw, work);
	
//	debug32("calc_midstate, work->data: \n");
//	hexdump(work->data, 44);
	
//	memcpy(work_t, work->data, 44);
//	rev(work_t, 32);
//	rev(work_t + 32, 12);
//	memcpy(work->data, work_t, 44);

//	hexdump(work->data, 44);
}

int fulltest(const unsigned char *hash, const unsigned char *target)
{
	uint32_t *hash32 = (uint32_t *)hash;
	uint32_t *target32 = (uint32_t *)target;
	int rc = 1;
	int i;

	for (i = 28 / 4; i >= 0; i--) {
		uint32_t h32tmp = bswap_32(hash32[i]);
		uint32_t t32tmp = bswap_32(target32[i]);

		if (h32tmp > t32tmp) {
			rc = NONCE_VALID;
			break;
		}
		if (h32tmp < t32tmp) {
			rc = NONCE_DIFF;
			break;
		}
	}

	return rc;
}

int test_nonce(struct mm_work *mw, uint32_t nonce2, uint32_t nonce)
{
	/* Decode nonce2 and nonce */
//	uint32_t nonce2, nonce;
//	memcpy((uint8_t *)(&nonce2), ret->task_id + 4, 4);
//	memcpy((uint8_t *)(&nonce), ret->nonce, 4);
//	nonce -= 0x180;

	/* Generate the work base on nonce2 */
	struct work work;
	debug32("Test: %08x %08x\n", nonce2, nonce);
	miner_gen_nonce2_work(mw, nonce2, &work);

	/* Write the nonce to block header */
	uint32_t *work_nonce = (uint32_t *)(work.header + 64 + 12);
	*work_nonce = bswap_32(nonce);

	/* Regen hash */
	uint32_t *data32 = (uint32_t *)(work.header);
	unsigned char swap[80];
	uint32_t *swap32 = (uint32_t *)swap;
	unsigned char hash1[32];
	uint32_t *hash_32 = (uint32_t *)(hash1 + 28);

	flip80(swap32, data32);
	dsha256(swap, 80, hash1);

	if (*hash_32 != 0)
		return NONCE_HW;
	/* Compare hash with target */

	return fulltest(hash1, mw->target);
}
