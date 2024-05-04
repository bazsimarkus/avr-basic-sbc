
////////////////////////////////////////////////////////////////////////////////
// TinyBasic Plus / AVR Tiny BASIC Computer


#include <PS2uartKeyboard.h>
#include <TVout.h>
#include <video_gen.h>
//#include <font4x6.h>
#include <font6x8.h>
#include <font8x8.h>
//#include <font8x8ext.h>
//#include <fontALL.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>
#include <spieeprom.h>
#include <avr/pgmspace.h>
//#include <EepromFS.h>

//#####################################################################################
// DEFINES
//#####################################################################################
#define kVersion              "v0.20"
#define ARDUINO               1
#define FONTS                 6         // 6 = 6x8, 8 = 8x8
#define kPiezoPin             15
#define ENABLE_EEPROM         1
#define kConsoleBaud          9600
#define boolean               int
#define true                  1
#define false                 0
#define ECHO_CHARS            1
#define CURSOR_BLINK_PERIOD   500       // ms
#define XEEPROM_TYP           0         // 0=16bit, 1=24bit
#define XEEPROM_SIZE          8192
#define ENABLE_FILEIO         1         // enables LOAD, SAVE, FILES for SD card (adds 9k of usage)
#define ENABLE_AUTORUN        0         //if there's FileIO, and a file "autorun.bas" then it will load it and run it when starting up
#define kAutorunFilename      "autorun.bas"
#define ENABLE_EAUTORUN       0         // this is the alternate autorun.  Autorun the program in the eeprom. it will load whatever is in the EEProm and run it
#define ENABLE_TONES          1         // this will enable the "TONE", "NOTONE" command using a piezo element on the specified pin.  it adds 1.5k of usage
#define kSD_CS                4         // CS Pin for SD Card
#define kSD_Fail              0
#define kSD_OK                1
#define kRamFileIO            (200)       //(1100) /* approximate */
#define kRamTones             (0)       //(40)


////////////////////////////////////////////////////////////////////////////////
// ASCII Characters
#define CR                    '\r'
#define NL                    '\n'
#define LF                    0x0a
#define TAB                   '\t'
#define BELL                  '\b'
#define SPACE                 ' '
#define SQUOTE                '\''
#define DQUOTE                '\"'
#define CTRLC                 0x1B  // Changed to ESC key (27 - 0x1B) BREAK KEY
#define CTRLH                 0x7F //0x08 BACKSPACE
#define CTRLS                 0x13
#define CTRLX                 0x18
#define CTRTA                 0x9 // TAB
#define CTRUP                 0xB // UP
#define CTRDW                 0xA // DOWN
#define CTRLE                 0x8 // LEFT
#define CTRRI                 0x15 // RIGHT
//#define PAGEUP                0x19 // F12

#define FUNC_PEEK             0
#define FUNC_ABS              1
#define FUNC_AREAD            2
#define FUNC_DREAD            3
#define FUNC_RND              4
#define FUNC_UNKNOWN          5

#define RELOP_GE              0
#define RELOP_NE              1
#define RELOP_GT              2
#define RELOP_EQ              3
#define RELOP_LE              4
#define RELOP_LT              5
#define RELOP_NE_BANG         6
#define RELOP_UNKNOWN         7



#ifndef RAMEND
#define RAMEND 4096-1
#endif

#define kRamSize  (RAMEND - 8803 - kRamFileIO - kRamTones) //8803


//#undef ENABLE_FILEIO
#undef ENABLE_AUTORUN
#undef ENABLE_EAUTORUN


PS2uartKeyboard keyboard;
TVout TV;
File fp;
//EepromFS EFS(0x50, 16384);

// #############################################################################################################
// STRINGS
// #############################################################################################################
static const unsigned char sentinel[]            PROGMEM = "  **** AVR Tiny Basic PLUS ****   ";
static const unsigned char okmsg[]            PROGMEM = "READY.";
static const unsigned char syntaxmsg[]        PROGMEM = "Syntax Error";
static const unsigned char invalidexprmsg[]   PROGMEM = "Invalid expression";
static const unsigned char badlinemsg[]       PROGMEM = "Invalid line number";
static const unsigned char notenoughmem[]     PROGMEM = "Not enough memory!";
static const unsigned char initmsg[]          PROGMEM = "16K SRAM SYSTEM ATMega1284P " kVersion;
static const unsigned char memorymsg[]        PROGMEM = " BASIC bytes free.";

static const unsigned char eeprommsg[]        PROGMEM = " EEProm bytes.";
static const unsigned char eepromamsg[]       PROGMEM = " EEProm bytes available.";
static const unsigned char eepromamsgX[]       PROGMEM = " XEEProm bytes available.";

static const unsigned char breakmsg[]         PROGMEM = "break!";
static const unsigned char unimplimentedmsg[] PROGMEM = "Unimplemented";
static const unsigned char backspacemsg[]     PROGMEM = "\b \b";
static const unsigned char indentmsg[]        PROGMEM = "    ";
static const unsigned char sderrormsg[]       PROGMEM = "SD card error.";
static const unsigned char sdfilemsg[]        PROGMEM = "SD file error.";
static const unsigned char dirextmsg[]        PROGMEM = "(dir)";
static const unsigned char slashmsg[]         PROGMEM = "/";
static const unsigned char spacemsg[]         PROGMEM = " ";
static const unsigned char format[]           PROGMEM = "Formating...";
static const unsigned char prozentsign[]      PROGMEM = "%";


// #############################################################################################################
// Variablen
// #############################################################################################################

char eliminateCompileErrors = 1;  // fix to suppress arduino build errors

// Cursor
bool blink;
unsigned long lastBlinkedTime = 0;
bool cursorOn;

bool isSerial1Open = false;

// EEprom
int eepos = 0;
long eeposX = 0;
SPIEEPROM disk1(XEEPROM_TYP);  // Type 0 = 16bit address




#ifdef ENABLE_FILEIO
// functions defined elsehwere
void cmd_Files( void );
#endif



#ifndef byte
typedef unsigned char byte;
#endif

// some catches for AVR based text string stuff...
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte( A ) *(A)
#endif

////////////////////

#ifdef ENABLE_FILEIO
unsigned char * filenameWord(void);
static boolean sd_is_initialized = false;
#endif

boolean inhibitOutput = false;
static boolean runAfterLoad = false;
static boolean triggerRun = false;

// these will select, at runtime, where IO happens through for load/save
enum {
  kStreamSerial = 0,
  kStreamEEProm,
  kStreamEEPromX,
  kStreamSercom,
  kStreamFile
};
static unsigned char inStream = kStreamSerial;
static unsigned char outStream = kStreamSerial;
static unsigned char outStreamX = kStreamSerial;


typedef short unsigned LINENUM;

static unsigned char program[kRamSize];

static unsigned char *txtpos, *list_line, *tmptxtpos;;
static unsigned char expression_error;
static unsigned char *tempsp;
static unsigned char cursorposx;
static unsigned char cursorposy;

/***********************************************************/
// Keyword table and constants - the last character has 0x80 added to it
const static unsigned char keywords[] PROGMEM = {
  'L', 'I', 'S', 'T' + 0x80,
  'L', 'O', 'A', 'D' + 0x80,
  'N', 'E', 'W' + 0x80,
  'R', 'U', 'N' + 0x80,
  'S', 'A', 'V', 'E' + 0x80,
  'N', 'E', 'X', 'T' + 0x80,
  'L', 'E', 'T' + 0x80,
  'I', 'F' + 0x80,
  'G', 'O', 'T', 'O' + 0x80,
  'G', 'O', 'S', 'U', 'B' + 0x80,
  'R', 'E', 'T', 'U', 'R', 'N' + 0x80,
  'R', 'E', 'M' + 0x80,
  'F', 'O', 'R' + 0x80,
  'I', 'N', 'P', 'U', 'T' + 0x80,
  'I', 'N', 'C', 'H', 'R', 'N' + 0x80,
  'P', 'R', 'I', 'N', 'T' + 0x80,
  'P', 'O', 'K', 'E' + 0x80,
  'S', 'T', 'O', 'P' + 0x80,
  'B', 'Y', 'E' + 0x80,
  'F', 'I', 'L', 'E', 'S' + 0x80,
  'M', 'E', 'M' + 0x80,
  '?' + 0x80,
  '\'' + 0x80,
  'A', 'W', 'R', 'I', 'T', 'E' + 0x80,
  'D', 'W', 'R', 'I', 'T', 'E' + 0x80,
  'D', 'E', 'L', 'A', 'Y' + 0x80,
  'E', 'N', 'D' + 0x80,
  'R', 'S', 'E', 'E', 'D' + 0x80,
  'C', 'L', 'S' + 0x80,
  'N', 'L', 'I', 'S', 'T' + 0x80,
  'D', 'R', 'A', 'W', 'P', 'I', 'X' + 0x80,
  'D', 'R', 'A', 'W', 'L', 'I', 'N', 'E' + 0x80,
  'D', 'R', 'A', 'W', 'R', 'E', 'C', 'T' + 0x80,
  'D', 'R', 'A', 'W', 'C', 'I', 'R', 'C' + 0x80,
  'D', 'R', 'A', 'W', 'C', 'H', 'A', 'R' + 0x80,
  'G', 'E', 'T', 'P', 'I', 'X' + 0x80,
  'L', 'O', 'C', 'A', 'T', 'E' + 0x80, 

  'T', 'O', 'N', 'E', 'W' + 0x80,
  'T', 'O', 'N', 'E' + 0x80,
  'N', 'O', 'T', 'O', 'N', 'E' + 0x80,

  'E', 'L', 'I', 'S', 'T' + 0x80,
  'E', 'L', 'O', 'A', 'D' + 0x80,
  'E', 'F', 'O', 'R', 'M', 'A', 'T' + 0x80,
  'E', 'S', 'A', 'V', 'E' + 0x80,
 
  'X', 'L', 'I', 'S', 'T' + 0x80,
  'X', 'L', 'O', 'A', 'D' + 0x80,
  'X', 'F', 'O', 'R', 'M', 'A', 'T' + 0x80,
  'X', 'S', 'A', 'V', 'E' + 0x80,

  'I', 'N', 'K', 'E', 'Y' + 0x80,
  'X', 'P', 'O', 'K', 'E' + 0x80,
  'X', 'P', 'E', 'E', 'K' + 0x80,
  'E', 'P', 'O', 'K', 'E' + 0x80,
  'E', 'P', 'E', 'E', 'K' + 0x80,

  'S', 'E', 'R', 'O', 'P', 'E', 'N' + 0x80,
  'S', 'E', 'R', 'C', 'L', 'O', 'S', 'E' + 0x80,
  'S', 'E', 'R', 'P', 'R', 'I', 'N', 'T' + 0x80,
  'S', 'E', 'R', 'R', 'E',  'A', 'D' + 0x80,
  'S', 'E', 'R', 'L', 'O', 'A', 'D' + 0x80,

  'D', 'E', 'L', 'E', 'T', 'E' + 0x80,
  0
};

