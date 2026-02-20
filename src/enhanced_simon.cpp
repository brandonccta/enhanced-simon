#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "LCD.h"
#include "irAVR.h"
#include "pitches.h"
#include "serialATmega.h"

// global variables
decode_results irInput;
unsigned char activeLED = 0;
unsigned char inputStart = 0;
unsigned char levelStart = 0;
unsigned char loseGame = 0;
unsigned char updateScore = 0;
unsigned char systemOn = 0;
unsigned char displayMenu = 1;
unsigned char displayPrompt = 0;
unsigned char oneLocked = 0;
unsigned char twoLocked = 1;
unsigned char playAgain = 0;
unsigned char gameStart = 0;
unsigned char playNote = 0;
unsigned char melodyPlayed = 0;
unsigned char levelCleared = 0;
unsigned int levelCount = 1;
unsigned int playerScore = 0;
unsigned int operatingLeds = 0;
unsigned int sequence[20];
unsigned int score[20];
float depreciationFactor = 0.01;
unsigned int introMelody[] = {NOTE_B3, NOTE_C4, NOTE_E4, NOTE_G4, NOTE_A4, NOTE_F4, NOTE_D4, NOTE_E4, NOTE_C4};
unsigned int introDuration[] = {4, 4, 8, 4, 6, 4, 8, 8, 4};
unsigned int loseMelody[] = {NOTE_A2, NOTE_GS2, NOTE_G2, NOTE_FS2, NOTE_G2};
unsigned int loseDuration[] = {4, 4, 6, 6, 4};
unsigned int levelMelody[] = {NOTE_C5, NOTE_C5, NOTE_E5, NOTE_G5};
unsigned int levelDuration[] = {10, 10, 8, 4};

#define NUM_TASKS 7

typedef struct _task{
	signed 	 char state;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
} task;

const unsigned long ledPeriod = 3;
const unsigned long joystickPeriod = 1;
const unsigned long buzzerPeriod = 2;
const unsigned long lcdPeriod = 500;
const unsigned long levelPeriod = 100;
const unsigned long scorePeriod = 250;
const unsigned long remotePeriod = 1;
const unsigned long GCD_PERIOD = 1;

task tasks[NUM_TASKS];

void TimerISR() {
  TimerFlag = 1;
}

// helper functions
void ClearShiftRegister() {
  for (int i = 0; i < 8; i++) { // turn off all LEDs by clearing the shift register
    PORTB = SetBit(PORTB, 3, 0); // send 0 to the data bit (SER)
    PORTB = SetBit(PORTB, 4, 0); // SRCLK low
    PORTB = SetBit(PORTB, 4, 1); // SRCLK high
  }
  // latch to update outputs (turn all LEDs off)
  PORTB = SetBit(PORTB, 5, 1); // RCLK high
  PORTB = SetBit(PORTB, 5, 0); // RCLK low
}

void ShiftRegisterHelper() {
  int numPulses = 0;

  switch (activeLED) { // map activeLED to the corresponding number of pulses required to output relative led
    case 1: numPulses = 1; break;
    case 2: numPulses = 2; break;
    case 3: numPulses = 3; break;
    case 4: numPulses = 4; break;
    case 6: numPulses = 5; break;
    case 7: numPulses = 6; break;
    case 8: numPulses = 7; break;
    case 9: numPulses = 8; break;
  }
  ClearShiftRegister();
  PORTB = SetBit(PORTB, 3, 1); // send 1 to the data bit (SER)

  for (int i = 0; i < numPulses; i++) { // clock the appropriate number of pulses to shift the "1" to the desired position
    PORTB = SetBit(PORTB, 4, 0); // SRCLK low
    PORTB = SetBit(PORTB, 4, 1); // SRCLK high
    PORTB = SetBit(PORTB, 3, 0);
  }
  PORTB = SetBit(PORTB, 4, 0); // reset SRCLK to low

  PORTB = SetBit(PORTB, 5, 1); // latch the data to update the outputs
  PORTB = SetBit(PORTB, 5, 0); // reset RCLK to low
}

