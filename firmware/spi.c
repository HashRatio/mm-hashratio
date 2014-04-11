#include <stdint.h>
#include <stddef.h>

#include "minilibc.h"
#include "system_config.h"
#include "defines.h"
#include "intr.h"
#include "io.h"
#include "uart.h"

static struct lm32_spi *spi[2] = { \
(struct lm32_spi *)SPI0_BASE, (struct lm32_spi *)SPI1_BASE};

//static struct lm32_spi *spi1 = (struct lm32_spi *)SPI1_BASE;
static struct lm32_spi * spi0 = (struct lm32_spi *)SPI0_BASE;

#define BUFFER_SIZE 44
static unsigned char read_buffer[BUFFER_SIZE * 2];
static unsigned char write_buffer[BUFFER_SIZE * 2];

void spi_select(struct lm32_spi * tar_spi, uint8_t cidx)
{
	writeb(cidx,&tar_spi->ssmask);
}


void spi_transfer(uint16_t idx, uint8_t * wbuff, uint8_t * rbuff, uint32_t cnt)
{	
	uint8_t board_idx = (uint8_t)(idx >> 8);
	uint8_t chip_idx = (uint8_t)idx;
	struct lm32_spi * tar_spi = spi[board_idx];
	uint32_t i;
	spi_select(tar_spi,chip_idx);
	
	for(i=0;i<cnt;i++)
	{
		while (!(tar_spi->status & LM32_SPI_STAT_TRDY))
			;
		writeb(wbuff[i],&tar_spi->tx);
		rbuff[i] = tar_spi->rx;
	}
	
}

void spi_test(uint8_t c)
{
	//uint32_t i;
	write_buffer[0] = 0x80;
	write_buffer[1] = c;
	write_buffer[2] = 0x40;
	write_buffer[3] = 0x00;
	write_buffer[4] = 0x00;
	spi_transfer(0x0001,read_buffer,write_buffer,5);
	/*for(i=0;i<4;i++)
	{
		uart1_writeb(read_buffer[i]);
	}*/
	if(write_buffer[1] != read_buffer[4]){
		uart1_writeb(0x55);
		uart1_writeb(0xAA);
		uart1_writeb(write_buffer[1]);
		uart1_writeb(read_buffer[3]);
	}
}

void spi_writeb(struct lm32_spi * tar_spi, uint8_t c)
{
	
	while (!(tar_spi->status & LM32_SPI_STAT_TRDY))
		;
	writeb(c,&tar_spi->tx);
	
}

void spi_write(unsigned char cid,char c)
{
	//unsigned char s;
	writeb(c,&spi0->tx);
	uart1_writeb(spi0->rx);

}

unsigned char spi_read(unsigned char idx, unsigned char * pbuff)
{
	//if(!(readb(&spi0->status)&LM32_SPI_STAT_RRDY))
	//	return 0;
	*pbuff = readb(&spi0->rx);
	return 1;
}

void be200_wr(unsigned char idx, unsigned char rid, char c)
{
	spi_write(0x01,0x80);
	spi_write(0x01,0x55);
}

unsigned char be200_rd(unsigned char idx, unsigned char rid, unsigned char * pbuff)
{
	spi_write(0x01,0x40);
	if(!spi_read(0x01,pbuff))
		return 0;
	return 1;
}

