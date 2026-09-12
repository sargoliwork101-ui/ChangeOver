#ifndef JITTER_H
#define JITTER_H

#include <stdbool.h>

void Jitter_Init(void);
void Jitter_Run(void);
bool Jitter_ChannelTripped(uint8_t channel /* 1 or 2 */);

#endif /* JITTER_H */
