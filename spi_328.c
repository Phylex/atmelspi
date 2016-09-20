/*>-----Library for the Use with the SPI of Atmel µC's>->-------*/
/*>-----Creator:>-------Alexander Becker>------->------->-------*/
/*>-----Creation date:  05.06.2015>----->------->------->-------*/
/*>-----Usage:>->-------provide Funktions for Use>------>-------*/
/*>----->------->-------with the SPI on Atmel IC's implementing */
/*>----->------->-------SPI as Master and Slave		>-------*/
/*>-----Last update:>---10.07.2015>-----version 0.3>---->-------*/

#include "spi_328.h"
#include <avr/io.h>

#define SPI_DDR DDRB
#define SPI_MOSI PB3
#define SPI_MISO PB4
#define SPI_CLK PB5
#define SPI_SS_DDR DDRB
#define SPI_SS_PORT PORTB
#define SPI_SS_PIN  PINB
#define SPI_SS1 PB2

#define SPIMASTER 0
#define SPIMOSIBUFFERFULL 4
#define SPIMOSIBUFFEREMPTY 1
#define SPIMISOBUFFERFULL 5
#define SPIMISOBUFFEREMPTY 6
#define SPIRECIEVEDPACKET 2
#define SPIPACKETSENDING 3
#define SPICLKRANGE CS01

volatile uint8_t spi_status;
volatile uint8_t spi_byte_sent;

volatile uint8_t spi_mosi_data[SPI_PACKET_LENGTH*SPI_PACKET_AMOUNT];
volatile uint8_t spi_mosi_packet_status[SPI_PACKET_AMOUNT];
volatile uint8_t spi_mosi_packet_size;
volatile uint8_t spi_mosi_next_byte_indicator;
volatile uint8_t spi_mosi_packet_selector;

volatile uint8_t spi_miso_packet_selector;
volatile uint8_t spi_miso_next_byte_indicator;

#ifndef SPIASMASTER
volatile uint8_t spi_miso_packet_status[SPI_PACKET_AMOUNT];
volatile uint8_t spi_miso_data[SPI_PACKET_AMOUNT*SPI_PACKET_LENGTH];
#else //SPI is set as master
volatile uint8_t spi_miso_data[SPI_PACKET_LENGTH*2];
volatile uint8_t spi_miso_packet_status[2];
#endif

#ifdef SPIASMASTER
void init_spi(void)
{
	// todo look up the input and output pins and ports of the SPI interface and write them into the header file.
	SPI_DDR|=(1<<SPI_MOSI);	//set MOSI to output,
	SPI_DDR&=~(1<<SPI_MISO);//miso to input
	SPI_DDR|=(1<<SPI_CLK);	// and CLK and SS as output
	SPI_DDR|=(1<<SPI_SS1);	//
	SPI_SS_PORT|=(1<<SPI_SS1); // the SS is pulled high so that no transmission can occurr
	SPCR|=(1<<SPIE);	// enable SPI interrupts
	SPCR|=(1<<SPE);		// enable SPI interface
	SPCR&=~(1<<DORD); 	//set the transmission order to MSB first
	SPCR|=(1<<MSTR);	//sets the SPI into Master mode
	SPCR&=~(1<<CPOL);	//sets the leading edge of the Clk to the rising flank
				//and the trailing edge to the falling one
	SPCR&=~(1<<CPHA);	//sets leading edge to sample and trailing edge to setup
	SPCR|=(1<<SPR1)|(1<<SPR0);	//sets CLK to 156250 bit/second
	spi_status|=(1<<SPIMASTER);	//set the transmission software to master mode
	spi_status|=(1<<SPIMOSIBUFFEREMPTY);
	spi_status|=(1<<SPIMISOBUFFEREMPTY);
	for(uint8_t i=0;i<SPI_PACKET_AMOUNT;i++){
		spi_mosi_packet_status[i]=0;
	}
}

