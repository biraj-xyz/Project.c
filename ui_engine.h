#ifndef UI_ENGINE_H
#define UI_ENGINE_H

void clearScreen();
void drawHeader();
void drawFooter(char role);
void drawBoxMenu(const char *title, const char *options[], int size);
void loadingAnimation();
void pauseScreen();
void statusBar(const char *message);
char confirmAction();

#endif