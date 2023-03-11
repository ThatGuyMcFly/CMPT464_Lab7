/**
 * Martin Knoetze
 * SN: 3086754
 * CMPT464 Lab6
 * Due: March 8th, 2023
 * 
 * app.cc
*/

#include "sysio.h"
#include "ser.h"
#include "serf.h"
#include "tcv.h"

#include "phys_cc1350.h"
#include "plug_null.h"

#define	MAX_PACKET_LENGTH 250
#define PAYLOAD_LENGTH 27

byte nodeId; // 11
byte sequence;

int sfd = -1;

typedef struct {
    byte senderId;
    byte receiverId;
    byte sequenceNumber;
    byte payload[PAYLOAD_LENGTH];
} message;

fsm receiver {
    address packet;
    message * receivedMessage;

    state Receiving:
        packet = tcv_rnp(Receiving, sfd);
    
    state Get_Message:
        receivedMessage = (message *)(packet + 1);

        if(receivedMessage->receiverId == nodeId) {
            proceed From_Direct;
        } else if (receivedMessage->receiverId == '0' || receivedMessage->receiverId == 0) {
            proceed From_Broadcast;
        }

        proceed Receiving;
    
    state From_Direct:
        ser_outf(Get_Message, "Message ");
        proceed Show_Message;
    
    state From_Broadcast:
        ser_outf(From_Broadcast, "Broadcast ");

    state Show_Message:
        ser_outf(Show_Message, "from node %d (Seq %d): %s\n\r", receivedMessage->senderId, receivedMessage->sequenceNumber, receivedMessage->payload);

        tcv_endp(packet);
        proceed Receiving;

}

fsm transmitter (message * messagePtr) {
    state Transmit_Message:

        address spkt;

        spkt = tcv_wnp (Transmit_Message, sfd, sizeof(message) + 4);
        spkt [0] = 0;
        byte * p = (byte*)(spkt + 1);
        *p = messagePtr->senderId; p++;
        *p = messagePtr->receiverId; p++;
        *p = messagePtr->sequenceNumber; p++;

        strcpy(p, messagePtr->payload);

        tcv_endp (spkt);
        
        sequence++;

    state Confirm_Transmission:
        ser_outf(Transmit_Message, "Message Sent\n\r");
        finish;
}

Boolean isValidNodeId(byte node) {
    if (node < 1 || node > 25) {
        return NO;
    }

    return YES;
}

fsm root {

    byte receiverId;

    message * messagePtr;

    char * menuText = "(C)hange node ID\n\r"
"(D)irect transmission\n\r"
"(B)roadcast transmission\n\r"
"Selection: ";

    state Initialize:
        nodeId = 1;
        sequence = 0;

        messagePtr = (message *) umalloc(sizeof(message));

        phys_cc1350(0, MAX_PACKET_LENGTH);

        tcv_plug(0, &plug_null);
        sfd = tcv_open(WNONE, 0, 0);
		tcv_control(sfd, PHYSOPT_ON, NULL);

		if (sfd < 0) {
			diag("Cannot open tcv interface");
			halt();
		}

        runfsm receiver;

    state Menu_Start:
        receiverId = 0;
        ser_outf(Menu_Start, "P2P Chat (Node #%d)\n\r", nodeId);
    
    state Menu_Choices:
        ser_outf(Menu_Choices, menuText);

    state Choice:
        char choice;

        ser_inf(Choice, "%c", &choice);
    
        switch (choice)
        {
            case 'C':
            case 'c':
                proceed Change_ID;
                break;

            case 'D':
            case 'd':
                proceed Direct_Transmission;
                break;

            case 'B':
            case 'b':
                proceed Broadcast_Transmission;
                break;

            default:
                proceed Menu_Start;
                break;
        }
    
    state Change_ID:
        ser_outf(Change_ID, "New node ID (1-25):");
    
    state Get_New_ID:
        ser_inf(Get_New_ID, "%d", &nodeId);

        if (!isValidNodeId(nodeId)) {
            proceed Change_ID;
        }

        proceed Menu_Start;
    
    state Direct_Transmission:
        ser_outf(Direct_Transmission, "Receiver node ID (1-25):");
    
    state Get_Receiver_Node:
        ser_inf(Get_Receiver_Node, "%d", &receiverId);

        if(!isValidNodeId(receiverId)) {
            proceed Direct_Transmission;
        }

    state Broadcast_Transmission:
        ser_outf(Broadcast_Transmission, "Message: ");
    
    state Get_Message:
        //byte mes[PAYLOAD_LENGTH];

        ser_in(Get_Message, messagePtr->payload, PAYLOAD_LENGTH);

        if(strlen(messagePtr->payload) > PAYLOAD_LENGTH) {
            messagePtr->payload[PAYLOAD_LENGTH - 1] = '/0';
        }

    state Transmit:
        messagePtr->senderId = nodeId;
        messagePtr->receiverId = receiverId;
        messagePtr->sequenceNumber = sequence;

        call transmitter(messagePtr, Menu_Start);
}