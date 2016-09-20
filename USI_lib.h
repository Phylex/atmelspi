#ifndef USI_LIB_H
#define USI_LIB_H

#include <avr/io.h>
#include <avr/interrupt.h>

// this line configures the software into Master or Slave at compiletime
#define USIASMASTER
#define DOut 5
#define SCLk 4
#define DIn  6

#define USI_SS_PORT PORTA
#define USI_SS1 3

#define MAXFREQ 2500

#define USIFREQUENCY 2500
#define USI_PACKET_AMOUNT 4
#define USI_PACKET_LENGTH 6

void init_USI_SPI(void);
int usi_packet_write(uint8_t data[], uint8_t start, uint8_t size);
int usi_master_read(uint8_t data[], uint8_t start);
int usi_slave_read(uint8_t data[], uint8_t start);
void usi_packet_slave_write(uint8_t outputdata[], uint8_t start, uint8_t size);
uint8_t usi(void);
#endif
