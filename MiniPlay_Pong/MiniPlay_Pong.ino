#include <Minitel1B_Hard.h>
#include <EEPROM.h>

// -- Uncomment your version
//#define MINIPLAY // atmega328p compatible
#define MINIDEV // atmega32u4 compatible

#define MAGIC 0x27 // for eeprom setup

// -- Debug mode for MINIDEV version only
//#define DEBUG_MODE 0 
#define DEBUG_MODE 1 // send debug msg on USB serial port
//#define DEBUG_MODE 2 // same as 1 but wait for Serial at setup

#if defined(MINIPLAY)
  #pragma message "Compiling for Atmega328p (MiniPlay)"
  #define MINITEL_PORT Serial //atmega328p
#elif defined(MINIDEV)
  #pragma message "Compiling for Atmega32u4 (MiniDev)"
  #define MINITEL_PORT Serial1 //atmega32u4
  #if DEBUG_MODE == 1 // Debug enabled
    #pragma message "DEBUG enabled"
    #define debugWait()       
    #define debugPrint(x)     Serial.print(x)
    #define debugPrintln(x)   Serial.println(x)
  #elif DEBUG_MODE == 2 // Debug blocking
    #pragma message "DEBUG2 enabled: be sure USB serial port is open"
    #define debugWait()      while (!Serial)
    #define debugPrint(x)     Serial.print(x)
    #define debugPrintln(x)   Serial.println(x)
  #else // Debug disabled (empty macros)
    #pragma message "DEBUG disabled"
    #define debugWait()      
    #define debugPrint(x)     
    #define debugPrintln(x)   
  #endif
#else
  #pragma message "Please define MINIPLAY or MINIDEV"
#endif

// SCREEN SIZE
#define WIDTH 40
#define HEIGHT 24

// GAME FIELD
#define X1 2 //player1 column
#define X2 40 //player2 column
#define XNET 21 //net column
#define SSCORE 2 //score digit space
#define YSCORE 2 //score top row
#define WSCORE 2 //score width
#define HSCORE 4 //score height
#define XSCORE1 14 //player 1 score column - align right
#define XSCORE1_U XSCORE1-WSCORE+1 //unit digit
#define XSCORE1_T XSCORE1_U-WSCORE-SSCORE //tenth digit
#define XSCORE2 26 //player 2 score column - align left
#define XSCORE2_T XSCORE2 // unit digit
#define XSCORE2_U XSCORE2+WSCORE+SSCORE // tenth digit

// PADDLE TYPES
#define WOOD   0
#define RUBBER 1
#define SMALL_PADDLE 3
#define LARGE_PADDLE 5

// PLAYER MODE
#define ROOKIE_BOT 0
#define PRO_BOT    1
#define ROBERT     2
#define HUMAN      3

#define HUMAN_VS_HUMAN        0
#define HUMAN_VS_ROOKIE_BOT   1
#define HUMAN_VS_PRO_BOT      2
#define ROOKIE_BOT_VS_PRO_BOT 3
#define HUMAN_VS_ROBERT       4

// BOT PARAMETERS
#define AGRESSIVITY  0
#define ENDURANCE    1
#define REACTIVITY   2
#define ANTICIPATION 3

// TROPHIES
#define BEAT_WOOD_ROOKIE    0b1
#define BEAT_RUBBER_ROOKIE  0b10
#define BEAT_WOOD_PRO       0b100
#define BEAT_RUBBER_PRO     0b1000
#define ALL_TROPHIES        0b1111

// BALL DIRECTION
#define DROITE    0b10000000
#define GAUCHE    0b01000000
#define TRES_BAS  0b00010000
#define BAS       0b00001000
#define DROIT     0b00000100
#define HAUT      0b00000010
#define TRES_HAUT 0b00000001

Minitel minitel(MINITEL_PORT);

bool sound = true;
uint8_t nBall = 15;
uint8_t paddle = WOOD;
uint8_t paddleSize = SMALL_PADDLE;
uint8_t halfPaddleSize = (paddleSize-1)/2;
uint8_t trophies = 0;
uint8_t gameMode = 0;
uint8_t player1 = HUMAN;
uint8_t player2 = HUMAN;
uint8_t yP1 = 13; //player1 position
uint8_t yP2 = 13; //player2 position
uint8_t xBall = 0;
uint8_t yBall = 0;
uint8_t xBallOld = 0;
uint8_t yBallOld = 0;
uint8_t ballDirection = DROITE | HAUT;
uint8_t p1 = 0; //player1 score
uint8_t p2 = 0; //player2 score
uint8_t blinkCounter = 0;
uint8_t speed = 2; //the smaller the quicker
uint8_t speedCounter = 0; 
uint8_t hitCounter = 0; //nb hit
uint8_t botStrategy = 0;

uint8_t botParams[3][4] = { // Agressivity(rubber only), Endurance (wood only), Reactivity, Anticipation
  {1,1,3,1}, // player = ROOKIE_BOT
  {3,3,2,2}, // player = PRO_BOT
  {3,3,2,2}  // player = ROBERT
};

const byte digit[10][WSCORE*HSCORE] = { // digit de 0 à 9 sur 2 cellules par 4
  // les cellules sont décrites horizontalement de gauche a droite et de haut en bas
  {0b111110,0b111101,0b101010,0b010101,0b101010,0b010101,0b101111,0b011111}, //0
  {0b000000,0b010101,0b000000,0b010101,0b000000,0b010101,0b000000,0b010101}, //1
  {0b111100,0b111101,0b001111,0b011111,0b101010,0b000000,0b101111,0b001111}, //2
  {0b111100,0b111101,0b001111,0b011111,0b000000,0b010101,0b001111,0b011111}, //3
  {0b101010,0b010101,0b101111,0b011111,0b000000,0b010101,0b000000,0b010101}, //4
  {0b111110,0b111100,0b101111,0b001111,0b000000,0b010101,0b001111,0b011111}, //5
  {0b101010,0b000000,0b101111,0b001111,0b101010,0b010101,0b101111,0b011111}, //6
  {0b111100,0b111101,0b000000,0b010101,0b000000,0b010101,0b000000,0b010101}, //7
  {0b111110,0b111101,0b101111,0b011111,0b101010,0b010101,0b101111,0b011111}, //8
  {0b111110,0b111101,0b101111,0b011111,0b000000,0b010101,0b000000,0b010101}  //9
};

