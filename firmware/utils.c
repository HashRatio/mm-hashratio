#include "utils.h"
#include "system_config.h"

void delay(unsigned int ms)
{
	unsigned int i;

	while (ms && ms--) {
		for (i = 0; i < CPU_FREQUENCY / 1000 / 5; i++)
			__asm__ __volatile__("nop");
	}
}

void delay_us(unsigned int us)
{
	unsigned int i;
	
	while (us && us--) {
		for (i = 0; i < CPU_FREQUENCY / 1000 / 1000 / 5; i++)
			__asm__ __volatile__("nop");
	}
}
