/*
 * Copyright (c) 2013, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/*
 *  ======== UARTUtils.c ========
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>

/* TI-RTOS Header files */
#include <ti/drivers/UART.h>

/* Example/Board Header files */
#include "Board.h"
#include "UARTUtils.h"

#include <stdio.h>
#include <file.h>
#include <string.h>

#define NUM_PORTS 2

/* Typedefs */
typedef struct {
    UART_Handle handle; /* Handle for all UART APIs */
    UInt        base;   /* Base address of the UART */
    UInt        open;   /* Number of users for this UART */
    Bool        binary; /* UART has been opened in binary mode */
} UARTPorts;

/* Static variables and handles */
static UART_Handle systemHandle = NULL;
static UART_Handle loggerHandle = NULL;

/* This example only uses UART0 */
static UARTPorts ports[NUM_PORTS] = {{NULL, Board_UART0, 0, FALSE}, {NULL, Board_UART1, 0, FALSE}};

/*
 *  ======== openHandle ========
 *  The UART driver will return NULL if there was an error creating a UART 
 *
 *  @param  index  Index into the ports array of UARTPorts
 *  @param  binary Open the UART in binary mode
 *  @return        UART_Handle to the opened UART
 */
static UART_Handle openHandle(Int index, Bool binary)
{
    UART_Params uartParams;
    
    /* Only UART 0 is supported in this example. */
    if (index >= NUM_PORTS) {
        System_printf("UART index %d not supported, valid range is 0-%d", index, (NUM_PORTS - 1));
        return (NULL);
    }

    /* The UART driver only allows creating once, return if its already open. */
    if (ports[index].open) {
        /* Make sure the index is not already opened in the wrong mode */
        if (binary != ports[index].binary) {
            return (NULL);
        }
        ports[index].open++;
        return (ports[index].handle);
    }

    /* Create a UART with the parameters below. */
    UART_Params_init(&uartParams);
    if (binary == TRUE) {
        uartParams.readEcho = UART_ECHO_OFF;
        uartParams.writeDataMode = UART_DATA_BINARY;
        ports[index].binary = TRUE;
    }
    else {
        ports[index].binary = FALSE;
        uartParams.baudRate = 115200;
    }
    ports[index].handle = UART_open(ports[index].base, &uartParams);
    if (ports[index].handle != NULL) {
        ports[index].open = 1;
    }
    
    return (ports[index].handle);
}

/*
 *  ======== closeHandle ========
 */
static Void closeHandle(Int index)
{
    ports[index].open--;
    if (ports[index].open == 0) {
        UART_close(ports[index].handle);
    }
}

/*
 *  ======== UARTUtils_loggerIdleInit ========
 */
Void UARTUtils_loggerIdleInit(Int index)
{
    Assert_isTrue(ports[index].open == FALSE, NULL);
    loggerHandle = openHandle(index, TRUE);
    if (loggerHandle == NULL) {
        System_printf("Failed to open UART %d", index);
    }
}

/*
 *  ======== UARTUtils_loggerIdleSend ========
 *  Plugged into LoggerIdle to send log data during idle.
 */
Int UARTUtils_loggerIdleSend(UChar *a, Int size)
{    
    /* Make sure UART is initialized */    
    if (loggerHandle) {
        /* 
         * Write up to 16 bytes at a time.  This function runs during idle and
         * should not tie up other idle functions from running.  The idle loop
         * is generally short enough that this function will run again before all
         * 16 bytes have been transmitted from the FIFO.
         */
        if (size < 16) {
            return (UART_writePolling(loggerHandle, (Char *)a, size));
        }
        else {
            return (UART_writePolling(loggerHandle, (Char *)a, 16));
        }
    }
    else {
        return (0);
    }
}

/*
 *  ======== UARTUtils_deviceclose ========
 */
int UARTUtils_deviceclose(int fd)
{
    /* Return if a UART other than UART 0 was specified. */
    if (fd != 0) {
        return (-1);
    }

    closeHandle(fd);

    return (0);
}

/*
 *  ======== UARTUtils_devicelseek ========
 */
off_t UARTUtils_devicelseek(int fd, off_t offset, int origin)
{
    return (-1);
}

/*
 *  ======== UARTUtils_deviceopen ========
 */
int UARTUtils_deviceopen(const char *path, unsigned flags, int mode)
{
    Int fd;
    UART_Handle handle;
    

    /* Get the UART specified for opening. */
    fd = path[0] - '0';   

    handle = openHandle(fd, FALSE);
    
    if(handle == NULL) {
        return (-1);
    }
    else {        
        return (fd);
    }
}

/*
 *  ======== UARTUtils_deviceread ========
 */
int UARTUtils_deviceread(int fd, char *buffer, unsigned size)
{
    Int ret;

    /* Return if a UART other than UART 0 was specified. */
   // if (fd != 0) {
   //     return (-1);
   // }

    /* Read character from the UART and block until a newline is received. */
    ret = UART_read(ports[fd].handle, buffer, size);

    return (ret);
}

/*
 *  ======== UARTUtils_devicewrite ========
 */
int UARTUtils_devicewrite(int fd, const char *buffer, unsigned size)
{
    Int ret;

    /* Return if a UART other than UART 0 was specified. */
    //if (fd != 0) {
   //     return (-1);
   // }

    /* Write to the UART and block until the transfer is finished. */
    ret = UART_write(ports[fd].handle, buffer, size);

    return (ret);
}

/*
 *  ======== UARTUtils_deviceunlink ========
 */
int UARTUtils_deviceunlink(const char *path)
{
    return (-1);
}

/*
 *  ======== UARTUtils_devicerename ========
 */
int UARTUtils_devicerename(const char *old_name, const char *new_name)
{
    return (-1);
}

/*
 *  ======== UARTUtils_systemAbort ========
 */
Void UARTUtils_systemAbort(String str)
{
    /* Make sure UART is initialized */
    if (systemHandle) {
        UART_writePolling(systemHandle, str, strlen(str));
    }
}

/*
 *  ======== UARTUtils_systemInit ========
 */
Void UARTUtils_systemInit(Int index)
{
    systemHandle = openHandle(index, FALSE);
    if (systemHandle == NULL) {
        Log_print1(Diags_USER1, "Failed to open UART %d", index);
    }
}

/*
 *  ======== UARTUtils_systemPutch ========
 */
Void UARTUtils_systemPutch(Char a)
{    
    /* Make sure UART is initialized */
    if (systemHandle) {
        UART_writePolling(systemHandle, &a, 1);
    }
}

/*
 *  ======== UARTUtils_systemReady ========
 */
Bool UARTUtils_systemReady()
{    
    return (systemHandle ? TRUE : FALSE);    
}