// sprites vdt
typedef const PROGMEM uint8_t pgm_byte;
pgm_byte PONG_LOGO[] = {0x1B,0x54,0x20,0x12,0x41,0x1B,0x55,0x12,0x42,0x1B,0x56,0x12,0x42,0x1B,0x50,0x20,0x1B,0x57,0x12,0x43,0x1B,0x50,0x54,0x20,0x40,0x1B,0x57,0x1B,0x40,0x23,0x20,0x20,0x20,0x1B,0x50,0x1B,0x47,0x54,0x20,0x20,0x58,0x1B,0x57,0x20,0x20,0x1B,0x50,0x54,0x20,0x40,0x1B,0x57,0x1B,0x40,0x23,0x20,0x20,0x4A,0x1B,0x50,0x20,0x12,0x49,0x1B,0x54,0x12,0x42,0x1B,0x55,0x12,0x42,0x1B,0x56,0x12,0x42,0x1B,0x50,0x20,0x1B,0x57,0x20,0x1B,0x40,0x4A,0x1B,0x50,0x1B,0x47,0x22,0x1B,0x57,0x20,0x1B,0x40,0x2A,0x21,0x40,0x1B,0x50,0x1B,0x47,0x21,0x20,0x23,0x1B,0x57,0x1B,0x40,0x30,0x22,0x1B,0x50,0x1B,0x47,0x48,0x1B,0x57,0x20,0x1B,0x40,0x58,0x54,0x20,0x1B,0x50,0x1B,0x47,0x34,0x1B,0x57,0x1B,0x40,0x21,0x40,0x1B,0x50,0x1B,0x47,0x21,0x20,0x12,0x4B,0x1B,0x54,0x12,0x42,0x1B,0x55,0x12,0x42,0x1B,0x56,0x12,0x42,0x1B,0x50,0x20,0x1B,0x57,0x20,0x1B,0x40,0x2A,0x1B,0x50,0x1B,0x47,0x58,0x1B,0x57,0x20,0x1B,0x40,0x30,0x20,0x1B,0x50,0x20,0x20,0x20,0x20,0x1B,0x47,0x4A,0x1B,0x57,0x20,0x12,0x42,0x1B,0x50,0x12,0x42,0x1B,0x57,0x12,0x43,0x1B,0x50,0x12,0x4E,0x1B,0x54,0x12,0x42,0x1B,0x55,0x12,0x42,0x1B,0x56,0x12,0x42,0x1B,0x50,0x20,0x1B,0x57,0x20,0x1B,0x40,0x40,0x50,0x1B,0x50,0x1B,0x47,0x23,0x4A,0x1B,0x57,0x20,0x1B,0x50,0x30,0x20,0x12,0x42,0x1B,0x57,0x1B,0x40,0x25,0x20,0x34,0x20,0x1B,0x50,0x20,0x20,0x1B,0x57,0x20,0x20,0x20,0x1B,0x50,0x1B,0x47,0x30,0x4A,0x1B,0x57,0x20,0x1B,0x40,0x4A,0x1B,0x50,0x20,0x12,0x49,0x1B,0x54,0x12,0x42,0x1B,0x55,0x12,0x42,0x1B,0x56,0x12,0x42,0x1B,0x50,0x20,0x1B,0x57,0x20,0x1B,0x40,0x4A,0x1B,0x50,0x20,0x20,0x20,0x1B,0x57,0x30,0x22,0x1B,0x50,0x1B,0x47,0x50,0x50,0x58,0x1B,0x57,0x20,0x1B,0x50,0x25,0x4A,0x1B,0x57,0x20,0x1B,0x50,0x12,0x42,0x1B,0x57,0x20,0x1B,0x40,0x4A,0x30,0x22,0x1B,0x50,0x1B,0x47,0x50,0x1B,0x57,0x20,0x1B,0x40,0x4A,0x1B,0x50,0x1B,0x47,0x40,0x30,0x20,0x12,0x47,0x1B,0x54,0x12,0x42,0x1B,0x55,0x12,0x42,0x1B,0x56,0x12,0x42,0x1B,0x50,0x20,0x1B,0x57,0x20,0x1B,0x40,0x4A,0x1B,0x50,0x20,0x20,0x20,0x1B,0x47,0x22,0x1B,0x57,0x1B,0x40,0x50,0x20,0x20,0x40,0x58,0x1B,0x50,0x20,0x1B,0x47,0x4A,0x1B,0x57,0x20,0x1B,0x50,0x12,0x42,0x1B,0x57,0x20,0x1B,0x40,0x4A,0x1B,0x50,0x1B,0x47,0x22,0x1B,0x57,0x1B,0x40,0x50,0x20,0x20,0x4A,0x32,0x45};
pgm_byte WOOD_ROOKIE_TROPHY[] = {0x1B,0x43,0x40,0x50,0x1B,0x47,0x30,0x0A,0x08,0x08,0x08,0x08,0x1B,0x43,0x58,0x1B,0x57,0x1B,0x42,0x21,0x20,0x1B,0x45,0x22,0x1B,0x50,0x1B,0x47,0x54,0x0A,0x08,0x08,0x08,0x08,0x08,0x1B,0x56,0x4A,0x1B,0x55,0x34,0x1B,0x57,0x1B,0x45,0x48,0x1B,0x43,0x4A,0x1B,0x42,0x4A,0x0A,0x08,0x08,0x08,0x08,0x08,0x1B,0x57,0x1B,0x40,0x54,0x20,0x1B,0x45,0x21,0x21,0x1B,0x40,0x58,0x0A,0x08,0x08,0x08,0x08,0x1B,0x50,0x1B,0x45,0x42,0x1B,0x57,0x1B,0x46,0x25,0x1B,0x50,0x1B,0x42,0x21,0x0A,0x08,0x08,0x08,0x08,0x1B,0x41,0x58,0x1B,0x53,0x1B,0x44,0x21,0x1B,0x57,0x1B,0x46,0x21,0x1B,0x40,0x22,0x1B,0x50,0x1B,0x45,0x30};
pgm_byte RUBBER_ROOKIE_TROPHY[] = {0x1B,0x43,0x40,0x50,0x1B,0x47,0x30,0x0A,0x08,0x08,0x08,0x08,0x1B,0x43,0x58,0x1B,0x57,0x1B,0x42,0x21,0x50,0x1B,0x45,0x22,0x1B,0x50,0x1B,0x47,0x54,0x0A,0x08,0x08,0x08,0x08,0x08,0x1B,0x56,0x4A,0x1B,0x57,0x1B,0x45,0x4A,0x1B,0x46,0x28,0x31,0x1B,0x42,0x4A,0x0A,0x08,0x08,0x08,0x08,0x08,0x1B,0x57,0x1B,0x40,0x54,0x1B,0x45,0x22,0x20,0x22,0x1B,0x40,0x58,0x0A,0x08,0x08,0x08,0x08,0x1B,0x50,0x1B,0x45,0x42,0x1B,0x57,0x1B,0x46,0x25,0x1B,0x50,0x1B,0x42,0x31,0x0A,0x08,0x08,0x08,0x08,0x1B,0x41,0x48,0x1B,0x53,0x1B,0x44,0x21,0x1B,0x57,0x1B,0x46,0x51,0x1B,0x44,0x52,0x1B,0x50,0x1B,0x45,0x34};
pgm_byte WOOD_PRO_TROPHY[] = {0x40,0x50,0x12,0x43,0x1B,0x43,0x50,0x1B,0x47,0x30,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x52,0x1B,0x57,0x1B,0x43,0x48,0x1B,0x46,0x48,0x1B,0x42,0x40,0x1B,0x46,0x48,0x1B,0x43,0x21,0x1B,0x50,0x1B,0x47,0x51,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x1B,0x53,0x1B,0x40,0x4A,0x1B,0x57,0x34,0x1B,0x45,0x32,0x26,0x1B,0x42,0x26,0x1B,0x40,0x48,0x1B,0x50,0x1B,0x47,0x4A,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x22,0x1B,0x42,0x2C,0x1B,0x53,0x1B,0x40,0x54,0x1B,0x57,0x1B,0x43,0x30,0x1B,0x40,0x58,0x1B,0x50,0x1B,0x47,0x2C,0x21,0x0A,0x08,0x08,0x08,0x08,0x08,0x1B,0x46,0x58,0x1B,0x57,0x1B,0x43,0x41,0x1B,0x50,0x1B,0x47,0x54};
pgm_byte RUBBER_PRO_TROPHY[] = {0x1B,0x43,0x40,0x1B,0x47,0x50,0x12,0x43,0x1B,0x43,0x50,0x1B,0x47,0x30,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x1B,0x42,0x52,0x1B,0x53,0x1B,0x47,0x4A,0x1B,0x57,0x1B,0x46,0x48,0x1B,0x43,0x2C,0x30,0x25,0x1B,0x50,0x1B,0x47,0x51,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x1B,0x52,0x1B,0x40,0x4A,0x1B,0x53,0x34,0x1B,0x52,0x1B,0x47,0x45,0x1B,0x57,0x1B,0x46,0x23,0x1B,0x42,0x24,0x1B,0x40,0x48,0x1B,0x50,0x1B,0x47,0x4A,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x1B,0x46,0x22,0x1B,0x45,0x2C,0x1B,0x56,0x1B,0x40,0x54,0x1B,0x57,0x1B,0x43,0x30,0x1B,0x40,0x58,0x1B,0x50,0x1B,0x47,0x2C,0x21,0x0A,0x08,0x08,0x08,0x08,0x08,0x1B,0x46,0x50,0x1B,0x53,0x31,0x1B,0x50,0x1B,0x47,0x50};
pgm_byte LAST_TROPHY[] = {0x1B,0x46,0x50,0x1B,0x57,0x1B,0x45,0x34,0x1B,0x56,0x1B,0x47,0x4A,0x1B,0x57,0x20,0x1B,0x53,0x50,0x1B,0x57,0x1B,0x45,0x28,0x1B,0x50,0x1B,0x43,0x50,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x1B,0x55,0x1B,0x40,0x2A,0x1B,0x50,0x1B,0x47,0x2A,0x1B,0x52,0x26,0x1B,0x57,0x1B,0x43,0x50,0x1B,0x53,0x1B,0x47,0x2A,0x1B,0x50,0x25,0x1B,0x54,0x4A,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x1B,0x52,0x1B,0x40,0x54,0x1B,0x50,0x1B,0x43,0x54,0x1B,0x47,0x22,0x1B,0x57,0x1B,0x45,0x31,0x1B,0x50,0x1B,0x47,0x21,0x58,0x1B,0x57,0x1B,0x40,0x58,0x0A,0x08,0x08,0x08,0x08,0x08,0x08,0x1B,0x50,0x1B,0x43,0x22,0x1B,0x55,0x1B,0x40,0x34,0x1B,0x57,0x1B,0x42,0x21,0x1B,0x52,0x1B,0x40,0x48,0x1B,0x50,0x1B,0x47,0x21,0x0A,0x08,0x08,0x08,0x08,0x1B,0x53,0x1B,0x46,0x34,0x1B,0x51,0x1B,0x43,0x52,0x1B,0x53,0x1B,0x47,0x23};
pgm_byte ROBERT_BODY[] = {0x1F,0x54,0x48,0x0E,0x1B,0x53,0x1B,0x40,0x51,0x1B,0x50,0x1B,0x46,0x25,0x2A,0x1B,0x53,0x1B,0x40,0x50,0x1B,0x50,0x1B,0x41,0x24,0x1E,0x1F,0x53,0x48,0x0E,0x1B,0x42,0x4A,0x1B,0x51,0x1B,0x40,0x4A,0x1B,0x50,0x1B,0x44,0x4A,0x1B,0x52,0x1B,0x40,0x4A,0x1F,0x52,0x45,0x0E,0x1B,0x44,0x2A,0x1B,0x53,0x52,0x1B,0x50,0x1B,0x41,0x21,0x1B,0x45,0x4A,0x1B,0x51,0x1B,0x40,0x48,0x34,0x1B,0x55,0x4A,0x1B,0x50,0x1B,0x44,0x22,0x1B,0x56,0x51,0x1B,0x50,0x1B,0x41,0x25,0x1F,0x51,0x45,0x0E,0x1B,0x44,0x4A,0x1B,0x53,0x1B,0x45,0x4C,0x1B,0x40,0x4A,0x1B,0x57,0x1B,0x41,0x30,0x1B,0x46,0x50,0x50,0x1B,0x45,0x40,0x1B,0x52,0x1B,0x40,0x34,0x1B,0x51,0x1B,0x43,0x43,0x1B,0x40,0x4A,0x1F,0x50,0x46,0x0E,0x1B,0x51,0x1B,0x42,0x2A,0x1B,0x55,0x1B,0x44,0x4A,0x1B,0x53,0x1B,0x47,0x2A,0x1B,0x57,0x1B,0x41,0x41,0x41,0x21,0x1B,0x51,0x1B,0x44,0x4A,0x1B,0x52,0x1B,0x41,0x52,0x1F,0x4F,0x46,0x0E,0x1B,0x45,0x40,0x1B,0x52,0x1B,0x41,0x41,0x1B,0x53,0x1B,0x45,0x22,0x1B,0x55,0x1B,0x43,0x50,0x50,0x1B,0x57,0x1B,0x42,0x25,0x1B,0x51,0x48,0x1B,0x50,0x1B,0x45,0x34,0x1F,0x4E,0x47,0x0E,0x1B,0x46,0x48,0x1B,0x56,0x1B,0x40,0x23,0x1B,0x41,0x12,0x42,0x1B,0x53,0x1B,0x40,0x23,0x1B,0x55,0x23,0x1F,0x4D,0x47,0x0E,0x1B,0x45,0x22,0x1B,0x53,0x1B,0x44,0x32,0x1B,0x45,0x23,0x23,0x1B,0x41,0x41,0x1B,0x50,0x1B,0x46,0x21,0x1F,0x4C,0x47,0x0E,0x1B,0x51,0x1B,0x42,0x4A,0x1B,0x57,0x1B,0x44,0x4A,0x1B,0x54,0x1B,0x45,0x25,0x2A,0x1B,0x46,0x4A,0x1B,0x53,0x1B,0x45,0x4A,0x1F,0x4B,0x47,0x0E,0x1B,0x42,0x40,0x1B,0x57,0x1B,0x44,0x41,0x1B,0x53,0x50,0x50,0x1B,0x57,0x32,0x1B,0x50,0x1B,0x43,0x34,0x1F,0x4A,0x48,0x0E,0x1B,0x41,0x40,0x50,0x1B,0x51,0x1B,0x40,0x2A,0x1B,0x50,0x1B,0x41,0x30,0x1F,0x49,0x4A,0x0E,0x1B,0x44,0x30};
pgm_byte ROBERT_HEAD[] = {0x1B,0x57,0x1B,0x40,0x41,0x1B,0x53,0x50,0x50,0x1B,0x57,0x32,0x0A,0x08,0x08,0x08,0x08,0x1B,0x57,0x1B,0x40,0x4A,0x1B,0x50,0x1B,0x47,0x25,0x2A,0x1B,0x46,0x4A,0x0A,0x08,0x08,0x08,0x08,0x1B,0x53,0x1B,0x40,0x30,0x23,0x23,0x40};