// by moving the command list to an enum, we can easily remove sections
// above and below simultaneously to selectively obliterate functionality.
enum {
  KW_LIST = 0,
  KW_LOAD, KW_NEW, KW_RUN, KW_SAVE,
  KW_NEXT, KW_LET, KW_IF,
  KW_GOTO, KW_GOSUB, KW_RETURN,
  KW_REM,
  KW_FOR,
  KW_INPUT, KW_INCHRN, KW_PRINT,
  KW_POKE,
  KW_STOP, KW_BYE,
  KW_FILES,
  KW_MEM,
  KW_QMARK, KW_QUOTE,
  KW_AWRITE, KW_DWRITE,
  KW_DELAY,
  KW_END,
  KW_RSEED,
  KW_CLEAR,
  KW_NLIST,
  KW_DRAWPIX,
  KW_DRAWLINE,
  KW_DRAWRECT,
  KW_DRAWCIRC,
  KW_DRAWCHAR,
  KW_GETPIX,
  KW_LOCATE,
//#ifdef ENABLE_TONES
  KW_TONEW, KW_TONE, KW_NOTONE,
//#endif
  KW_ELIST, KW_ELOAD, KW_EFORMAT, KW_ESAVE, KW_XLIST, KW_XLOAD, KW_XFORMAT, KW_XSAVE,
  KW_INKEY,
  KW_XPOKE,
  KW_XPEEK,
  KW_EPOKE,
  KW_EPEEK,
  KW_SEROPEN,
  KW_SERCLOSE,
  KW_SERPRINT,
  KW_SERREAD,
  KW_SERLOAD,
  KW_DELETE,
  KW_DEFAULT /* always the final one*/
};

struct stack_for_frame {
  char frame_type;
  char for_var;
  short int terminal;
  short int step;
  unsigned char *current_line;
  unsigned char *txtpos;
};

struct stack_gosub_frame {
  char frame_type;
  unsigned char *current_line;
  unsigned char *txtpos;
};

const static unsigned char func_tab[] PROGMEM = {
  'P', 'E', 'E', 'K' + 0x80,
  'A', 'B', 'S' + 0x80,
  'A', 'R', 'E', 'A', 'D' + 0x80,
  'D', 'R', 'E', 'A', 'D' + 0x80,
  'R', 'N', 'D' + 0x80,
  0
};


const static unsigned char to_tab[] PROGMEM = {
  'T', 'O' + 0x80,
  0
};

const static unsigned char step_tab[] PROGMEM = {
  'S', 'T', 'E', 'P' + 0x80,
  0
};

const static unsigned char then_tab[] PROGMEM = {
  'T', 'H', 'E', 'N' + 0x80,
  0
};

const static unsigned char relop_tab[] PROGMEM = {
  '>', '=' + 0x80,
  '<', '>' + 0x80,
  '>' + 0x80,
  '=' + 0x80,
  '<', '=' + 0x80,
  '<' + 0x80,
  '!', '=' + 0x80,
  0
};



const static unsigned char highlow_tab[] PROGMEM = {
  'H', 'I', 'G', 'H' + 0x80,
  'H', 'I' + 0x80,
  'L', 'O', 'W' + 0x80,
  'L', 'O' + 0x80,
  0
};
#define HIGHLOW_HIGH    1
#define HIGHLOW_UNKNOWN 4

#define STACK_SIZE (sizeof(struct stack_for_frame)*5)
#define VAR_SIZE sizeof(short int) // Size of variables in bytes

static unsigned char *stack_limit;
static unsigned char *program_start;
static unsigned char *program_end;
static unsigned char *stack; // Software stack for things that should go on the CPU stack
static unsigned char *variables_begin;
static unsigned char *current_line;
static unsigned char *sp1;
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'
static unsigned char table_index;
static LINENUM linenum;

static int inchar(void);
static void outchar(unsigned char c);
static void line_terminator(void);
static short int expression(void);
static unsigned char breakcheck(void);








//################################################################################################################
// SETUP (MAIN)
//################################################################################################################
void setup()
{

  //TV.begin(PAL, 752, 504);
  TV.begin(PAL, 720, 480);
  if(FONTS == 6) TV.select_font(font6x8);
  if(FONTS == 8) TV.select_font(font8x8);

  cursorOn = true;

 // EEPROM IC
  disk1.setup(); 

  // mount or format
  //if (!EFS.begin()) EFS.format(16);

  

//#ifdef ENABLE_FILEIO
  initSD();

#ifdef ENABLE_AUTORUN
  if ( SD.exists( kAutorunFilename )) {
    program_end = program_start;
    fp = SD.open( kAutorunFilename );
    inStream = kStreamFile;
    inhibitOutput = true;
    runAfterLoad = true;
  }
#endif /* ENABLE_AUTORUN */

//#endif /* ENABLE_FILEIO */

#ifdef ENABLE_EEPROM
#ifdef ENABLE_EAUTORUN
  // read the first byte of the eeprom. if it's a number, assume it's a program we can load
  int val = EEPROM.read(0);
  if ( val >= '0' && val <= '9' ) {
    program_end = program_start;
    inStream = kStreamEEProm;
    eepos = 0;
    inhibitOutput = true;
    runAfterLoad = true;
  }
#endif /* ENABLE_EAUTORUN */
#endif /* ENABLE_EEPROM */

  delay(100);
  TV.clear_screen();
  printmsg( sentinel );
  printmsg(initmsg);
  CursorReset();
  //delay(200);
  TV.set_hbi_hook(keyboard.begin());
}



/***************************************************************************/
static void getln(char prompt)
{
  //outchar(prompt);

  char flag;
  
  txtpos = program_end + sizeof(LINENUM);
  //TV.print_char(cursorposx-FONTS,cursorposy,128);

  
  //if (millis() - lastBlinkedTime >= CURSOR_BLINK_PERIOD) 
  //{
  //  blink = !blink;
  //  lastBlinkedTime = millis();
  //}
  
  while (1)
  {
    char c = inchar();
    
    switch (c)
    {

    
      case NL:
      //break;
      TV.print_char(cursorposx-FONTS,cursorposy,' ');
      line_terminator();
        // Terminate all strings with a NL
        txtpos[0] = NL;
        return 1;
        
      case CTRLC: // ESCAPE
        cursorOn = true;
        TV.clear_screen();
        cursorposx = FONTS;
        cursorposy = 0;
        break;
      
      case CR: // ENTER
        TV.print_char(cursorposx-FONTS,cursorposy,' ');
        line_terminator();
        // Terminate all strings with a NL
        txtpos[0] = NL;
        return;
      
      case CTRTA:
        break;

      case CTRUP:
       // cursorposy = cursorposy - 8;
        break;
      //case CTRDW:
      //  break;
      case CTRLE:
      
          break;
       case CTRRI:
        
        break;
        
      case CTRLH: // BACKSPACE
              
        if (txtpos == program_end+2)
          break;
        txtpos--;
     
   
        printmsgNoNL(backspacemsg);
        blink = 0;
        lastBlinkedTime = millis();
        TV.print_char(cursorposx,cursorposy,127);
        cursorposx = cursorposx - FONTS;
   
        break;
      default:
      
        // We need to leave at least one space to allow us to shuffle the line into order
        if (txtpos == variables_begin - 2)
          outchar(BELL);
         
        else
        {
            
          txtpos[0] = c;
          txtpos++;
          outchar(c);
          
        
         if(cursorposx > 197) 
         {
           
            TV.print_char(cursorposx,cursorposy,127);
            
            flag = 1;
            cursorposx = 0;
            //TV.print_char(cursorposx,cursorposy,127);
            cursorposy = cursorposy + 8;
           
         }
           
           TV.print_char(cursorposx,cursorposy,127);
         
           cursorposx = cursorposx + FONTS;
            if(flag == 1)
            {
              
              cursorposx = 0;
              flag = 0;
            }
          
          
       
                    
        }
    }
  }
}















