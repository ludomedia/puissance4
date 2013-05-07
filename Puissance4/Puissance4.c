/*
 * Puissance4.c
 *
 * Created: 04.05.2013 21:11:23
 *  Author: marc
 */ 


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define byte unsigned char
#define F_CPU 8000000

#define COLS 7
#define ROWS 6
#define HUMAN 0
#define CPU 1
#define EMPTY 2
#define MAX_LEVEL 4
const byte colPond[] = { 1, 2, 3, 4, 3, 2, 1 };
byte grid[ROWS*(COLS+1)]; // PAD sur 8 colonnes pour faciliter calcul de l'index
const int keys[] PROGMEM = { 0, 316, 483, 587, 657, 708, 746, 1023 };
volatile unsigned long milli = 0;

ISR(TIM0_COMPA_vect) { // TIMER0_OVF_vect (each ms)
	milli++;
}

void delay(unsigned long ms) {
	unsigned long now = milli;
	while((milli-now)<ms);
}

void delayMicroseconds(unsigned long mu) {
	for(long s=0;s<mu;s++);
}

unsigned long millis() {
	return milli;
}

//-------------------------------------------------- KEYBOARD --------------------------------------------------------

byte rk() {

	ADCSRA |= (1 << ADSC); // set ADSC to start a conversion
	while (ADCSRA & (1<<ADSC)); // wait until ADSC returns back to 0;
	byte adcl = ADCL; // read ADCL first
	int v = ADCH<<8;
	v |= adcl;

	int * ptr = keys;		
	int limit_a = pgm_read_word_near(ptr++);
	for(byte b=0;b<7;b++) {
		int limit_b = pgm_read_word_near(ptr++);
		int mid = (limit_b + limit_a) >> 1;
		//int mid = (keys[b+1] + keys[b]) >> 1;
		if(v<mid) return '1' + (6-b);
		limit_a = limit_b;
	}
	return 0;
	
}

byte readKey() {
  delay(5);
  byte k = rk();
  delay(10);
  if(k==rk()) return k;
  return 0;
}

//-------------------------------------------------- AI --------------------------------------------------------------

byte freeRow(byte c) {
  byte n = ROWS;
  for(byte y=0;y<ROWS;y++) {
    if(grid[y<<3|c] != EMPTY) break;
    n = y;
  }
  return n;
}

byte countSame(byte x, byte y, byte dx, byte dy, byte player) {
  for(byte cnt=0;;cnt++) {
    x += dx;
    //if(x==0xFF) return cnt;
    if(x>=COLS) return cnt;
    y += dy;
    //if(y==0xFF) return cnt;
    if(y>=ROWS) return cnt;
    if(grid[y<<3|x]!=player) return cnt;
  }
}

inline byte countH(byte x, byte y, byte p) {
  return (countSame(x, y, -1, 0, p) + countSame(x, y, 1, 0, p));
}

inline byte countV(byte x, byte y, byte p) {
  return (countSame(x,y, 0, -1, p) + countSame(x, y, 0, 1, p));
}

inline byte countD1(byte x, byte y, byte p) {
  return (countSame(x, y, -1, -1, p) + countSame(x, y, 1, 1, p));
}

inline byte countD2(byte x, byte y, byte p) {
  return (countSame(x, y, -1, 1, p) + countSame(x, y, 1, -1, p));
}

inline byte check4(byte x, byte y, byte p) {
  if(countH(x, y, p)>=3) return 1;	// Horrizontale
  if(countV(x, y, p)>=3) return 2;	// Verticale
  if(countD1(x, y, p)>=3) return 3;	// Diagonale descendente
  if(countD2(x, y, p)>=3) return 4;	// Diagonale montante
  return 0;
}

