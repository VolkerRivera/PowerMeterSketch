#ifndef __ADEDRIVER_H
#define __ADEDRIVER_H
#include <ADE9153A.h>
#include <ADE9153AAPI.h>
/* Basic initializations */
#define SPI_SPEED 1000000     //SPI Speed          
#define CS_PIN 16 //8-->Arduino Zero. 15-->ESP8266 (en nuestro caso 16)
#define ADE9153A_RESET_PIN 2  //On-board Reset Pin
#define USER_INPUT 0 //On-board User Input Button Pin

void initADE9153A(void);
void readandwrite(void);
void resetADE9153A(void);
void autocalibrateADE9153A(void);
void runLength(long seconds);

#endif /* __ADEDRIVER_H*/