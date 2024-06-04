//Press 'i' to increase the volume
//Press 'd' to decrease the volume
//Press 'm' to mute/unmute
//Enter a number between 60-240 and then press 't' to set your desired tempo
//Enter a number between -5 and 5 and the press 'k' to set your desired key


#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#define DAC_MAIN (*(unsigned char *) 0x4000741c)

// Structures

typedef struct {
    Object super;
    int index; //index for the buffer[50]
    char buffer[50]; //The buffer that stores each char
    int new_buff; 
} App;

typedef struct{
    Object super;
    unsigned int length; // the period used as baseline for the play_note()
    unsigned int deadline;
    unsigned int key;
    unsigned int tempo;
    unsigned int period; // the periods which is used as sent and then used as baseline for tone_generator()
    unsigned int volume;
    int periods[25];
    int indices[32];
    float beats[32];
    int counter;
} Music_Player;

typedef struct{
    Object super;
    unsigned int period;
    unsigned int deadline;
    unsigned int volume;
    unsigned int mute;
    unsigned int length; //ta bort
    unsigned int internal_mute;
} Tone_Generator;

// Method declarations
void reader(App*, int);
void receiver(App*, int);
void startApp(App *, int);

// Music Player
void play_note(Music_Player *self, int unused);
void tempo(Music_Player *self, int tempo);
void start(Music_Player *self, int);
void key(Music_Player *self, int key);
void vol_inc(Music_Player *self, int volume);
void vol_dec(Music_Player *self, int volume);
void mute(Music_Player *self, int unused);
void correct_index(Music_Player *self, int unused);
void length_calc(Music_Player *self, int tempo);
void wait(Tone_Generator *self, int unused);

// Tone Generator
void generate_tone(Tone_Generator *self, int);
void mute_tone(Tone_Generator *self, int length);
void set_period(Tone_Generator *self, int period);
void get_length(Tone_Generator *self, int length);
void vol_control(Tone_Generator *self, int vol);
void mute_toggler(Tone_Generator *self, int);
void unmute_ext(Tone_Generator *self, int);
void internal_mute_toggle(Tone_Generator *self, int unused);
void set_length(Tone_Generator *self, int length);
void printff(char *, int);

App app = { initObject(), 0, {}, 0 };
Music_Player music_player = {initObject(), MSEC(500), USEC(500), 0, 120, 0, 5,
{2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506},
{0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0},
{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 2.0, 0.5, 0.5, 0.5, 0.5, 1.0, 1.0, 0.5, 0.5, 0.5, 0.5, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 2.0}, 0};
Tone_Generator tone_generator = {initObject(), USEC(1136), USEC(100), 15, 0, 0, 0};
Serial sci0 = initSerial(SCI_PORT0, &app, reader);

// Main function

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}

// Function definitions

void reader(App *self, int c) {

    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
    int value;
    switch (c) {
        case '0' ... '9':
        case '-':
            self->buffer[self->index++]=c;
            break;
            
        case 't':
            self->buffer[self->index]='\0';
            value = atoi(self->buffer);
            printff("value: %d\n", value);
            if(value>= 60 && value <= 240){
                self->new_buff=value;
                char subuf[100];
                snprintf(subuf,100,"Entered tempo: %d\n", value);
                SCI_WRITE(&sci0,subuf);
                ASYNC(&music_player, tempo, value);
            }
			self->index=0;
            break;
            
        case 'k':
            self->buffer[self->index]='\0';
            value = atoi(self->buffer);
            if(value>= -5 && value <= 5){
                self->new_buff=value;
                char subuf[100];
                snprintf(subuf,100,"Entered key: %d\n", value);
                SCI_WRITE(&sci0,subuf);
                ASYNC(&music_player, key, value);
            }
			self->index=0;
            break;

        case 'i':
            // Increase volume
            ASYNC(&music_player, vol_inc, 0);
            break;

        case 'd':
            // Decrease volume
            ASYNC(&music_player, vol_dec, 0);
            break;

        case 'm':
            // Mute/unmute
            ASYNC(&music_player, mute, 0);
            break;

        default:
            return;
    }
}

void startApp(App *self, int arg) {
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");
    
	ASYNC(&music_player, start, 0);
}


void correct_index (Music_Player *self, int unused) {
    // index 0 is actually index -10
    int set_key = self->key + 10;  // Corrected key usage
    int newIndex = self->indices[self->counter] + set_key;
    int period = self->periods[newIndex];
	//printff("newIndex: %d\n", newIndex);
	//printff("Co period: %d\n", period);
    self->period = period;
	self->counter = (self->counter+1) % 32;
	SYNC(&tone_generator, set_period, self->period);  
}


void tempo(Music_Player *self, int tempo) {
    self->tempo = tempo;
}

void length_calc(Music_Player *self, int tempo) {
    float x = (60000000.0 / self->tempo); // Convert seconds to milliseconds
    x = x * self->beats[self->counter];
    int length = (int)x;
    self->length = USEC(length);
	SYNC(&tone_generator, set_length, self->length); 
}


void internal_mute_toggle(Tone_Generator *self, int status){
    self->internal_mute = status;
}


void play_note(Music_Player *self, int length) {
	ASYNC(&music_player, correct_index, 0);
	ASYNC(&music_player, length_calc, 0);
	SEND(self->length - USEC(100000), USEC(50), &tone_generator, wait, 0);
	ASYNC(&tone_generator, internal_mute_toggle, 0);
	ASYNC(&tone_generator, generate_tone, 0);

}

void start(Music_Player *self, int unused) {
	ASYNC(&music_player, play_note, 0);
}

void key(Music_Player *self, int key) {
    self->key = key;
}

void vol_inc(Music_Player *self, int volume) {
    if(self->volume >= 20) return;
    self->volume++;
    vol_control(&tone_generator, 1);
}

void vol_dec(Music_Player *self, int volume) {

    if(self->volume <= 0) return;
    self->volume--;
    vol_control(&tone_generator, 0);
}

void mute(Music_Player *self, int unused) {
    mute_toggler(&tone_generator, 0);
}

void wait(Tone_Generator *self, int unused){
	
    SEND(MSEC(100), USEC(50), &music_player, play_note, self->length);
    ASYNC(&tone_generator, internal_mute_toggle, 1);
}
///////////////////////////////////////////////////////////////////////
// Tone Generator
void set_length(Tone_Generator *self, int length){
	self->length = length;
}
void generate_tone(Tone_Generator *self, int unused) {
    //printff("gen tone period: %d\n",self->period);
    if(self->mute || self->internal_mute) {
        return;
    } else if(DAC_MAIN) {
        DAC_MAIN = 0;
    } else {
        DAC_MAIN = self->volume;
    }
    SEND(self->period, self->deadline, &tone_generator, generate_tone, 0);
}

void set_period(Tone_Generator *self, int period) {
    self->period = USEC(period);
	
}

void vol_control(Tone_Generator *self, int vol) {
    if(vol) {
        self->volume++;
    } else {
        self->volume--;
    }
}

void mute_toggler(Tone_Generator *self, int unused) {
    if(self->mute) {
        self->mute = 0;
        ASYNC(&tone_generator, generate_tone, 0);
    } else{
        self->mute = 1;
    }
}

void printff(char *f, int value) {
    char buffer[128];
    snprintf(buffer, 128, f, value);
    SCI_WRITE(&sci0, buffer);
}