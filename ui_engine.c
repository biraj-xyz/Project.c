#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "ui_engine.h"

#define CYAN "\033[1;36m"
#define GREEN "\033[1;32m"
#define RED "\033[1;31m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"

void clearScreen() {
    system("cls");
}

void drawHeader() {
    printf(CYAN);
    printf("====================================================\n");
    printf("     SMART ACADEMIC RESULT MANAGEMENT SYSTEM\n");
    printf("             ENTERPRISE EDITION v2.0\n");
    printf("====================================================\n");
    printf(RESET);
}

void drawFooter(char role) {
    printf("\n----------------------------------------------------\n");
    if(role=='A')
        printf("Logged in as: ADMIN | Full Access\n");
    else if(role=='T')
        printf("Logged in as: TEACHER | Limited Access\n");
    printf("System Status: ACTIVE\n");
    printf("----------------------------------------------------\n");
}

void drawBoxMenu(const char *title, const char *options[], int size) {
    printf(YELLOW "\n[%s]\n" RESET, title);
    for(int i=0;i<size;i++) {
        printf("  %d. %s\n", i+1, options[i]);
    }
}

void loadingAnimation() {
    printf("Loading");
    for(int i=0;i<3;i++){
        Sleep(400);
        printf(".");
    }
    printf("\n");
}

void pauseScreen() {
    printf("\nPress Enter to continue...");
    getchar();
    getchar();
}

void statusBar(const char *message) {
    printf(GREEN "\n[STATUS]: %s\n" RESET, message);
}

char confirmAction() {
    char ch;
    printf("Confirm (Y/N): ");
    scanf(" %c",&ch);
    return ch;
}