void ClearAllLeds() {
  operatingLeds = 0;
  ClearShiftRegister();
  PORTB = SetBit(PORTB, 0, 0);
}

void PowerAllLeds() {
  operatingLeds = 0b11111111;

  for (int i = 0; i < 8; i++) { // turn on all LEDs by shifting 1 to each output
    PORTB = SetBit(PORTB, 3, 1); // send 0 to the data bit (SER)
    PORTB = SetBit(PORTB, 4, 0); // SRCLK low
    PORTB = SetBit(PORTB, 4, 1); // SRCLK high
  }
  // latch to update outputs (turn all LEDs on)
  PORTB = SetBit(PORTB, 5, 1); // RCLK high
  PORTB = SetBit(PORTB, 5, 0); // RCLK low

  PORTB = SetBit(PORTB, 0, 1);
}

int MapToShiftRegisterOutput(int ledNumber) {
  switch (ledNumber) {
    case 1: return 0;
    case 2: return 1;
    case 3: return 2;
    case 4: return 3;
    case 6: return 4;
    case 7: return 5;
    case 8: return 6;
    case 9: return 7;
  }
}

void LedPattern(int patternSequence) {
  int shiftRegisterOutput;

  if (patternSequence == 5) { // update the active leds bitmask for the current pattern sequence
    PORTB = SetBit(PORTB, 0, 1);
  }
  else { // add the current LED to the active bitmask
    shiftRegisterOutput = MapToShiftRegisterOutput(patternSequence);
    if (shiftRegisterOutput != -1) {operatingLeds |= (1 << shiftRegisterOutput);}
  }
  // send the accumulated state to the shift register
  for (int i = 7; i >= 0; i--) { // iterate over all 8 outputs (Q7 to Q0)
    if (operatingLeds & (1 << i)) {PORTB = SetBit(PORTB, 3, 1);} // SER = 1 for active LEDs
    else {PORTB = SetBit(PORTB, 3, 0);} // SER = 0 for inactive LEDs
    // clock the data into the shift register
    PORTB = SetBit(PORTB, 4, 0); // SRCLK low
    PORTB = SetBit(PORTB, 4, 1); // SRCLK high
  }
  // latch the data to the outputs
  PORTB = SetBit(PORTB, 5, 1);
  PORTB = SetBit(PORTB, 5, 0);
}

void DisplayScore() {
  char printScore[5];

  sprintf(printScore, "%04d", playerScore);
  
  lcd_goto_xy(1, 0);
  if (loseGame) {lcd_write_str("Final Score:"); lcd_write_str(printScore);}
  else {lcd_write_str("Score:"); lcd_write_str(printScore);}
}

void DisplayLevel() {
  char printLevel [8];
  
  sprintf(printLevel, "%d", levelCount);

  lcd_goto_xy(0, 0);
  lcd_write_str("Level ");
  lcd_write_str(printLevel);
}

void ResetSystem() {
  activeLED = 0;
  inputStart = 0;
  levelStart = 0;
  loseGame = 0;
  updateScore = 0;
  systemOn = 0;
  displayMenu = 1;
  displayPrompt = 0;
  oneLocked = 0;
  twoLocked = 1;
  playAgain = 0;
  gameStart = 0;
  levelCount = 1;
  playerScore = 0;
  playNote = 0;
  melodyPlayed = 0;
  levelCleared = 0;
  lcd_clear();
  ClearAllLeds();
}

void ResetGame() {
  activeLED = 0;
  loseGame = 0;
  updateScore = 0;
  levelCount = 1;
  playerScore = 0;
  melodyPlayed = 0;
  levelCleared = 0;
}

void InitializeGame() {
  for (unsigned int i = 0; i < 20; i++) { // initialize led sequence
    sequence[i] = (rand() % 8) + 1; // range from 1-8
  }

  for (unsigned int i = 0; i < 20; i++) { // initialize level score values
    if (i == 0) {score[i] = 50;}
    else {score[i] = score[i - 1] + 50;}
  }
}

void PlayNote(int top, int note) {
  top = (16000000/(note * 8)) - 1;
  ICR1 = top;
  OCR1A = top / 2;
}