void setup() {

  // set debug port
  debugWait();
  debugPrintln("debug port ready");

  // set minitel at 4800 bauds, or fall back to default
  delay(500);
  if (minitel.searchSpeed() != 4800) {     // search speed
    if (minitel.changeSpeed(4800) < 0) {   // set to 4800 if different
      minitel.searchSpeed();               // search speed again if change has failed
    }
  }

  // set minitel mode
  minitel.modeVideotex();
  minitel.echo(false);
  minitel.noCursor();
  minitel.extendedKeyboard();
  minitel.capitalMode();
  eraseScreen();
  debugPrintln("minitel ready");

  // EEPROM data
  setupEEPROM();
  trophies = EEPROM.read(0);
  debugPrint("trophies: ");debugPrintln(trophies);
  
  // wait CRT warming
  waitingBar();

}

void waitingBar() {
  int8_t x = 14;
  int8_t y = 10;
  delay(1000);
  minitel.newXY(x+1,y-1); minitel.print("wArMinG uP");
  minitel.newXY(x,y); minitel.print("_"); minitel.repeat(11);
  minitel.newXY(x-1,y+1); minitel.print("}");
  minitel.newXY(x+12,y+1); minitel.print("{");
  minitel.newXY(x,y+2); minitel.print("~"); minitel.repeat(11);
  minitel.newXY(x,y+1); minitel.attributs(CARACTERE_VERT); minitel.attributs(INVERSION_FOND);
  while (x++ < 26) { 
    minitel.printChar(' '); 
    delay(500);
  }
  minitel.newXY(15,y-1);
  while (x++ < 40) {
    minitel.printChar(' ');
    delay(25);
  }
  delay(250);
}

void loop() {
  welcome();
  beginGame();
  while (p1+p2 < nBall) playGame();
  endGame();
}

void welcome() {

  displayWelcome();

  unsigned long key = minitel.getKeyCode();
  while (true) {
    if (key == 0x1341) break; // Envoi
    else if (key == 0x1346) { // Sommaire
      displayOptions();
      // APPUYER SUR ENVOI ?
    }
    else if (key == 0x1344) { // Guide
      displayHowTo();
      displayWelcome();
    }
    key = minitel.getKeyCode();
  }
}

void beginGame() {
  //clean up
  eraseScreen();
  minitel.graphicMode();

  //draw game field
  drawGameField();
  debugPrintln("game field done");

  // get new seed for random numbers
  randomSeed(millis());
  //randomSeed(micros());
  debugPrintln("random seed set");

  debugPrint("Player1 type: ");debugPrintln(player1);
  if (player1 != HUMAN) {
    debugPrint(" - anticipation: "); debugPrintln(botParams[player1][ANTICIPATION]);
    debugPrint(" - reactivity:   "); debugPrintln(botParams[player1][REACTIVITY]);
    debugPrint(" - endurance:    "); debugPrintln(botParams[player1][ENDURANCE]);
    debugPrint(" - agressivity:  "); debugPrintln(botParams[player1][AGRESSIVITY]);
  }
  debugPrint("player2 type: ");debugPrintln(player2);
  if (player2 != HUMAN) {
    debugPrint(" - anticipation: "); debugPrintln(botParams[player2][ANTICIPATION]);
    debugPrint(" - reactivity:   "); debugPrintln(botParams[player2][REACTIVITY]);
    debugPrint(" - endurance:    "); debugPrintln(botParams[player2][ENDURANCE]);
    debugPrint(" - agressivity:  "); debugPrintln(botParams[player2][AGRESSIVITY]);
  }

  //init game
  startNewBall();
  debugPrintln("init game done");
}

