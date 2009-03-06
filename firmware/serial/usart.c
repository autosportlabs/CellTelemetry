/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support 
 * ----------------------------------------------------------------------------
 * Copyright (c) 2008, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

//------------------------------------------------------------------------------
//         Headers
//------------------------------------------------------------------------------

#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"


#define USART_INTERRUPT_LEVEL 5
#define USART_QUEUE_LENGTH 100

void usart0_irq_handler (void);
void usart1_irq_handler (void);

xQueueHandle xUsart0Tx; 
xQueueHandle xUsart0Rx;
xQueueHandle xUsart1Tx; 
xQueueHandle xUsart1Rx;

int initSerial()
{
	if (!initQueues()) return 0;

	unsigned int mode =
                        AT91C_US_CLKS_CLOCK
                        | AT91C_US_CHRL_8_BITS
                        | AT91C_US_PAR_NONE
                        | AT91C_US_NBSTOP_1_BIT
                        | AT91C_US_CHMODE_NORMAL;

	initUsart0(mode, 115200);
	initUsart1(mode, 38400);
	return 1;
}

int initQueues(){

	int success = 1;
	
	/* Create the queues used to hold Rx and Tx characters. */
	xUsart0Rx = xQueueCreate( USART_QUEUE_LENGTH, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
	xUsart0Tx = xQueueCreate( USART_QUEUE_LENGTH + 1, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
	xUsart1Rx = xQueueCreate( USART_QUEUE_LENGTH, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
	xUsart1Tx = xQueueCreate( USART_QUEUE_LENGTH + 1, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );

	if (xUsart0Rx == NULL ||
		xUsart1Rx == NULL ||
		xUsart0Rx == NULL ||
		xUsart0Rx == NULL
		) success = 0;
	
	return success;
}


unsigned int getRx1QueueWaiting(){
	return uxQueueMessagesWaiting(xUsart1Rx);	
}

void initUsart0(unsigned int mode, unsigned int baud){

	//Enable USART0
	
 	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			AT91C_PA6_TXD0, 				// mux function A
			0); // mux funtion B

	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			AT91C_PA5_RXD0, 				// mux function A
			0); // mux funtion B  

    // Configure the USART in the desired mode @115200 bauds
    USART_Configure(AT91C_BASE_US0, mode, baud, BOARD_MCK);

    // Enable receiver & transmitter
    AT91C_BASE_US0->US_CR = AT91C_US_TXEN;
	AT91C_BASE_US0->US_CR = AT91C_US_RXEN;

	/* Enable the Rx interrupts.  The Tx interrupts are not enabled
	until there are characters to be transmitted. */
	AT91F_US_EnableIt( AT91C_BASE_US0, AT91C_US_RXRDY );
	
	/* Enable the interrupts in the AIC. */
	AT91F_AIC_ConfigureIt( AT91C_BASE_AIC, AT91C_ID_US0, USART_INTERRUPT_LEVEL, AT91C_AIC_SRCTYPE_EXT_LOW_LEVEL, usart0_irq_handler );
	AT91F_AIC_EnableIt( AT91C_BASE_AIC, AT91C_ID_US0 );
	
}

void initUsart1(unsigned int mode, unsigned int baud){
	
 	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			AT91C_PA22_TXD1, 				// mux function A
			0); // mux funtion B

	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			AT91C_PA21_RXD1, 				// mux function A
			0); // mux funtion B  

    // Configure the USART in the desired mode @115200 bauds
    USART_Configure(AT91C_BASE_US1, mode, baud, BOARD_MCK);

    // Enable receiver & transmitter
    AT91C_BASE_US1->US_CR = AT91C_US_TXEN;
	AT91C_BASE_US1->US_CR = AT91C_US_RXEN;

	/* Enable the Rx interrupts.  The Tx interrupts are not enabled
	until there are characters to be transmitted. */
	AT91F_US_EnableIt( AT91C_BASE_US1, AT91C_US_RXRDY );
	
	/* Enable the interrupts in the AIC. */
	AT91F_AIC_ConfigureIt( AT91C_BASE_AIC, AT91C_ID_US1, USART_INTERRUPT_LEVEL, AT91C_AIC_SRCTYPE_EXT_LOW_LEVEL, usart1_irq_handler );
	AT91F_AIC_EnableIt( AT91C_BASE_AIC, AT91C_ID_US1 );
}

//------------------------------------------------------------------------------
//         Exported functions
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
/// Configures an USART peripheral with the specified parameters.
/// \param usart  Pointer to the USART peripheral to configure.
/// \param mode  Desired value for the USART mode register (see the datasheet).
/// \param baudrate  Baudrate at which the USART should operate (in Hz).
/// \param masterClock  Frequency of the system master clock (in Hz).
//------------------------------------------------------------------------------
void USART_Configure(AT91S_USART *usart,
                            unsigned int mode,
                            unsigned int baudrate,
                            unsigned int masterClock)
{
    // Reset and disable receiver & transmitter
    usart->US_CR = AT91C_US_RSTRX | AT91C_US_RSTTX
                   | AT91C_US_RXDIS | AT91C_US_TXDIS;

    // Configure mode
    usart->US_MR = mode;

    // Configure baudrate
    // Asynchronous, no oversampling
    if (((mode & AT91C_US_SYNC) == 0)
        && ((mode & AT91C_US_OVER) == 0)) {
    
        usart->US_BRGR = (masterClock / baudrate) / 16;
    }
    // TODO other modes


}

char usart0_getchar()
{
	char rx;
	
	/* Get the next character from the buffer.  Return false if no characters
	are available, or arrive before xBlockTime expires. */
	xQueueReceive( xUsart0Rx, &rx, portMAX_DELAY );
	return rx;
}

char usart1_getchar()
{
	char rx;
	
	/* Get the next character from the buffer.  Return false if no characters
	are available, or arrive before xBlockTime expires. */
	xQueueReceive( xUsart1Rx, &rx, portMAX_DELAY );
	return rx;
}

void usart0_putchar(char c){
	xQueueSend( xUsart0Tx, &c, portMAX_DELAY );
	//Enable transmitter interrupt
	AT91F_US_EnableIt( AT91C_BASE_US0, AT91C_US_TXRDY | AT91C_US_RXRDY );
}

void usart1_putchar(char c){
	xQueueSend( xUsart1Tx, &c, portMAX_DELAY );
	//Enable transmitter interrupt
	AT91F_US_EnableIt( AT91C_BASE_US1, AT91C_US_TXRDY | AT91C_US_RXRDY );
}

int usart0_puts (char* s )
{
	while ( *s ) usart0_putchar(*s++ );
	return 0;
}

int usart1_puts (char* s )
{
	while ( *s ) usart1_putchar(*s++ );
	return 0;
}


int usart0_readLine(char *s, int len)
{
	int count = 0;
	while(count < len - 1){
		int c = usart0_getchar();
		*s++ = c;
		count++;
		if (c == '\n') break;
	}
	*s = '\0';
	return count;
}

int usart1_readLine(char *s, int len)
{
	int count = 0;
	while(count < len - 1){
		int c = usart1_getchar();
		*s++ = c;
		count++;
		if (c == '\n') break;
	}
	*s = '\0';
	return count;
}
