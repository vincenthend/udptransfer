#ifndef SENDING_WINDOW_H
#define SENDING_WINDOW_H

typedef struct {
  unsigned int sendingWindowSize;
  unsigned int lastAckReceived;
  unsigned int lastFrameSent;
} SendingWindow;

#define NONE 0

#define SWS(S) (S).sendingWindowSize
#define LAR(S) (S).lastAckReceived
#define LFS(S) (S).lastFrameSent

void init(SendingWindow *s, unsigned int windowSize);
void receiveAcks(SendingWindow *s, int ack);
void sendFrames(SendingWindow s, int frameCount);

#endif