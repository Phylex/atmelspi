/* Header file for the spi_328 Library */
/* Last Change: 3.2.16			*/

#ifndef SPI_328_DEF
#define SPI_328_DEF

#include <avr/io.h>
#include <avr/interrupt.h>

// this sets the CHIP into MASTER or SLAVE mode at compiletime
//#define SPIASMASTER

#define SPI_PACKET_LENGTH 6
#define SPI_PACKET_AMOUNT 4

void init_spi_master(void);
void init_spi_slave(void);
uint8_t spi_slave_read(uint8_t data[], uint8_t start);
void spi_slave_write(uint8_t outputdata[],uint8_t start, uint8_t size);
uint8_t spi_master_read(uint8_t data[], uint8_t start);
uint8_t spi_master_write(/*uint8_t slave,*/uint8_t data[], uint8_t start,uint8_t size);

#endif
