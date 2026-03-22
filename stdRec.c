#include<stdio.h>
#include<conio.h>
#include<string.h>
#include<ctype.h>

#define NAME 50
int checkPassword();
int isStrong(char pass[]);
void setPass();

struct Date { int day, month, year; };

struct User {
    int id;
    char name[NAME];
    char email[40];
    char password[20]; // plain-text OK for academic project (but warn in report)
    char role[12]; // "admin", "teacher", "student"
    int active; // 1 = active, 0 = blocked
};

struct Student {
    int rollno;
    char name[NAME];
    int department; 
    int semester;
    char phone[15];
    struct Date dob;
};

struct Teacher {
    int id;
    char name[NAME];
    char subject[NAME];
    char contact[15];
};

struct Grade {
    int rollno;
    int subject_id;
    int semester; 
    float marks;
    char grade[3]; 
};

struct Attendance {
    int rollno;
    struct Date date;
    int status; // 1=present, 0=absent, 2=leave
};


int main(){
    int n;
    printf("Enter your\n ");
    scanf("%d", &n);
    printf("1.Admin\n");
    printf("2.Teacher\n");
    printf("3.Student\n");
    void setPass();
    /*if ((checkPassword())==1){
        printf("Yay!!");
    }*/

    setPass();
    return 0;
}

void setPass(){
    char name[30];
    char pass[20];
    FILE *ptr;
    ptr= fopen("password.txt","w");
    printf("Enter your username: ");
    scanf(ptr,"%s", name);
    repeat:
    printf("Enter password: ");
    scanf(ptr,"%s", pass);
    if (isStrong(pass))
        printf("Strong password!\n");
    else{
        printf("Weak Password! Please reenter\n ");
        goto repeat;
    }

}

//checking the pw for login
int checkPassword(){
    char ch;
    int i,found=0;
    char pass[20];
    char savedPass[20], username[20],fileuser[20],filepass[20];
    FILE *ptr=fopen("password.txt","r");
    if(ptr=NULL){
        printf("File not found\n");
        return 0;
    }
    repeat:
    printf("Enter username");
    scanf("%s", username);
    printf("Enter password: ");
    scanf("%s", pass);
    while(fscanf(ptr,"%s %s",fileuser,filepass)!=EOF){
        if (strcmp(username,fileuser)==0 && strcmp(savedPass,filepass)==0){
            found=1;
            break;
        }
    }

    fclose(ptr);
    if (found)
        printf("LOGIN SUCCESSFUL\n");
    else{
        printf("INVALID USERNAME AND PASSWORD\n");
        goto repeat;
    }

    return 0;
}

int isStrong(char pass[]){
    // to check weather pw is strong or not
    int i=0, hasUpper=0, hasLower=0, hasDigit=0, hasSpecial=0;

    if (strlen(pass)<8)
        return 0;

    for(i=0;pass[i]!='\0';i++){
        if(isupper(pass[i]))
            hasUpper=1;
        else if(islower(pass[i]))
            hasLower=1;
        else if(isdigit(pass[i]))
            hasDigit=1;
        else hasSpecial=1;
    }
    return hasUpper && hasLower && hasDigit && hasSpecial; 
}

