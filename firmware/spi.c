#include <stdint.h>

#include "minilibc.h"
#include "system_config.h"
#include "defines.h"
#include "intr.h"
#include "io.h"
#include "uart.h"

static struct lm32_spi *spi0 = (struct lm32_spi *)SPI0_BASE;
static struct lm32_spi *spi1 = (struct lm32_spi *)SPI1_BASE;


void spi_write(unsigned int idx,char c)
{
	//unsigned char i = 0;
	/*unsigned char mask = 1;
	for(i=0;i<=1;i++){
		mask = mask << i;
		//while (!(readb(&spi0->status) & LM32_SPI_STAT_TRDY))
			//	;
		writeb(mask,&spi0->ssmask);
		writeb(c, &spi0->tx);
	}*/
	/*while (!(i = readb(&spi0->status) & LM32_SPI_STAT_TRDY)){
		uart1_write(i);*/
	/*while(1){
		i = readb(&spi0->status);
		uart1_write(i);
		if(!(i & LM32_SPI_STAT_TRDY))
			break;
	}*/
	
	writeb(0x01,&spi0->ssmask);
	writeb(c, &spi0->tx);
	writeb(0x01,&spi1->ssmask);
	writeb(c, &spi1->tx);
}

