#ifndef PEYOTE_H
#define PEYOTE_H

// Peyote cacti in the desert (see peyote.c).
int IsPeyoteSeed(int seed);
int PeyoteAt(int tx, int ty);
int PeyoteBroken(int seed);
void HitPeyote(int tx, int ty);
int TryGatherPeyote(void);
void EatPeyote(void);
void DrawPeyoteMelt(HDC hdc);

#endif