void PlayNoteForActiveLED(int top) {
  switch (activeLED) {
    case 1: PlayNote(top, NOTE_C4); break;
    case 2: PlayNote(top, NOTE_D4); break;
    case 3: PlayNote(top, NOTE_E4); break;
    case 4: PlayNote(top, NOTE_F4); break;
    case 6: PlayNote(top, NOTE_G4); break;
    case 7: PlayNote(top, NOTE_A4); break;
    case 8: PlayNote(top, NOTE_B4); break;
    case 9: PlayNote(top, NOTE_C5); break;
  }
}

// tasks
enum Joystick_States {joystick_start, read};
enum Remote_States {remote_start, input};
enum Lcd_States {lcd_start, welcome, menu, game, lose, prompt};
enum Level_States {level_start, dark, delay, off, on};
enum Led_States {led_start, pattern_on, pattern_off, blink, blink_on, blink_off, wait, center, verify, flash, limbo};
enum Buzzer_States {buzzer_start, intro, intro_delay, idle, buzz, beep, cleared, cleared_delay, womp, womp_delay};
enum Score_States {score_start, stop, calculate, update};

int TickFct_JoystickInput(int state) {
  unsigned int VRx;
  unsigned int VRy;
  
  switch (state) {
    case joystick_start:
      if (systemOn) {state = read;}
      else {state = joystick_start;}
      break;
    case read:
      if (!systemOn) {state = joystick_start; break;}
      
      if (inputStart) {
        VRx = ADC_read(1);
        VRy = ADC_read(2);
        // map VRx and VRy to grid positions
        if ((VRx > 500 && VRx < 600) && (VRy > 500 && VRy < 600)) {activeLED = 5;} // center
        else if ((VRx < 40) && (VRy > 900)) {activeLED = 1;} // top-left
        else if ((VRx > 500 && VRx < 600) && (VRy > 900)) {activeLED = 2;} // top-center
        else if ((VRx > 900) && (VRy > 900)) {activeLED = 3;} // top-right
        else if ((VRx < 40) && (VRy > 500 && VRy < 600)) {activeLED = 4;} // middle-left
        else if ((VRx > 900) && (VRy > 500 && VRy < 600)) {activeLED = 6;} // middle-right
        else if ((VRx < 40) && (VRy < 40)) {activeLED = 7;} // bottom-left
        else if ((VRx > 500 && VRx < 600) && (VRy < 40)) {activeLED = 8;} // bottom-center
        else if ((VRx > 900) && (VRy < 40)) {activeLED = 9;} // bottom-right
      }
      state = read;
      break;
    default:
      state = joystick_start;
      break;
  }
  return state;
}

int TickFct_RemoteInput(int state) {
  switch (state) {
    case remote_start:
      state = input;
      break;
    case input:
      if (IRdecode(&irInput)) {
        if (irInput.value == 0xFFA25D) { // if power button is pressed
          if (systemOn) {systemOn = !systemOn; ResetSystem();} // if system is already on
          else {systemOn = !systemOn;}
        }
        else if (systemOn && (!oneLocked) && (irInput.value == 0xFF30CF)) { // if button 1 is pressed
          if (displayPrompt) {ResetGame(); InitializeGame(); playAgain = 1; oneLocked = 1;} // if lcd is in prompt state
          else {displayMenu = 0;} // if lcd is in menu state
        }
        else if (systemOn && (!twoLocked) && (irInput.value == 0xFF18E7)) {ResetGame(); InitializeGame(); displayMenu = 1; twoLocked = 1;} // if button 2 is pressed
        IRresume();
      }
      state = input;
      break;
    default:
      state = remote_start;
      break;
  }
  return state;
}