void endGame() {  
  // animation ball missed
  blinkCounter = 20;
  while (blinkCounter > 0) {blinkBall(); delay(50);}

  //Blink winner score 
  minitel.attributs(CLIGNOTEMENT);
  if (p1>p2) drawScore1(p1);
  else drawScore2(p2);
  minitel.attributs(FIXE);
  buzzer();buzzer();buzzer();delay(1000);

  // Display winner message
  eraseBall(xBall, yBall);
  minitel.attributs(CARACTERE_BLANC);
  minitel.graphic(0b000000, XNET, 15);
  minitel.graphic(0b000000, XNET, 23);
  minitel.textMode();
  minitel.newXY(6,17);
  minitel.attributs(DOUBLE_GRANDEUR);
  minitel.print("—");minitel.repeat(14);
  minitel.newXY(8,19);
  minitel.attributs(DOUBLE_GRANDEUR);
  minitel.print("PLAYER-"+ String((p1>p2)?1:2) + " WINS");
  minitel.newXY(6,21);
  minitel.attributs(DOUBLE_GRANDEUR);
  minitel.print("—");minitel.repeat(14);
  minitel.graphicMode();
  buzzer(); buzzer();

  // Erase loser's paddle
  byte charColor = CARACTERE_BLANC+4;
  uint8_t yP = (p1>p2)?yP2:yP1;
  uint8_t X = (p1>p2)?X2:X1;
  while (charColor > CARACTERE_NOIR) {
    //debugPrintln(charColor);
    if (charColor > CARACTERE_JAUNE) charColor-=4;
    else charColor+=3;
    minitel.attributs(charColor);
    for (int8_t i = -(paddleSize-1)/2; i < (paddleSize+1)/2; i++) {
      minitel.graphic(0b111111,X,yP+i);
    }
    delay(200);
  }

  checkTrophies();

  //displayCup();

  // reset variables
  p1 = 0;
  p2 = 0;
  yP1 = 13;
  yP2 = 13;
  xBall = 0;
  yBall = 0;

  flushInputs();
  while (!minitel.getKeyCode());
  buzzer();
}

void checkTrophies() {
  if (player2 == HUMAN) return;
  if (trophies == ALL_TROPHIES) return;
  if (p1<p2) return;

  uint8_t trophy = 0;
  if (paddle == WOOD) trophy = (player2 == ROOKIE_BOT)? BEAT_WOOD_ROOKIE : BEAT_WOOD_PRO;
  else trophy = (player2 == ROOKIE_BOT)? BEAT_RUBBER_ROOKIE : BEAT_RUBBER_PRO;

  if (!(trophies & trophy)) {
    if (nBall < 15) {
      printStatus("Play with 15 balls for trophy", CARACTERE_BLANC);
      return;
    }
    // new trophy !
    trophies |= trophy;
    EEPROM.update(0,trophies);
    delay(1000); buzzer();
    displayCup(trophy); buzzer();
    delay(1000); buzzer();
    minitel.graphic(0b000000, XNET, 1);
    if (trophy == BEAT_WOOD_ROOKIE) printStatus("Rubber paddle unlocked!", CARACTERE_VERT);
    else if (trophy == BEAT_RUBBER_ROOKIE) printStatus("Human vs ProBot unlocked!", CARACTERE_VERT);
    else if (trophy == BEAT_WOOD_PRO) printStatus("Robot vs ProBot unlocked!", CARACTERE_VERT);
    else if (trophy == BEAT_RUBBER_PRO) printStatus("Balls number unlocked!", CARACTERE_VERT);
    if (trophies == ALL_TROPHIES) {
      while (!minitel.getKeyCode());
      buzzer();
      //printStatus("Got all trophies, Félicitations !", INVERSION_FOND);
      printStatus("OMG, You Beat Them All !!!", INVERSION_FOND);
      eraseCup();
      while (!minitel.getKeyCode());
      buzzer();
      eraseStatus();
      displayCup(ALL_TROPHIES); buzzer();
      delay(1000); buzzer();
      printStatus("Custom bot Robert unlocked!", CARACTERE_VERT);
    }
  }
}

void displayCup(uint8_t trophy) {
  minitel.attributs(CARACTERE_BLANC);
  minitel.graphic(0b000000, XNET, 7);
  minitel.newXY(17,9);
  minitel.textMode();
  minitel.print("NEW TROPHY");
  minitel.newXY(18,11);
  minitel.graphicMode();
  drawTrophy(trophy);
}

void eraseCup() {
  minitel.newXY(17,9); minitel.printChar(' '); minitel.repeat(9);
  minitel.newXY(18,11); minitel.repeat(7);
  minitel.newXY(18,12); minitel.repeat(7);
  minitel.newXY(18,13); minitel.repeat(7);
  minitel.newXY(18,14); minitel.repeat(7);
  minitel.newXY(20,15); minitel.repeat(3);
}

void drawTrophy(uint8_t trophy) {
  switch (trophy) {
    case BEAT_WOOD_ROOKIE:
      for (int i=0; i<sizeof(WOOD_ROOKIE_TROPHY); i++) {
        minitel.writeByte(pgm_read_byte_near(WOOD_ROOKIE_TROPHY + i));
      }
      break;
    case BEAT_RUBBER_ROOKIE:
      for (int i=0; i<sizeof(RUBBER_ROOKIE_TROPHY); i++) {
        minitel.writeByte(pgm_read_byte_near(RUBBER_ROOKIE_TROPHY + i));
      }
      break;
    case BEAT_WOOD_PRO:
      for (int i=0; i<sizeof(WOOD_PRO_TROPHY); i++) {
        minitel.writeByte(pgm_read_byte_near(WOOD_PRO_TROPHY + i));
      }
      break;
    case BEAT_RUBBER_PRO:
      for (int i=0; i<sizeof(RUBBER_PRO_TROPHY); i++) {
        minitel.writeByte(pgm_read_byte_near(RUBBER_PRO_TROPHY + i));
      }
      break;
    case ALL_TROPHIES:
      for (int i=0; i<sizeof(LAST_TROPHY); i++) {
        minitel.writeByte(pgm_read_byte_near(LAST_TROPHY + i));
      }
      break;
  }
}

void buzzer() {
  if (sound) minitel.bip();
  else delay(500);
  delay(500);
  flushInputs();
}

void handlePlayer() {
  int8_t dy1 = 0;
  int8_t dy2 = 0;
  
  byte key = getKeyCodeOverride();

  if (player1 == HUMAN) {
    if ((key == 'Q' || key == 'S') && yP1 > halfPaddleSize+1) dy1--;
    if (key == 'W' && yP1 < HEIGHT - halfPaddleSize) dy1++;
  } 
  else {
    dy1 = computeMove1();
  }

  if (player2 == HUMAN) {
    if ((key == 'H' || key == 'J') && yP2 > halfPaddleSize+1) dy2--;
    if (key == 'N' && yP2 < HEIGHT - halfPaddleSize) dy2++;
  } 
  else {
    dy2 = computeMove2();
  }
  
  minitel.graphic(0b111111, X1, yP1 + (halfPaddleSize+1)*dy1);
  minitel.moveCursorXY(X1, yP1- halfPaddleSize*dy1);
  if (dy1!=0) minitel.graphic(0b000000);
  else minitel.graphic(0b111111); //preserve frame rate
  yP1+=dy1;

  minitel.graphic(0b111111, X2, yP2 + (halfPaddleSize+1)*dy2);
  minitel.moveCursorXY(40, yP2 - halfPaddleSize*dy2);
  if (dy2!=0) minitel.graphic(0b000000);
  else minitel.graphic(0b111111); //preserve frame rate
  yP2+=dy2;
}

int8_t computeMove1() {
  if (ballDirection & DROITE) return 0;
  if (xBall == X2-1) setStrategy(player1);
  uint8_t reactivity = botParams[player1][REACTIVITY];
  if (xBall > X2-14+2*reactivity) return 0; // X2-2 maxi
  int8_t move = computeBallLanding(xBall-(X1+1), player1);
  move = computeMove(move-yP1);
  return checkBoundary(move, yP1);
}

int8_t computeMove2() {
  if (ballDirection & GAUCHE) return 0;
  if (xBall == X1+1) setStrategy(player2);
  uint8_t reactivity = botParams[player2][REACTIVITY];
  if (xBall < X1+14-2*reactivity) return 0; // X1+2 mini
  int8_t move = computeBallLanding((X2-1)-xBall, player2);
  move = computeMove(move-yP2);
  return checkBoundary(move, yP2);
}

