/*
    (c) 2016 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
*/

#include "MCP2515.h"
#include "digio.h"

//Defines for chip select
#define MCP2515_CS_HIGH()   GPIO_BSRR(CAN3_CS_PORT) = CAN3_CS_PIN;
#define MCP2515_CS_LOW()    GPIO_BSRR(CAN3_CS_PORT) = (CAN3_CS_PIN << 16);

#define SPI_TIMEOUT        10

//Static variables
static uint8_t readDummy= 0x00;
static uint8_t writeDummy = 0x00;

//Set CAN controller to config mode
void MCP2515_Initialize(void)
{
   MCP2515_CS_HIGH();
   spi_enable(SPI_CAN);
}

//Set CAN controller to config mode
void MCP2515_SetTo_ConfigMode(void)
{

   MCP2515_Write_Byte(MCP2515_CANCTRL, 0x80);

   for (uint8_t i = 0; i <= 10; i++)
   {
      if (0x80 != (MCP2515_Read_Byte(MCP2515_CANSTAT) & 0xE0)) // Wait until MCP2515 is in config mode
      {
         MCP2515_Write_Byte(MCP2515_CANCTRL, 0x80);
      }
   }


}

//Set CAN controller to normal mode
void MCP2515_SetTo_NormalMode(void)
{
   MCP2515_Write_Byte(MCP2515_CANCTRL, 0x00);

   for (uint8_t i = 0; i <= 10; i++)
   {
      if(0x00 != (MCP2515_Read_Byte(MCP2515_CANSTAT) & 0xE0)) // Wait until MCP2515  is in normal mode
      {
         MCP2515_Write_Byte(MCP2515_CANCTRL, 0x00);
      }

   }
}

//Set CAN controller to sleep mode
void MCP2515_SetTo_Sleep_Mode(void)
{
   MCP2515_Write_Byte(MCP2515_CANCTRL, 0x20);

   for (uint8_t i = 0; i <= 10; i++)
   {
      if(0x20 != (MCP2515_Read_Byte(MCP2515_CANSTAT) & 0xE0)) // Wait until MCP2515  is in normal mode
      {
         MCP2515_Write_Byte(MCP2515_CANCTRL, 0x20);
      }
   }
}

//Reset CAN controller
void MCP2515_Reset(void)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,MCP2515_RESET);
   MCP2515_CS_HIGH();
}

//Read one byte from given CAN controller address
uint8_t MCP2515_Read_Byte (uint8_t readAddress)
{
   uint8_t addressData;

   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,MCP2515_READ);
   readDummy = spi_xfer(SPI_CAN,readAddress);
   addressData = spi_xfer(SPI_CAN,writeDummy);
   MCP2515_CS_HIGH();

   return addressData;
}

//Read RX buffer using Read Rx Buffer instruction
uint8_t MCP2515_Read_RxBuffer(uint8_t readRxBuffInst)
{
   uint8_t rxBufferData;

   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,readRxBuffInst);
   rxBufferData = spi_xfer(SPI_CAN,writeDummy);
   MCP2515_CS_HIGH();

   return rxBufferData;
}

// readRxBuffInst = instruction for the first Rx buffer to read
// rxLength = number of byte addresses to read from the set rx buffer to RXBnD7
// *rxData = pointer to the first rx buffer to read
void MCP2515_Read_RxbSequence(uint8_t readRxBuffInst, uint8_t rxLength, uint8_t *rxData)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,readRxBuffInst);
   for(uint8_t i = 0; i < rxLength; i++)
   {
      *(rxData++) = spi_xfer(SPI_CAN,writeDummy);
   }
   MCP2515_CS_HIGH();
}

//Write a single byte to the given register address
//writeAddress = address where the data will be written
//writeData = data to write to writeAddress
void MCP2515_Write_Byte (uint8_t writeAddress, uint8_t writeData)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,MCP2515_WRITE);
   readDummy = spi_xfer(SPI_CAN,writeAddress);
   readDummy = spi_xfer(SPI_CAN,writeData);
   MCP2515_CS_HIGH();
}

//Write a sequence of bytes from the given startAddress to the endAddress
//startAddress = first address to write
//endAddress = last address to write
//*data = pointer to the starting address
void MCP2515_Write_ByteSequence (uint8_t startAddress, uint8_t endAddress, uint8_t *data)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,MCP2515_WRITE);
   readDummy = spi_xfer(SPI_CAN,startAddress);
   do
   {
      readDummy = spi_xfer(SPI_CAN,*(data++));
   }
   while(startAddress++ != endAddress);
   MCP2515_CS_HIGH();
}