int TickFct_LcdDisplay(int state) {
  static unsigned int lcdCounter = 0;
  static unsigned int delayCounter = 0;

  switch (state) {
    case lcd_start:
      if (systemOn) {state = welcome;}
      else {lcd_clear();state = lcd_start;}
      break;
    case welcome:
      if (!systemOn) {state = lcd_start; break;}

      if (melodyPlayed) {lcd_clear(); melodyPlayed = 0; state = menu;}
      else {lcd_goto_xy(0, 3); lcd_write_str("Welcome to"); lcd_goto_xy(1, 5); lcd_write_str("Simon!"); state = welcome;}
      break;
    case menu:
      if (!systemOn) {state = lcd_start; break;}

      if (!displayMenu) {
        if (delayCounter > 2) {lcd_clear(); delayCounter = 0; oneLocked = 1; levelStart = 1; gameStart = 1; state = game;}
        else {delayCounter++;}
      }
      else {lcd_goto_xy(0, 0); lcd_write_str("Main Menu"); lcd_goto_xy(1, 0); lcd_write_str("1. Play"); state = menu;}
      break;
    case game:
      if (!systemOn) {state = lcd_start; break;}

      if (loseGame) {lcd_clear(); state = lose; break;}
      else if (levelStart) {DisplayLevel(); DisplayScore();}
      state = game;
      break;
    case lose:
      if (!systemOn) {state = lcd_start; break;}

      if (lcdCounter > 3) {lcd_clear(); lcdCounter = 0; oneLocked = 0; twoLocked = 0; displayPrompt = 1; state = prompt;}
      else {lcd_goto_xy(0, 0); lcd_write_str("You Lose"); DisplayScore(); lcdCounter++;}
      break;
    case prompt:
      if (!systemOn) {state = lcd_start; break;}

      if (playAgain) {lcd_clear(); playAgain = 0; displayPrompt = 0; levelStart = 1; state = game;}
      else if (displayMenu) {lcd_clear(); displayPrompt = 0; state = menu;}
      else {lcd_goto_xy(0, 3); lcd_write_str("Try Again?"); lcd_goto_xy(1, 2); lcd_write_str("1.Yes   2.No");}
      break;
    default:
      state = lcd_start;
      break;
  }
  return state;
}

int TickFct_LevelSequences(int state) {
  static unsigned int delayCounter = 0;
  static unsigned int flashCounter = 0;
  static unsigned int sequenceIndex = 0;

  switch (state) {
    case level_start:
      if (systemOn) {state = dark;}
      else {sequenceIndex = 0; state = level_start;}
      break;
    case dark:
      if (!systemOn) {state = level_start; break;}

      if (levelStart) {state = delay;}
      else {state = dark;}
      break;
    case delay:
      if (!systemOn) {state = level_start; break;}

      if (delayCounter >= 8) {delayCounter = 0; state = off;}
      else {delayCounter++;}

      if (delayCounter >= 2) {ClearShiftRegister();}
      break;
    case off:
      if (!systemOn) {state = level_start; break;}

      if (sequenceIndex >= levelCount) {inputStart = 1; levelStart = 0; sequenceIndex = 0; state = dark;}
      else {
        if (flashCounter >= 2) {activeLED = sequence[sequenceIndex]; ShiftRegisterHelper(); playNote = 1; flashCounter = 0; state = on;}
        else {flashCounter++; state = off;}
      }
      break;
    case on:
      if (!systemOn) {state = level_start; break;}

      if (flashCounter >= 2) {ClearShiftRegister(); playNote = 0; sequenceIndex++; flashCounter = 0; state = off;}
      else {flashCounter++; state = on;}
      break;
    default:
      state = level_start;
      break;
  }
  return state;
}

