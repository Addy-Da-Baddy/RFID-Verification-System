#include <LPC17xx.h>
#include <stdio.h>
#include <string.h>

#define SCK  (1 << 7)
#define MISO (1 << 8)
#define MOSI (1 << 9)
#define SSEL (1 << 6)

#define LCD_RS  (1 << 23)
#define LCD_EN  (1 << 24)
#define LCD_D4  (1 << 25)
#define LCD_D5  (1 << 26)
#define LCD_D6  (1 << 27)
#define LCD_D7  (1 << 28)

#define BUTTON (1 << 10)

// Extras
#define RC522_RST   (1 << 0)   // P0.0
#define LED_STATUS  (1 << 18)  // P1.18

// MFRC522 Registers
#define CommandReg     0x01
#define CommIEnReg     0x02
#define CommIrqReg     0x04
#define ErrorReg       0x06
#define Status2Reg     0x08
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define ControlReg     0x0C
#define BitFramingReg  0x0D
#define ModeReg        0x11
#define TxControlReg   0x14
#define TxAutoReg      0x15
#define TModeReg       0x2A
#define TPrescalerReg  0x2B
#define TReloadRegH    0x2C
#define TReloadRegL    0x2D

// MFRC522 Commands
#define PCD_IDLE        0x00
#define PCD_RESETPHASE  0x0F
#define PCD_TRANSCEIVE  0x0C
#define PICC_REQIDL     0x26
#define PICC_ANTICOLL   0x93

#define MI_OK        1
#define MI_ERR       0
#define MI_NOTAGERR  2
#define MAX_LEN      16

void delay_ms(uint32_t ms) {
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 2500; j++) {
            // simple busy-wait
        }
    }
}

/* ---------------- LCD Functions ---------------- */

void lcd_pulse(void) {
    LPC_GPIO0->FIOSET = LCD_EN;
    delay_ms(2);
    LPC_GPIO0->FIOCLR = LCD_EN;
}

void lcd_send_nibble(uint8_t nib) {
    LPC_GPIO0->FIOCLR = LCD_D4 | LCD_D5 | LCD_D6 | LCD_D7;
    if (nib & 0x01) LPC_GPIO0->FIOSET = LCD_D4;
    if (nib & 0x02) LPC_GPIO0->FIOSET = LCD_D5;
    if (nib & 0x04) LPC_GPIO0->FIOSET = LCD_D6;
    if (nib & 0x08) LPC_GPIO0->FIOSET = LCD_D7;
    lcd_pulse();
}

void lcd_cmd(uint8_t cmd) {
    LPC_GPIO0->FIOCLR = LCD_RS;
    lcd_send_nibble(cmd >> 4);
    lcd_send_nibble(cmd & 0x0F);
    delay_ms(2);
}

void lcd_data(uint8_t data) {
    LPC_GPIO0->FIOSET = LCD_RS;
    lcd_send_nibble(data >> 4);
    lcd_send_nibble(data & 0x0F);
    delay_ms(2);
}

void lcd_init(void) {
    LPC_PINCON->PINSEL1 &= ~((3<<14)|(3<<16)|(3<<18)|(3<<20)|(3<<22)|(3<<24));
    LPC_GPIO0->FIODIR |= LCD_RS | LCD_EN | LCD_D4 | LCD_D5 | LCD_D6 | LCD_D7;

    delay_ms(20);
    lcd_cmd(0x02);
    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
}

void lcd_clear(void) {
    lcd_cmd(0x01);
    delay_ms(2);
}

void lcd_print(char *str) {
    while (*str) lcd_data(*str++);
}

void lcd_gotoxy(uint8_t x, uint8_t y) {
    uint8_t addr = (y == 0) ? 0x80 + x : 0xC0 + x;
    lcd_cmd(addr);
}

/* ---------------- SPI & RFID ---------------- */

void SPI_INIT(void) {
    // Power up SSP1 (used with P0.7/0.8/0.9)
    LPC_SC->PCONP |= (1 << 10);
    LPC_PINCON->PINSEL0 |= (0x02 << 14) | (0x02 << 16) | (0x02 << 18);
    LPC_GPIO0->FIODIR |= SSEL;
    LPC_GPIO0->FIOSET = SSEL;
    LPC_SSP1->CR0 = 0x0707;
    LPC_SSP1->CPSR = 8;
    LPC_SSP1->CR1 = 0x02;
}

uint8_t SPI_Transfer(uint8_t data) {
    // Wait for TX FIFO not full
    while (!(LPC_SSP1->SR & (1 << 1)));
    LPC_SSP1->DR = data;
    // Wait until not busy and RX FIFO has data
    while (LPC_SSP1->SR & (1 << 4));
    while (!(LPC_SSP1->SR & (1 << 2)));
    return (uint8_t)LPC_SSP1->DR;
}

void CS_LOW(void)  { LPC_GPIO0->FIOCLR = SSEL; }
void CS_HIGH(void) { LPC_GPIO0->FIOSET = SSEL; }

void RFID_WriteReg(uint8_t reg, uint8_t val) {
    CS_LOW();
    SPI_Transfer((reg << 1) & 0x7E);
    SPI_Transfer(val);
    CS_HIGH();
}

uint8_t RFID_ReadReg(uint8_t reg) {
    uint8_t val;
    CS_LOW();
    SPI_Transfer(((reg << 1) & 0x7E) | 0x80);
    val = SPI_Transfer(0x00);
    CS_HIGH();
    return val;
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp & (~mask));
}

