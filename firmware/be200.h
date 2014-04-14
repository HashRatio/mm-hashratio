#ifndef _BE200_H
#define _BE200_H

#include "be200.h"
#include "spi.h"
#include "defines.h"
#include "uart.h"
#include <stdint.h>

#define BE200_ICLK 200

#define BE200_REG_START 44
#define BE200_REG_CLEAR 62
#define BE200_REG_PLL 45

#define BE200_CORE_NUM 1
#define BE200_CMD_CHECK 0x00

#define BE200_CMD_RESET 0xC0
#define BE200_CMD_WRITE_REG(n) (0x80 | (n))
#define BE200_CMD_READ_REG(n) (0x40 | (n))

#define BE200_STAT_W_ALLOW 0x01
#define BE200_STAT_R_READY 0x02
#define BE200_STAT_NONCE_MASK 0x3C

#define BUFFER_SIZE 44

#define BASE_FREQ 12 //Mhz

void be200_reset(uint16_t idx);
uint8_t be200_check_idle(uint16_t idx);
uint8_t be200_input_task(uint16_t idx, const uint8_t * task);
void be200_start(uint16_t idx);
uint8_t be200_get_done(uint16_t idx, uint8_t * nonce_mask);
uint8_t be200_get_result(uint16_t idx, uint8_t nonce_mask,uint32_t * result);
uint8_t be200_output_result();
void be200_dump_register(uint16_t idx);
uint8_t be200_cmd_ck(uint16_t idx);
uint8_t be200_cmd_rd(uint16_t idx,uint8_t reg);
void be200_cmd_wr(uint16_t idx, uint8_t reg, uint8_t value);
void be200_cmd_rst(uint16_t idx);
void be200_set_pll(uint16_t idx, uint8_t factor);
void be200_clear(uint16_t idx);

#endif