int evalMove(byte lvl, byte player) {
  int pts, mxpts;
  byte h, v, d1, d2;
  byte r;
  byte bestcol = COLS;

  if(player==HUMAN) mxpts = 32767;
  else mxpts = -32768;

  for(byte c=0;c<COLS;c++) {
    r = freeRow(c);										
    if(r>=ROWS) continue;
    grid[r<<3|c] = player; // Pose une pièce
    h = countH(c, r, player);
    v = countV(c, r, player);
    d1 = countD1(c, r, player);
    d2 = countD2(c, r, player);
    
    if((h>=3)||(v>=3)||(d1>=3)||(d2>=3)) { 
      grid[r<<3|c] = EMPTY;		// Retire la pièce
      if(lvl==0) return c;		// Condition gagnante
      if(player==CPU) return 1000/lvl;	// ...
      return -1000/lvl;
      //return (1000 * (-0.9));	        // Condition perdante
    }
    
    pts = 6*(int)(h+v+d1+d2) + (int)r + (int)colPond[c]; // + (Rnd.nextInt()%2);
    //if(player == HUMAN) pts = pts * (-0.9);

    if(lvl<MAX_LEVEL) {
      int subpts = evalMove(lvl + 1, 1 - player);
      pts += subpts;
    }

    if(player==HUMAN)		// Humain cherche à faire le minimum
    {
      if(pts<mxpts) { mxpts = pts; bestcol = c;	}
    }
    else                        // Ordino cherche à faire le maximum
    {
      if(pts>mxpts) { mxpts = pts; bestcol = c; }
    }

    grid[r<<3|c] = EMPTY;		// Retire la pièce
  }

  if(lvl==0) return bestcol;
  else return mxpts;
}

int computerChoice() {
  return evalMove(0, CPU);
}

//----------------------------------------------- DISPLAY ------------------------------------------------------------

void transferByte(byte b) {
	for(byte i=0;i<8;i++) {
		delayMicroseconds(2);
		if(b & 0x80) PORTB |= 1<<PB0;
		else PORTB &= ~(1<<PB0);
		delayMicroseconds(2);
		PORTB &= ~(1<<PB2);
		delayMicroseconds(2);
		PORTB |= 1<<PB2;
		b <<= 1;
		delayMicroseconds(2);
	}
}	

void drawPixel(byte x, byte y, byte v) {
    transferByte(0xA5);
    delayMicroseconds(20);
    transferByte(y<<3|x);
    delayMicroseconds(20);
    transferByte(v);
    delayMicroseconds(20);
}

void screenSelect(byte v) {
	if(v) PORTB |= 1<<PB3;
	else PORTB &= ~(1<<PB3);
}

void drawSame(byte x, byte y, byte dx, byte dy, byte player, byte color) {
  byte nx, ny;
  nx = x;
  ny = y;
  for(;;) {
    drawPixel(nx, ny+2, color);
    nx += dx;
    //if(nx<0) return;
    if(nx>=COLS) return;
    ny += dy;
    //if(ny<0) return;
    if(ny>=ROWS) return;
    if(grid[ny<<3|nx] != player) return;    
  }
}

int drawPlay(byte player, byte x) {
  screenSelect(0);
  delayMicroseconds(20);

  int y;
  for(y=-2;y<ROWS;y++) {
    if(y>=0) {
      if(grid[y<<3|x]!=EMPTY) break;
    }
    if(y>-2) drawPixel(x, y+1, 0);
    drawPixel(x, y+2, player==0 ? 070 : 007);
    delay(220);
  }

  y--;
  grid[y<<3|x] = player;

  screenSelect(1);
  delayMicroseconds(20);
  
  byte c = check4(x, y, player);
  if(c==0) return 0;
  unsigned long m = millis();
  for(;;) {    
    byte color = (millis()-m) & 0x100 ? (player == HUMAN ? 070 : 007) : (player == HUMAN ? 010 : 001);
    switch(c) {
      case 1:		// Horrizontale
        drawSame(x, y, -1, 0, player, color);
        drawSame(x, y,  1, 0, player, color);
        break;
      case 2:		// Verticale
        drawSame(x, y, 0, -1, player, color);
        drawSame(x, y, 0,  1, player, color);
        break;
      case 3:	        // Diagonale descendente
        drawSame(x, y, -1, -1, player, color);
        drawSame(x, y,  1,  1, player, color);
        break;
      case 4:	        // Diagonale montante
        drawSame(x, y,  1, -1, player, color);
        drawSame(x, y, -1,  1, player, color);
        break;
    }
    if(readKey()!=0) {
      while(readKey()!=0);
      break;
    }
  }
  return 1;
}

