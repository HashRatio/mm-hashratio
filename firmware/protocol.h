/*
 * Author: Xiangfu Liu <xiangfu@openmobilefree.net>
 * Bitcoin:	1CanaaniJzgps8EV6Sfmpb7T8RutpaeyFn
 *
 * This is free and unencumbered software released into the public domain.
 * For details see the UNLICENSE file at the root of the source tree.
 */

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

/* Avalon2 protocol package type */
#define MM_VERSION_LEN	15
#define HRTO_H1	'H'
#define HRTO_H2	'R'

#define HRTO_P_COINBASE_SIZE  (6 * 1024)
#define HRTO_P_MERKLES_COUNT  20

#define HRTO_P_COUNT     39
#define HRTO_P_DATA_LEN  (HRTO_P_COUNT - 7)

#define HRTO_P_DETECT   10
#define HRTO_P_STATIC   11
#define HRTO_P_JOB_ID   12
#define HRTO_P_COINBASE 13
#define HRTO_P_MERKLES  14
#define HRTO_P_HEADER   15
#define HRTO_P_POLLING  16
#define HRTO_P_TARGET   17
#define HRTO_P_REQUIRE  18
#define HRTO_P_SET      19
#define HRTO_P_TEST     20

#define HRTO_P_ACK        21
#define HRTO_P_NAK		  22
#define HRTO_P_NONCE      23
#define HRTO_P_STATUS     24
#define HRTO_P_ACKDETECT  25
#define HRTO_P_TEST_RET   26

#endif	/* _PROTOCOL_H_ */