int8_t checkBoundary(int8_t dy, uint8_t yP) {
  if ((dy<0) && (yP <= halfPaddleSize + 1)) return 0;
  if ((dy>0) && (yP >= HEIGHT - halfPaddleSize)) return 0;
  return dy;
}

void setStrategy(uint8_t botLevel) {
  if (speedCounter == 0) {
    if (paddle==WOOD) {
      uint8_t endurance = botParams[botLevel][ENDURANCE];
      endurance += 4;
      if (hitCounter < endurance) botStrategy = 0;
      else botStrategy = (random(endurance)==0)?4:0;
    }
    else { // RUBBER
      uint8_t agressivity = botParams[botLevel][AGRESSIVITY];
      botStrategy = (random(4) < (4-agressivity))?2:0;
      botStrategy += random(2);
    }
    debugPrintln();debugPrint("> strategy: ");debugPrintln(botStrategy);
  }
}

int8_t computeMove(int8_t DY) {
  switch (botStrategy) {
    case 0: //minimal move
      if (abs(DY) <= halfPaddleSize) return 0;
      else if (DY>0) return 1;
      else return -1;
      break;
    case 1: //big lift
      if (abs(DY) == halfPaddleSize) return 0;
      else if (DY > halfPaddleSize) return 1;
      else if (DY < -halfPaddleSize) return -1;
      else if (ballDirection >= 0) return 1;
      else return -1;
      break;
    case 2: //lift
      if (abs(DY) == 1) return 0;
      else if (DY>0) return 1;
      else if (DY<0) return -1;
      else if (ballDirection >= 0) return 1;
      else return -1;
      break;
    case 3: //center
      if (DY == 0) return 0;
      else if (DY>0) return 1;
      else return -1;
      break;
    case 4: //missed (wood only)
      if (abs(DY) == halfPaddleSize+1) return 0;
      else if (DY > halfPaddleSize+1) return 1;
      else if (DY < -halfPaddleSize-1) return -1;
      else if (ballDirection >= 0) return -1;
      else return 1;
      break;
  }
}

int8_t computeBallLanding(uint8_t DX, uint8_t botLevel) {
  int8_t nStep = DX/2; // steps from ball to border
  uint8_t anticipation = botParams[botLevel][ANTICIPATION];
  anticipation = anticipation+2;
  nStep = min(nStep, anticipation);
  int8_t direction = 0; // direction DROIT
  if (ballDirection & HAUT) direction = -1;
  else if (ballDirection & BAS) direction = 1;
  else if (ballDirection & TRES_HAUT) direction = -2;
  else if (ballDirection & TRES_BAS) direction = 2;
  int8_t y = yBall + direction*nStep;
  y--; // {1, .., 24} --> {0, .., 23}
  int8_t height = 2*HEIGHT-2; // --> {0, ..,22, 23, 22, .., 0}
  if (abs(direction) == 2) { // account for inexact bounces
    if ((y > height) && (y%2==0)) y--;
    if (y > HEIGHT-1) y--;
    if ((y <= -(HEIGHT-1)) && (y%2==1)) y++;
    if (y < 0) y++;
  }
  y = (y+height)%height;
  if (y<0) y+height; // positive remainder
  if (y > HEIGHT-1) y = height - y; // {22, .., 0} --> {0, .., 22}
  y++; // {0, .., 23} --> {1, .., 24}
  //debugPrint("> yP2=");debugPrint(yP2);
  //debugPrint(", xBall=");debugPrint(xBall);
  //debugPrint(", yBall=");debugPrint(yBall);
  //debugPrint(", direction=");debugPrint(direction);
  //debugPrint(", yEnd=");debugPrintln(y);
  return y;
}

void playGame() {

  handlePlayer();

  // animation when ball is missed
  if (blinkCounter > 0) {
    blinkBall();
    return;
  }

  // speed effect : bypass ball move to slow down
  if (speedCounter < speed) {
    speedCounter++;
    return;
  }
  speedCounter=0;     
  
  xBallOld = xBall;
  yBallOld = yBall;

  // move ball
  if (ballDirection & DROITE) xBall+=2;
  if (ballDirection & GAUCHE) xBall-=2;

  if (ballDirection & HAUT) yBall--;
  if (ballDirection & BAS)  yBall++;
  if (ballDirection & TRES_HAUT) yBall = (yBall > 2) ? yBall-2 : 1;
  if (ballDirection & TRES_BAS)  yBall = (yBall < HEIGHT-1) ? yBall+2 : HEIGHT;

  //erase old ball
  eraseBall(xBallOld, yBallOld);
  //draw new ball
  minitel.graphic(0b111111, xBall, yBall);      

  // top bounce
  if(yBall == 1) {
    if(ballDirection & HAUT) {
      ballDirection &= ~HAUT;
      ballDirection |= BAS;
    }
    else if(ballDirection & TRES_HAUT) {
      ballDirection &= ~TRES_HAUT;
      ballDirection |= TRES_BAS;
    }
  }

  // bottom bounce
  if(yBall  == HEIGHT) {
    if (ballDirection & BAS) {
      ballDirection &= ~BAS;
      ballDirection |= HAUT;
    }
    else if(ballDirection & TRES_BAS) {
      ballDirection &= ~TRES_BAS;
      ballDirection |= TRES_HAUT;
    }
  }

  // player1 side
  if (xBall <= X1 + 2) {
    // player1 send back
    if(abs(yBall-yP1) < halfPaddleSize + 1) {
      // change direction
      if (paddle == WOOD) {
        ballDirection &= ~GAUCHE;
        ballDirection |= DROITE;
        hitCount();
      }
      else { // paddle == RUBBER
        ballDirection = DROITE;
        rubberBounce(yBall-yP1);
      }
    }
    else { // player2 win
      buzzer();
      p2++;
      drawScore2(p2);
      startNewBall();
    }
  }

  // player2 side
  if (xBall >= X2 - 2) {
    // player2 send back
    if(abs(yBall-yP2) < halfPaddleSize + 1) {
      // change direction
      if (paddle == WOOD) {
        ballDirection &= ~DROITE;
        ballDirection |= GAUCHE;
        hitCount();
      }
      else { // paddle == RUBBER
        ballDirection = GAUCHE;
        rubberBounce(yBall-yP2);
      }
    }
    else{ // player1 win
      buzzer();
      p1++;
      drawScore1(p1);
      startNewBall();
    }
  }
}

void rubberBounce(int8_t DY) {
  if (DY == 0) {
    ballDirection |= DROIT;
    speed = 2;
  } else if (DY == 1) {
    ballDirection |= BAS;
    speed = 1;
  } else if (DY == -1) {
    ballDirection |= HAUT;
    speed = 1;
  } else if (DY == 2) {
    ballDirection |= TRES_BAS;
    speed = 0;
  } else if (DY == -2) {
    ballDirection |= TRES_HAUT;
    speed = 0;
  }
}

void hitCount() {
  hitCounter++;
  if (hitCounter == 3) speed=1;
  else if (hitCounter == 6) speed=0;
}

void eraseBall(int x, int y) {
  // erase ball preserving game field and score
  minitel.moveCursorXY(x,y);
  bool erase = true;
  if (x == XNET) { // Ball in net
    if (y%2 == 1) erase = false;
  }
  if (y >= YSCORE && y < YSCORE + HSCORE) { // Ball in score
    if (x >= XSCORE1_T && x < XSCORE1_T + WSCORE) { //ball in score1 tenth
      if (p1 >= 10) {
        drawDigit(p1/10, XSCORE1_T, YSCORE);
        erase = false;
      }
    }
    if (x >= XSCORE1_U && x < XSCORE1_U + WSCORE) { //ball in score1 unit
      drawDigit(p1%10, XSCORE1_U, YSCORE);
      erase = false;
    }
    if (x >= XSCORE2_T && x < XSCORE2_T + WSCORE) { //ball in score2 tenth
      if (p2 >= 10) {
        drawDigit(p2/10, XSCORE2_T, YSCORE);
        erase = false;
      }
    }
    if (x >= XSCORE2_U && x < XSCORE2_U + WSCORE) { //ball in score2 unit
      drawDigit(p2%10, XSCORE2_U, YSCORE);
      erase = false;
    }
  }
  if (erase) minitel.graphic(0b000000);
}

