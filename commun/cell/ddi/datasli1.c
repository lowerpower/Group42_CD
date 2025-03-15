
//
// Data Slicer Demo, Hacked up By G-Man of Group 42
//

/***************************************************************************

COM Port Documentation
1st  2nd  3rd  4th  Offs. DLAB  Register
-----------------------------------------------------------------------------
3F8h 2F8h 3E8h 2E8h  +0     0   RBR  Receive Buffer Register (read only) or
                                THR  Transmitter Holding Register (write only)
3F9h 2F9h 3E9h 2E9h  +1     0   IER  Interrupt Enable Register
3F8h 2F8h 3E8h 2E8h  +0     1   DL   Divisor Latch (LSB)  These registers can
3F9h 2F9h 3E9h 2E9h  +1     1   DL   Divisor Latch (MSB)  be accessed as word
3FAh 2FAh 3EAh 2EAh  +2     x   IIR  Interrupt Identification Register (r/o) or
                                FCR  FIFO Control Register (w/o, 16550+ only)
3FBh 2FBh 3EBh 2EBh  +3     x   LCR  Line Control Register
3FCh 2FCh 3ECh 2ECh  +4     x   MCR  Modem Control Register
3FDh 2FDh 3EDh 2EDh  +5     x   LSR  Line Status Register
3FEh 2FEh 3EEh 2EEh  +6     x   MSR  Modem Status Register
3FFh 2FFh 3EFh 2EFh  +7     x   SCR  Scratch Register (16450+ and some 8250s,
                                     special use with some boards)


           80h      40h      20h      10h      08h      04h      02h      01h
Register  Bit 7    Bit 6    Bit 5    Bit 4    Bit 3    Bit 2    Bit 1    Bit 0
-------------------------------------------------------------------------------
IER         0        0        0        0      EDSSI    ELSI     ETBEI    ERBFI
IIR (r/o) FIFO en  FIFO en    0        0      IID2     IID1     IID0    pending
FCR (w/o)  - RX trigger -     0        0      DMA sel  XFres    RFres   enable
LCR       DLAB     SBR    stick par  even sel Par en  stopbits  - word length -
MCR         0        0       AFE     Loop     OUT2     OUT1     RTS     DTR
LSR       FIFOerr  TEMT     THRE     Break    FE       PE       OE      RBF
MSR       DCD      RI       DSR      CTS      DDCD     TERI     DDSR    DCTS

****************************************************************************/

#include <stdio.h>

#define     COM1        0x3f8
#define     COM2        0X2F8
#define     COM3        0x3e8
#define     COM4        0x2e8

#define     MCR         4
#define     MSR         6

main()
{
    int     base;
    int     data_in;

    //
    // Lets use COM1 in this example.
    //
    base = COM1;

    //
    // Turn data slicer interface on.
    //
    outp(base+MCR,0x01);      // Set DTR 1 DSR 0 to power up interface

    //
    // Loop forever, polling the DSR to read the input state of the
    //  Data Slicer,  Press ctrl-break to exit loop.
    //
    while(1)
    {
        //
        // Read in MSR and check to see if DSR is lit
        //
        if(inp(base+MSR) & 0x20)
            printf("1\n");
        else
            printf("0\n");
    }
}