/***************************************************************************/
static void ignore_blanks(void)
{
  while (*txtpos == SPACE || *txtpos == TAB)
    txtpos++;
   
}


/***************************************************************************/
static void scantable(unsigned char *table)
{
  int i = 0;
  table_index = 0;
  while (1)
  {
    // Run out of table entries?
    if (pgm_read_byte( table ) == 0)
      return;

    // Do we match this character?
    if (txtpos[i] == pgm_read_byte( table ))
    {
      i++;
      table++;
    }
    else
    {
      // do we match the last character of keywork (with 0x80 added)? If so, return
      if (txtpos[i] + 0x80 == pgm_read_byte( table ))
      {
        txtpos += i + 1; // Advance the pointer to following the keyword
        ignore_blanks();
        return;
      }

      // Forward to the end of this keyword
      while ((pgm_read_byte( table ) & 0x80) == 0)
        table++;

      // Now move on to the first character of the next word, and reset the position index
      table++;
      table_index++;
      ignore_blanks();
      i = 0;
    }
  }
}

/***************************************************************************/
static void pushb(unsigned char b)
{
  sp1--;
  *sp1 = b;
}

/***************************************************************************/
static unsigned char popb()
{
  unsigned char b;
  b = *sp1;
  sp1++;
  return b;
}

