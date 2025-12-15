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

#define CELSIUS 1

#define ADC_SAMPLES 64

//============================================

unsigned int adc_buffer[ADC_SAMPLES];
unsigned int adc_avg = 0;
unsigned long adc_sum = 0;

char tx_string[32];
volatile unsigned char tx_index = 0;
volatile unsigned char uart_tx_busy = 0;

unsigned char temp_unit = CELSIUS;
unsigned int temp_c = 0;
unsigned int temp_f = 0;
unsigned char digit3 = 0;
unsigned char digit2 = 0;
unsigned char digit1 = 0;
unsigned char digit0 = 0;

volatile unsigned long ta1_overflows;

volatile unsigned char new_sample = 0;

//============================================

void clocks_config(void);
void TA0_config(void);
void TA1_config(void);
void ADC10_config(void);
void GPIO_config(void);
void UART_config(void);

void SPI_config(void);
void MAX7219_config(void);
void MAX7219_SPI_TX(unsigned char reg, unsigned char data);

void get_temp_digits(unsigned int temp);
unsigned long get_timestamp_ticks(void);


void build_temp_string(unsigned char hh,
                       unsigned char mm,
                       unsigned char ss,
                       char unit_char);


//============================================
void main(void) {
  WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer

  //----------------------------------

  GPIO_config();
  clocks_config();
  ADC10_config();
  TA0_config();
  TA1_config();
  UART_config();
  SPI_config();
  MAX7219_config();

  __enable_interrupt();
 
  //----------------------------------

  while (1) {

    if (new_sample && !uart_tx_busy) {
      unsigned int temp = 0;
      unsigned char temp_unit_display = 0;
      char temp_unit_char = ' ';

      unsigned long ts = get_timestamp_ticks();
      unsigned long seconds = ts >> 15;
      unsigned int hh = seconds / 3600;
      unsigned int mm = (seconds % 3600) / 60;
      unsigned int ss = seconds % 60;

      new_sample = 0;
      
      //----------------------------------

      if (temp_unit == CELSIUS) {
        temp = temp_c;
        temp_unit_display = 0b01001110;
        temp_unit_char = 'C';

        P1OUT &= ~BIT0;
        P1OUT |= BIT6;
      } else {
        temp_f = (temp_c * 9) / 5 + 3200;
        temp = temp_f;
        temp_unit_display = 0b01000111;
        temp_unit_char = 'F';

        P1OUT &= ~BIT6;
        P1OUT |= BIT0;
      }
 
      get_temp_digits(temp);
    
      //----------------------------------

      MAX7219_SPI_TX(DIGIT0, temp_unit_display);
      MAX7219_SPI_TX(DIGIT5, digit3);
      MAX7219_SPI_TX(DIGIT4, digit2 + 0b10000000);
      MAX7219_SPI_TX(DIGIT3, digit1);
      MAX7219_SPI_TX(DIGIT2, digit0);

      //----------------------------------
      
      build_temp_string(hh, mm, ss, temp_unit_char);

      tx_index = 0;
      uart_tx_busy = 1;

      UCA0TXBUF = tx_string[tx_index++];

      IE2 |= UCA0TXIE;
      // IFG2 |= UCA0TXIFG;
    }
  }
}


//============================================
void GPIO_config(void) {
  P1DIR = ~BIT3;
  P1REN = BIT3;
  P1OUT = BIT3;

  P1IFG = 0;
  P1IES = BIT3;
  P1IE = BIT3;
   
  P2DIR = 0xFF;
  P2OUT = BIT0; // SPI CS/SS pin = HIGH
}


//============================================
void clocks_config(void) {
  // MCLK = 16 MHz, SMCLK = 4 MHz
  DCOCTL = CALDCO_16MHZ;
  BCSCTL1 = CALBC1_16MHZ;
  BCSCTL2 = DIVS1;

  BCSCTL3 = XCAP0 + XCAP1;

  while(BCSCTL3 & LFXT1OF);
}


//============================================
void TA0_config(void) {
  // Timer0 CLK = ACLK
  TA0CTL = TASSEL0 + MC0;
  TA0CCTL1 = OUTMOD0 + OUTMOD1 + OUTMOD2 + OUT;
  TA0CCR0 = 32767; // PWM period = sample-and-conversion period
  TA0CCR1 = 16383; // PWM duty cycle = 50 %
}


//============================================
void TA1_config(void) {
  // Timer1 CLK = ACLK
  TA1CTL = TASSEL0 + MC1 + TAIE;
}


//============================================
void ADC10_config(void) {
  // ADC10CLK = SMCLK / 4 = 1 MHz
  ADC10CTL0 = SREF0 +
              ADC10SHT0 +
              ADC10SHT1 +
              ADC10SR +
              MSC +
              REFON +
              ADC10ON +
              ADC10IE;

  ADC10CTL1 = INCH2 +
              SHS0 + // trigger source = PWM on TA0.1
              ADC10DIV0 +
              ADC10DIV1 +
              ADC10SSEL0 +
              ADC10SSEL1 +
              CONSEQ1;

  ADC10AE0 = BIT4;

  ADC10DTC1 = ADC_SAMPLES;
  ADC10SA = (unsigned int)&adc_buffer[0];

  ADC10CTL0 |= ENC;
}


//============================================
void UART_config(void) {
  UCA0CTL1 = UCSSEL1 + UCSWRST;

  UCA0BR0 = 0xA0;
  UCA0BR1 = 0x01;
  
  UCA0MCTL = UCBRS1 + UCBRS2;
  
  P1SEL |= BIT1 + BIT2;
  P1SEL2 |= BIT1 + BIT2;

  UCA0CTL1 &= ~UCSWRST;
  
  IFG2 &= ~UCA0TXIFG;
  
  // IE2 |= UCA0TXIE;
}


