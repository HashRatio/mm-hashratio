#include <stdint.h>
#include <stddef.h>

#include "be200.h"
#include "spi.h"
#include "uart.h"
#include "be200_test.h"
#include "defines.h"
#include "utils.h"

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


void be200_base_test(uint8_t idx)
{
	uint8_t i;
	for(i=0;i<44;i++)
	{
		be200_cmd_wr(idx,i,i);
	}
	for(i=0;i<44;i++)
	{
		uart1_writeb(be200_cmd_rd(idx,i));
	}
}

void be200_mine(uint8_t idx)
{
    uint8_t ready;
	uint8_t nonce_mask;
	uint32_t res;
    be200_reset(idx);
	delay(100);
	be200_cmd_wr(idx,BE200_REG_PLL,19);
	delay(100);
	while(1)
	{	
		uart1_writeb(0x55);
		uart1_writeb(0xAA);
		uart1_writeb(0x55);
		uart1_writeb(0xAA);
		if( be200_is_idle(idx))
		{
			prepare();
		    be200_input_task(idx,test_data);
			be200_start(idx);
		}
		while(!be200_is_idle(idx))
		{
		    ready = be200_get_done(idx,&nonce_mask);
			if(ready > 0)
			{
			    be200_get_result(idx,nonce_mask,&res);
				be200_dump_register(idx);
				be200_clear(idx);
			}
		}
	}
}

void be200_test(uint8_t idx)
{
	uint8_t cmd;
	uint8_t c;
	while(1)
	{	
		cmd = uart1_read();
		switch(cmd){
		case 0x01://full test
			be200_mine(idx);
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
		case 0x55:
			be200_base_test(idx);
			break;
			
		}
	}
}

void uart1_test()
{
	uint8_t c=0xAA;
	while(1)
	{
		//c = uart1_read();
		uart1_writeb(c);
		delay(100);
	}
}

static uint8_t task_buffer[44];
uint32_t be200_uart_nonce_test()
{
	uint8_t i;
	uint8_t idx;
	uint8_t ready;
	uint8_t nonce_mask;
	uint32_t res;
	
	idx = uart1_read();
	
	for(i=0;i<44;i++)
	{
		task_buffer[i] = uart1_read();
	}

	be200_input_task(idx,task_buffer);
	be200_start(idx);
	while(!be200_is_idle(idx))
	{
	    ready = be200_get_done(idx,&nonce_mask);
		if(ready > 0)
		{
		    be200_get_result(idx,nonce_mask,&res);
			uart1_writel(res);
			//be200_dump_register(idx);
			while(!be200_is_idle(idx));
			be200_clear(idx);
			return 1;
		}
	}
	uart1_writel(0x00000000);
	return 0;
}

void be200_uart_set_freq()
{
	uint8_t multi;
	uint8_t idx;
	
	idx = uart1_read();
	multi = uart1_read();
	be200_cmd_wr(idx,BE200_REG_PLL,multi);
	uart1_writeb(multi);
}

void be200_uart_check_idle()
{
	uint8_t idx;
	idx = uart1_read();
	if(be200_is_idle(idx))
		uart1_writeb('T');
	else
		uart1_writeb('F');
}

void be200_uart_handler()
{
	uint8_t cmd;
	cmd = uart1_read();
	switch(cmd){
		case 0x01:
			be200_uart_set_freq();
			break;
		case 0x02:
			be200_uart_nonce_test();
			break;
		case 0x03:
			be200_uart_check_idle();
			break;
		default:
			break;
	}
}

