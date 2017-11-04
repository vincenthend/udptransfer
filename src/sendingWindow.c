#include "sendingWindow.h"

void init(SendingWindow *s, unsigned int windowSize) {
    s->sendingWindowSize = windowSize;
    s->lastAckReceived = NONE;
    s->lastFrameSent = NONE;
}

void receiveAcks(SendingWindow *s, int ack) {
    LAR(*s) = (LAR(*s) < ack) ? ack : LAR(*s);
}

void sendFrames(SendingWindow s, int frameCount) {
    // frameCount < SWS(s) validation
    frameCount = (frameCount < SWS(s)) ? frameCount : SWS(s);

    // LFS - LAR <= SWS validation
    if (LFS(s) + frameCount > SWS(s) + LAR(s)) {
        // Too many frames sent -> reduce count
        frameCount = SWS(s) + LAR(s) - LFS(s);
    }

    LFS(s) += frameCount;
}