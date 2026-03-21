#include <stdio.h>
#include <string.h>
#include <conio.h>
#include "auth.h"

void initializeUsers() {
    FILE *fp = fopen("users.dat","r");
    if(!fp){
        fp = fopen("users.dat","w");
        fprintf(fp,"admin admin123 A\n");
        fprintf(fp,"teacher teacher123 T\n");
    }
    fclose(fp);
}

void getPassword(char *pass){
    char ch;
    int i=0;
    while((ch=getch())!=13){
        if(ch==8 && i>0){
            printf("\b \b");
            i--;
        }else{
            pass[i++]=ch;
            printf("*");
        }
    }
    pass[i]='\0';
}

char login(){
    char user[50],pass[50],fuser[50],fpass[50],role;

    FILE *fp=fopen("users.dat","r");

    printf("\nUsername: ");
    scanf("%s",user);
    printf("Password: ");
    getPassword(pass);

    while(fscanf(fp,"%s %s %c",fuser,fpass,&role)!=EOF){
        if(strcmp(user,fuser)==0 && strcmp(pass,fpass)==0){
            fclose(fp);
            return role;
        }
    }
    fclose(fp);
    return 'X';
}