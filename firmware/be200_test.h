#ifndef _BE200_TEST_H
#define _BE200_TEST_H

#include <stdint.h>

void prepare();
void be200_base_test(uint8_t idx);
void be200_mine(uint8_t idx);
void be200_test(uint8_t idx);
void uart1_test();
uint32_t be200_uart_nonce_test();
void be200_uart_set_freq();
void be200_uart_check_idle();
void be200_uart_handler();

#endif