void startNewBall() {

  // init counters
  hitCounter = 0;
  speedCounter = 0;

  // blink animation for previous missed ball
  if (xBall != 0) blinkCounter = 10;
  else blinkCounter = 0; // no ball has been played yet
  
  // choose random player for first ball or serve on winner side
  if (xBall == 0) ballDirection = (random(2)) ? DROITE : GAUCHE;
  else ballDirection = (xBall <= X1 + 2) ? GAUCHE : DROITE;

  // set strategy if receiver is a robot
  if (ballDirection == DROITE) {
    if (player2 != HUMAN) setStrategy(player2);
  } else {
    if (player1 != HUMAN) setStrategy(player1);
  }
  
  // add random inclination to the ball
  if (paddle == WOOD) {
    ballDirection |= (random(2)) ? HAUT : BAS;
    speed = 2;
  }
  else { // RUBBER mode
    ballDirection |= (1 << random(6));
    speed = random(5);
    if (speed>0) speed = speed%2+1; // 1/5 is quick
    //if (ballDirection & TRES_HAUT) speed = 0;
    //if (ballDirection & TRES_BAS) speed = 0;
    //if (ballDirection & HAUT) speed = 1;
    //if (ballDirection & BAS) speed = 1;
    //if (ballDirection & DROIT) speed = 2;
  }

  // get random starting point
  xBallOld = xBall;
  yBallOld = yBall;
  xBall = random(3,7)*2+1;
  if (ballDirection & GAUCHE) xBall = 40-xBall;
  yBall = random(5,20);


  // draw new ball
  minitel.graphic(0b111111, xBall, yBall);

}

void blinkBall() {
  if (blinkCounter%4 == 0) { // 8 & 4
    minitel.attributs(CARACTERE_BLEU);
    minitel.graphic(0b111111, xBallOld, yBallOld);
    minitel.attributs(CARACTERE_BLANC);
  }
  else if (blinkCounter%2 == 0) { // 6 & 2
    minitel.attributs(CARACTERE_VERT);
    minitel.graphic(0b111111, xBallOld, yBallOld);
    minitel.attributs(CARACTERE_BLANC);
  }
  else if (blinkCounter == 1) {
    minitel.graphic(0b000000, xBallOld, yBallOld); //erase ball
  }
  blinkCounter--;
}

byte getKeyCodeOverride() {
  byte b = 255;
  if (MINITEL_PORT.available()) {
    b = MINITEL_PORT.read();
    MINITEL_PORT.flush();   
    //debugPrint(b);
  }
  return b;
}

void drawGameField() {
  //draw net
  for (uint8_t i = 1; i < HEIGHT; i+=2) {
    minitel.graphic(0b111111, XNET, i);
  }
  //draw players
  for (int8_t i = -halfPaddleSize; i <= halfPaddleSize; i++) {
    minitel.graphic(0b111111,X1,yP1+i);
    minitel.graphic(0b111111,X2,yP2+i);
  }
  // draw score
  drawScore1(p1);
  drawScore2(p2);
}

void drawScore1(uint8_t score) {
  drawDigit(score%10, XSCORE1_U, YSCORE);
  if (score>=10) drawDigit(score/10, XSCORE1_T, YSCORE);
}

void drawScore2(uint8_t score) {
  drawDigit(score%10, XSCORE2_U, YSCORE);
  if (score>=10) drawDigit(score/10, XSCORE2_T, YSCORE);
}

void drawDigit(uint8_t num, uint8_t x, uint8_t y) {
  for (uint8_t i = 0; i < WSCORE; i++) {
    for (uint8_t j = 0; j < HSCORE; j++) {
      minitel.graphic(digit[num][i+WSCORE*j],x+i,y+j);
    }
  }
}

//----- Display screens & menus  -----------------------

void eraseScreen() {
  minitel.newXY(1,0); minitel.cancel(); // erase status row 0
  minitel.newScreen(); // erase rows 1 to 24
}

void flushInputs() {
  if (minitel.getKeyCode()) {
    while (minitel.getKeyCode()) {};
  }
}

void displayWelcome() {

  eraseScreen();
  minitel.textMode();

  // display welcome at low speed for visual effect
  if (minitel.changeSpeed(1200) < 0) minitel.searchSpeed();
  
  minitel.newXY(14,2);minitel.attributs(CARACTERE_VERT);
  minitel.attributs(DOUBLE_LARGEUR);minitel.printChar('M');minitel.attributs(GRANDEUR_NORMALE);minitel.print("initel ");
  minitel.attributs(DOUBLE_LARGEUR);minitel.printChar('P');minitel.attributs(GRANDEUR_NORMALE);minitel.print("lay ");
  minitel.newXY(17,4);minitel.attributs(CARACTERE_BLEU);minitel.print("presents");

  // put speed back to 4800 for logo
  if (minitel.changeSpeed(4800) < 0) minitel.searchSpeed();
  flushInputs();
  
  minitel.newXY(5,8); minitel.graphicMode();
  for (int i=0; i<sizeof(PONG_LOGO); i++) {
    minitel.writeByte(pgm_read_byte_near(PONG_LOGO + i));
  }

  minitel.newXY(1,23);minitel.attributs(CARACTERE_VERT);minitel.printChar(' ');
    minitel.attributs(INVERSION_FOND); minitel.print("GUIDE");
    minitel.attributs(FOND_NORMAL); minitel.print("    Aide");
  minitel.moveCursorReturn(1);minitel.printChar(' ');
    minitel.attributs(INVERSION_FOND);minitel.print("SOMMAIRE");
    minitel.attributs(FOND_NORMAL); minitel.print(" Options");
  minitel.newXY(32,23);minitel.attributs(CARACTERE_MAGENTA);minitel.print("iodeo.fr");
  minitel.newXY(32,24);minitel.attributs(CARACTERE_MAGENTA);minitel.print("(C) 2024");

  buzzer();

  minitel.newXY(12,19);minitel.attributs(CARACTERE_JAUNE);minitel.attributs(CLIGNOTEMENT);
    minitel.print("APPUYER SUR ENVOI");

}

void displayHowTo() {
  eraseScreen();
  minitel.newXY(1,0); minitel.cancel(); // erase status row
  minitel.newXY(1,1); minitel.attributs(DOUBLE_GRANDEUR); minitel.attributs(INVERSION_FOND);
  minitel.printChar(' '); minitel.repeat(7); minitel.print(F("PONG"));minitel.cancel();
  minitel.newXY(1,4); minitel.attributs(INVERSION_FOND); minitel.print(F(" BUT DU JEU "));
  minitel.newXY(1,5); minitel.print(F("Une partie se joue en 15 balles."));
  minitel.newXY(1,6); minitel.print(F("A la fin de la partie, celui qui a"));
  minitel.newXY(1,7); minitel.print(F("le plus de points gagne !"));
  minitel.newXY(1,9); minitel.attributs(INVERSION_FOND); minitel.print(F(" COMMENT JOUER ? "));
  minitel.newXY(1,10); minitel.print(F("Utilisez les touches clavier pour"));
  minitel.newXY(1,11); minitel.print(F("déplacer les raquettes."));
  minitel.newXY(11,13); minitel.attributs(CARACTERE_ROUGE); minitel.print(" Joueur 1   Joueur 2");
    /*minitel.attributs(CARACTERE_VERT);minitel.print(" Joueur 1");
    minitel.attributs(CARACTERE_BLANC);minitel.print(" |");
    minitel.attributs(CARACTERE_VERT);minitel.print(" Joueur 2");
    minitel.attributs(CARACTERE_VERT);minitel.print(" |");*/
  minitel.newXY(5,14); minitel.print(F("HAUT |   Q,S    |    J,S   |"));
  minitel.newXY(5,15); minitel.print(F("BAS  |    W     |     N    |"));
  minitel.newXY(1,17); minitel.attributs(INVERSION_FOND); minitel.print(F(" MODES DE JEU "));
  minitel.newXY(1,18); minitel.print(F(" > Affrontez l'ordinateur pour remporter"));
  minitel.newXY(1,19); minitel.print(F("   des trophées et avoir plus d'options"));
  minitel.newXY(1,21); minitel.print(F(" > Débloquez la raquette RUBBER pour"));
  minitel.newXY(1,22); minitel.print(F("   faire des effets sur la balle"));
  minitel.newXY(10,24); minitel.attributs(CARACTERE_MAGENTA); minitel.print(F("(C) iodeo.fr, 2024"));
  while (!minitel.getKeyCode());
}

