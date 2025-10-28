#include <stdio.h>
#include <LPC17xx.h>

/* Global Variables */
int temp1, temp2;
int flag_command;
int i, j, r;
signed int row, col;
unsigned int flag;
int mode; /* 0 = Verification, 1 = Registration */
int uid_index;
char uid_buffer[5];
int uid_count;
int uid_database[20];
int button_pressed;
int last_button_state;
int debounce_counter;

/* Keypad Mapping */
unsigned int MatrixMap[4][3] = {
    {10, 0 , 11},
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}  /* 10=Mode Change, 0=Zero, 11=Enter */
};

/* Function Prototypes */
void lcd_wr(void);
void port_wr(void);
void delay(int r1);
void lcd_cmd(unsigned char cmd);
void lcd_data(unsigned char data);
void lcd_string(unsigned char *str);
void lcd_clear(void);
void lcd_init(void);
void input_keyboard(void);
void scan(void);
void display_mode(void);
void process_key(int key);
void verify_uid(void);
void register_uid(void);
int find_uid(int uid_val);
void int_to_string(int num, char *str);
void read_mode_button(void);

/* LCD Functions */
void port_wr()
{
    int j;
    LPC_GPIO0->FIOPIN = temp2 << 23;
    if (flag_command == 0){
        LPC_GPIO0->FIOCLR = 1 << 27;
    }
    else{
        LPC_GPIO0->FIOSET = 1 << 27;
    }
    LPC_GPIO0->FIOSET = 1 << 28;
    for (j = 0; j < 50; j++);
    LPC_GPIO0->FIOCLR = 1 << 28;
    for (j = 0; j < 10000; j++);
}

void lcd_wr()
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
    flag_command = 1;
    i = 0;
    while (str[i] != '\0')
    {
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
    int command_init[] = {3, 3, 3, 2, 2, 0x28, 0x0C, 0x06, 0x01};
    
    flag_command = 0;
    for (i = 0; i < 9; i++)
    {
        temp1 = command_init[i];
        lcd_wr();
        delay(30000);
    }
}

/* Keypad Functions */
void input_keyboard(void)
{
    int Break_flag;
    Break_flag = 0;
    row = 0;
    col = 0;
    flag = 0;

    for (row = 0; row < 4; row++) {
        unsigned long temp;
        switch(row) {
            case 0: temp = 1 << 10; break;
            case 1: temp = 1 << 11; break;
            case 2: temp = 1 << 12; break;
            case 3: temp = 1 << 13; break;
        }
        LPC_GPIO2->FIOPIN = temp;
        delay(1000);
        scan();
        if (flag == 1) {
            Break_flag = 1;
            break;
        }
    }
    
    if (Break_flag == 1) {
        process_key(MatrixMap[row][col]);
        /* Wait for key release */
        while(1) {
            flag = 0;
            for (row = 0; row < 4; row++) {
                unsigned long temp;
                switch(row) {
                    case 0: temp = 1 << 10; break;
                    case 1: temp = 1 << 11; break;
                    case 2: temp = 1 << 12; break;
                    case 3: temp = 1 << 13; break;
                }
                LPC_GPIO2->FIOPIN = temp;
                delay(1000);
                scan();
                if (flag == 1) break;
            }
            if (flag == 0) break;
        }
        delay(50000); /* Debounce delay */
    }
}

void scan(void)
{
    unsigned long temp3;
    temp3 = LPC_GPIO1->FIOPIN & 0x03800000;
    if (temp3 != 0x00000000) {
        flag = 1;
        if (temp3 == (1 << 23)) col = 0;
        else if (temp3 == (1 << 24)) col = 1;
        else if (temp3 == (1 << 25)) col = 2;
    }
}

/* Helper Functions */
void int_to_string(int num, char *str)
{
    int temp_num;
    int digit_count;
    int k;
    
    temp_num = num;
    digit_count = 0;
    
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    while (temp_num > 0) {
        temp_num = temp_num / 10;
        digit_count++;
    }
    
    str[digit_count] = '\0';
    temp_num = num;
    
    for (k = digit_count - 1; k >= 0; k--) {
        str[k] = (temp_num % 10) + '0';
        temp_num = temp_num / 10;
    }
}