//Write messages starting from the given txBnSIDH up to the TXBnD7
//loadtxBnSidhInst = instruction to load to a TXBnSIDH buffer
//*idReg = pointer to the address of data to load to the four tx id buffers
//dlc = data length
//*txData = pointer to the address of data to load to the 8 tx data buffers
void MCP2515_Load_TxSequence(uint8_t loadtxBnSidhInst, uint8_t *idReg, uint8_t dlc, uint8_t *txData)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,loadtxBnSidhInst);
   for(uint8_t i = 0; i < 4; i++)
   {
      readDummy = spi_xfer(SPI_CAN,*(idReg++));      // Write id from the four id registers
   }
   readDummy = spi_xfer(SPI_CAN,dlc);
   for(uint8_t i = 0; i < 8; i++)
   {
      readDummy = spi_xfer(SPI_CAN,*(txData++));     // Write 8 data bytes
   }
   MCP2515_CS_HIGH();
}

//Write one byte to a tx buffer
//loadTxBuffInst = instruction to load to a TXBnSIDH or TXBnD0 buffer
//loadTxData = data to load to the given buffer
void MCP2515_Load_TxBuffer (uint8_t loadTxBuffInst, uint8_t txBufferData)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,loadTxBuffInst);
   readDummy = spi_xfer(SPI_CAN,txBufferData);
   MCP2515_CS_HIGH();
}

//Message request to send using the RTS instruction for a TX buffer
void MCP2515_RequestToSend (uint8_t rtsTxBuffInst)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,rtsTxBuffInst);
   MCP2515_CS_HIGH();
}

//Returns the controller status using the READ STATUS instruction
uint8_t MCP2515_Read_Status (void)
{
   uint8_t ctrlStat;

   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,MCP2515_READ_STATUS);
   ctrlStat = spi_xfer(SPI_CAN,writeDummy);
   MCP2515_CS_HIGH();

   return ctrlStat;
}

//Returns the controller RX status using the RX STATUS instruction
uint8_t MCP2515_Get_RxStatus (void)
{
   uint8_t rxStatus;

   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,MCP2515_RX_STATUS);
   rxStatus = spi_xfer(SPI_CAN,writeDummy);
   MCP2515_CS_HIGH();

   return rxStatus;
}

//Modify one bit from the given register
//regAddress = address of the register that contains the bit to modify
void MCP2515_Bit_Modify (uint8_t regAddress, uint8_t maskByte, uint8_t dataByte)
{
   MCP2515_CS_LOW();
   readDummy = spi_xfer(SPI_CAN,MCP2515_BIT_MOD);
   readDummy = spi_xfer(SPI_CAN,regAddress);
   readDummy = spi_xfer(SPI_CAN,maskByte);
   readDummy = spi_xfer(SPI_CAN,dataByte);
   MCP2515_CS_HIGH();
}

// From MCP2515 Datasheet
// --------------------------------------------------
// RX0BF and RX1BF Pins CONFIGURED AS DIGITAL OUTPUT
// --------------------------------------------------
// When used as digital outputs, the BnBFM bits
// (BFPCTRL[1:0]) must be cleared and the BnBFE bits
// (BFPCTRL[3:2]) must be set for the associated buffer.
// In this mode, the state of the pin is controlled by the
// BnBFS bits (BFPCTRL[5:4]), and should be modified only 
// by using the SPI BIT MODIFY command to prevent glitches
// from occurring on either of the buffer full pins.
// The RXnBF pins can be disabled to the high-impedance
// state by clearing the BnBFE bits (BFPCTRL[3:2]).


// Enable RXBn pin, or set to HiZ
void MCP2515_PinEn(uint8_t pin, bool enable) {
    uint8_t BnFMbit = (pin == 0) ?  BFPCTRL_B0BFM:BFPCTRL_B1BFM ;
    uint8_t BnFEbit = (pin == 0) ?  BFPCTRL_B0BFE:BFPCTRL_B1BFE ;

    if (enable) {
        MCP2515_Bit_Modify(MCP2515_BFPCTRL, BnFMbit, 0x00);
        MCP2515_Bit_Modify(MCP2515_BFPCTRL, BnFEbit, 0xFF);
    } else {
        MCP2515_Bit_Modify(MCP2515_BFPCTRL, BnFEbit, 0x00);
    }
}

// set/clear an enabled RXBn output pin 
void MCP2515_Out_Pin(uint8_t pin, bool state) {
   uint8_t BnBFSbit = (pin == 0) ? BFPCTRL_B0BFS : BFPCTRL_B1BFS;
   MCP2515_Bit_Modify(MCP2515_BFPCTRL, BnBFSbit, state ? 0xFF : 0x00);
}
