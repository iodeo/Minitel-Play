
//----- Libraries --------------------------------------

#include <Minitel1B_Hard.h>
#include <EEPROM.h>
#include <stdarg.h> // for debug printf implementation

//----- Macros -----------------------------------------

#define MINITEL_PORT Serial1 //for MiniDev or other atmega32u4 based boards
// #define MINITEL_PORT Serial //for Miniplay or other atmega328p based boards

#define DEBUG false //WARNING: only for artmega32u4 

#define MAGIC 0x27 // for EEPROM setup

#if DEBUG // Debug enabled
  #define DEBUG_PORT Serial
  #define debugPrint(x)     DEBUG_PORT.print(x)
  #define debugPrintln(x)   DEBUG_PORT.println(x)
  #define debugPrintf(...)  pf(__VA_ARGS__) // for Arduinos
  void pf(char *fmt, ... ){
          char buf[128]; // resulting string limited to 128 chars
          va_list args;
          va_start (args, fmt );
          vsnprintf(buf, 128, fmt, args);
          va_end (args);
          DEBUG_PORT.print(buf);
  }
#else // Debug disabled : Empty macro functions
  #define debugBegin(x)
  #define debugPrint(x)
  #define debugPrintln(x)
  #define debugPrintf(...)
#endif

// GAME DEFs
#define START_TILES 2
#define MAX_BOARD_SIZE 4
#define MAX_NB_OF_CELLS 16


// GAME GRAPHICs
#define TILE_HEIGHT 4
#define TILE_WIDTH 6
#define TILE_SPACER 1

//----- Globals ----------------------------------------

Minitel minitel(MINITEL_PORT);

bool sound = true;
bool anima = true;
int8_t boardSize = MAX_BOARD_SIZE;
int8_t nbOfCells = boardSize * boardSize;
int8_t xFirstTile = 7 + (MAX_BOARD_SIZE-boardSize)*(TILE_WIDTH+TILE_SPACER)/2;
int8_t yFirstTile = 4 + (MAX_BOARD_SIZE-boardSize)*(TILE_HEIGHT+TILE_SPACER)/2;

enum {MOVE_UP, MOVE_RIGHT, MOVE_DOWN, MOVE_LEFT,
       MENU_OPTIONS, MENU_HOWTO, MENU_CANCEL, MENU_REPEAT};

uint32_t score = 0;
uint32_t bestScore = 0;
uint32_t oldScore = 0;
int8_t maxTile = 0;
bool newTile = false;

int8_t board[MAX_NB_OF_CELLS] = {0};
int8_t oldBoard[MAX_NB_OF_CELLS] = {0};

bool over = false;
int8_t ccount = 0;

//----- Setup ------------------------------------------

