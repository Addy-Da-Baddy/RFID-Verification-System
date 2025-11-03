#include "LPC17xx.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ----------- LCD / Keypad Globals ----------- */
int temp1, temp2;
int flag_command;
int i, j, r;
signed int row, col;
unsigned int flag;

unsigned int MatrixMap[4][3] = {
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9},
    {10, 0, 11}
};

/* ----------- LED ----------- */
#define LED_PORT LPC_GPIO0
#define LED_PIN  (1 << 22)

/* ----------- RFID (MFRC522) Defines ----------- */
#define SCK  (1 << 7)
#define MISO (1 << 8)
#define MOSI (1 << 9)
#define SSEL (1 << 6)

#define CommandReg         0x01
#define ModeReg            0x11
#define TxControlReg       0x14
#define TxAutoReg          0x15
#define TModeReg           0x2A
#define TPrescalerReg      0x2B
#define TReloadRegH        0x2C
#define TReloadRegL        0x2D
#define CommIEnReg         0x02
#define BitFramingReg      0x0D
#define FIFODataReg        0x09
#define FIFOLevelReg       0x0A
#define CommIrqReg         0x04
#define ErrorReg           0x06
#define ControlReg         0x0C

#define PCD_IDLE           0x00
#define PCD_RESETPHASE     0x0F
#define PCD_TRANSCEIVE     0x0C

#define PICC_REQIDL        0x26
#define PICC_ANTICOLL      0x93

#define MI_OK              1
#define MI_ERR             0
#define MI_NOTAGERR        2
#define MAX_LEN            16

/* ----------- UID DB ----------- */
int uid_count_rfid;
unsigned char uid_database_rfid[20][5];

/* ----------- Function Declarations ----------- */
void delay(int r1);
void lcd_init(void);
void lcd_cmd(unsigned char cmd);
void lcd_data(unsigned char data);
void lcd_string(unsigned char *str);
void lcd_clear(void);
void port_wr(void);
void lcd_wr(void);

void scan(void);
int get_first_row_key(void);

void SPI_INIT(void);
unsigned char SPI_Transfer(unsigned char data);
void CS_LOW(void);
void CS_HIGH(void);
void RFID_WriteReg(unsigned char reg, unsigned char val);
unsigned char RFID_ReadReg(unsigned char reg);
void MFRC522_SetBitMask(unsigned char reg, unsigned char mask);
void MFRC522_ClearBitMask(unsigned char reg, unsigned char mask);
void RFID_AntennaOn(void);
void RFID_Init(void);
unsigned char RFID_ToCard(unsigned char command, unsigned char *sendData, unsigned char sendLen, unsigned char *backData, unsigned int *backLen);
unsigned char MFRC522_Request(unsigned char reqMode, unsigned char *tagType);
unsigned char MFRC522_Anticoll(unsigned char *serNum);

void show_uid_on_lcd(unsigned char *serNum);
int find_uid_rfid(unsigned char *serNum);
void register_uid_rfid(void);
void verify_uid_rfid(void);

void delay_ms(uint32_t ms);

/* ----------- LCD Implementation ----------- */
void port_wr(void)
{
    int jj;
    LPC_GPIO0->FIOPIN = temp2 << 23;
    if (flag_command == 0)
        LPC_GPIO0->FIOCLR = 1 << 27;
    else
        LPC_GPIO0->FIOSET = 1 << 27;

    LPC_GPIO0->FIOSET = 1 << 28;
    for (jj = 0; jj < 50; jj++);
    LPC_GPIO0->FIOCLR = 1 << 28;
    for (jj = 0; jj < 10000; jj++);
}

void lcd_wr(void)
{
    temp2 = (temp1 >> 4) & 0xF;
    port_wr();
    temp2 = temp1 & 0xF;
    port_wr();
}

void delay(int r1)
{
    for (r = 0; r < r1; r++);
}

void lcd_cmd(unsigned char cmd)
{
    flag_command = 0;
    temp1 = cmd;
    lcd_wr();
    delay(30000);
}

void lcd_data(unsigned char data)
{
    flag_command = 1;
    temp1 = data;
    lcd_wr();
    delay(30000);
}

void lcd_string(unsigned char *str)
{
    i = 0;
    flag_command = 1;
    while (str[i] != '\0') {
        temp1 = str[i];
        lcd_wr();
        delay(10000);
        i++;
    }
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
    delay(50000);
}

void lcd_init(void)
{
    int ii;
    int command_init[9] = {3, 3, 3, 2, 2, 0x28, 0x0C, 0x06, 0x01};
    flag_command = 0;
    for (ii = 0; ii < 9; ii++) {
        temp1 = command_init[ii];
        lcd_wr();
        delay(30000);
    }
}