void displayOptions() {
  int8_t y = 16;
  minitel.newXY(4,y++);
    //minitel.attributs(CARACTERE_VERT);
    minitel.printChar(' ');minitel.printChar('_');minitel.repeat(31);
  minitel.newXY(4,y++);minitel.print(" { ");
    minitel.attributs((y%2)?CARACTERE_MAGENTA:CARACTERE_CYAN);
    minitel.attributs(INVERSION_FOND);minitel.print("-M-");
    minitel.attributs(CARACTERE_BLANC);minitel.attributs(FOND_NORMAL);
    minitel.print("  Mode  : ");minitel.repeat(17);minitel.print("{");
  minitel.newXY(4,y++);minitel.print(" { ");
    minitel.attributs((y%2)?CARACTERE_MAGENTA:CARACTERE_CYAN);
    minitel.attributs(INVERSION_FOND);minitel.print("-P-");
    minitel.attributs(CARACTERE_BLANC);minitel.attributs(FOND_NORMAL);
    minitel.print((trophies < BEAT_WOOD_ROOKIE)?"  ******* ":"  Paddle: ");minitel.repeat(17);minitel.print("{");
  minitel.newXY(4,y++);minitel.print(" { ");
    minitel.attributs((y%2)?CARACTERE_MAGENTA:CARACTERE_CYAN);
    minitel.attributs(INVERSION_FOND);minitel.print("-S-");
    minitel.attributs(CARACTERE_BLANC);minitel.attributs(FOND_NORMAL);
    minitel.print("  Sound : ");minitel.repeat(17);minitel.print("{");
  minitel.newXY(4,y++);minitel.print(" { ");
    minitel.attributs((y%2)?CARACTERE_MAGENTA:CARACTERE_CYAN);
    minitel.attributs(INVERSION_FOND);minitel.print("-B-");
    minitel.attributs(CARACTERE_BLANC);minitel.attributs(FOND_NORMAL);
    minitel.print((trophies < BEAT_RUBBER_PRO)?"  ******* ":"  Balls : ");minitel.repeat(17);minitel.print("{");
  minitel.newXY(4,y++);minitel.print(" { ");
    minitel.attributs((y%2)?CARACTERE_MAGENTA:CARACTERE_CYAN);
    minitel.attributs(INVERSION_FOND);minitel.print("-T-");
    minitel.attributs(CARACTERE_BLANC);
    minitel.attributs(FOND_NORMAL);minitel.print("  *Trophies* ");
    if (trophies < ALL_TROPHIES) {
      minitel.repeat(14);
      minitel.print("{");
    } else {
      minitel.repeat(3);
      minitel.attributs(CARACTERE_MAGENTA);
      minitel.attributs(INVERSION_FOND);minitel.print("-R-");
      minitel.attributs(CARACTERE_BLANC); minitel.attributs(FOND_NORMAL);
      minitel.print(" Robert {");
    }
  
  minitel.newXY(4,y++);
    //minitel.attributs(CARACTERE_VERT);
    minitel.printChar(' ');minitel.print("~");minitel.repeat(31);minitel.printChar(' ');
  
  printModeOption();
  if (trophies >= BEAT_WOOD_ROOKIE) printPaddleOption();
  printSoundOption();
  if (trophies >= BEAT_RUBBER_PRO) printBallsOption();

  unsigned long key = minitel.getKeyCode();
  while (true) {
     if (key == 'M') {
      changeModeOption();
      printModeOption();
    } else if (key == 'P') {
      if (trophies >= BEAT_WOOD_ROOKIE) {
        changePaddleOption();
        printPaddleOption();
      } else {
        printStatus("Trophée manquant !", 0);
        delay(1000); eraseStatus();
      }
    } else if (key == 'S') {
      sound = !sound;
      printSoundOption();
    } else if (key == 'B') {
      if (trophies >= BEAT_RUBBER_PRO) {
        changeBallsOption();
        printBallsOption();
      } else {
        printStatus("Trophée manquant !", 0);
        delay(1000); eraseStatus();
      }
    } else if (key == 'T') {
      displayTrophies();
      displayWelcome();
      break;
    } else if (key == 'R') {
      if (trophies == ALL_TROPHIES) {
        displayRobert();
        displayWelcome();
        gameMode = HUMAN_VS_ROBERT;
        player1 = HUMAN; player2 = ROBERT;
        displayOptions();
        break;
      } else {
        printStatus("ENVOI pour valider", 0);
        delay(1000); eraseStatus();
      }
    }
    else if (key == 0x1341) { // Envoi
      y = 15;
      while (y++<22) {
        minitel.newXY(4,y);minitel.printChar(' ');minitel.repeat(33);
      }
      buzzer();
      minitel.newXY(12,19);minitel.attributs(CARACTERE_JAUNE);minitel.attributs(CLIGNOTEMENT);
      minitel.print("APPUYER SUR ENVOI");
      break;
    }
    else if (key != 0) { // Autres
      printStatus("ENVOI pour valider", 0);
      delay(1000); eraseStatus();
    }
    key = minitel.getKeyCode();
  }
}

void printModeOption() {
  minitel.newXY(20,17);
  minitel.print((player1==HUMAN)?"Human":"Robot");
  minitel.print(" vs ");
  if (player2==ROOKIE_BOT) minitel.print("Robot ");
  else if (player2==PRO_BOT) minitel.print("ProBot");
  else if (player2==ROBERT) minitel.print("Robert");
  else minitel.print("Human ");
}

void printPaddleOption() {
  minitel.newXY(20,18);
  minitel.print((paddle==WOOD)?"Wood  ":"Rubber");
}

void printSoundOption() {
  minitel.newXY(20,19);
  minitel.print((sound)?"Yes":"No ");
}

void printBallsOption() {
  minitel.newXY(20,20);
  if (nBall>=10) minitel.printChar('1');
  minitel.printChar(nBall%10+'0');
  if (nBall<10) minitel.printChar(' ');
}

void changeModeOption() {
  uint8_t nModes = 2;
  if (trophies >= BEAT_RUBBER_ROOKIE) nModes++;
  if (trophies >= BEAT_WOOD_PRO) nModes++;
  if (trophies == ALL_TROPHIES) nModes++;
  gameMode = (gameMode+1)%nModes;
  if (gameMode == HUMAN_VS_HUMAN) {
    player1 = HUMAN; player2 = HUMAN;
  } else if (gameMode == HUMAN_VS_ROOKIE_BOT) {
    player1 = HUMAN; player2 = ROOKIE_BOT;
  } else if (gameMode == HUMAN_VS_PRO_BOT) {
    player1 = HUMAN; player2 = PRO_BOT;
  } else if (gameMode == ROOKIE_BOT_VS_PRO_BOT) {
    player1 = ROOKIE_BOT; player2 = PRO_BOT;
  } else if (gameMode == HUMAN_VS_ROBERT) {
    player1 = HUMAN; player2 = ROBERT;
  }
}

void changePaddleOption() {
  paddle = (paddle == WOOD)? RUBBER : WOOD;
  paddleSize = (paddle == WOOD)? SMALL_PADDLE : LARGE_PADDLE;
  halfPaddleSize = (paddleSize-1)/2;
}

void changeBallsOption() {
  nBall = (nBall < 5)? 15:nBall-4;
}