#else //this is the init for the Slave
void init_spi(void)	//todo look up correct and compleate slave initialisation.
{
	uint8_t i;
	//set MISO output all other input
	SPI_DDR |= (1<<SPI_MISO);
	SPI_DDR &=~(1<<SPI_MOSI);
	SPI_DDR &=~(1<<SPI_CLK);
	SPI_DDR &=~(1<<SPI_SS1);
	//set Clk Polarisation
	SPCR&=~(1<<CPOL);	//sets the leading edge of the Clk to the rising flank
				//and the trailing edge to the falling one
	SPCR&=~(1<<CPHA);	//sets leading edge to sample and trailing edge to setup
	//enable the SPI
	SPCR |= (1<<SPE)|(1<<SPIE);
	//set configuration to slave
	SPCR &= ~(1<<MSTR);
	//initialize the variables
	spi_status&=~(1<<SPIMASTER);
	spi_status|=(1<<SPIMOSIBUFFEREMPTY);
	spi_status|=(1<<SPIMISOBUFFEREMPTY);
	for(i=0;i<SPI_PACKET_AMOUNT;i++){
		spi_mosi_packet_status[i]=0;
	}
	SPDR=0;
}
#endif

uint8_t spi_slave_read(uint8_t data[], uint8_t start)
{
	uint8_t i=0;
	uint8_t bufferempty=0;
	uint8_t j=0;
	//check for packets in the Buffer ready for readout
	while(!(spi_mosi_packet_status[i])){
		i++;
		if(i>=SPI_PACKET_AMOUNT){
			bufferempty=1;
			break;
		}
	}
	// if a packet is in the buffer it is returned and the size is returned as returnvalue of the funktion
	if(!bufferempty){
		while(j<spi_mosi_packet_status[i]){
			data[start+j]=spi_mosi_data[i*SPI_PACKET_LENGTH+j];
			j++;
		}
		spi_mosi_packet_status[i]=0;
		j++;
	}
	return j;
}

void spi_slave_write(uint8_t outputdata[], uint8_t start, uint8_t size)
{
	uint8_t i=0;
	if(!(spi_status&(1<<SPIMISOBUFFERFULL))){
		while(spi_miso_packet_status[i]){
			i++;
			if(i>=SPI_PACKET_AMOUNT){
				i=0;
				break;
				spi_status|=(1<<SPIMISOBUFFERFULL);
			}
		}
		if(!(spi_status&(1<<SPIMISOBUFFERFULL))){
			i*=SPI_PACKET_LENGTH;
			for(int j=0;j<size;j++){
				spi_miso_data[i+j]=outputdata[start+j];
			}
			spi_miso_packet_status[i]= size;
		}
	}
	if(spi_status&(1<<SPIMISOBUFFEREMPTY))
	{
		SPDR=spi_miso_data[i];
		spi_miso_data[i]=0;
		if(size>1){
			spi_miso_next_byte_indicator++;
			spi_status&=~(1<<SPIMISOBUFFEREMPTY);
		}
	}
}

uint8_t spi_master_read(uint8_t data[], uint8_t start){
	//there are 2 miso-packet buffers. In one the Packet is presented to the Routine in the other the recieved packet is written into
	uint8_t readoutpacket= 1-spi_miso_packet_selector;
	uint8_t packetsize = spi_miso_packet_status[readoutpacket];
	uint8_t readoutpointer= readoutpacket*SPI_PACKET_LENGTH;
	// this checks if there is a packet in the buffer
	if(packetsize){
		for(uint8_t i=0;i<packetsize;i++){
			data[start+i]=spi_miso_data[readoutpointer+i];
		}
	}
	return packetsize;
}