/* ----------- Keypad ----------- */
void scan(void)
{
    unsigned long temp3;
    temp3 = LPC_GPIO1->FIOPIN & 0x03800000;
    flag = 0;
    if (temp3 != 0x00000000) {
        flag = 1;
        if (temp3 == (1 << 23)) col = 0;
        else if (temp3 == (1 << 24)) col = 1;
        else if (temp3 == (1 << 25)) col = 2;
    }
}

int get_first_row_key(void)
{
    int found_key = -1;
    unsigned int col_copy;
    while (1) {
        LPC_GPIO2->FIOPIN = (1 << 10);
        delay(1000);
        scan();
        if (flag == 1) {
            col_copy = col;
            found_key = MatrixMap[0][col_copy];
            while (1) {
                LPC_GPIO2->FIOPIN = (1 << 10);
                delay(1000);
                scan();
                if (!flag) break;
            }
            delay(50000);
            return found_key;
        }
    }
}

/* ----------- SPI & RFID ----------- */
void SPI_INIT(void)
{
    LPC_SC->PCONP |= (1 << 21);
    LPC_PINCON->PINSEL0 |= (0x02 << 14) | (0x02 << 16) | (0x02 << 18);
    LPC_GPIO0->FIODIR |= SSEL;
    LPC_GPIO0->FIOSET = SSEL;
    LPC_SSP1->CR0 = 0x0707;
    LPC_SSP1->CPSR = 8;
    LPC_SSP1->CR1 = 0x02;
}

unsigned char SPI_Transfer(unsigned char data)
{
    LPC_SSP1->DR = data;
    while (LPC_SSP1->SR & (1 << 4));
    return (unsigned char)LPC_SSP1->DR;
}

void CS_LOW(void)  { LPC_GPIO0->FIOCLR = SSEL; }
void CS_HIGH(void) { LPC_GPIO0->FIOSET = SSEL; }

void RFID_WriteReg(unsigned char reg, unsigned char val)
{
    CS_LOW();
    SPI_Transfer((reg << 1) & 0x7E);
    SPI_Transfer(val);
    CS_HIGH();
}

unsigned char RFID_ReadReg(unsigned char reg)
{
    unsigned char val;
    CS_LOW();
    SPI_Transfer(((reg << 1) & 0x7E) | 0x80);
    val = SPI_Transfer(0x00);
    CS_HIGH();
    return val;
}

void MFRC522_SetBitMask(unsigned char reg, unsigned char mask)
{
    unsigned char tmp;
    tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp | mask);
}

void MFRC522_ClearBitMask(unsigned char reg, unsigned char mask)
{
    unsigned char tmp;
    tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp & (~mask));
}

void RFID_AntennaOn(void)
{
    unsigned char val;
    val = RFID_ReadReg(TxControlReg);
    if (!(val & 0x03))
        RFID_WriteReg(TxControlReg, val | 0x03);
}

void RFID_Init(void)
{
    RFID_WriteReg(CommandReg, PCD_RESETPHASE);
    delay(100000);
    RFID_WriteReg(TModeReg, 0x8D);
    RFID_WriteReg(TPrescalerReg, 0x3E);
    RFID_WriteReg(TReloadRegL, 0x1E);
    RFID_WriteReg(TReloadRegH, 0);
    RFID_WriteReg(TxAutoReg, 0x40);
    RFID_WriteReg(ModeReg, 0x3D);
    RFID_AntennaOn();
}

unsigned char RFID_ToCard(unsigned char command, unsigned char *sendData, unsigned char sendLen, unsigned char *backData, unsigned int *backLen)
{
    unsigned char status, irqEn, waitIRq, lastBits, n;
    unsigned int i;
    status = MI_ERR;
    irqEn = 0x00;
    waitIRq = 0x00;

    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    RFID_WriteReg(CommIEnReg, irqEn | 0x80);
    RFID_WriteReg(CommIrqReg, 0x7F);
    RFID_WriteReg(FIFOLevelReg, 0x80);
    RFID_WriteReg(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++)
        RFID_WriteReg(FIFODataReg, sendData[i]);

    RFID_WriteReg(CommandReg, command);
    if (command == PCD_TRANSCEIVE)
        MFRC522_SetBitMask(BitFramingReg, 0x80);

    i = 2000;
    do {
        n = RFID_ReadReg(CommIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    MFRC522_ClearBitMask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(RFID_ReadReg(ErrorReg) & 0x1B)) {
            status = MI_OK;
            n = RFID_ReadReg(FIFOLevelReg);
            lastBits = RFID_ReadReg(ControlReg) & 0x07;
            if (n == 0) n = 1;
            if (n > MAX_LEN) n = MAX_LEN;
            for (i = 0; i < n; i++)
                backData[i] = RFID_ReadReg(FIFODataReg);
            *backLen = n;
        }
    }
    return status;
}

