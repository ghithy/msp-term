#include <msp430.h>

#define DIGIT0 0x01
#define DIGIT1 0x02
#define DIGIT2 0x03
#define DIGIT3 0x04
#define DIGIT4 0x05
#define DIGIT5 0x06
#define DIGIT6 0x07
#define DIGIT7 0x08

#define DECODE_MODE 0x09
#define INTENSITY 0x0A
#define SCAN_LIMIT 0x0B
#define SHUTDOWN 0x0C
#define DISPLAY_TEST 0x0F


void config_clocks(void);
void config_gpio(void);
void config_spi(void);
void init_max7219(void);
void max7219_send(unsigned char reg, unsigned data);


void main(void) {
  WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer

  // __enable_interrupt();

  config_clocks();
  config_gpio();
  config_spi();
  init_max7219();
  // config_adc10();
  // config_timer();

  // max7219_send(DIGIT0, 7);
  max7219_send(DIGIT0, 0b01110111); // A

  while(1);
}

void config_clocks(void) {
  // --- Config clocks ---
  // MCLK = 16 MHz, SMCLK = 2 MHz
  DCOCTL = CALDCO_16MHZ;
  BCSCTL1 = CALBC1_16MHZ;
  BCSCTL2 = DIVS0 + DIVS1;
  BCSCTL3 = XCAP0 + XCAP1;

  while(BCSCTL3 & LFXT1OF);
}

void config_gpio(void) {
  P1DIR = 0xff;
  P1OUT = BIT0;

  P2DIR = 0xff;
  P2OUT = 0;
}

void config_spi(void) {
  UCB0CTL1 = UCSWRST; 

  UCB0CTL0 = UCCKPH +
             UCMSB +
             UCMST +
             UCSYNC;

  UCB0CTL1 |= UCSSEL1;

  UCB0BR0 = 1;
  UCB0BR1 = 0;

  // P1SEL  |= BIT5 | BIT6 | BIT7;
  // P1SEL2 |= BIT5 | BIT6 | BIT7;

  // P1DIR &= ~BIT6; 

  P1SEL |= BIT5 + BIT7;
  P1SEL2 |= BIT5 + BIT7;

  UCB0CTL1 &= ~UCSWRST;
}

void init_max7219(void) {
  max7219_send(DISPLAY_TEST, 0x00);
  max7219_send(SHUTDOWN, 0x01);
  max7219_send(SCAN_LIMIT, 0x00);
  max7219_send(DECODE_MODE, 0x00);
  max7219_send(INTENSITY, 0x03);

  for (int i = 1; i <= 8; i++)
    max7219_send(i, 0x0F);   // blank (code B: 0xF = blank)
}

void max7219_send(unsigned char reg, unsigned data) {
  P1OUT &= ~BIT0;

  UCB0TXBUF = reg;
  while (UCB0STAT & UCBUSY);

  UCB0TXBUF = data;
  while (UCB0STAT & UCBUSY);

  P1OUT |= BIT0;
}
