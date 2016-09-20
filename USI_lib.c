/*	Library for the Use with the USI of Atmel µC's		*/
/*	Creator:	Alexander Becker			*/
/*	Creation date:  05.06.2015				*/
/*	Usage:		provide Funktions for Use		*/
/*			with the USI on Atmel IC's implementing */
/*			and SPI as Master and Slave		*/
/*	Last update:	10.07.2015	version 0.3		*/

#include <avr/io.h>
#include "USI_lib.h"
#define MAXFREQ 2500

#define USIMASTER 0
#define USIBUFFEREMPTY 1

#define USIBUFFERFULL 6
#define USIPACKETSENDING 2
#define USIRECIEVEDPACKET 5

#define USIMISOBUFFEREMPTY 3
#define USIMISOBUFFERFULL 4

#define USICLKRANGE CS02

#define USISLAVENUMBER 4

volatile uint8_t usi_status;

volatile uint8_t usi_data_mosi[USI_PACKET_AMOUNT*USI_PACKET_LENGTH];
volatile uint8_t usi_data_mosi_status[USI_PACKET_AMOUNT];

//volatile uint8_t usi_data_slaveselection_indicator[USISLAVENUMBER];

volatile uint8_t usi_packet_miso_selector;
volatile uint8_t usi_next_byte_miso_indicator;
volatile uint8_t usi_byte_sent;
volatile uint8_t usi_packet_selector;
volatile uint8_t usi_packet_size;
volatile uint8_t usi_next_byte_indicator;

#ifndef USIASMASTER
volatile uint8_t usi_data_miso[USI_PACKET_LENGTH*USI_PACKET_AMOUNT];
volatile uint8_t usi_data_miso_status[USI_PACKET_AMOUNT];
#else //USI is set as master
volatile uint8_t usi_data_miso[USI_PACKET_LENGTH*2];
volatile uint8_t usi_data_miso_status[2];
#endif

#ifdef USIASMASTER
void init_USI_SPI(void)
{
	//set up the output specifications
	DDRA|= (1<<DOut);
	DDRA|= (1<<SCLk);			//set DO as Output and SCL
	DDRA&= ~(1<<DIn);			//set DI as Input
	DDRA|= (1<<3);				//pull the SS line high
	PORTA|=(1<<3);				//TODO make work for multiple slaves
	//set up mode of operation
	USICR|=(1<<USIWM0);
	// CLK-source= External positive edge; 4bit counter= USITC
	USICR|= (1<<USICS1)|(1<<USICLK);
	USICR&= ~(1<<USICS0);
	//overflow interrupt enable
	USICR|=(1<<USIOIE);
	//set up transmitting speed with timer value
	OCR0A=MAXFREQ/USIFREQUENCY;
	TIMSK0|=(1<<OCIE0A);
	TCCR0A|=(1<<WGM01);
	//set up software flags for further processing by interrupt handeling vector
	usi_status|=(1<<USIMASTER);
	usi_status|=(1<<USIBUFFEREMPTY);
	usi_status|=(1<<USIMISOBUFFEREMPTY);
	//initialize variables
	usi_packet_selector=0;
}

#else //this is the init for the Slave
void init_USI_SPI(void)
{
	DDRA|=(1<<DOut); 			//set DO as Output
	DDRA&= ~((1<<DIn)|(1<<SCLk));	//set SCL and DI as Inputs
	DDRA&=~(1<<3);			//set the SS line
	USICR|=(1<<USIOIE); 		//overflow interrupt enable
	USICR|=(1<<USIWM0); 		//set wire mode to three wire mode (SPI compatible)
	USICR|=(1<<USICS1); 		//set CLK source to be external positive edge
	USICR&=~(1<<USICS0);		//set CLK source to be external positive edge
	usi_status=0;
	usi_status|=(1<<USIBUFFEREMPTY);
	usi_status|=(1<<USIMISOBUFFEREMPTY);
	USISR&=0b11110000;		//set the CLK to 0;
}
#endif

int usi_packet_write(uint8_t data[], uint8_t start, uint8_t size)	//TODO EXPAND CODE TO WORK WITH MULTIPLE SLAVES everything else is finished
{
	uint8_t i=0;
	uint8_t j;
	uint8_t k=0;
	uint8_t returnval=0;
	//check where there is the next free slot in the buffer
	while(usi_data_mosi_status[i]){
		if(i<USI_PACKET_AMOUNT){
			i++;
		}
		else{
			usi_status|=(1<<USIBUFFERFULL);
			break;
		}
	}
	// if the buffer is not full
	if(!(usi_status&(1<<USIBUFFERFULL))){
		k=i*USI_PACKET_LENGTH;
		// write the data into the send buffer and send it away
		for(j=0;j<size;j++){
			usi_data_mosi[k+j]=data[start+j];
		}
		usi_data_mosi_status[i]= size;
	}
	else{
		// if the Buffer is full, the packet is simply droped and returns an error value is returned
		returnval=1;
	}
	// if the buffer is empty then the clock has to be restarted 
	if(usi_status&(1<<USIBUFFEREMPTY)){
		TCNT0=0;				// Stop Timer
		usi_status&=~(1<<USIBUFFEREMPTY);	// clear buffer Empty flags
		PORTA&=~(1<<3);				// Pull SS line low
		USIDR=usi_data_mosi[k];			// fill the output register of the USI
		TCCR0B|= (1<<USICLKRANGE);		// Start up the timer again to shift out packet
		if(size>1){
			usi_status|=(1<<USIPACKETSENDING);
			usi_packet_selector=i;		// Set the Packet selector
			usi_next_byte_indicator=k+1;
		}
		usi_byte_sent++;
	}
	// the returnval shows if the packet has been written into the buffer
	return returnval;
}