//REPLACED
uint8_t spi_master_write(uint8_t data[], uint8_t start, uint8_t size)	//TODO EXPAND CODE TO WORK WITH MULTIPLE SLAVES everything else is finished
{
	uint8_t i=0;
	uint8_t j;
	uint8_t k=0;
	uint8_t returnval=0;
	//check where there is the next free slot in the buffer
	while(spi_mosi_packet_status[i]){
		if(i<SPI_PACKET_AMOUNT){
			i++;
		}
		else{
			spi_status|=(1<<SPIMOSIBUFFERFULL);
			break;
		}
	}
	// if the buffer is not full
	if(!(spi_status&(1<<SPIMOSIBUFFERFULL))){
		k=i*SPI_PACKET_LENGTH;
		// write the data into the send buffer and send it away
		for(j=0;j<size;j++){
			spi_mosi_data[k+j]=data[start+j];
		}
		spi_mosi_packet_status[i]= size;
	}
	else{
		// if the Buffer is full, the packet is simply droped and returns an error value is returned
		returnval=1;
	}
	if(spi_status&(1<<SPIMOSIBUFFEREMPTY)){
		spi_status&=~(1<<SPIMOSIBUFFEREMPTY);//remove the buffer empty flag
		PORTB&=~(1<<PB2);		//set the SS-Line to LOW to indicate transmission to the slave
		SPDR=spi_mosi_data[k];		//write first byte of packate into the data send register
		if(size>1){			//if the packet is longe rthan one byte set the software into the sending mode
			spi_mosi_packet_selector=i;	//set the packet selector
			spi_status|=(1<<SPIPACKETSENDING);
			spi_mosi_next_byte_indicator=k+1;
		}
		spi_byte_sent++;		//set the pointer to the next byte to be sent
	}
	// the returnval shows if the packet has been written into the buffer
	return returnval;
}