int TickFct_LedMatrix(int state) {
  static unsigned int userIndex = 0; // user's position in the sequence
  static unsigned char prevLED = 0;
  unsigned int arrayPattern[9] = {1, 2, 3, 6, 5, 4, 7, 8, 9};
  static unsigned int patternIndex = 0;
  static unsigned int delayCounter = 0;

  switch (state) {
    case led_start:
      if (systemOn) {userIndex = 0; patternIndex = 0; state = pattern_on;}
      else {state = led_start;} 
      break;
    case pattern_on:
      if (!systemOn) {state = led_start; break;}

      if (patternIndex < 9) {LedPattern(arrayPattern[patternIndex]); patternIndex++; state = pattern_off;}
      else {ClearAllLeds(); patternIndex = 0; state = blink;}
      break;
    case pattern_off:
      if (!systemOn) {state = led_start; break;}

      if (delayCounter > 100) {delayCounter = 0; state = pattern_on;}
      else {delayCounter++; state = pattern_off;}
      break;
    case blink:
      if (!systemOn) {state = led_start; break;}

      if (delayCounter < 50) {ClearAllLeds();} 
      else if (delayCounter < 250) {PowerAllLeds();} 
      else {ClearAllLeds(); delayCounter = 0; state = wait; break;}
      delayCounter++;
      state = blink;
      break;
    case wait:
      if (!systemOn) {state = led_start; break;}

      if (inputStart) {userIndex = 0; prevLED = 0; ClearShiftRegister(); state = center;} // reset user progress for the new sequence and prepare for user traversal
      else {PORTB = SetBit(PORTB, 0, 0); state = wait;}
      break;
    case center:
      if (!systemOn) {state = led_start; break;}

      if (activeLED != 5) {PORTB = SetBit(PORTB, 0, 0); ShiftRegisterHelper(); state = verify;} // if moved from the center, turn off the center led and traverse to activeLED
      else {PORTB = SetBit(PORTB, 0, 1); prevLED = activeLED; state = center;}
      break;
    case verify:
      if (!systemOn) {state = led_start; break;}

      if (activeLED == 5) {ClearShiftRegister(); PORTB = SetBit(PORTB, 0, 1); state = center;} // if at the center, turn off outer leds and turn on center led
      else {
        if (activeLED != prevLED) { // prevents double inputs in the cause the joystick holds the activeLED
          prevLED = activeLED;

          if (sequence[userIndex] == activeLED) { // checks if activeLED matches the sequence
            userIndex++;
            if (userIndex >= levelCount) { // if sequence of the level is completed
              levelCleared = 1;
              updateScore = 1;
              inputStart = 0; // turn off joystick input
              levelCount++;
              state = wait; // standby until sequence of the next level finishes displaying
              break;
            }
          }
          else { // case if user gets the sequence wrong
            loseGame = 1;
            inputStart = 0;
            levelStart = 0;
            PORTB = SetBit(PORTB, 0, 0);
            ClearShiftRegister();
            state = flash;
            break;
          }
        }
        state = verify;
      }
      break;
    case flash:
      if (!systemOn) {state = led_start; break;}

      if (delayCounter < 50 || (delayCounter >= 100 && delayCounter < 150) || (delayCounter >= 200 && delayCounter < 250)) {ClearAllLeds();} 
      else if ((delayCounter >= 50 && delayCounter < 100) || (delayCounter >= 150 && delayCounter < 200) || (delayCounter >= 250 && delayCounter < 300)) {PowerAllLeds();} 
      else if (delayCounter >= 300) {ClearAllLeds(); delayCounter = 0; state = limbo; break;}
      delayCounter++;
      state = flash;
      break;
    case limbo:
      if (!systemOn) {state = led_start; break;}

      if (playAgain) {state = wait;}
      else if (gameStart) {gameStart = 0; state = wait;}
      else {state = limbo;}
      break;
    default:
      state = led_start;
      break;
  }
  return state;
}