int usi_master_read(uint8_t data[], uint8_t start){

	uint8_t readoutpacket= 1-usi_packet_miso_selector;
	uint8_t packetsize = usi_data_miso_status[readoutpacket];
	uint8_t readoutpointer= readoutpacket*USI_PACKET_LENGTH;
	// this checks if there is a packet in the buffer
	if(packetsize){
		for(uint8_t i=0;i<packetsize;i++){
			data[start+i]=usi_data_miso[readoutpointer+i];
		}
	}
	return packetsize;
}

int usi_slave_read(uint8_t data[], uint8_t start)
{
	uint8_t i=0;
	uint8_t bufferempty=0;
	uint8_t j=0;
	while(!usi_data_mosi_status[i]){
		if(i<(USI_PACKET_AMOUNT-1)){
			i++;
		}
		else{
			bufferempty=1;
			break;
		}
	}
	if(!bufferempty){
		while(j<usi_data_mosi_status[i]){
			data[start+j]=usi_data_mosi[i*USI_PACKET_LENGTH+j];
			j++;
		}
		usi_data_mosi_status[i]=0;
		j++;
	}
	return j;
}
void usi_packet_slave_write(uint8_t outputdata[], uint8_t start, uint8_t size)
{
	uint8_t i=0;
	if(!(usi_status&(1<<USIMISOBUFFERFULL))){
		while(usi_data_miso_status[i]){
			i++;
			if(i>=USI_PACKET_AMOUNT){
				i=0;
				break;
				usi_status|=(1<<USIMISOBUFFERFULL);
			}
		}
		if(!(usi_status&(1<<USIMISOBUFFERFULL))){
			i*=USI_PACKET_LENGTH;
			for(int j=0;j<size;j++){
				usi_data_miso[i+j]=outputdata[start+j];
			}
			usi_data_miso_status[i]= size;
		}
	}
	if(usi_status&(1<<USIMISOBUFFEREMPTY))
	{
		USIDR=usi_data_miso[i];
		usi_data_miso[i]=0;
		if(size>1){
			usi_next_byte_miso_indicator++;
			usi_status&=~(1<<USIMISOBUFFEREMPTY);
		}
	}
}

/*-----------------------------------------------------------------ISR--------------------------------------------------------*/
ISR(TIM0_COMPA_vect)
{
	USICR|=(1<<USITC);
}