//start of the ISR it handles the sending of the next byte of a packet while in sending mode or loads a new paccet from the buffer if one is available 
ISR(SPI_STC_vect)
#ifdef SPIASMASTER
{
	//determine if a packet is being sent
	if(spi_status&(1<<SPIPACKETSENDING)){
		// read in the recieved byte from the slave 
		spi_miso_data[spi_miso_next_byte_indicator]=SPDR;
		// write the next byte of the packet to be sent
		SPDR=spi_mosi_data[spi_mosi_next_byte_indicator];
		// increment pointer
		spi_mosi_next_byte_indicator++;
		spi_miso_next_byte_indicator++;
		spi_byte_sent++;
		// check if we have reached the last byte of the paket
		if(spi_byte_sent>=spi_mosi_packet_status[spi_mosi_packet_selector]){
			spi_status&=~(1<<SPIPACKETSENDING);
			spi_mosi_packet_status[spi_mosi_packet_selector]=0;
			spi_mosi_packet_selector=0;
		}
	}
	// this is the start of the transmission of a New Packet and will have to be handled differently
	else 
	{
		SPI_SS_PORT|=(1<<SPI_SS1); //pull up the SS line to indicate end of last Package

		// read in the last byte of the last package to have been sent before selscting a new one
		spi_miso_data[spi_miso_next_byte_indicator]=SPDR;
		spi_miso_packet_status[spi_miso_packet_selector]=spi_byte_sent;
		spi_byte_sent=0;

		//determin the next package to be sent 
		spi_mosi_packet_selector=0;
		while(!spi_mosi_packet_status[spi_mosi_packet_selector]){
			spi_mosi_packet_selector++;
			if(spi_mosi_packet_selector>=SPI_PACKET_AMOUNT){
				spi_mosi_packet_selector=0;
				spi_status|=(1<<SPIMOSIBUFFEREMPTY);
				break;
			}
		}
		//determine the size of the package to be sent
		spi_mosi_packet_size=spi_mosi_packet_status[spi_mosi_packet_selector];
		spi_mosi_next_byte_indicator=spi_mosi_packet_selector*SPI_PACKET_LENGTH; 

		//cycle through the storage for incoming packets
		spi_miso_packet_selector = 1-spi_miso_packet_selector;
		spi_miso_next_byte_indicator = spi_miso_packet_selector*SPI_PACKET_LENGTH;

		// if the buffer is not empty start writing the packet into the SPI data register
		if(!(spi_status&(1<<SPIMOSIBUFFEREMPTY))){
			SPDR=spi_mosi_data[spi_mosi_next_byte_indicator];
			//if the package has more than one byte then nothing has to be done exept increment the neccesary counters and set some flags
			if(spi_mosi_packet_size > 1){
				spi_status|=(1<<SPIPACKETSENDING);
				spi_byte_sent++;
				spi_mosi_next_byte_indicator++;
			}

			//if the packet has only one byte some cleaning up has to be done
			else{
				spi_mosi_packet_status[spi_mosi_packet_selector]=0;
			}
			//pull low the SS line to begin the new package or keep it pulled low for the next byte
			SPI_SS_PORT&=~(1<<USI_SS1); //TODO there still is no multiple Slave support
		}
	}
}
#else /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//check if the packet has ended (more than once)
	for(uint8_t i=0;i<10;i++){
		if(SPI_SS_PIN&(1<<SPI_SS1)){
			spi_status|=(1<<SPIRECIEVEDPACKET);
			break;
		}
	}
	//here comes the code for the haandeling of an entire packet (for example writing the length into the appropriate indicator and signaling that the buffer occupied)
	if(spi_status&(1<<SPIRECIEVEDPACKET)){
		//write the last remaining byte to the buffer
		spi_mosi_data[spi_mosi_next_byte_indicator]=SPDR;
		spi_mosi_packet_size++;
		//indicate that the packet has been recieved
		spi_mosi_packet_status[spi_mosi_packet_selector]=spi_mosi_packet_size;

		//set the miso-bufferposition of the packet that was transmitted to 0, indicating a free spot in the buffer. If there was no packet, then ther is already a zero at the Position written to.
		spi_miso_packet_status[spi_miso_packet_selector]=0;

		//pick the next packet location and initialize the variables
		spi_mosi_packet_selector=0;
		while(spi_mosi_packet_status[spi_mosi_packet_selector]){
			spi_mosi_packet_selector++;
			if(spi_mosi_packet_selector>=SPI_PACKET_AMOUNT){
				spi_mosi_packet_selector=0;
				spi_status|=(1<<SPIMOSIBUFFERFULL);
				break;
			}
		}
		// pick the next packet from the heap of the packets to send
		spi_miso_packet_selector=0;
		while(!spi_miso_packet_status[spi_miso_packet_selector]){
			spi_miso_packet_selector++;
			if(spi_miso_packet_selector>=SPI_PACKET_AMOUNT){
				spi_status|=(1<<SPIMISOBUFFEREMPTY);
				spi_miso_packet_selector=0;
				break;
			}
		}
		// write the first byte of the first found miso packet into write register so it canbeshifted to the master
		if(!(spi_status&(1<<SPIMISOBUFFEREMPTY))){
			spi_miso_next_byte_indicator=SPI_PACKET_LENGTH*spi_miso_packet_selector;
			SPDR=spi_miso_data[spi_miso_next_byte_indicator];
			spi_miso_data[spi_miso_next_byte_indicator]=0;
			//set the indicator to the next field 
			spi_miso_next_byte_indicator++;
		}
		else{
			spi_miso_next_byte_indicator=0;
			SPDR=0;
		}
		// if there is space in the input buffer reserve a location in the buffer for next packet
		if(!(spi_status&(1<<SPIMOSIBUFFERFULL)))
			spi_mosi_next_byte_indicator=spi_mosi_packet_selector*SPI_PACKET_LENGTH;

		//tidy up the variables and flags
		spi_mosi_packet_size=0;
		spi_status&=~(1<<SPIRECIEVEDPACKET);
	}
	//here a packet was not recieved entirely, only one byte of the packet was recieved therefor no recalculations of the pointers have to be made
	else{
		//simply write the next byte into the indicated field in the buffer
		spi_mosi_data[spi_mosi_next_byte_indicator]=SPDR;
		//set the indicator to the next field
		spi_mosi_next_byte_indicator++;
		//increase the packet size
		spi_mosi_packet_size++;
		if(!(spi_status&(1<<SPIMISOBUFFEREMPTY))){
		//write the next byte to be sent into the data register 
		SPDR=spi_miso_data[spi_miso_next_byte_indicator];
		spi_miso_data[spi_miso_next_byte_indicator]=0;
		//set the indicator to the next field 
		spi_miso_next_byte_indicator++;
		}
		else{
			SPDR=0;
		}
	}
}
#endif //SPIASMASTER