int TickFct_GameSound(int state) {
  static unsigned char prevLED = 5; // keep track of the previously active led (initialized to center)
  static unsigned char buzzerPlayed = 0;
  static unsigned int durationCount = 0;
  static unsigned int noteIndex = 0;
  static unsigned int delayCounter = 0;
  unsigned int buzzerTop;
  unsigned int noteDuration;

  switch (state) {
    case buzzer_start:
      if (systemOn) {durationCount = 0; noteIndex = 0; buzzerPlayed = 0; state = intro;}
      else {ICR1 = buzzerTop; OCR1A = buzzerTop; state = buzzer_start;}
      break;
    case intro:
      if (!systemOn) {state = buzzer_start; break;}

      if (noteIndex < 9) { // number of notes in melody
        noteDuration = 1000 / introDuration[noteIndex];
        PlayNote(buzzerTop, introMelody[noteIndex]);

        if (durationCount < noteDuration) {durationCount++;}
        else {durationCount = 0; noteIndex++; ICR1 = buzzerTop; OCR1A = buzzerTop; state = intro_delay; break;}
        state = intro;
      }
      else {melodyPlayed = 1; noteIndex = 0; durationCount = 0; ICR1 = buzzerTop; OCR1A = buzzerTop; state = idle;}
      break;
    case intro_delay:
      if (!systemOn) {state = buzzer_start; break;}

      if (durationCount < (noteDuration * 1.3)) {durationCount++; state = intro_delay;} // pause time between each note
      else {durationCount = 0; state = intro;}
      break;
    case idle:
      if (!systemOn) {state = buzzer_start; break;}

      if (levelCleared) {
        if (delayCounter < 100) {delayCounter++; state = idle;}
        else {delayCounter = 0; levelCleared = 0; state = cleared;}
        
        if (delayCounter > 90) {ClearShiftRegister();}
      }
      else if (levelStart) {state = buzz;}
      else if (inputStart) {
        if (activeLED == 5) {buzzerPlayed = 0;} // reset for each step in the sequence
        // play sound if led changes or if it hasn't been played
        if ((activeLED != prevLED || !buzzerPlayed) && activeLED != 5) {buzzerPlayed = 1; state = beep;} // mark buzzer as played for this led
      }
      else if (loseGame && !melodyPlayed) {state = womp;}
      else {state = idle;}
      break;
    case buzz:
      if (!systemOn) {state = buzzer_start; break;}

      if (playNote) {PlayNoteForActiveLED(buzzerTop);}
      else {ICR1 = buzzerTop; OCR1A = buzzerTop;}
      state = idle;
      break;
    case beep:
      if (!systemOn) {state = buzzer_start; break;}

      if (durationCount < 80) {PlayNoteForActiveLED(buzzerTop); durationCount++;}
      else {durationCount = 0; prevLED = activeLED; ICR1 = buzzerTop; OCR1A = buzzerTop; state = idle;} // update current led
      break;
    case cleared:
      if (!systemOn) {state = buzzer_start; break;}
      
      if (noteIndex < 5) {
        noteDuration = 1000 / levelDuration[noteIndex];
        PlayNote(buzzerTop, levelMelody[noteIndex]);

        if (durationCount < noteDuration) {durationCount++;}
        else {durationCount = 0; noteIndex++; ICR1 = buzzerTop; OCR1A = buzzerTop; state = cleared_delay; break;}
        state = cleared;
      }
      else {levelStart = 1; noteIndex = 0; durationCount = 0; ICR1 = buzzerTop; OCR1A = buzzerTop; state = idle;}
      break;
    case cleared_delay:
      if (!systemOn) {state = buzzer_start; break;}

      if (durationCount < (noteDuration * 1.3)) {durationCount++; state = cleared_delay;}
      else {durationCount = 0; state = cleared;}
      break;
    case womp:
      if (!systemOn) {state = buzzer_start; break;}

      if (noteIndex < 5) {
        noteDuration = 1000 / loseDuration[noteIndex];
        PlayNote(buzzerTop, loseMelody[noteIndex]);

        if (durationCount < noteDuration) {durationCount++;}
        else {durationCount = 0; noteIndex++; ICR1 = buzzerTop; OCR1A = buzzerTop; state = womp_delay; break;}
        state = womp;
      }
      else {melodyPlayed = 1; noteIndex = 0; durationCount = 0; ICR1 = buzzerTop; OCR1A = buzzerTop; state = idle;}
      break;
    case womp_delay:
      if (!systemOn) {state = buzzer_start; break;}

      if (durationCount < (noteDuration * 1.3)) {durationCount++; state = womp_delay;}
      else {durationCount = 0; state = womp;}
      break;
    default:
      state = buzzer_start;
      break;
  }
  return state;
}

