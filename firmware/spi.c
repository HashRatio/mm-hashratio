#include <stdint.h>

#include "minilibc.h"
#include "system_config.h"
#include "defines.h"
#include "intr.h"
#include "io.h"
#include "uart.h"

static struct lm32_spi *spi0 = (struct lm32_spi *)SPI0_BASE;
//static struct lm32_spi *spi1 = (struct lm32_spi *)SPI1_BASE;


void spi_select(unsigned char cid)
{
	writeb(cid,&spi0->ssmask);
}

void spi_write(unsigned char cid,char c)
{
	writeb(c, &spi0->tx);
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

