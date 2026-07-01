//=====[#include guards - begin]===============================================

#ifndef _GATE_H_
#define _GATE_H_

//=====[Declaration of public defines]=========================================

//=====[Declaration of public data types]======================================
extern int lastGateInterrupt;// 0=none, 1=open button, 2=close button

typedef enum {
    GATE_CLOSED,
    GATE_OPEN,
    GATE_OPENING,
    GATE_CLOSING,
    GATE_STOPPED      
} gateStatus_t;

//=====[Declarations (prototypes) of public functions]=========================

void gateInit();

void gateOpen();
void gateClose();

gateStatus_t gateStatusRead();

//=====[#include guards - end]=================================================

#endif // _GATE_H_