int find_uid(int uid_val)
{
    int k;
    for (k = 0; k < uid_count; k++) {
        if (uid_database[k] == uid_val) {
            return 1;
        }
    }
    return 0;
}

void display_mode(void)
{
    lcd_clear();
    if (mode == 0) {
        lcd_cmd(0x80);
        lcd_string("Verification:");
    } else {
        lcd_cmd(0x80);
        lcd_string("Registration:");
    }
    lcd_cmd(0xC0);
}

void process_key(int key)
{
    char display_char[2];
    int uid_value;
    int k;
    
    if (key == 10) { /* Mode Change */
        mode = 1 - mode;
        uid_index = 0;
        for (k = 0; k < 5; k++) uid_buffer[k] = '\0';
        display_mode();
        return;
    }
    
    if (key == 11) { /* Enter */
        if (uid_index == 4) {
            uid_value = 0;
            for (k = 0; k < 4; k++) {
                uid_value = uid_value * 10 + (uid_buffer[k] - '0');
            }
            
            if (mode == 0) {
                verify_uid();
            } else {
                register_uid();
            }
            
            uid_index = 0;
            for (k = 0; k < 5; k++) uid_buffer[k] = '\0';
            delay(2000000);
            display_mode();
        }
        return;
    }
    
    /* Number key (0-9) */
    if (uid_index < 4) {
        uid_buffer[uid_index] = key + '0';
        uid_index++;
        
        /* Refresh the second line to show all digits typed so far */
        lcd_cmd(0xC0); /* Move to second line start */
        for (k = 0; k < uid_index; k++) {
            lcd_data(uid_buffer[k]);
        }
    }
}

void verify_uid(void)
{
    int uid_value;
    int k;
    
    uid_value = 0;
    for (k = 0; k < 4; k++) {
        uid_value = uid_value * 10 + (uid_buffer[k] - '0');
    }
    
    lcd_clear();
    lcd_cmd(0x80);
    lcd_string("Verification:");
    lcd_cmd(0xC0);
    
    if (find_uid(uid_value)) {
        lcd_string("Verified UID!");
    } else {
        lcd_string("Unverified!");
    }
}

void register_uid(void)
{
    int uid_value;
    int k;
    
    uid_value = 0;
    for (k = 0; k < 4; k++) {
        uid_value = uid_value * 10 + (uid_buffer[k] - '0');
    }
    
    lcd_clear();
    lcd_cmd(0x80);
    lcd_string("Registration:");
    lcd_cmd(0xC0);
    
    if (find_uid(uid_value)) {
        lcd_string("UID Exists!");
    } else if (uid_count >= 20) {
        lcd_string("Memory Full!");
    } else {
        uid_database[uid_count] = uid_value;
        uid_count++;
        lcd_string("UID Registered!");
    }
}

/* Main Function */
int main(void)
{
    int k;
    
    SystemInit();
    SystemCoreClockUpdate();
    
    /* Initialize variables */
    temp1 = 0;
    temp2 = 0;
    flag_command = 0;
    mode = 0; /* Start in Verification mode */
    uid_index = 0;
    uid_count = 5; /* 5 hardcoded UIDs */
    
    /* Hardcoded UIDs */
    uid_database[0] = 1234;
    uid_database[1] = 5678;
    uid_database[2] = 9012;
    uid_database[3] = 3456;
    uid_database[4] = 7890;
    
    for (k = 5; k < 20; k++) {
        uid_database[k] = 0;
    }
    
    for (k = 0; k < 5; k++) {
        uid_buffer[k] = '\0';
    }
    
    /* LCD Pin configuration */
    LPC_GPIO0->FIODIR |= 0xF << 23 | 1 << 27 | 1 << 28;
    
    /* Keypad configuration */
    LPC_PINCON->PINSEL3 = 0;
    LPC_PINCON->PINSEL4 = 0;
    LPC_GPIO2->FIODIR = 0x00003C00;
    LPC_GPIO1->FIODIR = 0;
    
    /* Initialize LCD */
    lcd_init();
    display_mode();
    
    /* Main loop */
    while(1) {
        input_keyboard();
    }
}