//============================================
void SPI_config(void) {
  UCB0CTL1 = UCSWRST; 

  UCB0CTL0 = UCCKPH +
             UCMSB +
             UCMST +
             UCSYNC;

  UCB0CTL1 |= UCSSEL1;

  UCB0BR0 = 1;
  UCB0BR1 = 0;

  P1SEL |= BIT5 + BIT7;
  P1SEL2 |= BIT5 + BIT7;

  UCB0CTL1 &= ~UCSWRST;
}


//============================================
void MAX7219_config(void) {
  MAX7219_SPI_TX(DISPLAY_TEST, 0x00);
  MAX7219_SPI_TX(SHUTDOWN, 0x01);
  MAX7219_SPI_TX(SCAN_LIMIT, 0x05);
  MAX7219_SPI_TX(DECODE_MODE, 0b00111110);
  MAX7219_SPI_TX(INTENSITY, 0x03);

  for (unsigned char i = 1; i <= 8; i++)
    MAX7219_SPI_TX(i, 0x0F);   // blank (code B: 0xF = blank)
}


//============================================
void MAX7219_SPI_TX(unsigned char reg, unsigned char data) {
  P2OUT &= ~BIT0;

  UCB0TXBUF = reg;
  while (UCB0STAT & UCBUSY);

  UCB0TXBUF = data;
  while (UCB0STAT & UCBUSY);

  P2OUT |= BIT0;
}


//============================================
__attribute__((interrupt(ADC10_VECTOR)))
void ADC10_ISR(void) { 
  
  ADC10CTL0 &= ~(ENC + MSC);

  new_sample = 1;  
  adc_sum = 0;
  
  //----------------------------------

  for(unsigned int i = 0; i < ADC_SAMPLES; i++) {
    adc_sum += adc_buffer[i];
  }
  
  adc_avg = adc_sum >> 6;

  //----------------------------------

  // temp = (adc_avg / 1023.0 * 1.5 - 0.5) * 100; -> float
  temp_c = ((adc_avg * 15000L) / 1023 - 5000); // first 4 figures

  //----------------------------------

  ADC10SA = (unsigned int)&adc_buffer[0];
  ADC10DTC1 = ADC_SAMPLES;

  ADC10CTL0 |= ENC + MSC;
}


//============================================
__attribute__((interrupt(USCIAB0TX_VECTOR)))
void UART_TX_ISR(void) {
  if (tx_string[tx_index] == '\0') {
		IE2 &= ~UCA0TXIE;
		tx_index = 0;
		uart_tx_busy = 0;
	} else {
		UCA0TXBUF = tx_string[tx_index];
		tx_index++;
	}
}


//============================================
__attribute__((interrupt(PORT1_VECTOR)))
void port1_ISR(void) {
  if (P1IFG & BIT3) {
    P1IFG &= ~BIT3;
    P1IE &= ~BIT3;

    // Start 20 ms (655 ticks) debouncer
    // relative to current timer count.
    // Interrupt fires when TA1R = TA1R + 655
    TA1CCTL0 = CCIE;
    TA1CCR0 = TA1R + 655;
  }
}


//============================================
__attribute__((interrupt(TIMER1_A0_VECTOR)))
void TA1_CCR0_ISR(void) {
  TA1CCTL0 &= ~CCIE;

  if ((~P1IN) & BIT3) {
    temp_unit ^= 1;
    // temp_unit initially defined as 1
    // so temp_unit = 1 => 1 ^ 1 => temp_unit = 0.
    // temp_unit = 0 => 0 ^ 1 => temp_unit = 1.
  }

  P1IFG &= ~BIT3;
  P1IE |= BIT3;
}


//============================================
__attribute__((interrupt(TIMER1_A1_VECTOR)))
void TA1_ISR(void) {
  switch (TA1IV) {
    case TA1IV_TAIFG:
      ta1_overflows++;
      break;

    default:
      break;
  }
}


//============================================
void get_temp_digits(unsigned int temp) {
  digit3 = temp / 1000;
  digit2 = (temp / 100) % 10;

  digit1 = (temp % 100) / 10;
  digit0 = temp % 10;
  // Ex: temp = 2456 => digit3 = 2, digit2 = 4...
}


//============================================
unsigned long get_timestamp_ticks(void) {
  unsigned long ovf;
  unsigned int tar;

  ovf = ta1_overflows;
  tar  = TA1R;

  return (ovf << 16) | tar;
}


//============================================
void build_temp_string(unsigned char hh,
                       unsigned char mm,
                       unsigned char ss,
                       char unit_char) {
  tx_string[0]  = '[';
  tx_string[1]  = (hh / 10) + '0';
  tx_string[2]  = (hh % 10) + '0';
  tx_string[3]  = ':';
  tx_string[4]  = (mm / 10) + '0';
  tx_string[5]  = (mm % 10) + '0';
  tx_string[6]  = ':';
  tx_string[7]  = (ss / 10) + '0';
  tx_string[8]  = (ss % 10) + '0';
  tx_string[9]  = ']';
  tx_string[10] = ' ';
  tx_string[11] = 'T';
  tx_string[12] = 'e';
  tx_string[13] = 'm';
  tx_string[14] = 'p';
  tx_string[15] = ' ';
  tx_string[16] = '=';
  tx_string[17] = ' ';
  tx_string[18] = digit3 + '0';
  tx_string[19] = digit2 + '0';
  tx_string[20] = '.';
  tx_string[21] = digit1 + '0';
  tx_string[22] = digit0 + '0';
  tx_string[23] = ' ';
  tx_string[24] = unit_char;
  tx_string[25] = '\r';
  tx_string[26] = '\n';
  tx_string[27] = '\0';
}

