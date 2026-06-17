#ifndef __OLEDFONT_H
#define __OLEDFONT_H

#include <stdint.h>

typedef struct 
{
    unsigned char Index[1]; 
    unsigned char Msk[12];
}typASC_CHAR12; 

typedef struct 
{
    unsigned char Index[1]; 
    unsigned char Msk[16];
}typASC_CHAR16; 

typedef struct 
{
    unsigned char Index[1]; 
    unsigned char Msk[64];
}typASC_CHAR32; 

typedef struct 
{
    unsigned char Index[2]; 
    unsigned char Msk[24];
}typFNT_GB12; 

typedef struct 
{
    unsigned char Index[2]; 
    unsigned char Msk[32];
}typFNT_GB16; 

extern const typASC_CHAR12 tchar12[46];
extern const typASC_CHAR16 tchar16[39];
extern const typASC_CHAR32 tchar32[21];
extern const uint8_t draw_empty_bat[];
extern const uint8_t draw_lack_bat[];
extern const uint8_t arrow[];
extern const uint8_t time_icon[];
extern const uint8_t usb_icon[];
extern const uint8_t three_point[];
extern const uint8_t grade_icon1[];
extern const uint8_t grade_icon2[];
extern const uint8_t return_icon[];
extern const uint8_t radiation_icon[];
extern const uint8_t radiation_icon2[];
extern const uint8_t warning_icon[];
extern const uint8_t warning_icon2[];
extern const uint8_t bar_icon[];
extern const unsigned char quickmark[];
extern const unsigned char left_arrow[];
extern const unsigned char right_arrow[];
extern const uint8_t power_off_icon_2[];
extern const typFNT_GB12 tfont12[94];
extern const typFNT_GB16 tfont16[50];

//extern const uint8_t power_on[];
extern const uint8_t bar_left[];
extern const uint8_t bar_right[];
extern const uint8_t WHITE_LOGO[];
extern const uint8_t LOGO_1[];
extern const uint8_t LOGO_2[];
extern const uint8_t LOGO_3[];
extern const uint8_t LOGO_4[];

#endif