ISR(USI_OVF_vect)
#ifdef USIASMASTER
{
	PORTA|=(1<<PA0);
	//stop the timer for data handling
	TCCR0B&= 0b11111000;
	//reset the timer for next byte
	TCNT0=0;
	//determine if a packet is being sent
	if(usi_status&(1<<USIPACKETSENDING)){
		// read in the recieved byte from the slave
		usi_data_miso[usi_next_byte_miso_indicator]=USIDR;
		// write the next byte of the packet to be sent
		USIDR=usi_data_mosi[usi_next_byte_indicator];
		// increment pointer
		usi_next_byte_miso_indicator++;
		usi_next_byte_indicator++;
		usi_byte_sent++;
		// check if we have reached the last byte of the paket
		if(usi_byte_sent>=usi_data_mosi_status[usi_packet_selector]){
			usi_status&=~(1<<USIPACKETSENDING);
			usi_data_mosi_status[usi_packet_selector]=0;
			usi_next_byte_indicator=0;
		}
		//Send the next byte by starting the timer
		TCCR0B|= (1<<USICLKRANGE);
	}
	else // this is the start of the transmission of a New Packet and will have to be handled differently
	{
		PORTA|=(1<<PA1);
		USI_SS_PORT|=(1<<USI_SS1); //pull up the SS line to indicate end of last Package

		// read in the last byte of the last package to have been sent before selscting a new one
		usi_data_miso[usi_next_byte_miso_indicator]=USIDR;
		usi_data_miso_status[usi_packet_miso_selector]=usi_byte_sent;
		usi_byte_sent=0;

		//determin the next package to be sent
		usi_packet_selector=0;
		while(!usi_data_mosi_status[usi_packet_selector]){
			usi_packet_selector++;
			if(usi_packet_selector>=USI_PACKET_AMOUNT){
				usi_packet_selector=0;
				usi_status|=(1<<USIBUFFEREMPTY);
				break;
			}
		}
		//determine the size of the package to be sent
		usi_packet_size=usi_data_mosi_status[usi_packet_selector];
		usi_next_byte_indicator=usi_packet_selector*USI_PACKET_LENGTH;

		//cycle through the storage for incoming packets
		usi_packet_miso_selector= 1-usi_packet_miso_selector;
		usi_next_byte_miso_indicator=usi_packet_miso_selector*USI_PACKET_LENGTH;

		// if the buffer is not empty start writing the packet into the SPI data register
		if(!(usi_status&(1<<USIBUFFEREMPTY))){
			USIDR=usi_data_mosi[usi_next_byte_indicator];

		//if the package has more than one byte then nothing has to be done exept increment the neccesary counters and set some flags
			if(usi_packet_size > 1){
				usi_status|=(1<<USIPACKETSENDING);
				usi_byte_sent++;
				usi_next_byte_indicator++;
			}

			//if the packet has only one byte some cleaning up has to be done
			else{
				usi_data_mosi_status[usi_packet_selector]=0;
			}
			//pull low the SS line to begin the new package or keep it pulled low for the next byte
			USI_SS_PORT&=~(1<<USI_SS1); //TODO there still is no multiple Slave support
			TCCR0B|= (1<<USICLKRANGE);
		}
		PORTA&=~(1<<PA1);
	}
	//reenable the timer/counter 0;
	USISR&=0b1110000;
	PORTA&=~(1<<PA0);
}
#else /////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//check if the packet has ended (more than once)
	for(uint8_t i=0;i<10;i++){
		if((PINA&(1<<PA3))>0){
			usi_status|=(1<<USIRECIEVEDPACKET);
			break;
		}
	}
	//here comes the code for the haandeling of an entire packet (for example writing the length into the appropriate indicator and signaling that the buffer occupied)
	if(usi_status&(1<<USIRECIEVEDPACKET)){
		//write the last remaining byte to the buffer
		usi_data_mosi[usi_next_byte_indicator]=USIDR;
		usi_packet_size++;
		//indicate that the packet has been recieved
		usi_data_mosi_status[usi_packet_selector]=usi_packet_size;

		//set the miso-bufferposition of the packet that was transmitted to 0, indicating a free spot in the buffer. If there was no packet, then ther is already a zero at the Position written to.
		usi_data_miso_status[usi_packet_miso_selector]=0;

		//pick the next packet location and initialize the variables
		usi_packet_selector=0;
		while(usi_data_mosi_status[usi_packet_selector]){
			usi_packet_selector++;
			if(usi_packet_selector>=USI_PACKET_AMOUNT){
				usi_packet_selector=0;
				usi_status|=(1<<USIBUFFERFULL);
				break;
			}
		}
		// pick the next packet from the heap of the packets to send
		usi_packet_miso_selector=0;
		while(!usi_data_miso_status[usi_packet_miso_selector]){
			usi_packet_miso_selector++;
			if(usi_packet_miso_selector>=USI_PACKET_AMOUNT){
				usi_status|=(1<<USIMISOBUFFEREMPTY);
				usi_packet_miso_selector=0;
				break;
			}
		}
		// write the first byte of the first found miso packet into write register so it canbeshifted to the master
		if(!(usi_status&(1<<USIMISOBUFFEREMPTY))){
			usi_next_byte_miso_indicator=USI_PACKET_LENGTH*usi_packet_miso_selector;
			USIDR=usi_data_miso[usi_next_byte_miso_indicator];
			usi_data_miso[usi_next_byte_miso_indicator]=0;
			//set the indicator to the next field
			usi_next_byte_miso_indicator++;
		}
		else{
			usi_next_byte_miso_indicator=0;
			USIDR=0;
		}
		// if there is space in the input buffer reserve a location in the buffer for next packet
		if(!(usi_status&(1<<USIBUFFERFULL)))
			usi_next_byte_indicator=usi_packet_selector*USI_PACKET_LENGTH;

		//tidy up the variables and flags
		usi_packet_size=0;
		usi_status&=~(1<<USIRECIEVEDPACKET);
	}
	//here a packet was not recieved entirely, only one byte of the packet was recieved therefor no recalculations of the pointers have to be made	
	else{
		//simply write the next byte into the indicated field in the buffer
		usi_data_mosi[usi_next_byte_indicator]=USIDR;
		//set the indicator to the next field
		usi_next_byte_indicator++;
		//increase the packet size
		usi_packet_size++;
		//check if the miso Buffer is empty
		if(!(usi_status&(1<<USIMISOBUFFEREMPTY))){
		//write the next byte to be sent into the data register
		USIDR=usi_data_miso[usi_next_byte_miso_indicator];
		usi_data_miso[usi_next_byte_miso_indicator]=0;
		//set the indicator to the next field
		usi_next_byte_miso_indicator++;
		}
		else{
			USIDR=0;
		}
	}
	//reset counter
	USISR&=0b1110000;
}
#endif //USIASMASTER