void displayTrophies() {
  debugPrint("trophies:");debugPrintln(trophies);
  eraseScreen();
  minitel.textMode();
  minitel.newXY(1,1);minitel.attributs(DOUBLE_GRANDEUR);minitel.attributs(FOND_MAGENTA);
  minitel.print(" * TROPHIES BOARD * ");
  if (trophies >= BEAT_WOOD_ROOKIE) {
    minitel.newXY(15,5); minitel.print("WOOD");
    minitel.printChar(' ');minitel.repeat(7);minitel.print("RUBBER");
  }
  minitel.newXY(4,10); minitel.print("BEAT");
  minitel.newXY(4,11); minitel.print("ROBOT");
  if (trophies >= BEAT_RUBBER_ROOKIE) {
    minitel.newXY(4,16); minitel.print("BEAT");
    minitel.newXY(4,17); minitel.print("PROBOT");
    minitel.newXY(4,21); minitel.print("BEAT");
    minitel.newXY(4,22); minitel.print("THEM");
    minitel.newXY(4,23); minitel.print("ALL!");
  }

  minitel.graphicMode();
  if (trophies < BEAT_WOOD_ROOKIE) {
    minitel.attributs(CARACTERE_BLEU);
    minitel.graphic(0b001100, 16, 11);
  } else {
    minitel.newXY(16,6);
    minitel.graphicMode();
    drawTrophy(BEAT_WOOD_ROOKIE);
    if (trophies < BEAT_RUBBER_ROOKIE) {
      minitel.attributs(CARACTERE_BLEU);
      minitel.graphic(0b001100, 28, 11);
    } else {
      minitel.newXY(28,6);
      minitel.graphicMode();
      drawTrophy(BEAT_RUBBER_ROOKIE);
    }
  }
  if (trophies > BEAT_RUBBER_ROOKIE) {
    if (trophies & BEAT_WOOD_PRO) {
      minitel.newXY(14,13);
      minitel.graphicMode();
      drawTrophy(BEAT_WOOD_PRO);
    } else {
      minitel.attributs(CARACTERE_BLEU);
      minitel.graphic(0b001100, 16, 17);
    }
    if (trophies & BEAT_RUBBER_PRO) {
      minitel.newXY(26,13);
      minitel.graphicMode();
      drawTrophy(BEAT_RUBBER_PRO);
    } else {
      minitel.attributs(CARACTERE_BLEU);
      minitel.graphic(0b001100, 28, 17);
    }
    if (trophies == ALL_TROPHIES) {
      minitel.newXY(20,19);
      minitel.graphicMode();
      drawTrophy(ALL_TROPHIES);
    } else {
      minitel.attributs(CARACTERE_BLEU);
      minitel.graphic(0b001100, 22, 23);
    }
  }

  minitel.newXY(34,23);minitel.attributs(CARACTERE_ROUGE);minitel.print("Effacer");
  minitel.newXY(34,24);minitel.attributs(CARACTERE_ROUGE);minitel.attributs(INVERSION_FOND);minitel.print(" ANNUL.");

  unsigned long key = minitel.getKeyCode();
  while (true) {
    if (key == 0x1345) { // ANNULATION
      if (displayConfirm("Reset all progress ?")) {
        trophies = 0;
        EEPROM.update(0,0);
        break;
      }
    } else if (key != 0) { // Autres
      break;
    }
    key = minitel.getKeyCode();
  }

  //if (trophies<ALL_TROPHIES) trophies = (trophies << 1) + 1; //for debug

}

void displayRobert() {
  eraseScreen();
  for (int i=0; i<sizeof(ROBERT_BODY); i++) {
    minitel.writeByte(pgm_read_byte_near(ROBERT_BODY + i));
  }
  minitel.newXY(1,2); 
  minitel.println(" Salut,");
  minitel.println(" C'est moi, Robert I !");
  minitel.println(" Avec I comme Intelligence.");
  minitel.println(" Et non, j'ai rien d'Artificiel");
  minitel.newXY(8,8);minitel.attributs(CLIGNOTEMENT);minitel.print(" \\|/");
  
  drawParameterBar(REACTIVITY,   19, 9);
  drawParameterBar(ANTICIPATION, 19, 12);
  drawParameterBar(ENDURANCE,    19, 15);
  drawParameterBar(AGRESSIVITY,  19, 18);

  minitel.newXY(20,22); minitel.attributs(CARACTERE_ROUGE); minitel.println("* Only for Wood");
  minitel.newXY(20,24); minitel.attributs(CARACTERE_ROUGE); minitel.println("** Only for Rubber");

  unsigned long key = minitel.getKeyCode();
  while (true) {
    uint8_t param, val;
    uint8_t row = 0;
    if (key == 'R') {
      param = REACTIVITY;
      row = 9;
    } else if (key == 'A') {
      param = ANTICIPATION;
      row = 12;
    } else if (key == 'E') {
      param = ENDURANCE;
      row = 15;
    } else if (key == 'O') {
      param = AGRESSIVITY;
      row = 18;
    } else if (key == 0x1341) { // Envoi
      break;
    }
    else if (key != 0) { // Autres
      printStatus("ENVOI pour valider", 0);
      delay(1000); eraseStatus();
    }

    if (row != 0) {
      val = botParams[ROBERT][param];
      botParams[ROBERT][param] = (val+1)%5;
      refreshParameterBar(param, 19, row);
    }

    key = minitel.getKeyCode();
  }
  
  minitel.newXY(8,8);minitel.printChar(' '); minitel.repeat(4);
  delay(500);minitel.newXY(10,9);minitel.printChar(' ');
  minitel.graphicMode();minitel.attributs(CARACTERE_ROUGE);
  delay(500);minitel.graphic(0b001011,10,10);
  delay(500);minitel.graphic(0b000011,10,10);
  delay(500);minitel.graphic(0b000000,8,10);minitel.repeat(3);
  delay(500);minitel.newXY(8,11);minitel.graphicMode();
  for (int i=0; i<sizeof(ROBERT_HEAD); i++) {
    minitel.writeByte(pgm_read_byte_near(ROBERT_HEAD + i));
  }
  delay(1000);
  minitel.newXY(7,22);minitel.attributs(DOUBLE_HAUTEUR);minitel.print("READY !");
  delay(1500);
}

void drawParameterBar(uint8_t param, uint8_t x, uint8_t y) {
  String title;
  uint8_t value = botParams[ROBERT][param];

  switch (param) {
    case ANTICIPATION: title = "-A- Anticipation"; break;
    case REACTIVITY:   title = "-R- Reactivity  "; break;
    case ENDURANCE:    title = "-E- Endurance  *"; break;
    case AGRESSIVITY:  title = "-O- Offensive **"; break;
  }

  minitel.newXY(x,y++);
  minitel.attributs(DEBUT_LIGNAGE);
  minitel.printChar(' ');
  minitel.print(title);

  minitel.newXY(x,y);
  if (value > 0) minitel.attributs(FOND_ROUGE);
  minitel.print("} ");
  if ((value==0) || (value==4)) {
    minitel.repeat(14); minitel.printChar('}');
  } else {
    minitel.repeat(4*value-1); 
    minitel.attributs(FOND_NOIR);
    minitel.repeat(4*(4-value)-1); minitel.printChar('}');
  }

  minitel.newXY(x+4*botParams[ROOKIE_BOT][param],y); minitel.attributs(CARACTERE_BLEU);minitel.printChar('R');
  minitel.newXY(x+4*botParams[PRO_BOT][param],y); minitel.attributs(CARACTERE_BLEU);minitel.printChar('P');

  minitel.newXY(x+1,y+1);
  minitel.print("~"); minitel.repeat(15);
}

void refreshParameterBar(uint8_t param, uint8_t x, uint8_t y) {
  uint8_t value = botParams[ROBERT][param];
  minitel.newXY(x,++y);
  if (value > 0) minitel.attributs(FOND_ROUGE);
  minitel.print("} ");
  if ((value==0) || (value==4)) {
    minitel.repeat(14); minitel.printChar('}');
  } else {
    minitel.repeat(4*value-1); 
    minitel.attributs(FOND_NOIR);
    minitel.repeat(4*(4-value)-1); minitel.printChar('}');
  }
  minitel.newXY(x+4*botParams[ROOKIE_BOT][param],y); minitel.attributs(CARACTERE_BLEU);minitel.printChar('R');
  minitel.newXY(x+4*botParams[PRO_BOT][param],y); minitel.attributs(CARACTERE_BLEU);minitel.printChar('P');
}

void printStatus(String str, byte attribut) {
  minitel.newXY(1,0); 
  if (attribut) minitel.attributs(attribut);
  minitel.print(str);
}

void eraseStatus() {
  minitel.newXY(1,0); minitel.cancel();
}

bool displayConfirm(String str){
  minitel.newXY(1,0); minitel.attributs(CARACTERE_ROUGE); 
  minitel.print(str); minitel.printChar(' ');
  minitel.attributs(INVERSION_FOND); minitel.print("ENVOI");
  minitel.attributs(FOND_NORMAL); minitel.print(" / ");
  minitel.attributs(INVERSION_FOND); minitel.print("ANNUL.");
  unsigned long key = minitel.getKeyCode();
  bool confirm;
  while (true) {
    if (key == 0x1341) { // Envoi
      confirm = true;
      break;
    }
    else if (key == 0x1345) { // Annulation
      confirm = false;
      break;
    }
    key = minitel.getKeyCode();
  }
  minitel.newXY(1,0); minitel.cancel();
  return confirm;
}

// EEPROM 
// 0:trophies
// 1:magic

void setupEEPROM() {
  uint8_t magic = EEPROM.read(0);
  if (magic != MAGIC) {
    debugPrintln("Wrong magic : Initialize EEPROM");
    EEPROM.update(0,0);
    EEPROM.update(1,MAGIC);
  }
}