int TickFct_ScoreHandler(int state) {
  static unsigned int earnedScore;
  static unsigned int timeCounter = 0;
  static unsigned int scoreIndex = 0;
  static float minimumThreshold;
  static float adjustedScore;

  switch (state) {
    case score_start:
      if (systemOn) {state = stop;}
      else {scoreIndex = 0; state = score_start;}
      break;
    case stop:
      if (!systemOn) {state = score_start; break;}
      // initialize values before comparison
      if (inputStart) {earnedScore = score[scoreIndex]; adjustedScore = score[scoreIndex]; minimumThreshold = score[scoreIndex] * 0.2; state = calculate;} 
      else {state = stop;}
      break;
    case calculate:
      if (!systemOn) {state = score_start; break;}

      if (loseGame) {earnedScore = 0; scoreIndex = 0; timeCounter = 0; state = stop; break;}

      if (updateScore) {timeCounter = 0; updateScore = 0; state = update;}
      else {
        if (timeCounter > (levelCount * 2)) { // player has 0.5 second per sequence length to obtain full points upon completion
          adjustedScore = adjustedScore * (1.0 - depreciationFactor * timeCounter); // score depreciates by 1% every 0.5 seconds
          
          if (adjustedScore < minimumThreshold) {adjustedScore = minimumThreshold;} // ensures minimum 20% of base score for completing level

          earnedScore = (unsigned int)adjustedScore; // cast to unsigned int
        }
        else {timeCounter++;}
        state = calculate;
      }
      break;
    case update:
      if (!systemOn) {state = score_start; break;}

      playerScore += earnedScore;
      earnedScore = 0;
      scoreIndex++;
      state = stop;
      break;
    default:
      state = score_start;
      break;
  }
  return state;
}

int main() {
  DDRB = 0b11111111;
  PORTB = 0b00000000;

  DDRD = 0b11111111;
  PORTD = 0b00000000;

  DDRC = 0b00000000;
  PORTC = 0b11111111;

  ADC_init();
  lcd_init();
  IRinit(&DDRC, &PINC, 3);
  InitializeGame();
  srand(time(0));
  serial_init(9600);
  lcd_clear();

  OCR1A = 256; // sets duty cycle to 50% since TOP is always 256
  TCCR1A |= (1 << WGM11) | (1 << COM1A1); // COM1A1 sets it to channel A
  TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11); // CS11 sets the prescaler to be 8

  unsigned int i = 0;

  tasks[i].state = joystick_start;
  tasks[i].period = joystickPeriod;
  tasks[i].elapsedTime = joystickPeriod;
  tasks[i].TickFct = &TickFct_JoystickInput;
  i++;

  tasks[i].state = remote_start;
  tasks[i].period = remotePeriod;
  tasks[i].elapsedTime = remotePeriod;
  tasks[i].TickFct = &TickFct_RemoteInput;
  i++;

  tasks[i].state = lcd_start;
  tasks[i].period = lcdPeriod;
  tasks[i].elapsedTime = lcdPeriod;
  tasks[i].TickFct = &TickFct_LcdDisplay;
  i++;

  tasks[i].state = level_start;
  tasks[i].period = levelPeriod;
  tasks[i].elapsedTime = levelPeriod;
  tasks[i].TickFct = &TickFct_LevelSequences;
  i++;

  tasks[i].state = led_start;
  tasks[i].period = ledPeriod;
  tasks[i].elapsedTime = ledPeriod;
  tasks[i].TickFct = &TickFct_LedMatrix;
  i++;

  tasks[i].state = buzzer_start;
  tasks[i].period = buzzerPeriod;
  tasks[i].elapsedTime = buzzerPeriod;
  tasks[i].TickFct = &TickFct_GameSound;
  i++;

  tasks[i].state = score_start;
  tasks[i].period = scorePeriod;
  tasks[i].elapsedTime = scorePeriod;
  tasks[i].TickFct = &TickFct_ScoreHandler;

  TimerSet(GCD_PERIOD);
  TimerOn();

  while (1) {
    for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {
      if ( tasks[i].elapsedTime == tasks[i].period ) {
        tasks[i].state = tasks[i].TickFct(tasks[i].state);
        tasks[i].elapsedTime = 0;
      }
		  tasks[i].elapsedTime += GCD_PERIOD;
	  }
    while (!TimerFlag) {}
    TimerFlag = 0;
  }

  return 0;
}