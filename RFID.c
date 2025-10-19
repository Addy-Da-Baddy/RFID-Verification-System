#include <LPC17xx.h>
#include <stdio.h>
#include <string.h>

#define RS_CTRL 0x00000100     // P0.8
#define EN_CTRL 0x00000200     // P0.9
#define DT_CTRL 0x000000F0     // P0.4 TO P0.7
#define BUZZER_LED (1 << 17)   // P0.17 for buzzer
#define PRESCALE (25000 - 1)

unsigned long temp1 = 0, temp2 = 0, temp;
unsigned char flag1 = 0, flag2 = 0;
unsigned int flag;
signed int row, col;

// Push button for mode switch
#define MODE_BUTTON (1<<16)
unsigned char mode = 0; // 0: Check, 1: Registration

// Hardcoded IDs (4-digit) 
unsigned int registeredIDs[10] = {1234, 5678, 9012};
unsigned int totalIDs = 3;

unsigned int MatrixMap[3][3] = {
    {0, 1, 2},
    {3, 4, 5},
    {6, 7, 8}
};

unsigned char KeyMap[9] = {
    '1','2','3',
    '4','5','6',
    '7','8','9'
};

void delayMS(unsigned int milliseconds);
void input_keyboard(void);
void scan(void);
void display_lcd(unsigned char msg[]);
void lcd_write(void);
void port_write(void);
void delay_lcd(unsigned int r1);
void buzzer_success(void);
void buzzer_error(void);

int main(void) {
    SystemInit();
    SystemCoreClockUpdate();

    // Matrix keypad config
    LPC_PINCON->PINSEL3 = 0;           
    LPC_PINCON->PINSEL4 = 0;           
    LPC_GPIO2->FIODIR = 0x00003C00;    // P2.10-P2.13 output (rows)
    LPC_GPIO1->FIODIR = 0;             // P1.23-P1.26 input (cols)

    // LCD & buzzer config
    LPC_GPIO0->FIODIR |= DT_CTRL | RS_CTRL | EN_CTRL;
    LPC_GPIO0->FIODIR |= BUZZER_LED;

    // Mode button P0.16 as input
    LPC_GPIO0->FIODIR &= ~MODE_BUTTON;

    unsigned char msg[20];

    while(1) {
        // Check mode switch button
        if ((LPC_GPIO0->FIOPIN & MODE_BUTTON) == 0) {
            mode = !mode;
            if (mode) display_lcd((unsigned char *)"Registration Mode");
            else display_lcd((unsigned char *)"Check Mode");
            delayMS(500);
        }

        unsigned int inputID = 0;
        unsigned int digitCount = 0;

        display_lcd(mode ? (unsigned char *)"Enter ID to Reg" : (unsigned char *)"Enter ID to Check");

        while(1) {
            input_keyboard(); // sets row and col
            if (row<3 && col<3) { 
                unsigned char key = KeyMap[MatrixMap[row][col]];
                inputID = inputID*10 + (key - '0');
                digitCount++;
                sprintf((char *)msg,"ID: %04u",inputID);
                display_lcd(msg);
                delayMS(300);

                if(digitCount == 4) break; // 4-digit entry complete
            }
        }

        // Check mode
        if(!mode) {
            unsigned char found = 0;
            for(int i=0;i<totalIDs;i++) {
                if(registeredIDs[i] == inputID) {
                    found = 1; break;
                }
            }
            if(found) { display_lcd((unsigned char *)"Matched"); buzzer_success(); }
            else { display_lcd((unsigned char *)"Not Enrolled"); buzzer_error(); }
        }
        // Registration mode
        else {
            unsigned char exists = 0;
            for(int i=0;i<totalIDs;i++) {
                if(registeredIDs[i] == inputID) {
                    exists = 1; break;
                }
            }
            if(exists) { display_lcd((unsigned char *)"Already Exists"); buzzer_error(); }
            else {
                registeredIDs[totalIDs++] = inputID;
                display_lcd((unsigned char *)"Registered"); buzzer_success();
            }
        }

        delayMS(2000);
    }
}

// ======================== Helper functions ========================

void buzzer_success(void) {
    LPC_GPIO0->FIOSET = BUZZER_LED;
    delayMS(200);
    LPC_GPIO0->FIOCLR = BUZZER_LED;
}

void buzzer_error(void) {
    LPC_GPIO0->FIOSET = BUZZER_LED;
    delayMS(600);
    LPC_GPIO0->FIOCLR = BUZZER_LED;
}

void input_keyboard(void) {
    int Break_flag=0;
    row=0; col=0;
    while(1) {
        for(row=0;row<3;row++) {
            temp = 1<<(10+row);
            LPC_GPIO2->FIOPIN = temp;
            flag=0;
            delayMS(10);
            scan();
            if(flag==1) { Break_flag=1; delayMS(50); break; }
        }
        if(Break_flag==1) break;
    }
}

void scan(void) {
    unsigned long temp3;
    temp3 = LPC_GPIO1->FIOPIN & 0x07800000;
    if(temp3!=0x0) {
        flag=1;
        if(temp3 == 1<<23) col=0;
        else if(temp3 == 1<<24) col=1;
        else if(temp3 == 1<<25) col=2;
        else if(temp3 == 1<<26) col=3;
    }
}

void display_lcd(unsigned char msg[]) {
    flag1=0; // COMMAND
    unsigned long init_command[] = {0x30,0x30,0x30,0x20,0x28,0x0c,0x06,0x01,0x80};
    for(int i=0;i<9;i++) { temp1 = init_command[i]; lcd_write(); }
    flag1=1; // DATA
    for(int i=0;msg[i]!='\0';i++) { temp1=msg[i]; lcd_write(); }
}

void lcd_write(void) {
    flag2 = (flag1==1)?0:((temp1==0x30)||(temp1==0x20))?1:0;
    temp2 = temp1 & 0xf0;
    port_write();
    if(flag2==0) { temp2 = (temp1 & 0x0f)<<4; port_write(); }
}

void port_write(void) {
    LPC_GPIO0->FIOPIN = temp2;
    if(flag1==0) LPC_GPIO0->FIOCLR = RS_CTRL;
    else LPC_GPIO0->FIOSET = RS_CTRL;
    LPC_GPIO0->FIOSET = EN_CTRL;
    delay_lcd(25);
    LPC_GPIO0->FIOCLR = EN_CTRL;
    delay_lcd(3000);
}

void delay_lcd(unsigned int r1) { for(unsigned int r=0;r<r1;r++); }
void delayMS(unsigned int milliseconds) { for(unsigned int i=0;i<milliseconds*1000;i++); }