void RFID_AntennaOn(void) {
    uint8_t val = RFID_ReadReg(TxControlReg);
    if (!(val & 0x03)) {
        RFID_WriteReg(TxControlReg, val | 0x03);
    }
}

void RFID_Init(void) {
    // Configure and toggle RC522 RST (P0.0)
    LPC_PINCON->PINSEL0 &= ~(3 << 0); // P0.0 as GPIO
    LPC_GPIO0->FIODIR |= RC522_RST;
    LPC_GPIO0->FIOCLR = RC522_RST;
    delay_ms(10);
    LPC_GPIO0->FIOSET = RC522_RST;
    delay_ms(10);

    RFID_WriteReg(CommandReg, PCD_RESETPHASE);
    delay_ms(50);
    RFID_WriteReg(TModeReg, 0x8D);
    RFID_WriteReg(TPrescalerReg, 0x3E);
    RFID_WriteReg(TReloadRegL, 0x1E);
    RFID_WriteReg(TReloadRegH, 0);
    RFID_WriteReg(TxAutoReg, 0x40);
    RFID_WriteReg(ModeReg, 0x3D);
    RFID_AntennaOn();
}

uint8_t RFID_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint32_t *backLen) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    RFID_WriteReg(CommIEnReg, irqEn | 0x80);
    RFID_WriteReg(CommIrqReg, 0x7F);
    RFID_WriteReg(FIFOLevelReg, 0x80);
    RFID_WriteReg(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++) {
        RFID_WriteReg(FIFODataReg, sendData[i]);
    }

    RFID_WriteReg(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        MFRC522_SetBitMask(BitFramingReg, 0x80);
    }

    i = 2000;
    do {
        n = RFID_ReadReg(CommIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    MFRC522_ClearBitMask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(RFID_ReadReg(ErrorReg) & 0x1B)) {
            status = MI_OK;
            if (command == PCD_TRANSCEIVE) {
                n = RFID_ReadReg(FIFOLevelReg);
                lastBits = RFID_ReadReg(ControlReg) & 0x07;
                *backLen = lastBits ? (n - 1) * 8 + lastBits : n * 8;
                if (n > MAX_LEN) n = MAX_LEN;
                for (i = 0; i < n; i++) backData[i] = RFID_ReadReg(FIFODataReg);
            }
        }
    }
    return status;
}

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *tagType) {
    uint8_t status;
    uint32_t backBits;
    RFID_WriteReg(BitFramingReg, 0x07);
    tagType[0] = reqMode;
    status = RFID_ToCard(PCD_TRANSCEIVE, tagType, 1, tagType, &backBits);
    if ((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }
    return status;
}

uint8_t MFRC522_Anticoll(uint8_t *serNum) {
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint32_t unLen;
    RFID_WriteReg(BitFramingReg, 0x00);
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = RFID_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);
    if (status == MI_OK) {
        for (i = 0; i < 4; i++) serNumCheck ^= serNum[i];
        if (serNumCheck != serNum[4]) status = MI_ERR;
    }
    return status;
}

/* ---------------- MAIN ---------------- */

int main(void) {
    SystemInit();
    lcd_init();
    SPI_INIT();
    RFID_Init();

    // Button input on P0.10, ensure GPIO function and pull-up enabled
    LPC_PINCON->PINSEL0 &= ~(3 << 20);
    LPC_PINCON->PINMODE0 &= ~(3 << 20); // 00: pull-up
    LPC_GPIO0->FIODIR &= ~BUTTON; // Button input

    // Status LED on P1.18
    LPC_PINCON->PINSEL3 &= ~(3 << 4); // P1.18 as GPIO
    LPC_GPIO1->FIODIR |= LED_STATUS;
    LPC_GPIO1->FIOCLR = LED_STATUS;

    uint8_t tagType[2], serNum[5];
    uint8_t status;
    uint8_t registeredUIDs[10][5];
    uint8_t regCount = 0;
    uint8_t mode = 0; // 0 = Verify, 1 = Register

    lcd_clear();
    lcd_print("RFID System Ready");
    delay_ms(1000);

    while (1) {
        // Mode toggle
        if (!(LPC_GPIO0->FIOPIN & BUTTON)) {
            mode = !mode;
            lcd_clear();
            if (mode) lcd_print("MODE: REGISTER");
            else lcd_print("MODE: VERIFY");
            delay_ms(500);
        }

        status = MFRC522_Request(PICC_REQIDL, tagType);
        if (status == MI_OK) {
            status = MFRC522_Anticoll(serNum);
            if (status == MI_OK) {
                int found = 0;
                for (int i = 0; i < regCount; i++) {
                    if (memcmp(registeredUIDs[i], serNum, 5) == 0) {
                        found = 1;
                        break;
                    }
                }

                lcd_clear();
                if (mode) { // REGISTER MODE
                    if (!found && regCount < 10) {
                        memcpy(registeredUIDs[regCount++], serNum, 5);
                        lcd_print("Tag Registered");
                    } else if (found) {
                        lcd_print("Already Exists");
                    } else {
                        lcd_print("Memory Full");
                    }
                } else { // VERIFY MODE
                    if (found) {
                        lcd_print("Access Granted");
                        LPC_GPIO1->FIOSET = LED_STATUS;
                    } else {
                        lcd_print("Access Denied");
                        LPC_GPIO1->FIOCLR = LED_STATUS;
                    }
                }
                delay_ms(1500);
            }
        }
        delay_ms(500);
    }
}
/* End of RFID_Verify.c */