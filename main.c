#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "TM4C129.h"
#include "ES.h"

#define HPin (1<<2)
#define VPin (1<<3)

#define FB_WIDTH  20
#define FB_HEIGHT 30

#define RGB_PINS 0xF0
#define COLOR_WHITE 0xFC
#define COLOR_BLACK 0x00

#define PIXEL_CLOCK_DIV 3
#define H_SYNC_PIXELS    96
#define H_BACK_PORCH     48
#define H_ACTIVE         640
#define H_FRONT_PORCH    16
#define H_TOTAL          (H_SYNC_PIXELS + H_BACK_PORCH + H_ACTIVE + H_FRONT_PORCH)

#define V_SYNC_LINES     2
#define V_BACK_PORCH     33
#define V_ACTIVE         480
#define V_FRONT_PORCH    10
#define V_TOTAL          (V_SYNC_LINES + V_BACK_PORCH + V_ACTIVE + V_FRONT_PORCH)

#define H_TIMER_PERIOD   (H_TOTAL * PIXEL_CLOCK_DIV)
#define H_SYNC_CYCLES    (H_SYNC_PIXELS * PIXEL_CLOCK_DIV)
#define V_TIMER_PERIOD   (H_TIMER_PERIOD * V_TOTAL)
#define V_SYNC_CYCLES    (H_TIMER_PERIOD * V_SYNC_LINES)

#define ACTIVE_TICKS     (H_ACTIVE * PIXEL_CLOCK_DIV)
#define TICKS_PER_PIXEL  (ACTIVE_TICKS / FB_WIDTH)

uint8_t framebuffer[FB_HEIGHT][FB_WIDTH];
volatile int currentLine = 0;
volatile bool vblank_flag = false;

// Game state
int paddle1_y = 12;
int paddle2_y = 12;
int ball_x = 10 * 256;
int ball_y = 15 * 256;
int ball_vx = 85;
int ball_vy = 85;
int frame_counter = 0;

#define PADDLE_HEIGHT 5
#define BALL_SPEED 3

void TIMER5B_Handler();

// ================================ ADC SETUP

void setUp_ADC_GPIO(void) {
    SYSCTL->RCGCGPIO |= (1 << 4);
    while((SYSCTL->PRGPIO & (1 << 4)) == 0) {};
    
    GPIOE_AHB->AMSEL |= (1<<3);
    GPIOE_AHB->DIR &= ~(1<<3);
    GPIOE_AHB->AFSEL |= (1<<3);
    GPIOE_AHB->DEN &= ~(1<<3);
}

void setUp_ADC(void) {
    SYSCTL->RCGCADC |= (1 << 0);
    while((SYSCTL->PRADC & (1 << 0)) == 0) {};
    
    ADC0->PC = 0x1;
    ADC0->ACTSS &= ~(1 << 3);
    
    ADC0->EMUX &= ~0xF000;
    ADC0->SSMUX3 = 0;
    ADC0->SSCTL3 = 0x6;
    
    ADC0->ACTSS |= (1 << 3);
}

void read_paddle1(void) {
    ADC0->PSSI |= (1 << 3);
    while((ADC0->RIS & (1 << 3)) == 0) {};
    
    uint32_t adc_result = ADC0->SSFIFO3;
    ADC0->ISC = (1 << 3);
    
    paddle1_y = (adc_result * (FB_HEIGHT - PADDLE_HEIGHT)) / 4095;
}

// ================================ VGA SETUP

void setUpNVIC(void){
    NVIC->ISER[2] |= (1 << 2);
}

void setUp_RGB_GPIO(void) {
    SYSCTL->RCGCGPIO |= (1 << 9);
    while((SYSCTL->PRGPIO & (1 << 9)) == 0) {};
    
    GPIOK->DIR |= RGB_PINS;
    GPIOK->DEN |= RGB_PINS;
    GPIOK->DATA = 0x00;
}