void setup() {

  // be sure minitel is ready
  delay(500);
  
  // set minitel at 4800 bauds, or fall back to default
  if (minitel.searchSpeed() != 4800) {     // search speed
    if (minitel.changeSpeed(4800) < 0) {   // set to 4800 if different
      minitel.searchSpeed();               // search speed again if change has failed
    }
  }

  // set minitel mode
  minitel.modeVideotex();
  minitel.echo(false);
  minitel.extendedKeyboard();
  eraseScreen();
  debugPrintln("minitel ready");

  // get new seed for random numbers
  //randomSeed(analogRead(A0));

  setupEEPROM();

  // waiting bar
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

//----- Main loop --------------------------------------

void loop() {
  welcome();
  beginGame();
  while (!over) playGame();
  endGame();
}

//----- Main functions ---------------------------------

void welcome() {

  // display welcome at low speed for visual effect
  if (minitel.changeSpeed(1200) < 0) minitel.searchSpeed();
  displayWelcome();
  if (minitel.changeSpeed(4800) < 0) minitel.searchSpeed();
  flushInputs();

  unsigned long key = minitel.getKeyCode();
  while (true) {
    if (key == 0x1341) break; // Envoi
    else if (key == 0x1346) { // Sommaire
      displayOptions(false); 
      break;
    }
    else if (key == 0x1344) { // Guide
      displayHowTo();
      displayWelcome();
    }
    key = minitel.getKeyCode();
  }
}

void beginGame() {
  
  // reset game variables
  over = false;
  score = 0;
  maxTile = 0;
  newTile = false;
  ccount = 0;
  for (int8_t index = 0; index < nbOfCells; index++) {
    board[index] = 0; //index?index+1:2;
    oldBoard[index] = 0;
  }

  // get new seed for random numbers
  randomSeed(millis());
  
  // load best score
  loadBestScore();
  
  // add start tiles
  for (int8_t i = 0; i < START_TILES; i++) addRandomTile();

  // display board
  displayGame();

}

void playGame() {
  int8_t command = getPlayerCommand();
  if (command < MENU_OPTIONS) { // Touches de jeu
    debugPrintf(" > move direction %i\n", command);
    if(playMove(command)) {
      debugPrintln("   old board > new board");
      debugPrintf(" %6i %6i %6i %6i   > %6i %6i %6i %6i\n", oldBoard[0],oldBoard[1],oldBoard[2],oldBoard[3],board[0],board[1],board[2],board[3]);
      debugPrintf(" %6i %6i %6i %6i   > %6i %6i %6i %6i\n", oldBoard[4],oldBoard[5],oldBoard[6],oldBoard[7],board[4],board[5],board[6],board[7]);
      debugPrintf(" %6i %6i %6i %6i   > %6i %6i %6i %6i\n", oldBoard[8],oldBoard[9],oldBoard[10],oldBoard[11],board[8],board[9],board[10],board[11]);
      debugPrintf(" %6i %6i %6i %6i   > %6i %6i %6i %6i\n", oldBoard[12],oldBoard[13],oldBoard[14],oldBoard[15],board[12],board[13],board[14],board[15]);
      
      drawTile(addRandomTile());
      
      if (newTile) {
        newTile = false;
        if (sound) minitel.bip();
        if (maxTile == 11) display2048(); // 2^11 = 2048
      }
      
      if (!movesAvailable()) {
        over = true;
        if (sound) minitel.bip();
        minitel.newXY(1,0); minitel.attributs(CARACTERE_MAGENTA); minitel.attributs(CLIGNOTEMENT); minitel.print("Le jeu est bloqué !");
        unsigned long timeout = millis();
        while(millis()-timeout < 5000) {
          if (minitel.getKeyCode()) break;
          delay(100);
        }
      }
    }

  } else { // Touches de menu
    switch (command) {
      case MENU_OPTIONS:
        displayOptions(true);
        break;
      case MENU_HOWTO:
        displayHowTo();
        displayGame();
        break;
      case MENU_CANCEL:
        displayCancel();
        break;
      case MENU_REPEAT:
        displayGame();
        break;
    }
  }
}

void endGame() {
  bool best = false;
  if (score > bestScore) {
    best = true;
    bestScore = score;
    saveBestScore();
  }
  displayScore(best);
}

//----- Game logic --------------------------------------

bool playMove(int8_t command) {
  // set variables to loop on tiles in correct order
  int8_t yStart, yEnd, yIncr;
  int8_t xStart, xEnd, xIncr;
  int8_t tVector;
  switch (command) {
    case (MOVE_UP):
      yStart = 1; yEnd = boardSize-1; yIncr  = +1;
      xStart = 0; xEnd = boardSize-1; xIncr  = +1;
      tVector = -boardSize;
      break;
    case (MOVE_RIGHT):
      yStart = 0; yEnd = boardSize-1; yIncr  = +1;
      xStart = boardSize-2; xEnd = 0; xIncr  = -1;
      tVector  = +1;
      break;
    case (MOVE_DOWN):
      yStart = boardSize-2; yEnd = 0; yIncr  = -1;
      xStart = 0; xEnd = boardSize-1; xIncr  = +1;
      tVector = +boardSize;
      break;
    case (MOVE_LEFT):
      yStart = 0; yEnd = boardSize-1; yIncr  = +1;
      xStart = 1; xEnd = boardSize-1; xIncr  = +1;
      tVector  = -1;
      break;
  }

  // use to avoid merging one tile several times
  bool merged[nbOfCells] = {false};
  // use to flag tile move
  bool moved = false;

  // loop on each tiles, move them and merge them if needed
  yEnd += yIncr; // enable loop on last tile
  xEnd += xIncr; // enable loop on last tile
  for (int8_t y = yStart; y != yEnd; y+=yIncr) {
    for (int8_t x = xStart; x != xEnd; x+=xIncr) {
      // check if there is a tile here
      int8_t index = x +y*boardSize;
      if (board[index] == 0) continue;
      // calculate board bound
      int8_t bound;
      if (abs(tVector) == boardSize) bound = x+yStart*boardSize + tVector;
      if (abs(tVector) == 1)         bound = xStart+y*boardSize + tVector;
      // move tile
      int8_t next = index+tVector;
      while (board[next] == 0) { 
        // move until next tile and stop at board bound
        if (tVector > 0 && next > bound) break;
        if (tVector < 0 && next < bound) break;
        if (!moved) { // first tile move
          moved = true;
          // copy score and board
          oldScore = score;
          for (int8_t i = 0; i < nbOfCells; i++) {
            oldBoard[i] = board[i];
          }
        }
        board[next] = board[index];
        board[index] = 0;
        if (anima) {
          drawTile(index);
          drawTile(next);
        }
        // iterate
        index = next;
        next += tVector;
      }
      // merge ?
      if (index == bound) continue; // no tile to merge
      if (merged[next]) continue; // tile already merged
      if (board[index] == board[next]) {
        merged[next] = true;
        int8_t value = board[next]+1;
        if (value > maxTile) {
          maxTile = value;
          newTile = true;
        }
        if (!moved) { // first tile move
          moved = true;
          // copy score and board
          oldScore = score;
          for (int8_t i = 0; i < nbOfCells; i++) {
            oldBoard[i] = board[i];
          }
        }
        board[next] = value;
        board[index] = 0;
        if (anima) {
          drawTile(index);
          drawTile(next);
        }
        score += powerOf2(value);
      }
    }
  }

  // draw new board if not already been
  if (!anima && moved) {
    for (int8_t i = 0; i < boardSize; i++) {
      for (int8_t j = 0; j < boardSize; j++) {
        int8_t index;
        // refresh order is changed based on move direction
        if (command == MOVE_UP) index = j+i*boardSize;
        else if (command == MOVE_RIGHT) index = (boardSize-1-i)+j*boardSize;
        else if (command == MOVE_DOWN) index = j+(boardSize-1-i)*boardSize;
        else if (command == MOVE_LEFT) index = i+j*boardSize;
        if (board[index] != oldBoard[index]) drawTile(index);
      }
    }
  }

  // update score
  if (score != oldScore) {
    String str = String(score);
    //while (str.length() < 6) str = " " + str;
    minitel.newXY(19-str.length(),1);
    minitel.print(str);
  }
  return moved;
}

int8_t addRandomTile() {
  int8_t count = countAvailableCells();
  int8_t index = -1;
  if (count > 0) {
    int8_t value = (random(100) < 90) ? 1 : 2;
    int8_t nth = random(count) + 1;
    index = nthAvailableCell(nth);
    board[index] = value;
    debugPrintf(" > added tile %i at %i-th available cell\n", value, nth);
  }
  return index;
}

int8_t countAvailableCells() {
  int8_t count = 0;
  for (int8_t i = 0; i < nbOfCells; i++) {
    if (board[i] == 0) count++;
  }
  debugPrintf(" > there are %i available cells\n", count);
  return count;
}

int8_t nthAvailableCell(int8_t nth) {
  int8_t index = -1;
  while (nth) {
    index++;
    if (board[index] == 0) nth--;
  }
  debugPrintf(" > nth available cell is at index %i\n", index);
  return index;
}

bool movesAvailable() {
  if (countAvailableCells() > 0) {
    debugPrintln(" > moves available (available cell)");
    return true;
  }
  if (tilesMatchesAvailable()) {
    debugPrintln(" > moves available (tiles match)");
    return true;
  }
  debugPrintln(" > no move available");
  return false;
}

bool tilesMatchesAvailable() {
  // horizontal matches
  for (int8_t index = 1; index < nbOfCells; index++) {
    if (!(index%boardSize)) continue;
    if (board[index] == board[index-1]) {
      debugPrintf(" > tiles match at index %i and %i\n", index-1, index);
      return true;
    }
  }
  // vertical matches
  for (int8_t index = boardSize; index < nbOfCells; index++) {
    if (board[index] == board[index-boardSize]) {
      debugPrintf(" > tiles match at index %i and %i\n", index-boardSize, index);
      return true;
    }
  }
  return false;
}

void ccounted () {
  minitel.newXY(1,0); minitel.print("Bon ok d'accord !");
  if (sound) minitel.bip(); delay(500);
  if (sound) minitel.bip(); delay(500);
  minitel.newXY(1,0); minitel.cancel();
  ccount = 0; score = oldScore;
  for (int8_t index = 0; index < nbOfCells; index++) board[index] = oldBoard[index];
  displayGame();
}

//----- Keyboard inputs --------------------------------

int8_t getPlayerCommand() {
  int8_t command = 0;
  unsigned long key = minitel.getKeyCode();
  while (true) {
    switch (key) {
      // Touches de jeu
      case 0x1342:   //RETOUR
      case 0x1B5B41: //TOUCHE_FLECHE_HAUT
      case '2': case 'Z': case 'z':
        return MOVE_UP;
      case 0x1341:   //ENVOI
      case 0x1B5B43: //TOUCHE_FLECHE_DROITE
      case '6': case 'D': case 'd':
        return MOVE_RIGHT;
      case 0x1348:   //SUITE
      case 0x1B5B42: //TOUCHE_FLECHE_BAS
      case '8': case 'S': case 's':
        return MOVE_DOWN;
      case 0x1347:   //CORRECTION
      case 0x1B5B44: //TOUCHE_FLECHE_GAUCHE
      case '4': case 'Q': case 'q':
        return MOVE_LEFT;
        
      // Touches de menu
      case 0x1346:   //SOMMAIRE
        return MENU_OPTIONS;
      case 0x1344:   //GUIDE
        return MENU_HOWTO;
      case 0x1345:   //ANNULATION
      case 0x1A:     //CTRL Z
        return MENU_CANCEL;
      case 0x1343:   //REPETITION
        return MENU_REPEAT;
      default:
        break;
    }
    key = minitel.getKeyCode();
  }
}

//----- Elements drawing -------------------------------

void drawScoreBar() {
  minitel.newXY(6,1);  minitel.print("SCORE:");
  String str = String(score);
  while (str.length() < 6) str = "0" + str;
  if (str.length() == 6) str = " " + str;
  minitel.print(str);
  minitel.newXY(23,1); minitel.print("BEST:");
  str = String(bestScore);
  while (str.length() < 6) str = "0" + str;
  if (str.length() == 6) str = " " + str;
  minitel.print(str);
}

void drawBoardContour() {
  int8_t y = yFirstTile - TILE_SPACER - 1;
  minitel.newXY(xFirstTile-TILE_SPACER, y);
  minitel.printChar(0x5F); minitel.repeat(boardSize*(TILE_WIDTH+TILE_SPACER));
  while (y++ < yFirstTile + boardSize*(TILE_HEIGHT+TILE_SPACER) - 1) {
    minitel.newXY(xFirstTile-TILE_SPACER-1,y); minitel.printChar(0x7D);
    minitel.newXY(xFirstTile+boardSize*(TILE_WIDTH+TILE_SPACER),y); minitel.printChar(0x7B);
  }
  minitel.newXY(xFirstTile-TILE_SPACER, y);
  minitel.print("~"); minitel.repeat(boardSize*(TILE_WIDTH+TILE_SPACER));
}

void drawTile(int8_t index) {
  int8_t value = board[index];
  int8_t x = xFirstTile + (index%boardSize)*(TILE_WIDTH+TILE_SPACER);
  int8_t y = yFirstTile + (index/boardSize)*(TILE_HEIGHT+TILE_SPACER);
  for (int8_t i = 0; i < TILE_HEIGHT; i++) {
    if (i == 2) continue; // already drawn with DOUBLE_HAUTEUR
    minitel.newXY(x,y+i);
    if (i == 1) {
      setTileColor(value, true); minitel.attributs(DOUBLE_HAUTEUR);
      String str = String(powerOf2(value));
      while (str.length() < 5) str = " " + str + " ";
      if (str.length() < 6) str = " " + str;
      minitel.print(str);
    } else {
      setTileColor(value, false);
      minitel.printChar(' '); minitel.repeat(TILE_WIDTH-1);
    }
  }
}

void setTileColor(int8_t value, bool setFontColor) {
  switch (value) {
    case 0: // empty
      minitel.attributs(FOND_NOIR);
      if (setFontColor) minitel.attributs(CARACTERE_NOIR);
      break;
    case 1: // tile 2
      minitel.attributs(FOND_BLANC);
      if (setFontColor) minitel.attributs(CARACTERE_MAGENTA);
      break;
    case 2: // tile 4
      minitel.attributs(FOND_CYAN);
      if (setFontColor) minitel.attributs(CARACTERE_ROUGE);
      break;
    case 3: // tile 8
      minitel.attributs(FOND_VERT);
      if (setFontColor) minitel.attributs(CARACTERE_BLEU);
      break;
    case 4: // tile 16
      minitel.attributs(FOND_MAGENTA);
      if (setFontColor) minitel.attributs(CARACTERE_JAUNE);
      break;
    case 5: // tile 32
      minitel.attributs(FOND_ROUGE);
      if (setFontColor) minitel.attributs(CARACTERE_CYAN);
      break;
    case 6: // tile 64
      minitel.attributs(FOND_BLEU);
      if (setFontColor) minitel.attributs(CARACTERE_VERT);
      break;
    case 7: // tile 128
      minitel.attributs(FOND_JAUNE);
      if (setFontColor) minitel.attributs(CARACTERE_VERT);
      break;
    case 8: // tile 256
      minitel.attributs(FOND_CYAN);
      if (setFontColor) minitel.attributs(CARACTERE_MAGENTA);
      break;
    case 9: // tile 512
      minitel.attributs(FOND_VERT);
      if (setFontColor) minitel.attributs(CARACTERE_ROUGE);
      break;
    case 10: // tile 1024
      minitel.attributs(FOND_MAGENTA);
      if (setFontColor) minitel.attributs(CARACTERE_VERT);
      break;
    case 11: // tile 2048
      minitel.attributs(FOND_BLANC);
      if (setFontColor) minitel.attributs(CARACTERE_NOIR);
      break;
    case 12: // tile 4096
      minitel.attributs(FOND_NOIR);
      if (setFontColor) minitel.attributs(CARACTERE_BLANC);
      break;
    case 13: // tile 8192
      minitel.attributs(FOND_BLANC);
      if (setFontColor) minitel.attributs(CARACTERE_ROUGE);
      break;
    case 14: // tile 16384
      minitel.attributs(FOND_NOIR);
      if (setFontColor) minitel.attributs(CARACTERE_CYAN);
      break;      
    case 15: // tile 32768
      minitel.attributs(FOND_BLANC);
      if (setFontColor) minitel.attributs(CARACTERE_VERT);
      break;
    case 16: // tile 65536
      minitel.attributs(FOND_NOIR);
      if (setFontColor) minitel.attributs(CARACTERE_MAGENTA);
      break;
    case 17: // tile 131072
      minitel.attributs(FOND_NOIR);
      minitel.attributs(CARACTERE_BLANC);
      minitel.attributs(INVERSION_FOND);
      break;
    default:
      minitel.attributs(FOND_NOIR);
      if (setFontColor) minitel.attributs(CARACTERE_BLANC);
  }
}

uint32_t powerOf2(int8_t exponent) {
  return (uint32_t) 1 << exponent;
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
  minitel.newXY(14,2);minitel.attributs(CARACTERE_VERT);
  //minitel.attributs(DOUBLE_LARGEUR);minitel.printChar('A');minitel.attributs(GRANDEUR_NORMALE);minitel.print("rduino ");
  minitel.attributs(DOUBLE_LARGEUR);minitel.printChar('M');minitel.attributs(GRANDEUR_NORMALE);minitel.print("initel ");
  minitel.attributs(DOUBLE_LARGEUR);minitel.printChar('P');minitel.attributs(GRANDEUR_NORMALE);minitel.print("lay ");
  minitel.newXY(17,4);minitel.attributs(CARACTERE_BLEU);minitel.print("presents");
  int8_t y = 7;
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(CARACTERE_VERT);
    minitel.graphic(0b000111);minitel.graphic(0b111111);minitel.repeat(11);minitel.graphic(0b001011);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(CARACTERE_VERT);
    minitel.graphic(0b111111);minitel.repeat(13);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(CARACTERE_VERT);
    minitel.graphic(0b111111);minitel.repeat(13);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(FOND_VERT);minitel.attributs(CARACTERE_JAUNE);
    minitel.graphic(0);minitel.graphic(0);minitel.graphic(0b000101);minitel.graphic(0b111100);minitel.graphic(0b001010);minitel.graphic(0b011111);minitel.graphic(0b101101);minitel.graphic(0);minitel.graphic(0b000111);minitel.graphic(0b101000);minitel.graphic(0b011110);minitel.graphic(0b101101);minitel.graphic(0);minitel.graphic(0);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(FOND_VERT);minitel.attributs(CARACTERE_JAUNE);
    minitel.graphic(0);minitel.graphic(0);minitel.graphic(0b000001);minitel.graphic(0b011110);minitel.graphic(0b100000);minitel.graphic(0b101010);minitel.graphic(0b010101);minitel.graphic(0b010101);minitel.graphic(0b100011);minitel.graphic(0b001010);minitel.graphic(0b011010);minitel.graphic(0b100101);minitel.graphic(0);minitel.graphic(0);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(FOND_VERT);minitel.attributs(CARACTERE_JAUNE);
    minitel.graphic(0);minitel.graphic(0);minitel.graphic(0b010100);minitel.graphic(0b111100);minitel.graphic(0b101000);minitel.graphic(0b110100);minitel.graphic(0b111000);minitel.graphic(0);minitel.graphic(0b010100);minitel.graphic(0b101000);minitel.graphic(0b110100);minitel.graphic(0b111000);minitel.graphic(0);minitel.graphic(0);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(CARACTERE_VERT);
    minitel.graphic(0b111111);minitel.repeat(13);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(CARACTERE_VERT);
    minitel.graphic(0b111111);minitel.repeat(13);
  minitel.newXY(14,y++);minitel.graphicMode();minitel.attributs(CARACTERE_VERT);
    minitel.graphic(0b110100);minitel.graphic(0b111111);minitel.repeat(11);minitel.graphic(0b111000);

  minitel.newXY(1,23);minitel.attributs(CARACTERE_VERT);minitel.printChar(' ');
    minitel.attributs(INVERSION_FOND); minitel.print("GUIDE");
    minitel.attributs(FOND_NORMAL); minitel.print(" Règles du jeu");
  minitel.moveCursorReturn(1);minitel.printChar(' ');
    minitel.attributs(INVERSION_FOND);minitel.print("SOMMAIRE");
    minitel.attributs(FOND_NORMAL); minitel.print(" Paramètres");
  minitel.newXY(32,23);minitel.attributs(CARACTERE_MAGENTA);minitel.print("iodeo.fr");
  minitel.newXY(32,24);minitel.attributs(CARACTERE_MAGENTA);minitel.print("(C) 2023");

  if (sound) minitel.bip();
  
  minitel.newXY(12,19);minitel.attributs(CARACTERE_JAUNE);minitel.attributs(CLIGNOTEMENT);
  minitel.print("APPUYER SUR ENVOI");
}

void displayOptions(bool inGame) {
  // options
  int8_t y = inGame?14:16;
  minitel.newXY(6,y++);
    //minitel.attributs(CARACTERE_VERT);
    minitel.printChar(' ');minitel.printChar('_');minitel.repeat(26);minitel.printChar(' ');
  minitel.newXY(6,y++);minitel.print(" { ");
    //minitel.attributs(CARACTERE_VERT); 
    minitel.attributs(INVERSION_FOND);minitel.print("-F-");
    minitel.attributs(FOND_NORMAL);minitel.print("  |EFFET SONORE (");minitel.print((sound)?"OUI":"NON");minitel.print(") {");
  minitel.newXY(6,y++);minitel.print(" { ");
    //minitel.attributs(CARACTERE_VERT);
    minitel.attributs(INVERSION_FOND);minitel.print("-X-");
    minitel.attributs(FOND_NORMAL);minitel.print("  |ANIM. TUILES (");minitel.print((anima)?"OUI":"NON");minitel.print(") {");
  if (!inGame) {
    minitel.newXY(6,y++);minitel.print(" { ");
      //minitel.attributs(CARACTERE_VERT);
      minitel.attributs(INVERSION_FOND);minitel.print("-T-");
      minitel.attributs(FOND_NORMAL);minitel.print("  |TAILLE JEU   (");minitel.printChar(boardSize+48);minitel.print("x");minitel.printChar(boardSize+48);minitel.print(") {");
  }
  minitel.newXY(6,y++);minitel.print(" {      | ");minitel.repeat(18);minitel.printChar('{');
  if (inGame) {
    minitel.newXY(6,y++);minitel.print(" {");
      //minitel.attributs(CARACTERE_VERT);
      minitel.attributs(INVERSION_FOND);minitel.print("RETOUR");
      minitel.attributs(FOND_NORMAL);minitel.print("|SAUVEGARDER ");minitel.repeat(7);minitel.printChar('{');
    minitel.newXY(6,y++);minitel.print(" {");
      //minitel.attributs(CARACTERE_VERT);
      minitel.attributs(INVERSION_FOND);minitel.print("SUITE");
      minitel.attributs(FOND_NORMAL);minitel.print(" |CHARGER ");minitel.repeat(11);minitel.printChar('{');
    minitel.newXY(6,y++);minitel.print(" {      | ");minitel.repeat(18);minitel.printChar('{');
  }
  minitel.newXY(6,y++);minitel.print(" {");
    //minitel.attributs(CARACTERE_VERT);
    minitel.attributs(INVERSION_FOND);minitel.print("ENVOI");
    minitel.attributs(FOND_NORMAL);minitel.print(inGame?" |CONTINUER ":" |COMMENCER ");minitel.repeat(9);minitel.print("{");
  if (inGame) {
    minitel.newXY(6,y++);minitel.print(" {");
      //minitel.attributs(CARACTERE_VERT);
      minitel.attributs(INVERSION_FOND);minitel.print("ANNUL.");
      minitel.attributs(FOND_NORMAL);minitel.print("|QUITTER ");minitel.repeat(11);minitel.printChar('{');
  }
  minitel.newXY(6,y++);
    //minitel.attributs(CARACTERE_VERT);
    minitel.printChar(' ');minitel.print("~");minitel.repeat(26);minitel.printChar(' ');
  
  unsigned long key = minitel.getKeyCode();
  while (true) {
    if (key == 'F' || key == 'f') {
      sound = !sound;
      minitel.newXY(29,inGame?15:17);minitel.print((sound)?"OUI":"NON");
    }
    else if (key == 'X' || key == 'x') {
      anima = !anima;
      minitel.newXY(29,inGame?16:18);minitel.print((anima)?"OUI":"NON");
    }
    else if (!inGame && (key == 'T' || key == 't')) {
      boardSize = (boardSize < MAX_BOARD_SIZE)? boardSize+1:2;
      nbOfCells = boardSize*boardSize;
      xFirstTile = 7 + (MAX_BOARD_SIZE-boardSize)*(TILE_WIDTH+TILE_SPACER)/2;
      yFirstTile = 4 + (MAX_BOARD_SIZE-boardSize)*(TILE_HEIGHT+TILE_SPACER)/2;
      minitel.newXY(29,19);minitel.printChar(boardSize+48);minitel.print("x");minitel.printChar(boardSize+48);
    }
    else if (inGame && key == 0x1342) { // Retour
      if (displayConfirm("Sauvegarder ?")) {
      saveScore();
      saveBoard();
      break;
      }
    }
    else if (inGame && key == 0x1348) { // Suite
      if (displayConfirm("Charger la partie ?")) {
        loadScore();
        loadBoard();
        newTile = false;
        break;
      }
    }
    else if (inGame && key == 0x1345) { // Annulation
      bool confirm = true;
      if (inGame) confirm = displayConfirm("Quitter la partie ?");
      if (confirm) {
        over = true;
        break;
      }
    }
    else if (key == 0x1341) { // Envoi
      break;
    }
    key = minitel.getKeyCode();
  }
  if (inGame) displayGame();
}

void displayHowTo() {
  eraseScreen();
  minitel.newXY(1,0); minitel.cancel(); // erase status row
  minitel.newXY(1,1); minitel.attributs(DOUBLE_GRANDEUR); minitel.attributs(INVERSION_FOND);
  minitel.printChar(' '); minitel.repeat(7); minitel.print(F("2048"));minitel.cancel();
  minitel.newXY(1,4); minitel.attributs(DEBUT_LIGNAGE); minitel.print(F(" BUT DU JEU"));
  minitel.newXY(1,5); minitel.print(F("Reliez les nombres "));
  minitel.newXY(1,6); minitel.print(F("et obtenez la ")); minitel.attributs(INVERSION_FOND); minitel.print(F("tuile 2048 !"));
  minitel.newXY(1,8); minitel.attributs(DEBUT_LIGNAGE); minitel.print(F(" COMMENT JOUER ?"));
  minitel.newXY(1,9); minitel.print(F("Utilisez les touches clavier pour"));
  minitel.newXY(1,10); minitel.print(F("déplacer les tuiles."));
  minitel.newXY(1,11); minitel.print(F("Quand deux tuiles avec le même nombre"));
  minitel.newXY(1,12); minitel.print(F("se touchent, ")); minitel.attributs(INVERSION_FOND); minitel.print(F("elles fusionnent !"));
  minitel.newXY(1,14); minitel.attributs(DEBUT_LIGNAGE); minitel.print(F(" TOUCHES CLAVIER"));
  minitel.newXY(1,15); minitel.print(F("Flèches     ↑      ←      ↓      →"));
  minitel.newXY(1,16); minitel.print(F("ZQSD        Z      Q      S      D"));
  minitel.newXY(1,17); minitel.print(F("Pavé num.   2      4      8      6"));
  minitel.newXY(1,18); minitel.print(F("Minitel   Retour Corr.  Suite  Envoi"));
  minitel.newXY(1,20); minitel.attributs(DEBUT_LIGNAGE); minitel.print(F(" CREDITS"));
  minitel.newXY(1,21); minitel.print(F("Jeu Arduino pour minitel développé"));
  minitel.newXY(1,22); minitel.print(F("d'après 2048 de "));minitel.attributs(INVERSION_FOND);minitel.print(F("Gabriele Cirulli."));
  minitel.newXY(10,24); minitel.attributs(CARACTERE_MAGENTA); minitel.print(F("(C) iodeo.fr, 2023"));
  //delay(500); minitel.newXY(1,0); minitel.attributs(CARACTERE_MAGENTA); minitel.print(F("Appuyer sur une touche..."));
  while (!minitel.getKeyCode());
}

void displayGame() {
  eraseScreen();
  drawScoreBar();
  drawBoardContour();
  for (int8_t i = 0; i < nbOfCells; i++) {
    if (board[i] != 0) drawTile(i);
  }
}

void displayCancel() {
  minitel.newXY(1,0);
  if (ccount == 0) minitel.print("Jouer c'est jouer...");
  if (ccount == 1) minitel.print("C'est non !");
  if (ccount == 2) minitel.print("Et t'as essayé Ctrl Z aussi ?");
  if (ccount == 3) minitel.print("Laisse tomber");
  delay(1000); if (sound) minitel.bip(); minitel.newXY(1,0); minitel.cancel();
  if(++ccount > boardSize) ccounted();
  flushInputs();
}

bool displayConfirm(String str){
  minitel.newXY(1,0); minitel.attributs(CARACTERE_MAGENTA); 
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

void display2048() {
  eraseScreen();
  int8_t y = 8;
  delay(100);minitel.newXY(6,y++);minitel.print("  _");minitel.repeat(24);minitel.print("  ");
  delay(100);minitel.newXY(6,y++);minitel.print(" / ");minitel.repeat(24);minitel.print("\\ ");
  delay(100);minitel.newXY(6,y++);minitel.print(" {  Et voilà c'est fait:    {");
  delay(500);minitel.newXY(6,y++);minitel.print(" {   la tuile ");minitel.attributs(DOUBLE_LARGEUR);minitel.attributs(INVERSION_FOND); minitel.print("2048");minitel.attributs(GRANDEUR_NORMALE);minitel.attributs(FOND_NORMAL);minitel.print("      {");
  delay(500);minitel.newXY(6,y++);minitel.print(" {    le Graal ...          {");
  delay(500);minitel.newXY(6,y++);minitel.print(" { ");minitel.repeat(25);minitel.print("{");
  delay(100);minitel.newXY(6,y++);minitel.print(" {  ");
    minitel.attributs(DOUBLE_LARGEUR);minitel.attributs(INVERSION_FOND);minitel.attributs(CLIGNOTEMENT);minitel.print("  BRAVO!!! ");
    minitel.attributs(GRANDEUR_NORMALE);minitel.attributs(FOND_NORMAL);minitel.attributs(FIXE);minitel.print("  {");
  delay(100);minitel.newXY(6,y++);minitel.print(" \\_");minitel.repeat(24);minitel.print("/ ");
  if (sound) {
    uint32_t beat = 0b01010111011110110101011101111011; // 1=bip, 0=rest
    for (int8_t i = 31; i >= 0 ; i--) {
      if (bitRead(beat, i)) minitel.bip();
      delay(250);
    }
  } else {
    delay(4000);
  }
  minitel.newXY(6,y++);minitel.print(" / ");minitel.repeat(24);minitel.print("\\ ");
  minitel.newXY(6,y++);minitel.print(" {  On continue ?           {");
  minitel.newXY(6,y++);minitel.print(" { ");minitel.repeat(25);minitel.print("{");
  minitel.newXY(6,y++);minitel.print(" { ");minitel.repeat(9);
    minitel.attributs(INVERSION_FOND);minitel.print("ENVOI");
    minitel.attributs(FOND_NORMAL);minitel.print(" CONTINUER");minitel.print(" {");
  minitel.newXY(6,y++);minitel.print(" { ");minitel.repeat(9);
    minitel.attributs(INVERSION_FOND);minitel.print("ANNUL.");
    minitel.attributs(FOND_NORMAL);minitel.print("  QUITTER");minitel.print(" {");
  minitel.newXY(6,y++);minitel.print(" \\ ");minitel.repeat(24);minitel.print("/ ");
  minitel.newXY(6,y++);
    minitel.print("  ~");minitel.repeat(24);minitel.print("  ");

  flushInputs();
  unsigned long key = minitel.getKeyCode();
  while (true) {
    if (key == 0x1341) { // Envoi
      displayGame();
      break;
    }
    else if (key == 0x1345) { // Annul.
      over = true;
      break;
    }
    key = minitel.getKeyCode();
  }
}

void displayScore(bool best) {
  int8_t y = 8;
  minitel.newXY(6,y++);minitel.print("  _");minitel.repeat(24);minitel.print("  ");
  minitel.newXY(6,y++);minitel.print(" / ");minitel.repeat(24);minitel.print("\\ ");
  minitel.newXY(6,y++);minitel.print(" {Fin de partie en mode ");minitel.printChar(boardSize+48);minitel.printChar('x');minitel.printChar(boardSize+48);minitel.print(" {");
  minitel.newXY(6,y++);minitel.print(" { ");minitel.repeat(25);minitel.print("{");
  minitel.newXY(6,y);  minitel.print(" {  Votre score est         {");
  minitel.newXY(27,y++);minitel.attributs(INVERSION_FOND); minitel.print(String(score));
  minitel.newXY(6,y++);minitel.print(" { ");minitel.repeat(25);minitel.print("{");
  minitel.newXY(6,y++);minitel.print(" \\ ");minitel.repeat(24);minitel.print("/ ");
  minitel.newXY(6,y++);minitel.print("  ~");minitel.repeat(24);minitel.print("  ");
  delay(2000);
  minitel.newXY(1,0); minitel.cancel(); // erase status row 0
  if (!best) {
    if (sound) minitel.bip();
  }
  else {
    y -= 2;
    minitel.newXY(6,y++);minitel.printChar(' ');minitel.attributs(INVERSION_FOND);minitel.attributs(CLIGNOTEMENT);minitel.attributs(DOUBLE_HAUTEUR);y++;minitel.print("  NOUVEAU MEILLEUR SCORE ! ");minitel.attributs(FOND_NORMAL);minitel.attributs(FIXE);minitel.printChar(' ');
    minitel.newXY(6,y++);minitel.print(" { ");minitel.repeat(25);minitel.print("{");
    minitel.newXY(6,y++);minitel.print(" \\ ");minitel.repeat(24);minitel.print("/ ");
    minitel.newXY(6,y++);minitel.print("  ~");minitel.repeat(24);minitel.print("  ");
    if (sound) {
      uint32_t beat = 0b01111010011110100111101001111010; // 1=bip, 0=rest
      for (int8_t i = 31; i >= 0; i--) {
        if (bitRead(beat, i)) minitel.bip();
        delay(250);
      }
    }
  }
  delay(1000);
  flushInputs();
  minitel.newXY(1,0); minitel.attributs(CARACTERE_MAGENTA); minitel.print("Appuie sur une touche ..");
  while (!minitel.getKeyCode());
}

//----- EEPROM functions -------------------------------

/* EEPROM memory usage
 *  Bytes 0 to 11 : Best scores
 *   0 to 3 : mode 2x2 (uint32_t -> 4 bytes)
 *   4 to 7 : mode 3x3 (uint32_t -> 4 bytes)
 *   8 to 11: mode 4x4 (uint32_t -> 4 bytes)
 *  Bytes 12 to 23 : Saved scores
 *   12 to 15: mode 2x2 (uint32_t -> 4 bytes)
 *   16 to 19: mode 3x3 (uint32_t -> 4 bytes)
 *   20 to 23: mode 4x4 (uint32_t -> 4 bytes)
 *  Bytes 24 to 52 : Saved boards
 *   24 to 27: mode 2x2 (2*2 int8_t -> 4 bytes)
 *   28 to 36: mode 3x3 (3*3 int8_t -> 9 bytes)
 *   37 to 52: mode 4x4 (4*4 int8_t -> 16 bytes)
 *  Byte 54 : EEPROM magic
 */

void saveBestScore() {
  int addr = (boardSize-2)*sizeof(uint32_t);
  EEPROM.put(addr, bestScore);
}

void loadBestScore() {
  int addr = (boardSize-2)*sizeof(uint32_t);
  EEPROM.get(addr, bestScore);
}

void saveScore() {
  int addr = (MAX_BOARD_SIZE-1)*sizeof(uint32_t);
  addr += (boardSize-2)*sizeof(uint32_t);
  EEPROM.put(addr, score);
}

void loadScore() {
  int addr = (MAX_BOARD_SIZE-1)*sizeof(uint32_t);
  addr += (boardSize-2)*sizeof(uint32_t);
  EEPROM.get(addr, score);
}

void saveBoard() {
  int addr = 2*(MAX_BOARD_SIZE-1)*sizeof(uint32_t);
  if (boardSize > 2) addr += 2*2*sizeof(int8_t);
  if (boardSize > 3) addr += 3*3*sizeof(int8_t);
  for (int8_t i = 0; i < nbOfCells ; i++) {
    EEPROM.put(addr, board[i]);
    addr += sizeof(int8_t);
  }
}

void loadBoard() {
  int addr = 2*(MAX_BOARD_SIZE-1)*sizeof(uint32_t);
  if (boardSize > 2) addr += 2*2*sizeof(int8_t);
  if (boardSize > 3) addr += 3*3*sizeof(int8_t);
  for (int8_t i = 0; i < nbOfCells ; i++) {
    EEPROM.get(addr, board[i]);
    if (board[i] > maxTile) maxTile = board[i];
    addr += sizeof(int8_t);
  }
}

void setupEEPROM() {
  uint8_t magic = EEPROM.read(54);
  if (magic != MAGIC) {
    debugPrintln("Wrong magic : Initialize EEPROM");
    for (uint8_t i = 0; i < 54; i++) EEPROM.update(i,0);
    EEPROM.write(54,MAGIC);
  }
}
