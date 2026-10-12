#ifndef MOUSE_H
#define MOUSE_H

// Mouse play (Diablo style) and the 1-0 item belt shown at the bottom.

void MouseEvent(UINT msg, WPARAM wp, LPARAM lp);
void UpdateMouseInput(float *dx, float *dy);
int MouseBusy(void);
void DrawBelt(HDC hdc);

#endif
