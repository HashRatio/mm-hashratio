/*
 * Author: Xiangfu Liu <xiangfu@openmobilefree.net>
 * Bitcoin:	1CanaaniJzgps8EV6Sfmpb7T8RutpaeyFn
 *
 * This is free and unencumbered software released into the public domain.
 * For details see the UNLICENSE file at the root of the source tree.
 */

#ifndef _SPI_H
#define _SPI_H

void spi_test(uint8_t c);
void spi_transfer(uint8_t idx, uint8_t * rbuff, uint8_t * wbuff, uint32_t cnt);
void spi_select(unsigned char cid);
void spi_write(unsigned int idx, char c);
unsigned char spi_read(unsigned char idx, unsigned char * pbuff);
void be200_wr(unsigned char idx, unsigned char rid, char c);
unsigned char be200_rd(unsigned char idx, unsigned char rid, unsigned char * pbuff);



#endif	/* _SPI_H */