void drawGame() {

  screenSelect(0);
  delayMicroseconds(20);

  for(byte y=0;y<ROWS;y++) {
    for(byte x=0;x<COLS;x++) {
      switch(grid[y<<3|x]) {
       case 0:
       case '*':
         drawPixel(x, y+2, 070);
         break;
       case 1: 
       case '#':
         drawPixel(x, y+2, 007);
         break;
       default: 
         drawPixel(x, y+2, 000);
         break;
     }
      delayMicroseconds(20);     
    }
  }

  screenSelect(1);
  delayMicroseconds(20);
}

void drawScreen(byte * screen) {
	for(byte i=0;i<(ROWS*(COLS+1));i++) grid[i] = pgm_read_byte_near(screen++);
	drawGame();
}

byte player;
void play() {
  byte win = 0;

  for(byte play_cnt=0;play_cnt<(COLS*ROWS);) {
    if(player==HUMAN) {
        byte b = readKey();      
        if(b>='1' && b<='7') {
          while(readKey()!=0);
          if(freeRow(b - '1')<ROWS) {
            win = drawPlay(player, b - '1');
            if(win) return;
            player = 1 - player;      
			play_cnt++;
          }
        }  
    }
    else {
        byte c = computerChoice();
        if(c==COLS) break;
        win = drawPlay(player, c);
        if(win) return;
        player = 1 - player;
		play_cnt++;
    }
  }
  
  delay(4000); // match null  
}

/*
byte cases[] = "-------"
               "-------"
               "-#*----"
               "-***---"
               "-##*---"
               "*###*--";
*/
const byte screenP[] PROGMEM = 
	"******--"
    "**---**-"
    "**---**-"
    "******--"
    "**------"
    "**------";               
                 
const byte screen4[] PROGMEM = 
	"##------"
    "##------"
    "##--##--"
    "#######-"
    "----##--"
    "----##--";               

void initGame() {
  for(byte y=0;y<ROWS;y++) {
    for(byte x=0;x<COLS;x++) {      
      //byte c = cases[i++];
      grid[y<<3|x] = EMPTY; // c=='*' ? HUMAN : (c=='#' ? CPU : EMPTY);
    }
  }
  drawGame();
}

int main(void)
{
	DDRB = 1<<PB0 | 1<<PB2 | 1<<PB3;
	PORTB |= 1<<PB2;

	ADMUX = (1 << MUX1); // 10bit precision, use PB4 as input pin, set refs0 and refs1 to 0 to use Vcc as Vref
	ADCSRA = (1 << ADEN) | (1<<ADPS2) | (1<<ADPS1); // enable the ADC with 64 prescalar
	
	OCR0A = 125;					// Comparateur A
	TCNT0 = 0;						// Etat initial du timer 0
	TIMSK = TIMSK | (1<<OCIE0A);    // Enable compare match interrupt
	TCCR0A = 0b00000010;			// CTC (Clear at compare)
	TCCR0B = 0b00000011;			// Prescalar 64

	sei();
	delay(500); // laisse du temps à la carte graphique de s'initialiser

    while(1)
    {
		drawScreen(screenP);
		delay(1000);
		drawScreen(screen4);
		delay(1000);
		initGame();
		player = HUMAN;
		play();
    }
}