/***************************************************************************/
void printnum(int num)
{
  int digits = 0;

  if (num < 0)
  {
    num = -num;
    outchar('-');
  }
  do {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while (digits > 0)
  {
    outchar(popb());
    digits--;
  }
}

void sendnum(int num)
{
  int digits = 0;

  if (num < 0)
  {
    num = -num;
     Serial1.print('-');
  }
  do {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while (digits > 0)
  {
    Serial1.print((char)popb());
    digits--;
  }
  Serial1.println();
}

void printnumX(int num)
{
  int digits = 0;

  if (num < 0)
  {
    num = -num;
    outcharX('-');
  }
  do {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while (digits > 0)
  {
    outcharX(popb());
    digits--;
  }
}

void printUnum(unsigned int num)
{
  int digits = 0;

  do {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while (digits > 0)
  {
    outchar(popb());
    digits--;
  }
}

/***************************************************************************/
static unsigned short testnum(void)
{
  unsigned short num = 0;
  ignore_blanks();

  while (*txtpos >= '0' && *txtpos <= '9' )
  {
    // Trap overflows
    if (num >= 0xFFFF / 10)
    {
      num = 0xFFFF;
      break;
    }

    num = num * 10 + *txtpos - '0';
    txtpos++;
  }
  return	num;
}

/***************************************************************************/
static unsigned char print_quoted_string(void)
{
  int i = 0;
  unsigned char delim = *txtpos;
  if (delim != '"' && delim != '\'')
    return 0;
  txtpos++;

  // Check we have a closing delimiter
  while (txtpos[i] != delim)
  {
    if (txtpos[i] == NL)
      return 0;
    i++;
  }

  // Print the characters
  while (*txtpos != delim)
  {
    outchar(*txtpos);
    txtpos++;
    cursorposx = cursorposx + 6;
  }
  txtpos++; // Skip over the last delimiter
  //cursorposx = cursorposx + 6;
  return 1;
}

/***************************************************************************/
static unsigned char serialsend_quoted_string(void)
{
  int i = 0;
  unsigned char delim = *txtpos;
  if (delim != '"' && delim != '\'')
    return 0;
  txtpos++;

  // Check we have a closing delimiter
  while (txtpos[i] != delim)
  {
    if (txtpos[i] == NL)
      return 0;
    i++;
  }

  // Print the characters
  while (*txtpos != delim)
  {
    Serial1.print(char(*txtpos));
    txtpos++;
  }
  txtpos++; // Skip over the last delimiter
  Serial1.println("");
  //cursorposx = cursorposx + 6;
  return 1;
}


/***************************************************************************/
void printmsgNoNL(const unsigned char *msg)
{
  while ( pgm_read_byte( msg ) != 0 ) {
    outchar( pgm_read_byte( msg++ ) );
  };
}

/***************************************************************************/
void printmsg(const unsigned char *msg)
{
  printmsgNoNL(msg);
  line_terminator();
}



/***************************************************************************/
static unsigned char *findline(void)
{
  unsigned char *line = program_start;
  while (1)
  {
    if (line == program_end)
      return line;

    if (((LINENUM *)line)[0] >= linenum)
      return line;

    // Add the line length onto the current address, to get to the next line;
    line += line[sizeof(LINENUM)];
  }
}

/***************************************************************************/
static void toUppercaseBuffer(void)
{
  unsigned char *c = program_end + sizeof(LINENUM);
  unsigned char quote = 0;

  while (*c != NL)
  {
    // Are we in a quoted string?
    if (*c == quote)
      quote = 0;
    else if (*c == '"' || *c == '\'')
      quote = *c;
    else if (quote == 0 && *c >= 'a' && *c <= 'z')
      *c = *c + 'A' - 'a';
    c++;
  }
}

/***************************************************************************/
void printline()
{
  LINENUM line_num;

  line_num = *((LINENUM *)(list_line));
  list_line += sizeof(LINENUM) + sizeof(char);

  // Output the line */
  printnum(line_num);
  outchar(' ');
  while (*list_line != NL)
  {
    outchar(*list_line);
    list_line++;
  }
  list_line++;
  line_terminator();

}

void printlineX()
{
  LINENUM line_num;

  line_num = *((LINENUM *)(list_line));
  list_line += sizeof(LINENUM) + sizeof(char);

  // Output the line */
  printnumX(line_num);
  outcharX(' ');
  while (*list_line != NL)
  {
    outcharX(*list_line);
    list_line++;
  }
  list_line++;
  line_terminatorX();
}



/***************************************************************************/
static short int expr4(void)
{
  // fix provided by Jurg Wullschleger wullschleger@gmail.com
  // fixes whitespace and unary operations
  ignore_blanks();

  if ( *txtpos == '-' ) {
    txtpos++;
    return -expr4();
  }
  // end fix

  if (*txtpos == '0')
  {
    txtpos++;
    return 0;
  }

  if (*txtpos >= '1' && *txtpos <= '9')
  {
    short int a = 0;
    do 	{
      a = a * 10 + *txtpos - '0';
      txtpos++;
    }
    while (*txtpos >= '0' && *txtpos <= '9');
    return a;
  }

  // Is it a function or variable reference?
  if (txtpos[0] >= 'A' && txtpos[0] <= 'Z')
  {
    short int a;
    // Is it a variable reference (single alpha)
    if (txtpos[1] < 'A' || txtpos[1] > 'Z')
    {
      a = ((short int *)variables_begin)[*txtpos - 'A'];
      txtpos++;
      return a;
    }

    // Is it a function with a single parameter
    scantable(func_tab);
    if (table_index == FUNC_UNKNOWN)
      goto expr4_error;

    unsigned char f = table_index;

    if (*txtpos != '(')
      goto expr4_error;

    txtpos++;
    a = expression();
    if (*txtpos != ')')
      goto expr4_error;
    txtpos++;
    switch (f)
    {
      case FUNC_PEEK:
        return program[a];

      case FUNC_ABS:
        if (a < 0)
          return -a;
        return a;

//#ifdef ARDUINO
      case FUNC_AREAD:
        pinMode( a, INPUT );
        return analogRead( a );
      case FUNC_DREAD:
        pinMode( a, INPUT );
        return digitalRead( a );
//#endif

      case FUNC_RND:
//#ifdef ARDUINO
        return ( random( a ));
//#else
//        return ( rand() % a );
//#endif
    }
  }

  if (*txtpos == '(')
  {
    short int a;
    txtpos++;
    a = expression();
    if (*txtpos != ')')
      goto expr4_error;

    txtpos++;
    return a;
  }

expr4_error:
  expression_error = 1;
  return 0;

}

/***************************************************************************/
static short int expr3(void)
{
  short int a, b;

  a = expr4();

  ignore_blanks(); // fix for eg:  100 a = a + 1

  while (1)
  {
    if (*txtpos == '*')
    {
      txtpos++;
      b = expr4();
      a *= b;
    }
    else if (*txtpos == '/')
    {
      txtpos++;
      b = expr4();
      if (b != 0)
        a /= b;
      else
        expression_error = 1;
    }
    else
      return a;
  }
}

/***************************************************************************/
static short int expr2(void)
{
  short int a, b;

  if (*txtpos == '-' || *txtpos == '+')
    a = 0;
  else
    a = expr3();

  while (1)
  {
    if (*txtpos == '-')
    {
      txtpos++;
      b = expr3();
      a -= b;
    }
    else if (*txtpos == '+')
    {
      txtpos++;
      b = expr3();
      a += b;
    }
    else
      return a;
  }
}
/***************************************************************************/
static short int expression(void)
{
  short int a, b;

  a = expr2();

  // Check if we have an error
  if (expression_error)	return a;

  scantable(relop_tab);
  if (table_index == RELOP_UNKNOWN)
    return a;

  switch (table_index)
  {
    case RELOP_GE:
      b = expr2();
      if (a >= b) return 1;
      break;
    case RELOP_NE:
    case RELOP_NE_BANG:
      b = expr2();
      if (a != b) return 1;
      break;
    case RELOP_GT:
      b = expr2();
      if (a > b) return 1;
      break;
    case RELOP_EQ:
      b = expr2();
      if (a == b) return 1;
      break;
    case RELOP_LE:
      b = expr2();
      if (a <= b) return 1;
      break;
    case RELOP_LT:
      b = expr2();
      if (a < b) return 1;
      break;
  }
  return 0;
}

/***************************************************************************/
void loop()
{
  unsigned char *start;
  unsigned char *newEnd;
  unsigned char linelen;
  boolean isDigital;
  boolean alsoWait = false;
  int val;

//#ifdef ARDUINO
#ifdef ENABLE_TONES
  //noTone( kPiezoPin );
#endif
//#endif

  program_start = program;
  program_end = program_start;
  sp1 = program + sizeof(program); // Needed for printnum
  stack_limit = program + sizeof(program) - STACK_SIZE;
  variables_begin = stack_limit - 27 * VAR_SIZE;

  // memory free
  printnum(variables_begin - program_end);
  printmsg(memorymsg);
//#ifdef ARDUINO
//#ifdef ENABLE_EEPROM
  // eprom size
  printnum( E2END + 1 );
  printmsg( eeprommsg );
//#endif /* ENABLE_EEPROM */
//#endif /* ARDUINO */

warmstart:
  // this signifies that it is running in 'direct' mode.
  cursorOn = true;
  current_line = 0;
  sp1 = program + sizeof(program);
  printmsg(okmsg);
  //TV.set_cursor(0,32);
  //TV.print("READY.");
  

prompt:
  if ( triggerRun ) {
    triggerRun = false;
    current_line = program_start;
    goto execline;
  }

  getln( '>' );
  toUppercaseBuffer();

  txtpos = program_end + sizeof(unsigned short);

  // Find the end of the freshly entered line
  while (*txtpos != NL)
    txtpos++;

  // Move it to the end of program_memory
  {
    unsigned char *dest;
    dest = variables_begin - 1;
    while (1)
    {
      *dest = *txtpos;
      if (txtpos == program_end + sizeof(unsigned short))
        break;
      dest--;
      txtpos--;
    }
    txtpos = dest;
  }

  // Now see if we have a line number
  linenum = testnum();
  ignore_blanks();
  if (linenum == 0)
    goto direct;

  if (linenum == 0xFFFF)
    goto badline;

  // Find the length of what is left, including the (yet-to-be-populated) line header
  linelen = 0;
  while (txtpos[linelen] != NL)
    linelen++;
  linelen++; // Include the NL in the line length
  linelen += sizeof(unsigned short) + sizeof(char); // Add space for the line number and line length

  // Now we have the number, add the line header.
  txtpos -= 3;
  *((unsigned short *)txtpos) = linenum;
  txtpos[sizeof(LINENUM)] = linelen;


  // Merge it into the rest of the program
  start = findline();

  // If a line with that number exists, then remove it
  if (start != program_end && *((LINENUM *)start) == linenum)
  {
    unsigned char *dest, *from;
    unsigned tomove;

    from = start + start[sizeof(LINENUM)];
    dest = start;

    tomove = program_end - from;
    while ( tomove > 0)
    {
      *dest = *from;
      from++;
      dest++;
      tomove--;
    }
    program_end = dest;
  }

  if (txtpos[sizeof(LINENUM) + sizeof(char)] == NL) // If the line has no txt, it was just a delete
    goto prompt;



  // Make room for the new line, either all in one hit or lots of little shuffles
  while (linelen > 0)
  {
    unsigned int tomove;
    unsigned char *from, *dest;
    unsigned int space_to_make;

    space_to_make = txtpos - program_end;

    if (space_to_make > linelen)
      space_to_make = linelen;
    newEnd = program_end + space_to_make;
    tomove = program_end - start;


    // Source and destination - as these areas may overlap we need to move bottom up
    from = program_end;
    dest = newEnd;
    while (tomove > 0)
    {
      from--;
      dest--;
      *dest = *from;
      tomove--;
    }

    // Copy over the bytes into the new space
    for (tomove = 0; tomove < space_to_make; tomove++)
    {
      *start = *txtpos;
      txtpos++;
      start++;
      linelen--;
    }
    program_end = newEnd;
  }
  goto prompt;

unimplemented:
  printmsg(unimplimentedmsg);
  goto prompt;

invalidexpr:
  printmsg(invalidexprmsg);
  goto prompt;

badline:
 printmsg(badlinemsg);
  goto prompt;

SyntaxError:
  printmsgNoNL(syntaxmsg);
  if (current_line != NULL)
  {
    unsigned char tmp = *txtpos;
    if (*txtpos != NL)
      *txtpos = '>';
    list_line = current_line;
    printline();
    *txtpos = tmp;
  }
  line_terminator();
  goto prompt;

memory:
  printmsg(notenoughmem);
  goto warmstart;

run_next_statement:
  while (*txtpos == ':')
    txtpos++;
  ignore_blanks();
  if (*txtpos == NL)
  //cursorposy = cursorposy + 8;
    goto execnextline;
  goto interperateAtTxtpos;

direct:
  txtpos = program_end + sizeof(LINENUM);
  if (*txtpos == NL)
    goto prompt;

interperateAtTxtpos:
  if (breakcheck())
  {
    printmsg(breakmsg);
    goto warmstart;
  }

  scantable(keywords);

  switch (table_index)
  {
    case KW_DELAY:
      {

        expression_error = 0;
        val = expression();
        delay( val );
        goto execnextline;

      }

    case KW_FILES:
      goto files;
    case KW_LIST:
      goto list;
    case KW_CLEAR:
      TV.clear_screen();
      cursorposy = 0;
      goto run_next_statement;
    case KW_NLIST:
      goto nlist;
    case KW_DRAWPIX:
      goto drawpix;
    case KW_DRAWLINE:
      goto drawline;
    case KW_DRAWRECT:
      goto drawrect;
    case KW_DRAWCIRC:
      goto drawcirc;
    case KW_DRAWCHAR:
      goto drawchar;
    case KW_GETPIX:
      goto getpix;
    case KW_LOCATE:
      goto setcursor;
    case KW_LOAD:
      goto load;
    case KW_MEM:
      goto mem;
    case KW_NEW:
      if (txtpos[0] != NL)
        goto SyntaxError;
      program_end = program_start;
      goto prompt;
    case KW_RUN:
      current_line = program_start;
      goto execline;
    case KW_SAVE:
      goto save;
    case KW_NEXT:
      goto next;
    case KW_LET:
      goto assignment;
    case KW_IF:
      short int val;
      expression_error = 0;
      val = expression();
      if (expression_error || *txtpos == NL)
        goto invalidexpr;
      scantable(then_tab);
      if (table_index != 0)
        goto SyntaxError;
      if (val != 0)
        goto interperateAtTxtpos;
      goto execnextline;

    case KW_GOTO:
      expression_error = 0;
      linenum = expression();
      if (expression_error || *txtpos != NL)
        goto invalidexpr;
      current_line = findline();
      goto execline;

    case KW_GOSUB:
      goto gosub;
    case KW_RETURN:
      goto gosub_return;
    case KW_REM:
    case KW_QUOTE:
      goto execnextline;	// Ignore line completely
    case KW_FOR:
      goto forloop;
    case KW_INPUT:
      goto input;
    case KW_INCHRN:
      goto inchrn;
    case KW_PRINT:
    case KW_QMARK:
      goto print;
    case KW_POKE:
      goto poke;
    case KW_END:
    case KW_STOP:
      // This is the easy way to end - set the current line to the end of program attempt to run it
      if (txtpos[0] != NL)
        goto SyntaxError;
      current_line = program_end;
      goto execline;
    case KW_BYE:
    TV.clear_screen();
    printmsg( sentinel );
    printmsg(initmsg);
      // Leave the basic interperater
      CursorReset();
      return;

    case KW_AWRITE:  // AWRITE <pin>, HIGH|LOW
      isDigital = false;
      goto awrite;
    case KW_DWRITE:  // DWRITE <pin>, HIGH|LOW
      isDigital = true;
      goto dwrite;

    case KW_RSEED:
      goto rseed;

#ifdef ENABLE_TONES
    case KW_TONEW:
      alsoWait = true;
    case KW_TONE:
      goto tonegen;
    case KW_NOTONE:
      goto tonestop;
#endif


    case KW_EFORMAT:
      goto eformat;
    case KW_ESAVE:
      goto esave;
    case KW_ELOAD:
      goto eload;
    case KW_ELIST:
      goto elist;
    case KW_XFORMAT:
      goto xformat;
    case KW_XSAVE:
      goto xsave;
    case KW_XLOAD:
      goto xload;
    case KW_XLIST:
      goto xlist;

    case KW_INKEY:
      goto inkey;
    case KW_XPOKE:
      goto xpoke;
    case KW_XPEEK:
      goto xpeek;
    case KW_EPOKE:
      goto epoke;
    case KW_EPEEK:
      goto epeek;
    case KW_SEROPEN:
      goto seropen; 
    case KW_SERCLOSE:
      goto serclose;
    case KW_SERPRINT:
      goto serprint; 
    case KW_SERREAD:
      goto serread;
    case KW_SERLOAD:
      goto serload;
    case KW_DELETE:
      goto deletef;     
    case KW_DEFAULT:
      goto assignment;
    default:
      break;
  }

execnextline:
  if (current_line == NULL)		// Processing direct commands?
    goto prompt;
  current_line +=	 current_line[sizeof(LINENUM)];
  

execline:

  if (current_line == program_end)
  {// Out of lines to run
    //cursorposy = cursorposy - 8;
    goto warmstart;
  }
  txtpos = current_line + sizeof(LINENUM) + sizeof(char);
  goto interperateAtTxtpos;

//#ifdef ARDUINO
//#ifdef ENABLE_EEPROM
elist:
  {
    int i;
    for ( i = 0 ; i < (E2END + 1) ; i++ )
    {
      val = EEPROM.read( i );
      //cursorposy = cursorposy + i;
      if ( val == '\0' ) {
         goto execnextline;
     }

     if ( val == '\n' )
     {
      cursorposy = cursorposy + 8;  
      }
      
      if ( ((val < ' ') || (val  > '~')) && (val != NL) && (val != CR))  {
        outchar( '?' );
      }
      else {
        //cursorposy = cursorposy + 8;
        outchar( val );
      
      }
      
    }
    //goto warmstart;
  }
  goto execnextline;
  

eformat:
  {
    
    for ( uint16_t i = 0 ; i < E2END ; i++ )
    {
      if ( (i & 0x03f) == 0x20 ) outchar( '.' );
      EEPROM.write( i, 0 );
    }
    outchar( LF );
  }
  goto execnextline;

esave:
  {
    outStream = kStreamEEProm;
    eepos = 0;

    // copied from "List"
    list_line = findline();
    while (list_line != program_end)
      printline();

    // go back to standard output, close the file
    outStream = kStreamSerial;

    goto warmstart;
  }


eload:
  // clear the program
  program_end = program_start;

  // load from a file into memory
  eepos = 0;
  inStream = kStreamEEProm;
 // inhibitOutput = true;
  goto warmstart;

xlist:
  {
   
    uint16_t i;
    for (i = 0; i < XEEPROM_SIZE; i++)
    {
      val = disk1.read_byte(i);
      if ( val == '\0' ) {
        goto execnextline;
      }

      if ( val == '\n' )
      {
        cursorposy = cursorposy + 8;  
      }
      
      if ( ((val < ' ') || (val  > '~')) && (val != NL) && (val != CR))  {
        outcharX( '?' );
      }
      else {
        outcharX( val );
      }
    }
  }

  goto execnextline;

xformat:
  {
    
    uint16_t ESize = XEEPROM_SIZE;
    float prozent;
    TV.printPGM(format);
    //printmsg(format);
    
    for (uint16_t i = 0; i < ESize; i++)
    {
      if ( (i & 0x03f) == 0x20 )
      {
        //outcharX( '.' ); //0x3ff = 1024kbit
        prozent = prozent + 0.7816;
      }

      //TV.set_cursor(0,cursorposy+8);
      TV.println(0, cursorposy+8, prozent);
      //printnum(prozent);
      //TV.set_cursor(30,cursorposy+8);
      TV.printPGM(30, cursorposy+8, prozentsign);
      //printmsg(prozentsign);
      disk1.write( i, 0 );
    }
    cursorposy = cursorposy + 8;
    outcharX( LF );
    
  }
cursorposy = cursorposy + 8;
  //goto warmstart;
  goto execnextline;

xsave:
  {
   outStream = kStreamEEPromX;
     eeposX = 0;

    // copied from "List"
    list_line = findline();
    while (list_line != program_end)
      printlineX();

    // go back to standard output, close the file
    outStream = kStreamSerial;
    goto warmstart;
  }


xload:
 // clear the program
  program_end = program_start;

  // load from a file into memory
  eeposX = 0;
  inStream = kStreamEEPromX;
 // inhibitOutput = true;
  goto warmstart;



xpoke:
{
    unsigned int x;
    int z;
  
    //Get the x val
    expression_error = 0;
    
    x = expression();
    if (expression_error)
      goto SyntaxError;
       
     //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') 
      goto SyntaxError;
   
    txtpos++;
    ignore_blanks();

    expression_error = 0;
    z = expression();
    if (expression_error)
      goto SyntaxError;
  disk1.write( z, x );
 goto run_next_statement;
  }

xpeek:
 {
    int z;
    short int value;
    short int *var;
    
    if (*txtpos < 'A' || *txtpos > 'Z') {
      goto invalidexpr;
    }
    
    var = (short int *)variables_begin + *txtpos - 'A';
    txtpos++;
     ignore_blanks();

     if (*txtpos != ',') 
      goto SyntaxError;

    txtpos++;
    ignore_blanks();
  
    expression_error = 0;
    z = expression();
    if (expression_error)
      goto SyntaxError;
  
  value = disk1.read_byte( z );
 *var = value;

 goto run_next_statement;
  }

epoke:
{
    unsigned int x;
    byte z;
  
    //Get the x val
    expression_error = 0;
    
    x = expression();
    if (expression_error)
      goto SyntaxError;
       
     //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') 
      goto SyntaxError;
   
    txtpos++;
    ignore_blanks();

    expression_error = 0;
    z = expression();
    if (expression_error)
      goto SyntaxError;
  EEPROM.write( z, x );
 goto run_next_statement;
  }

epeek:
 {
    byte z;
    short int value;
    short int *var;
    
    if (*txtpos < 'A' || *txtpos > 'Z') {
      goto invalidexpr;
    }
    
    var = (short int *)variables_begin + *txtpos - 'A';
    txtpos++;
     ignore_blanks();

     if (*txtpos != ',') 
      goto SyntaxError;

    txtpos++;
    ignore_blanks();
  
    expression_error = 0;
    z = expression();
    if (expression_error)
      goto SyntaxError;
  
  value = EEPROM.read( z );
 *var = value;

 goto run_next_statement;
  }


//#endif /* ENABLE_EEPROM */
//#endif

input:
  cursorOn = true;
  {
    unsigned char var;
    int value;
    ignore_blanks();
    //if (*txtpos < (char) (-128) || *txtpos > (char) (-125)) {
    if (*txtpos < 'A' || *txtpos > 'Z') {
      goto invalidexpr;
    }
    //}
    var = *txtpos;
    txtpos++;
    ignore_blanks();
    if (*txtpos != NL && *txtpos != ':')
      goto SyntaxError;
inputagain:
    tmptxtpos = txtpos;
    getln( '?' );
    toUppercaseBuffer();
    txtpos = program_end + sizeof(unsigned short);
    ignore_blanks();
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto inputagain;
    ((short int *)variables_begin)[var - 'A'] = value;
    txtpos = tmptxtpos;

    goto run_next_statement;
  }
inchrn:
  cursorOn = true;
  {
    unsigned char var;
    int value;
    ignore_blanks();
    //if (*txtpos < (char) (-128) || *txtpos > (char) (-125)) {
    if (*txtpos < 'A' || *txtpos > 'Z') {
      goto invalidexpr;
    }
    //}
    var = *txtpos;
    txtpos++;
    ignore_blanks();
    if (*txtpos != NL && *txtpos != ':')
      goto SyntaxError;
inchrnagain:
    tmptxtpos = txtpos;
    while(!keyboard.available()) {}
    value = keyboard.read() - '0';
    ((short int *)variables_begin)[var - 'A'] = value;
    txtpos = tmptxtpos;

    goto run_next_statement;
  }

forloop:
  {
    unsigned char var;
    short int initial, step, terminal;
    ignore_blanks();
    if (*txtpos < 'A' || *txtpos > 'Z')
      goto SyntaxError;
    var = *txtpos;
    txtpos++;
    ignore_blanks();
    if (*txtpos != '=')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();

    expression_error = 0;
    initial = expression();
    if (expression_error)
      goto invalidexpr;

    scantable(to_tab);
    if (table_index != 0)
      goto SyntaxError;

    terminal = expression();
    if (expression_error)
      goto invalidexpr;

    scantable(step_tab);
    if (table_index == 0)
    {
      step = expression();
      if (expression_error)
        goto invalidexpr;
    }
    else
      step = 1;
    ignore_blanks();
    if (*txtpos != NL && *txtpos != ':')
      goto SyntaxError;


    if (!expression_error && *txtpos == NL)
    {
      struct stack_for_frame *f;
      if (sp1 + sizeof(struct stack_for_frame) < stack_limit)
        goto memory;

      sp1 -= sizeof(struct stack_for_frame);
      f = (struct stack_for_frame *)sp1;
      ((short int *)variables_begin)[var - 'A'] = initial;
      f->frame_type = STACK_FOR_FLAG;
      f->for_var = var;
      f->terminal = terminal;
      f->step     = step;
      f->txtpos   = txtpos;
      f->current_line = current_line;
      goto run_next_statement;
    }
  }
  goto invalidexpr;

gosub:
  expression_error = 0;
  linenum = expression();
  if(expression_error)
    goto invalidexpr;
  if (!expression_error && *txtpos == NL)
  {
    struct stack_gosub_frame *f;
    if (sp1 + sizeof(struct stack_gosub_frame) < stack_limit)
      goto memory;

    sp1 -= sizeof(struct stack_gosub_frame);
    f = (struct stack_gosub_frame *)sp1;
    f->frame_type = STACK_GOSUB_FLAG;
    f->txtpos = txtpos;
    f->current_line = current_line;
    current_line = findline();
    goto execline;
  }
  goto SyntaxError;

next:
  // Fnd the variable name
  ignore_blanks();
  if (*txtpos < 'A' || *txtpos > 'Z')
    goto invalidexpr;
  txtpos++;
  ignore_blanks();
  if (*txtpos != ':' && *txtpos != NL)
    goto SyntaxError;

gosub_return:
  // Now walk up the stack frames and find the frame we want, if present
  tempsp = sp1;
  while (tempsp < program + sizeof(program) - 1)
  {
    switch (tempsp[0])
    {
      case STACK_GOSUB_FLAG:
        if (table_index == KW_RETURN)
        {
          struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
          current_line	= f->current_line;
          txtpos			= f->txtpos;
          sp1 += sizeof(struct stack_gosub_frame);
          goto run_next_statement;
        }
        // This is not the loop you are looking for... so Walk back up the stack
        tempsp += sizeof(struct stack_gosub_frame);
        break;
      case STACK_FOR_FLAG:
        // Flag, Var, Final, Step
        if (table_index == KW_NEXT)
        {
          struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
          // Is the the variable we are looking for?
          if (txtpos[-1] == f->for_var)
          {
            short int *varaddr = ((short int *)variables_begin) + txtpos[-1] - 'A';
            *varaddr = *varaddr + f->step;
            // Use a different test depending on the sign of the step increment
            if ((f->step > 0 && *varaddr <= f->terminal) || (f->step < 0 && *varaddr >= f->terminal))
            {
              // We have to loop so don't pop the stack
              txtpos = f->txtpos;
              current_line = f->current_line;
              goto run_next_statement;
            }
            // We've run to the end of the loop. drop out of the loop, popping the stack
            sp1 = tempsp + sizeof(struct stack_for_frame);
            goto run_next_statement;
          }
        }
        // This is not the loop you are looking for... so Walk back up the stack
        tempsp += sizeof(struct stack_for_frame);
        break;
      default:
        //printf("Stack is stuffed!\n");
        goto warmstart;
    }
  }
  // Didn't find the variable we've been looking for
  goto invalidexpr;

assignment:
  {
    short int value;
    short int *var;

     if (*txtpos < 'A' || *txtpos > 'Z') {
      goto invalidexpr;
    }

    var = (short int *)variables_begin + *txtpos - 'A';
    txtpos++;

    ignore_blanks();

    if (*txtpos != '=')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto invalidexpr;
    // Check that we are at the end of the statement
    if (*txtpos != NL && *txtpos != ':')
      goto SyntaxError;
    *var = value;
  }
  goto run_next_statement;
poke:
  {
    short int value;
    unsigned char *address;

    // Work out where to put it
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto SyntaxError;
    address = (unsigned char *)value;

    // check for a comma
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();

    // Now get the value to assign
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto invalidexpr;
    //printf("Poke %p value %i\n",address, (unsigned char)value);
    // Check that we are at the end of the statement
    if (*txtpos != NL && *txtpos != ':')
      goto SyntaxError;
  }
  goto run_next_statement;

list:
  linenum = testnum(); // Retuns 0 if no line found.

  // Should be EOL
  if (txtpos[0] != NL)
    goto SyntaxError;

  // Find the line
  list_line = findline();
  while (list_line != program_end)
    printline();
    
  goto warmstart;

print:
  // If we have an empty list then just put out a NL
  cursorOn = false;
  if (*txtpos == ':' )
  {
    line_terminator();
    txtpos++;
    //cursorposx = cursorposx + 6;
    goto run_next_statement;
  }
  if (*txtpos == NL)
  {
    goto execnextline;
  }

  while (1)
  {
    ignore_blanks();
    if (print_quoted_string())
    {
      ;
    }
    else if (*txtpos == '"' || *txtpos == '\'')
      goto SyntaxError;
    else
    {
      short int e;
      expression_error = 0;
      e = expression();
      if (expression_error)
        goto invalidexpr;
      printnum(e);
    }

    // At this point we have three options, a comma or a new line
    if (*txtpos == ',')
      txtpos++;	// Skip the comma and move onto the next
    else if (txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':'))
    {
      txtpos++; // This has to be the end of the print - no newline
     // cursorposx = cursorposx + 6;
      break;
    }
    else if (*txtpos == NL || *txtpos == ':')
    {
      line_terminator();	// The end of the print statement
      break;
    }
    else
      goto SyntaxError;
  }
  cursorOn = true;
  goto run_next_statement;

mem:
  // memory free
   uint16_t ESize = XEEPROM_SIZE;
  
  printnum(variables_begin - program_end);
  printmsg(memorymsg);
 

  {
    // eprom size
    printnum( E2END + 1 );
    printmsg( eeprommsg );

    // figure out the memory usage;
    val = ' ';
    int i;
    for ( i = 0 ; (i < (E2END + 1)) && (val != '\0') ; i++ ) {
      val = EEPROM.read( i );
    }
    printnum( (E2END + 1) - (i - 1) );

    printmsg( eepromamsg );
  }

    //printnum(8192);
    //printmsg(eeprommsg);

    val = ' ';
    int i;
    for ( i = 0 ; (i < (ESize)) && (val != '\0') ; i++ ) {
      val = disk1.read_byte( i );
    }
    printnum( (ESize) - (i - 1) );
    printmsg( eepromamsgX );

  goto run_next_statement;


inkey:
  cursorOn= false;
  {
    while(1)
    {
    char c = inchar();
    switch (c)
    {
      default:
      cursorOn = true;
      goto run_next_statement;
      break;
    }
    }
    
    toUppercaseBuffer();
    cursorOn = true;
    goto run_next_statement;
  }


  /*************************************************/

//#ifdef ARDUINO
awrite: // AWRITE <pin>,val
dwrite:
  {
    short int pinNo;
    short int value;
    unsigned char *txtposBak;

    // Get the pin number
    expression_error = 0;
    pinNo = expression();
    if (expression_error)
      goto SyntaxError;

    // check for a comma
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    txtposBak = txtpos;
    scantable(highlow_tab);
    if (table_index != HIGHLOW_UNKNOWN)
    {
      if ( table_index <= HIGHLOW_HIGH ) {
        value = 1;
      }
      else {
        value = 0;
      }
    }
    else {

      // and the value (numerical)
      expression_error = 0;
      value = expression();
      if (expression_error)
        goto SyntaxError;
    }
    pinMode( pinNo, OUTPUT );
    if ( isDigital ) {
      digitalWrite( pinNo, value );
    }
    else {
      analogWrite( pinNo, value );
    }
  }
  goto run_next_statement;

seropen:
  {
    Serial1.begin(9600);
    isSerial1Open = true;
  }
  goto run_next_statement;

serclose:
  {
    Serial1.end();
    isSerial1Open = false;
  }
  goto run_next_statement;

serprint:
  {
  if (*txtpos == ':' )
  {
    line_terminator();
    txtpos++;
    //cursorposx = cursorposx + 6;
    goto run_next_statement;
  }
  if (*txtpos == NL)
  {
    goto execnextline;
  }

  while (1)
  {
    ignore_blanks();
    if (serialsend_quoted_string())
    {
      ;
    }
    else if (*txtpos == '"' || *txtpos == '\'')
      goto SyntaxError;
    else
    {
      short int e;
      expression_error = 0;
      e = expression();
      if (expression_error)
        goto invalidexpr;
      sendnum(e);
    }

    // At this point we have three options, a comma or a new line
    if (*txtpos == ',')
      txtpos++;	// Skip the comma and move onto the next
    else if (txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':'))
    {
      txtpos++; // This has to be the end of the print - no newline
     // cursorposx = cursorposx + 6;
      break;
    }
    else if (*txtpos == NL || *txtpos == ':')
    {
      //line_terminator();	// The end of the print statement, not needed as serial send, if enabled, unneccessary empty line is added after sercom send
      break;
    }
    else
      goto SyntaxError;
  }
      //Serial1.println("Hello world!");
  }
  goto run_next_statement;

serread:
  {
    if(isSerial1Open){
      unsigned long timeout = 1000; // Set your timeout value (in milliseconds)

      while (true) { // Loop indefinitely
        unsigned long startTime = millis();
        while (!Serial1.available() && ((millis() - startTime) <= timeout)) {} // Wait until data is available or timeout

        if ((millis() - startTime) > timeout) {
          // Timeout occurred, handle it here
          break; // For example, you can break the loop
        }

        char incomingChar = Serial1.read(); // Read the incoming character

        if (incomingChar == '\n') { // Check for LF
          line_terminator();
          break; // Exit the loop if LF is received
        } else {
          // Check if the incoming character is a human-readable ASCII character
          if (incomingChar >= 32 && incomingChar <= 126) {
            outchar( incomingChar );
          }
        }
      }
    }
  }
  goto run_next_statement;

serload:
  if(isSerial1Open){
 // clear the program
  program_end = program_start;

  inStream = kStreamSercom;
  inhibitOutput = true;
  }
  goto warmstart;
//#else
//pinmode: // PINMODE <pin>, I/O
//awrite: // AWRITE <pin>,val
//dwrite:
//  goto unimplemented;
//#endif

  /*************************************************/
files:
  // display a listing of files on the device.
  // version 1: no support for subdirectories

#ifdef ENABLE_FILEIO
  cmd_Files();
  goto warmstart;
#else
  goto unimplemented;
#endif // ENABLE_FILEIO


load:
  // clear the program
  program_end = program_start;

  // load from a file into memory
#ifdef ENABLE_FILEIO
  {
    unsigned char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if (expression_error)
      goto SyntaxError;

    // Arduino specific
    if ( !SD.exists( (char *)filename ))
    {
      printmsg( sdfilemsg );
    }
    else {

      fp = SD.open( (const char *)filename );
      inStream = kStreamFile;
     // inhibitOutput = true;
    }


  }
  
  goto warmstart;
#else // ENABLE_FILEIO
  goto unimplemented;
#endif // ENABLE_FILEIO



save:
#ifdef ENABLE_FILEIO
  {
    unsigned char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if (expression_error)
      goto SyntaxError;

    // remove the old file if it exists
    if ( SD.exists( (char *)filename )) {
      SD.remove( (char *)filename );
    }

    // open the file, switch over to file output
    fp = SD.open( (const char *)filename, FILE_WRITE );
    outStream = kStreamFile;

    // copied from "List"
    list_line = findline();
    while (list_line != program_end)
      printline();

    // go back to standard output, close the file
    outStream = kStreamSerial;

    fp.close();
    goto warmstart;
  }
#else // ENABLE_FILEIO
  goto unimplemented;
#endif // ENABLE_FILEIO

deletef:
#ifdef ENABLE_FILEIO
  {
    unsigned char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if (expression_error)
      goto SyntaxError;

    // remove the old file if it exists
    if ( SD.exists( (char *)filename )) {
      SD.remove( (char *)filename );
    }

   
    outStream = kStreamSerial;

    fp.close();
    goto warmstart;
  }
#else // ENABLE_FILEIO
  goto unimplemented;
#endif // ENABLE_FILEIO







rseed:
  {
    short int value;

    //Get the pin number
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto SyntaxError;

//#ifdef ARDUINO
    randomSeed( value );
//#else // ARDUINO
//    srand( value );
//#endif // ARDUINO
    goto run_next_statement;
  }

nlist: {
    uint16_t x;
    uint16_t z;
  
    //Get the x val
    expression_error = 0;
    
    x = expression();
    if (expression_error)
      goto SyntaxError;
       
     //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') 
      goto SyntaxError;
   
    txtpos++;
    ignore_blanks();

    expression_error = 0;
    z = expression();
    if (expression_error)
      goto SyntaxError;
    
    
    linenum = x; // Retuns 0 if no line found.

    // Should be EOL
    if (txtpos[0] != NL) {
      goto SyntaxError;
    }

    // Find the line
    list_line = findline();
   
    
          for(int y=0;y<z;y++)
          {
            if(list_line != program_end)
            {
              linenum++;
              printline();
            }else{
              break;
            }
            
          }

    goto warmstart;
  }



drawpix: {
    unsigned int x;
    unsigned int y;
    unsigned int col;

    //Get the x coordinate
    expression_error = 0;
    x = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    //Get the y coordinate
    expression_error = 0;
    y = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    //Get the color
    expression_error = 0;
    col = expression();
    if (expression_error)
      goto SyntaxError;

    if ( x < 0 || x > TV.hres() || y < 0 || y > TV.vres() || col > 2)
      goto SyntaxError;

    TV.set_pixel(x, y, col);
    goto run_next_statement;
  }

drawline: {
    unsigned int x0;
    unsigned int y0;
    unsigned int x1;
    unsigned int y1;
    unsigned int col;

    //Get the x0 coordinate
    expression_error = 0;
    x0 = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();


    //Get the y0 coordinate
    expression_error = 0;
    y0 = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();

    //Get the x1 coordinate
    expression_error = 0;
    x1 = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();


    //Get the y1 coordinate
    expression_error = 0;
    y1 = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();

    //Get the color
    expression_error = 0;
    col = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    if ( x0 < 0 || x0 > TV.hres() || y0 < 0 || y0 > TV.vres() || x1 < 0 || x1 > TV.hres() || y1 < 0 || y1 > TV.vres() || col > 2) {
      goto SyntaxError;
    }

    TV.draw_line(x0, y0, x1, y1, col);
    goto run_next_statement;
  }





drawrect: {
    unsigned int x0;
    unsigned int y0;
    unsigned int w;
    unsigned int h;
    unsigned int col;
    unsigned int fillC;


    //Get the x0 coordinate
    expression_error = 0;
    x0 = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();

    //Get the y0 coordinate
    expression_error = 0;
    y0 = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();


    //Get the width
    expression_error = 0;
    w = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();

    //Get the height
    expression_error = 0;
    h = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();

    //Get the height
    expression_error = 0;
    col = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();

    //Get the height
    expression_error = 0;
    fillC = expression();
    if (expression_error) {
      goto SyntaxError;
    }


    if ( x0 < 0 || x0 > TV.hres() || y0 < 0 || y0 > TV.vres() || h < 0 || h > TV.vres() || w < 0 || w > TV.hres()) {
      goto SyntaxError;
    }

    TV.draw_rect(x0, y0, w, h, col, fillC);
    goto run_next_statement;
  }

drawcirc: {
    unsigned int x0;
    unsigned int y0;
    unsigned int r;
    unsigned char col;
    unsigned char fillC;


    //Get the x0 coordinate
    expression_error = 0;
    x0 = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();

    //Get the y0 coordinate
    expression_error = 0;
    y0 = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    //Get the width
    expression_error = 0;
    r = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();

    //Get the height
    expression_error = 0;
    col = expression();
    if (expression_error) {
      goto SyntaxError;
    }

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',') {
      goto SyntaxError;
    }
    txtpos++;
    ignore_blanks();

    //Get the height
    expression_error = 0;
    fillC = expression();
    if (expression_error) {
      goto SyntaxError;
    }


    if ( x0 < 0 || x0 > TV.hres() || y0 < 0 || y0 > TV.vres() || r < 0 || r > TV.vres())
      goto SyntaxError;

    TV.draw_circle(x0, y0, r, col, fillC);
    goto run_next_statement;
  }

drawchar: {
    unsigned int x;
    unsigned int y;
    unsigned char c;

    //Get the x coordinate
    expression_error = 0;
    x = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    //Get the y coordinate
    expression_error = 0;
    y = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    //Get the character
    ignore_blanks();
    //if (*txtpos < ' ' || *txtpos > '~')
    //  goto SyntaxError;
    c = *txtpos;
    txtpos++;

    if ( x < 0 || x > TV.hres() || y < 0 || y > TV.vres())
      goto SyntaxError;

    TV.print_char(x, y, c);
    //goto prompt;
    goto run_next_statement;
  }

getpix: {
    unsigned int x;
    unsigned int y;

    //Get the x coordinate
    expression_error = 0;
    x = expression();
    if (expression_error)
      goto SyntaxError;

    //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    //Get the y coordinate
    expression_error = 0;
    y = expression();
    if (expression_error)
      goto SyntaxError;

    if ( x < 0 || x > TV.hres() || y < 0 || y > TV.vres())
      goto SyntaxError;

    //TV.set_pixel(x, y, col);
    //goto run_next_statement;
    printnum((int)TV.get_pixel(x, y));
    printmsg(spacemsg);
    goto run_next_statement;
  }

setcursor:
{
  short int x;
  short int y;

 expression_error = 0;
 x = expression();
if (expression_error)
   goto SyntaxError;

 //Ignore the blanks
    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();

    //Get the y coordinate
    expression_error = 0;
    y = expression();
    if (expression_error)
      goto SyntaxError;

if ( x < 0 || x > TV.hres() || y < 0 || y > TV.vres())
      goto SyntaxError;

TV.set_cursor(x * FONTS, y * 8);
cursorposy = (y * 8); 
goto run_next_statement;
}





#ifdef ENABLE_TONES
tonestop:
  //noTone( kPiezoPin);
  TV.noTone();
  goto run_next_statement;

tonegen:
  {
    // TONE freq, duration
    // if either are 0, tones turned off
    unsigned int freq;
    unsigned long duration;

    //Get the frequency
    expression_error = 0;
    freq = expression();
    if (expression_error)
      goto SyntaxError;

    ignore_blanks();
    if (*txtpos != ',')
      goto SyntaxError;
    txtpos++;
    ignore_blanks();


    //Get the duration
    expression_error = 0;
    duration = expression();
    if (expression_error)
      goto SyntaxError;

    if ( freq == 0 || duration == 0 )
      goto tonestop;

    //tone(kPiezoPin, freq, duration);
    TV.tone(freq, duration);
    if ( alsoWait ) {
      delay( duration );
      alsoWait = false;
    }
    goto run_next_statement;
  }
#endif /* ENABLE_TONES */
}

// returns 1 if the character is valid in a filename
static int isValidFnChar( char c )
{
  if ( c >= '0' && c <= '9' ) return 1; // number
  if ( c >= 'A' && c <= 'Z' ) return 1; // LETTER
  if ( c >= 'a' && c <= 'z' ) return 1; // letter (for completeness)
  if ( c == '_' ) return 1;
  if ( c == '+' ) return 1;
  if ( c == '.' ) return 1;
  if ( c == '~' ) return 1; // Window~1.txt

  return 0;
}

unsigned char * filenameWord(void)
{
  // SDL - I wasn't sure if this functionality existed above, so I figured i'd put it here
  unsigned char * ret = txtpos;
  expression_error = 0;

  // make sure there are no quotes or spaces, search for valid characters
  //while(*txtpos == SPACE || *txtpos == TAB || *txtpos == SQUOTE || *txtpos == DQUOTE ) txtpos++;
  while ( !isValidFnChar( *txtpos )) txtpos++;
  ret = txtpos;

  if ( *ret == '\0' ) {
    expression_error = 1;
    return ret;
  }

  // now, find the next nonfnchar
  txtpos++;
  while ( isValidFnChar( *txtpos )) txtpos++;
  if ( txtpos != ret ) *txtpos = '\0';

  // set the error code if we've got no string
  if ( *ret == '\0' ) {
    expression_error = 1;
  }

  return ret;
}

/***************************************************************************/
static void line_terminator(void)
{
  outchar(NL);
  outchar(CR);
  cursorposx = FONTS;
  if(cursorposy < 208)
  {
    cursorposy = cursorposy + 8;
  }else{
    cursorposy = 216;
  }
  
}

static void line_terminatorX(void)
{
  outcharX(NL);
  outcharX(CR);
  outchar(NL);
  outchar(CR);
  cursorposx = FONTS;
  if(cursorposy < 208)
  {
    cursorposy = cursorposy + 8;
  }else{
    cursorposy = 216;
  }
  
}


/***********************************************************/
static unsigned char breakcheck(void)
{

  if (keyboard.available())
    return keyboard.read() == CTRLC;
  return 0;
}
/***********************************************************/



static int inchar()
{
  int v;
//#ifdef ARDUINO

  switch ( inStream ) {
    case ( kStreamFile ):
//#ifdef ENABLE_FILEIO
      v = fp.read();
      if ( v == NL ) v = CR; // file translate
      if ( !fp.available() ) {
        fp.close();
        goto inchar_loadfinish;
      }
      return v;
//#else
//#endif
      break;
    
    
    case ( kStreamEEProm ):
      v = EEPROM.read( eepos++ );
      if ( v == '\0' ) {
        goto inchar_loadfinish;
      }
      return v;
      break;

      case ( kStreamEEPromX ):
      v = disk1.read_byte( eeposX++ );
      if ( v == '\0' ) {
        goto inchar_loadfinish;
      }
      return v;
      break;

    case ( kStreamSercom ):
      while (!Serial1.available()) {} // Wait until data is available
      v = Serial1.read(); // Read the incoming character

      if ( v == '\0' ) {
        goto inchar_loadfinish;
      }
      return v;
      break;

      
    case ( kStreamSerial ):
    default:
      while (1)
      {
        if (keyboard.available()) return keyboard.read();
        
    
       if (millis() - lastBlinkedTime >= CURSOR_BLINK_PERIOD) {
            blink = !blink;
            lastBlinkedTime = millis();
          }
        
        if(blink)
        {
          // TV.print_char(cursorposx-FONTS,cursorposy,127);
          // TV.draw_rect(cursorposx-(FONTS),cursorposy-1,5,7,2,2);
          // blink = 0;
        }else{
          if(cursorOn) TV.draw_cur(cursorposx-FONTS,cursorposy,5,7,2);
           //TV.set_pixel(cursorposx-FONTS,cursorposy,2);
          // if(cursorOn) TV.print_char(cursorposx-FONTS,cursorposy,128);
           TV.print_char(cursorposx,cursorposy,127);
           blink = 1;
        }
         
      
      
      }
      
  }

inchar_loadfinish:
  inStream = kStreamSerial;
  inhibitOutput = false;

  if ( runAfterLoad ) {
    runAfterLoad = false;
    triggerRun = true;
  }
  return NL; // trigger a prompt.

//#else
  // otherwise. desktop!
//  int got = getchar();

  // translation for desktop systems
//  if ( got == LF ) got = CR;

//  return got;
//#endif
}












/***********************************************************/
static void outchar(unsigned char c)
{
  if ( inhibitOutput ) return;

#ifdef ARDUINO
#ifdef ENABLE_FILEIO
  if ( outStream == kStreamFile ) {
    // output to a file
    fp.write( c );
  }
  else
#endif
#ifdef ARDUINO
#ifdef ENABLE_EEPROM
    if ( outStream == kStreamEEProm ) {
      EEPROM.write( eepos++, c );
      // disk1.write( eepos++, c );
    }
    else
    //if ( outStreamX == kStreamEEPromX ) {
    //  disk1.write( eepos++, c );
    //}
    //else
#endif /* ENABLE_EEPROM */
#endif /* ARDUINO */
      TV.print((char)c);

#else
  //putchar(c);
#endif
}


static void outcharX(unsigned char c)

{

    #ifdef ARDUINO
#ifdef ENABLE_EEPROM
    if ( outStream == kStreamEEPromX ) {
      disk1.write( eeposX++, c );
      // disk1.write( eepos++, c );
    }
    else
    //if ( outStreamX == kStreamEEPromX ) {
    //  disk1.write( eepos++, c );
    //}
    //else
#endif /* ENABLE_EEPROM */
#endif /* ARDUINO */
      TV.print((char)c);
        

}

/***********************************************************/
/* SD Card helpers */



static int initSD( void )
{
  // if the card is already initialized, we just go with it.
  // there is no support (yet?) for hot-swap of SD Cards. if you need to
  // swap, pop the card, reset the arduino.)

  if ( sd_is_initialized == true ) return kSD_OK;

  // due to the way the SD Library works, pin 10 always needs to be
  // an output, even when your shield uses another line for CS
  pinMode(4, OUTPUT); // change this to 53 on a mega

  if ( !SD.begin( kSD_CS )) {
    // failed
    //printmsg( sderrormsg );
    return kSD_Fail;
  }
  // success - quietly return 0
  sd_is_initialized = true;

  // and our file redirection flags
  outStream = kStreamSerial;
  inStream = kStreamSerial;
  inhibitOutput = false;

  return kSD_OK;
}


#if ENABLE_FILEIO
void cmd_Files( void )
{
  
  File dir = SD.open( "/" );
  dir.seek(0);

  while ( true ) {
    File entry = dir.openNextFile();
    if ( !entry ) {
      entry.close();
      break;
    }

    // common header
    printmsgNoNL( indentmsg );
    //printmsgNoNL( (const unsigned char *)entry.name() );
    TV.print(entry.name() );
   
    if ( entry.isDirectory() ) {
      printmsgNoNL( slashmsg );
    }

    if ( entry.isDirectory() ) {
      // directory ending
      for ( int i = strlen( entry.name()) ; i < 16 ; i++ ) {
        printmsgNoNL( spacemsg );
      }
      printmsgNoNL( dirextmsg );
    }
    else {
      // file ending
      for ( int i = strlen( entry.name()) ; i < 17 ; i++ ) {
        printmsgNoNL( spacemsg );
      }
      printUnum( entry.size() );
    }
    line_terminator();
    entry.close();
  }
  dir.close();
}
#endif




void CursorReset(void)
{
  cursorposx = 0;
  cursorposy = 16;
 
}
