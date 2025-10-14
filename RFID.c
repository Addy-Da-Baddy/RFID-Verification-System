#include <LPC17xx.h>
#include <stdio.h>

#define RS_CTRL (1 << 27)
#define EN_CTRL (1 << 28)
#define DT_CTRL (0xF << 23)

#define SWITCH_PIN (1 << 4)     // P0.4 switch
#define KEYPAD_ROWS_START 15    // P0.15-P0.18 inputs (rows)
#define KEYPAD_COLS_START 19    // P0.19-P0.22 outputs (cols)

#define REGISTER 0
#define VERIFY   1

#define MAX_UIDS 20

// Global variables
unsigned long int temp1 = 0, temp2 = 0;
unsigned char flag1 = 0;
unsigned int mode = REGISTER;
unsigned int uid;
unsigned int rows_val;
unsigned int registeredUIDs[MAX_UIDS];
unsigned int uidCount = 0;

// Variables previously declared locally, now global:
int i = 0, col = 0, row = 0;
int digit = 0;
char *header = NULL;

// Messages (16 chars max for LCD 2x16)
unsigned char msgRegister[]   = "Registration Sys";
unsigned char msgVerify[]     = "Verification Sys";
unsigned char msgEnter[]      = "Enter UID:      ";
unsigned char msgRegSuccess[] = "Registered      ";
unsigned char msgRegExist[]   = "Already Reg.    ";
unsigned char msgVerified[]   = "Verified        ";
unsigned char msgDenied[]     = "Denied          ";
unsigned char msgUIDFull[]    = "UID List Full   ";

// Function prototypes
void delay_lcd(unsigned int);
void lcd_write(void);
void port_write(void);
void lcd_init(void);
void lcd_set_cursor(unsigned char pos);
void displayHeader(void);
void displaySecondLine(char *msg);
int getKeypadDigit(void);
unsigned int readKeypad4Digits(void);
int uidExists(unsigned int uid);
void delay_ms(unsigned int ms);
void displayDigit(unsigned char digit);


// --- Function implementations ---

void delay_lcd(unsigned int r1) {
    volatile unsigned int r;
    for (r = 0; r < r1; r++);
}

void delay_ms(unsigned int ms) {
    unsigned int i;
    for (i = 0; i < ms; i++) {
        delay_lcd(1000);
    }
}

void port_write(void) {
    LPC_GPIO0->FIOCLR = DT_CTRL;
    LPC_GPIO0->FIOSET = temp2 & DT_CTRL;

    if (flag1 == 0)
        LPC_GPIO0->FIOCLR = RS_CTRL;
    else
        LPC_GPIO0->FIOSET = RS_CTRL;

    LPC_GPIO0->FIOSET = EN_CTRL;
    delay_lcd(5000);
    LPC_GPIO0->FIOCLR = EN_CTRL;
    delay_lcd(300000);
}

void lcd_write(void) {
    temp2 = ((temp1 >> 4) & 0x0F) << 23;
    port_write();

    temp2 = (temp1 & 0x0F) << 23;
    port_write();
}

void lcd_init(void) {
    unsigned long int init_command[] = {
        0x30, 0x30, 0x30, 0x20, 0x28, 0x0C, 0x06, 0x01, 0x80
    };

    flag1 = 0;
    delay_lcd(500000);
    for (i = 0; i < 9; i++) {
        temp1 = init_command[i];
        lcd_write();
        delay_lcd(50000);
    }
}

void lcd_set_cursor(unsigned char pos) {
    flag1 = 0;
    temp1 = pos;  // pos: 0x80 for first line, 0xC0 for second line
    lcd_write();
    delay_lcd(30000);
}

void displayHeader(void) {
    flag1 = 0;
    temp1 = 0x01; // Clear display before header
    lcd_write();
    delay_lcd(600000);

    lcd_set_cursor(0x80);  // First line

    flag1 = 1;
    header = (mode == REGISTER) ? (char*)msgRegister : (char*)msgVerify;

    for (i = 0; i < 16; i++) {
        if (header[i] != '\0') temp1 = header[i];
        else temp1 = ' ';
        lcd_write();
    }
}

void displaySecondLine(char *msg) {
    lcd_set_cursor(0xC0);  // Second line

    flag1 = 1;
    for (i = 0; i < 16; i++) {
        if (msg[i] != '\0') temp1 = msg[i];
        else temp1 = ' ';
        lcd_write();
    }
}