void setUp_H_GPIO(void){
    SYSCTL->RCGCGPIO |= (1 << 1);
    while((SYSCTL->PRGPIO & (1 << 1)) == 0) {};
    
    GPIOB_AHB->DIR |= HPin;
    GPIOB_AHB->AFSEL |= HPin;
    GPIOB_AHB->DEN |= HPin;
    GPIOB_AHB->PCTL |= ((GPIOB_AHB->PCTL & ~0xF00) | (0x300));
}

void setUp_V_GPIO(void){
    SYSCTL->RCGCGPIO |= (1 << 3);
    while((SYSCTL->PRGPIO & (1 << 3)) == 0) {};
    
    GPIOD_AHB->DIR |= VPin;
    GPIOD_AHB->AFSEL |= VPin;
    GPIOD_AHB->DEN |= VPin;
    GPIOD_AHB->PCTL |= ((GPIOD_AHB->PCTL & ~0xF000) | (0x3000));
}

void setUp_Timer5_Split(void){
    SYSCTL->RCGCTIMER |= (1<<5);
    while((SYSCTL->PRTIMER & (1 << 5)) == 0) {};
    
    TIMER5->CTL = 0x0;
    TIMER5->CFG = 0x4;
    
    TIMER5->TAMR = (0x2 | (1<<3));
    TIMER5->TAILR = H_TIMER_PERIOD - 1;
    TIMER5->TAMATCHR = H_TIMER_PERIOD - H_SYNC_CYCLES;
    TIMER5->TAPR = 0;
    TIMER5->CTL |= (1 << 6);
    
    TIMER5->TBMR = 0x2;
    TIMER5->TBILR = H_TIMER_PERIOD - 1;
    TIMER5->TBPR = 0;
    TIMER5->IMR |= (1 << 8);
    
    TIMER5->ICR = 0xFF;
    TIMER5->CTL |= (1<<0) | (1<<8);
}

void setUp_V_Timer(void){
    SYSCTL->RCGCTIMER |= (1<<1);
    while((SYSCTL->PRTIMER & (1 << 1)) == 0) {};
    
    TIMER1->CTL &= ~(1<<8);
    TIMER1->CFG = 0x4;
    TIMER1->TBMR = (0x2 | (1<<3));
    
    uint32_t period = V_TIMER_PERIOD - 1;
    TIMER1->TBILR = period & 0xFFFF;
    TIMER1->TBPR = (period >> 16) & 0xFF;
    
    uint32_t match = V_TIMER_PERIOD - V_SYNC_CYCLES;
    TIMER1->TBMATCHR = match & 0xFFFF;
    TIMER1->TBPMR = (match >> 16) & 0xFF;
    
    TIMER1->CTL |= (1 << 14);
    TIMER1->ICR = 0xFF;
    TIMER1->CTL |= (1<<8);
}

// ========================== INTERRUPT HANDLER

void TIMER5B_Handler(void) {
    TIMER5->ICR = (1<<8);
    
    currentLine++;
    if (currentLine >= 525) {
        currentLine = 0;
        vblank_flag = true;
    }
    
    if (currentLine >= 35 && currentLine < 515) {
        uint16_t visibleLine = currentLine - 35;
        uint16_t fbRow = visibleLine / 16;
        
        if (fbRow < FB_HEIGHT) {
            while(TIMER5->TAR > 1968);
            
            uint32_t start_time = TIMER5->TAR;
            
            for (int x = 0; x < FB_WIDTH; x++) {
                uint32_t pixel_deadline = x * TICKS_PER_PIXEL;
                
                uint32_t now, elapsed;
                do {
                    now = TIMER5->TAR;
                    elapsed = (start_time - now) & 0xFFFF;
                } while (elapsed < pixel_deadline);
                
                GPIOK->DATA = framebuffer[fbRow][x];
            }
            
            GPIOK->DATA = COLOR_BLACK;
        } else {
            GPIOK->DATA = COLOR_BLACK;
        }
    } else {
        GPIOK->DATA = COLOR_BLACK;
    }
}

// ========================== GAME LOGIC

