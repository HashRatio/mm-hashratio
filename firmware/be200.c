#include <stdint.h>
#include <stddef.h>

#include "minilibc.h"
#include "be200.h"
#include "spi.h"
#include "uart.h"
#include "miner.h"
#include "defines.h"
#include "utils.h"

static unsigned char read_buffer[BUFFER_SIZE * 2];
static unsigned char write_buffer[BUFFER_SIZE * 2];

uint8_t be200_cmd_ck(uint8_t idx)
{
	write_buffer[0] = BE200_CMD_CHECK;
	write_buffer[1] = BE200_CMD_CHECK;
	write_buffer[3] = BE200_CMD_CHECK;
	spi_transfer(idx,write_buffer,read_buffer,3);
	return read_buffer[2];
}

uint8_t be200_cmd_rd(uint8_t idx,uint8_t reg)
{
	write_buffer[0] = BE200_CMD_READ_REG(reg);
	write_buffer[1] = 0x00;
	write_buffer[2] = 0x00;
	write_buffer[3] = 0x00;
	spi_transfer(idx,write_buffer,read_buffer,4);
    return read_buffer[2];
}

void be200_cmd_wr(uint8_t idx,uint8_t reg, uint8_t value)
{
	write_buffer[0] = BE200_CMD_WRITE_REG(reg);
	write_buffer[1] = value;
	spi_transfer(idx,write_buffer,read_buffer,2);
	return;
}

void be200_cmd_rst(uint8_t idx)
{
	write_buffer[0] = BE200_CMD_RESET;
	spi_transfer(idx,write_buffer,read_buffer,1);
	return;
}


void be200_reset(uint8_t idx)
{
    be200_cmd_rst(idx);
}

uint8_t be200_is_idle(uint8_t idx)
{
	return be200_cmd_ck(idx) & BE200_STAT_W_ALLOW;
}

uint8_t be200_input_task(uint8_t idx,const uint8_t * task)
{
	uint8_t i,c;
	for (i = 0; i < 44; i++) {
		be200_cmd_wr(idx, i, task[i]);
		c = be200_cmd_rd(idx, i);
		if (c != task[i])
			return 0;
	}
    return 1;
}

void be200_start(uint8_t idx)
{
	be200_cmd_wr(idx, BE200_REG_START, 0xFF);
}

uint8_t be200_get_done(uint8_t idx, uint8_t * nonce_mask)
{
	uint8_t c;
	c = be200_cmd_ck(idx);
    if ( c & BE200_STAT_R_READY)
    {
        *nonce_mask = (c & BE200_STAT_NONCE_MASK) >> 2;
		return 1;
    }
	return 0;
}

uint8_t be200_get_result(uint8_t idx, uint8_t nonce_mask,uint32_t * result)
{
	uint32_t nonce_start_reg;
	if(nonce_mask & 0x01){
	    nonce_start_reg = 46;
	}else if (nonce_mask & 0x02){
	    nonce_start_reg = 50;
	}else if (nonce_mask & 0x04){
	    nonce_start_reg = 54;
	}else if (nonce_mask & 0x08){
	    nonce_start_reg = 58;
	}else{
	    return 0;
	}
	
	*result = ((uint32_t)be200_cmd_rd(idx,nonce_start_reg + 3) << 24) |
			((uint32_t)be200_cmd_rd(idx,nonce_start_reg + 2)   << 16) |
			((uint32_t)be200_cmd_rd(idx,nonce_start_reg + 1)   << 8)  |
			(((uint32_t)be200_cmd_rd(idx,nonce_start_reg + 0)  << 0) + 1);
	
	be200_cmd_rd(idx, BE200_REG_CLEAR);
	return 1;
}

uint8_t be200_output_result(uint32_t res)
{
	uart1_writel(res);
	return 0;
}

void be200_dump_register(uint8_t idx)
{
    uint8_t i;
	uint8_t c;
	for(i=0;i<64;i++)
	{
		c = be200_cmd_rd(idx,i);
//		uart1_writeb(c);
		debug32("%d: %02x ", i, c);
	}
	debug32("\n");
}

void be200_set_pll(uint8_t idx, uint8_t mult)
{
//	be200_cmd_wr(idx, BE200_REG_PLL, (0x80 | mult));
	be200_cmd_wr(idx, BE200_REG_PLL, (0x7F & mult));
}

void be200_clear(uint8_t idx)
{
    be200_cmd_wr(idx,BE200_REG_CLEAR,0x00);
}


/*void test_miner_status()
{
	uint8_t i;
	for(i=0;i<CHIP_NUMBER;i++)
	{
		miner_status[i].nonce2 = 0;
		miner_status[i].nonce_reg0 = 0;
		miner_status[i].nonce_reg1 = 0;
		miner_status[i].nonce_reg2 = 0;
		miner_status[i].nonce_reg3 = 0;
		miner_status[i].status = 0;
	}
}*/