int uidExists(unsigned int uid) {
    for (i = 0; i < uidCount; i++) {
        if (registeredUIDs[i] == uid) return 1;
    }
    return 0;
}

unsigned int readKeypad4Digits(void) {
    unsigned int uid = 0;
    for (i = 0; i < 4; i++) {
        digit = getKeypadDigit();
        displayDigit(digit);
        uid = uid * 10 + digit;
    }
    return uid;
}

// Scan keypad and return a digit (0-9)
// Blocking call, waits for key press
int getKeypadDigit(void) {
    // Keypad mapping (4x4):
    // Cols: P0.19-P0.22 outputs
    // Rows: P0.15-P0.18 inputs

    const int keypad_map[4][4] = {
        {1, 2, 3, 10},   // 10 can be ignored or special key
        {4, 5, 6, 11},
        {7, 8, 9, 12},
        {15,0, 14,13}    // 0 at position (3,1)
    };

    while (1) {
        for (col = 0; col < 4; col++) {
            // Set all cols high
            LPC_GPIO0->FIOSET = (0xF << KEYPAD_COLS_START);
            // Pull one col low
            LPC_GPIO0->FIOCLR = (1 << (KEYPAD_COLS_START + col));

            delay_ms(1); // Small delay for signal to settle

            // Read rows
            rows_val = (LPC_GPIO0->FIOPIN >> KEYPAD_ROWS_START) & 0xF;

            for (row = 0; row < 4; row++) {
                if (((rows_val & (1 << row)) == 0)) { // active low when pressed
                    int key = keypad_map[row][col];
                    if (key <= 9) {  // Valid digit 0-9
                        // Wait for key release (debounce)
                        while(((LPC_GPIO0->FIOPIN >> KEYPAD_ROWS_START) & (1 << row)) == 0);
                        delay_ms(50);
                        return key;
                    }
                }
            }
        }
    }
}

void displayDigit(unsigned char digit) {
    // Show digit on LCD at current cursor position (2nd line)
    flag1 = 1;
    temp1 = digit + '0';
    lcd_write();
}


// --- Main function ---

int main(void) {
    unsigned int prevSwitch = 1, currSwitch;

    SystemInit();
    SystemCoreClockUpdate();

    // LCD pins as output (P0.23-P0.28)
    LPC_GPIO0->FIODIR |= DT_CTRL | RS_CTRL | EN_CTRL;

    // Switch pin input (P0.4)
    LPC_GPIO0->FIODIR &= ~SWITCH_PIN;

    // Keypad cols as output (P0.19-P0.22)
    LPC_GPIO0->FIODIR |= (0xF << KEYPAD_COLS_START);

    // Keypad rows as input (P0.15-P0.18)
    LPC_GPIO0->FIODIR &= ~(0xF << KEYPAD_ROWS_START);

    // Optional: Enable pull-ups on keypad rows here if hardware needs

    lcd_init();

    // Show initial mode header and prompt
    displayHeader();
    displaySecondLine((char *)msgEnter);

    while (1) {
        currSwitch = (LPC_GPIO0->FIOPIN & SWITCH_PIN) ? 1 : 0;

        if (prevSwitch == 1 && currSwitch == 0) { // Switch pressed: toggle mode
            mode = (mode == REGISTER) ? VERIFY : REGISTER;
            displayHeader();
            displaySecondLine((char *)msgEnter);
            delay_ms(300);  // debounce
        }
        prevSwitch = currSwitch;

        // Read UID from keypad (blocking call)
					uid = readKeypad4Digits();

        if (mode == REGISTER) {
            if (uidExists(uid) == 0) {
                if (uidCount < MAX_UIDS) {
                    registeredUIDs[uidCount++] = uid;
                    displaySecondLine((char *)msgRegSuccess);
                } else {
                    displaySecondLine((char *)msgUIDFull);
                }
            } else {
                displaySecondLine((char *)msgRegExist);
            }
        } else { // VERIFY
            if (uidExists(uid)) {
                displaySecondLine((char *)msgVerified);
            } else {
                displaySecondLine((char *)msgDenied);
            }
        }
        delay_ms(2000);

        // Prompt again after delay
        displaySecondLine((char *)msgEnter);
    }
}