// Draw a 2x2 ball (makes it more square/ball-shaped at low res)
void draw_ball(int center_x, int center_y) {
    // Draw a 2x2 pixel ball
    for (int dy = 0; dy < 2; dy++) {
        for (int dx = 0; dx < 2; dx++) {
            int x = center_x + dx;
            int y = center_y + dy;
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
                framebuffer[y][x] = COLOR_WHITE;
            }
        }
    }
}

void update_framebuffer(void) {
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            framebuffer[y][x] = COLOR_BLACK;
        }
    }
    
    // Left paddle (player controlled via potentiometer)
    for (int i = 0; i < PADDLE_HEIGHT; i++) {
        int y = paddle1_y + i;
        if (y >= 0 && y < FB_HEIGHT) {
            framebuffer[y][1] = COLOR_WHITE;
        }
    }
    
    // Right paddle (AI controlled)
    for (int i = 0; i < PADDLE_HEIGHT; i++) {
        int y = paddle2_y + i;
        if (y >= 0 && y < FB_HEIGHT) {
            framebuffer[y][18] = COLOR_WHITE;
        }
    }
    
    // Ball (2x2 pixels for better ball shape)
    int pos_x = ball_x / 256;
    int pos_y = ball_y / 256;
    draw_ball(pos_x, pos_y);
}

void update_ball(void) {
    ball_x += ball_vx;
    ball_y += ball_vy;
    
    int pos_x = ball_x / 256;
    int pos_y = ball_y / 256;
    
    // Bounce off top and bottom (account for 2x2 ball size)
    if (pos_y <= 0 || pos_y >= FB_HEIGHT - 2) {
        ball_vy = -ball_vy;
    }
    
    // Left paddle collision (account for 2x2 ball)
    if (pos_x <= 2 && ball_vx < 0) {
        // Check if any part of the 2x2 ball hits the paddle
        if ((pos_y >= paddle1_y && pos_y < paddle1_y + PADDLE_HEIGHT) ||
            (pos_y + 1 >= paddle1_y && pos_y + 1 < paddle1_y + PADDLE_HEIGHT)) {
            ball_vx = -ball_vx;
        }
    }
    
    // Right paddle collision (account for 2x2 ball)
    if (pos_x >= 17 && ball_vx > 0) {
        // Check if any part of the 2x2 ball hits the paddle
        if ((pos_y >= paddle2_y && pos_y < paddle2_y + PADDLE_HEIGHT) ||
            (pos_y + 1 >= paddle2_y && pos_y + 1 < paddle2_y + PADDLE_HEIGHT)) {
            ball_vx = -ball_vx;
        }
    }
    
    // Reset ball if it goes off screen
    if (pos_x < 0 || pos_x >= FB_WIDTH - 1) {
        ball_x = 10 * 256;
        ball_y = 15 * 256;
        ball_vx = (pos_x < 0) ? 85 : -85;
    }
}

void update_ai_paddle(void) {
    int pos_y = ball_y / 256;
    int paddle_center = paddle2_y + PADDLE_HEIGHT / 2;
    
    if (pos_y < paddle_center - 1) {
        paddle2_y--;
    } else if (pos_y > paddle_center + 1) {
        paddle2_y++;
    }
    
    if (paddle2_y < 0) paddle2_y = 0;
    if (paddle2_y > FB_HEIGHT - PADDLE_HEIGHT) {
        paddle2_y = FB_HEIGHT - PADDLE_HEIGHT;
    }
}

int main(void){
    ES_setSystemClk(80);
    
    setUp_ADC_GPIO();
    setUp_ADC();
    setUpNVIC();
    setUp_RGB_GPIO();
    setUp_H_GPIO();
    setUp_V_GPIO();
    setUp_Timer5_Split();
    setUp_V_Timer();
    
    __enable_irq();
    
    while(1){
        if (vblank_flag) {
            vblank_flag = false;
            
            read_paddle1();
            
            frame_counter++;
            if (frame_counter >= BALL_SPEED) {
                frame_counter = 0;
                update_ball();
            }
            
            update_ai_paddle();
            update_framebuffer();
        }
    }
}