unsigned char MFRC522_Request(unsigned char reqMode, unsigned char *tagType)
{
    unsigned char status;
    unsigned int backBits;
    RFID_WriteReg(BitFramingReg, 0x07);
    tagType[0] = reqMode;
    status = RFID_ToCard(PCD_TRANSCEIVE, tagType, 1, tagType, &backBits);
    return status;
}

unsigned char MFRC522_Anticoll(unsigned char *serNum)
{
    unsigned char status, i, serNumCheck;
    unsigned int unLen;
    serNumCheck = 0;
    RFID_WriteReg(BitFramingReg, 0x00);
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = RFID_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);
    if (status == MI_OK) {
        for (i = 0; i < 4; i++)
            serNumCheck ^= serNum[i];
        if (serNumCheck != serNum[4])
            status = MI_ERR;
    }
    return status;
}

/* ----------- Helpers ----------- */
void show_uid_on_lcd(unsigned char *serNum)
{
    char buf[20];
    lcd_clear();
    lcd_cmd(0x80);
    lcd_string("UID:");
    lcd_cmd(0xC0);
    sprintf(buf, "%02X %02X %02X %02X %02X", serNum[0], serNum[1], serNum[2], serNum[3], serNum[4]);
    lcd_string((unsigned char*)buf);
    delay(300000);
}

int find_uid_rfid(unsigned char *serNum)
{
    int k, m, eq;
    for (k = 0; k < uid_count_rfid; k++) {
        eq = 1;
        for (m = 0; m < 5; m++) {
            if (uid_database_rfid[k][m] != serNum[m]) {
                eq = 0;
                break;
            }
        }
        if (eq) return 1;
    }
    return 0;
}

/* ----------- REGISTER MODE ----------- */
void register_uid_rfid(void)
{
    unsigned char status;
    unsigned char serNum[5];
    unsigned char tagType[2];
    int m;

    lcd_clear();
    lcd_string("Register Mode");
    delay(4000000);
    lcd_clear();
    lcd_string("Scan Card...");
    while (1) {
        status = MFRC522_Request(PICC_REQIDL, tagType);
        if (status == MI_OK) {
            status = MFRC522_Anticoll(serNum);
            if (status == MI_OK) {
                show_uid_on_lcd(serNum);
                LED_PORT->FIOSET = LED_PIN;
                delay(4000000);
                LED_PORT->FIOCLR = LED_PIN;

                if (uid_count_rfid >= 20) {
                    lcd_clear();
                    lcd_string("Memory Full!");
                    delay(4000000);
                    return;
                }
                if (find_uid_rfid(serNum)) {
                    lcd_clear();
                    lcd_string("Already Exists!");
                    delay(4000000);
                    return;
                }
                for (m = 0; m < 5; m++)
                    uid_database_rfid[uid_count_rfid][m] = serNum[m];
                uid_count_rfid++;
                lcd_clear();
                lcd_string("Registered!");
                delay(4000000);
                return;
            }
        }
        delay(50000);
    }
}

/* ----------- VERIFY MODE ----------- */
void verify_uid_rfid(void)
{
    unsigned char status;
    unsigned char serNum[5];
    unsigned char tagType[2];

    lcd_clear();
    lcd_string("Verify Mode");
    delay(4000000);
    lcd_clear();
    lcd_string("Scan Card...");
    while (1) {
        status = MFRC522_Request(PICC_REQIDL, tagType);
        if (status == MI_OK) {
            status = MFRC522_Anticoll(serNum);
            if (status == MI_OK) {
                show_uid_on_lcd(serNum);
                LED_PORT->FIOSET = LED_PIN;
                delay(4000000);
                LED_PORT->FIOCLR = LED_PIN;
                lcd_clear();
                if (find_uid_rfid(serNum))
                    lcd_string("Verified!");
                else
                    lcd_string("Not Verified!");
                delay(4000000);
                return;
            }
        }
        delay(50000);
    }
}

/* ----------- Main ----------- */
int main(void)
{
    int key;
    unsigned char msg1[] = "1:Verify 2:Register";

    SystemInit();
    uid_count_rfid = 0;

    LPC_GPIO0->FIODIR |= 0xF << 23 | 1 << 27 | 1 << 28;
    LPC_GPIO2->FIODIR = 0x00003C00;
    LPC_GPIO1->FIODIR = 0;
    LED_PORT->FIODIR |= LED_PIN;
    LED_PORT->FIOCLR = LED_PIN;

    lcd_init();
    lcd_clear();

    SPI_INIT();
    RFID_Init();

    while (1) {
        lcd_clear();
        lcd_string(msg1);
        key = get_first_row_key();
        if (key == 1) {
            verify_uid_rfid();
        } else if (key == 2) {
            register_uid_rfid();
        }
    }
}
