#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define MAX_STUDENTS 100
#define MAX_NAME 100
#define MAX_ROLL 20

typedef struct Dob{
     int d;
     int m;
     int y;
 }dob;

typedef struct
{
    struct Dob dob;
    int ph;
    char g;
    char add[40];

}Sd;

typedef struct {
    int year;
    char roll[MAX_ROLL];
    char name[MAX_NAME];
    int theory;
    int practical;
    int viva;
    float attendance;
    int total;
    char grade;
    char result[10];
} Student;

typedef struct {
    char username[30];
    char password[30];
    char role[20];
}User;

typedef struct {
    char name[50];
    char subject[50];
    char phone[20];
    char gender[10];
} Teacher;

Student students[MAX_STUDENTS];
User users[10];
Sd sd[MAX_STUDENTS];
int student_count = 0;
int user_count = 0;
int logged_in = 0;
char current_user[30];  
char current_role[20];  

const char *STUDENT_FILE = "students.csv";
const char *USER_FILE = "users.csv";
const char *TEACHER_FILE= "teacher.csv";

void save_students();
void load_students();
void save_users();
void load_users();
void add_student();
void view_all();
void search_student();
void update_student();
void delete_student();
void show_statistics();
void sort_by_marks();
void show_attendance();
void update_attendance();
void login();
void student_menu();  
void clear_screen();
void pause_screen();
void print_header(char *title);
char calculate_grade(int marks);
void calculate_total(Student *s);
void assign_roll_numbers(int year);
void save_studentdetails();  
void load_studentdetails();   
void edit_student();
void main_menu_admin();
void main_menu_teacher();

void clear_screen() {
    system("cls");
}

void pause_screen() {
    printf("\nPress Enter to continue...");
    getchar();
}

void print_animated(char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        printf("%c", text[i]);
        Sleep(40);
    }
    printf("\n");
}

void animate_line(int length) {
    for (int i = 0; i < length; i++) {
        printf("=");
        Sleep(15);
    }
    printf("\n");
}

void print_header_w_animation(char *title) {
    printf("\n");
    animate_line(40);
    int spaces = (40 - strlen(title)) / 2;
    for (int i = 0; i < spaces; i++) {
        printf(" ");
    }
    print_animated(title);
    animate_line(40);
    printf("\n");
}

void print_header(char *title) {
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n\n");
}

void calculate_total(Student *s) {
    s->total = s->theory + s->practical + s->viva;
    s->grade = calculate_grade(s->total);

    if (s->total >= 40 && s->attendance >= 75) {
        strcpy(s->result, "PASS");
    } else if (s->attendance < 75) {
        strcpy(s->result, "LOW ATTEND");
    } else {
        strcpy(s->result, "FAIL");
    }
}

char calculate_grade(int marks) {
    if (marks >= 90) return 'A';
    else if (marks >= 80) return 'B';
    else if (marks >= 70) return 'C';
    else if (marks >= 60) return 'D';
    else if (marks >= 50) return 'E';
    else return 'F';
}

void load_students() {
    FILE *fp = fopen(STUDENT_FILE, "r");
    if (fp == NULL) {
        student_count = 0;
        return;
    }

    char line[500];
    fgets(line, sizeof(line), fp);
    student_count = 0;
    
    while (fgets(line, sizeof(line), fp) && student_count < MAX_STUDENTS) {
        Student s;
        char result[20];
        int d, m, y;
        char add[40];
        char g;
        int ph;
        
        sscanf(line, "%[^,],%[^,],%d,%d,%d,%f,%d,%c,%[^,],%d-%d-%d,%[^,],%c,%d",
               s.roll, s.name, &s.theory, &s.practical, &s.viva,
               &s.attendance, &s.total, &s.grade, result,
               &d, &m, &y, add, &g, &ph);
        
        strcpy(s.result, result);
        sd[student_count].dob.d = d;
        sd[student_count].dob.m = m;
        sd[student_count].dob.y = y;
        strcpy(sd[student_count].add, add);
        sd[student_count].g = g;
        sd[student_count].ph = ph;

        if (strstr(s.roll, "082BME")) s.year = 1;
        else if (strstr(s.roll, "081BME")) s.year = 2;
        else if (strstr(s.roll, "080BME")) s.year = 3;
        else if (strstr(s.roll, "079BME")) s.year = 4;

        students[student_count] = s;
        student_count++;
    }
    fclose(fp);
}

void load_users() {
    FILE *fp = fopen(USER_FILE, "r");
    user_count = 0;

    if (fp != NULL) {
        char line[200];
        fgets(line, sizeof(line), fp);

        while (fgets(line, sizeof(line), fp) && user_count < 10) {
            sscanf(line, "%[^,],%[^,],%[^\n]",
                   users[user_count].username,
                   users[user_count].password,
                   users[user_count].role);

            int len = strlen(users[user_count].password);
            if (len > 0 && users[user_count].password[len - 1] == '\n') {
                users[user_count].password[len - 1] = '\0';
            }

            len = strlen(users[user_count].role);
            if (len > 0 && users[user_count].role[len - 1] == '\n') {
                users[user_count].role[len - 1] = '\0';
            }

            user_count++;
        }
        fclose(fp);
    }

    int found_admin = 0, found_teacher = 0;

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, "admin") == 0)
            found_admin = 1;
        if (strcmp(users[i].username, "teacher") == 0)
            found_teacher = 1;
    }

    if (!found_admin && user_count < 10) {
        strcpy(users[user_count].username, "admin");
        strcpy(users[user_count].password, "admin123");
        strcpy(users[user_count].role, "admin");
        user_count++;
    }

    if (!found_teacher && user_count < 10) {
        strcpy(users[user_count].username, "teacher");
        strcpy(users[user_count].password, "teacher123");
        strcpy(users[user_count].role, "teacher");
        user_count++;
    }

    save_users();
}

void load_teacher(Teacher *t) {
    FILE *fp = fopen(TEACHER_FILE, "r");

    if (fp == NULL) {
        strcpy(t->name, "Bikal Adhikari");
        strcpy(t->subject, "Computer");
        strcpy(t->phone, "0000000000");
       strcpy(t->gender,"M"); 

        fp = fopen(TEACHER_FILE, "w");
        fprintf(fp, "%s,%s,%s,%s\n",
                t->gender, t->name,
                t->subject, t->phone);
        fclose(fp);
        return;
    }

    fscanf(fp, "%[^,],%[^,],%[^,],%[^\n]",
           t->gender, t->name,
           t->subject, t->phone);

    fclose(fp);
}

void save_teacher(Teacher *t) {
    FILE *fp = fopen(TEACHER_FILE, "w");

    if (fp == NULL) {
        printf("Error saving teacher data!\n");
        return;
    }

    fprintf(fp, "%s,%s,%s,%s\n",
            t->gender, t->name,
            t->subject, t->phone);

    fclose(fp);
}

void view_teacher(Teacher *t) {
    clear_screen();
    print_header("TEACHER DETAILS");

    printf("Name     : %s\n", t->name);
    printf("Subject  : %s\n", t->subject);
    printf("Phone    : %s\n", t->phone);
    printf("Gender : %s\n", t->gender);
}

void edit_teacher(Teacher *t) {
    clear_screen();
    print_header("EDIT TEACHER DETAILS");

    printf("Enter Name: ");
    fgets(t->name, 50, stdin);
    t->name[strcspn(t->name, "\n")] = '\0';

    printf("Enter Subject: ");
    fgets(t->subject, 50, stdin);
    t->subject[strcspn(t->subject, "\n")] = '\0';

    printf("Enter Phone: ");
    fgets(t->phone, 20, stdin);
    t->phone[strcspn(t->phone, "\n")] = '\0';

    printf("Enter Gender: ");
    fgets(t->gender, 20, stdin);
    t->gender[strcspn(t->gender, "\n")] = '\0';


    save_teacher(t);

    printf("\nDetails updated successfully!\n");
}

void teacher_details() {
    Teacher t;
    load_teacher(&t);

    int ch;

    do {
        clear_screen();
        print_header("TEACHER PANEL");

        printf("1. View Personal Details\n");
        printf("2. Edit Personal Details\n");
        printf("3. Back\n");

        printf("\nChoice: ");
        scanf("%d", &ch);
        getchar();

        switch (ch) {
            case 1:
                view_teacher(&t);
                pause_screen();
                break;

            case 2:
                edit_teacher(&t);
                pause_screen();
                break;

            case 3:
                return;

            default:
                printf("Invalid choice!\n");
                pause_screen();
        }

    } while (1);
}
void save_studentdetails() {
    
    for (int i = 0; i < student_count; i++) {
        int exists = 0;
        for (int j = 0; j < user_count; j++) {
            if (strcmp(users[j].username, students[i].roll) == 0) {
                exists = 1;
                break;
            }
        }
        if (!exists && user_count < 10) {
            strcpy(users[user_count].username, students[i].roll);
            strcpy(users[user_count].password, "student123"); 
            strcpy(users[user_count].role, "student");
            user_count++;
        }
    }
    save_users();
}

void load_studentdetails() {
    save_studentdetails();
}

void save_students() {
    FILE *fp = fopen(STUDENT_FILE, "w");
    if (fp == NULL) {
        printf("Error saving data!\n");
        return;
    }
    fprintf(fp, "Roll,Name,Theory,Practical,Viva,Attendance,Total,Grade,Result,DOB,Address,Gender,Phn_no\n");
    for (int i = 0; i < student_count; i++) {
        fprintf(fp, "%s,%s,%d,%d,%d,%.2f,%d,%c,%s,%d-%d-%d,%s,%c,%d\n",
                students[i].roll, students[i].name,
                students[i].theory, students[i].practical, students[i].viva,
                students[i].attendance, students[i].total,
                students[i].grade, students[i].result,sd[i].dob.d,sd[i].dob.m,sd[i].dob.y,sd[i].add,sd[i].g,sd[i].ph);
    }
    fclose(fp);
    save_studentdetails();
}
void edit_student(){

 clear_screen();
    char roll[MAX_ROLL];
    printf("Enter Roll Number to update: ");
    scanf("%s", roll);
    getchar();

    int index = -1;
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, roll) == 0) {
            index = i;
            break;
        } }

    if (index == -1) {
        printf("Student not found!\n");
        return;
    }

    printf("Enter phone no:");
    scanf("%d",&sd[index].ph);
    getchar();

    printf("Enter address:");
    scanf("%s",sd[index].add);
    getchar();

    printf("Enter gender(M/F): ");
    scanf(" %c",&sd[index].g);

    printf("Enter DOB(dd-mm-yyyy):");
    scanf("%d%d%d",&sd[index].dob.d,&sd[index].dob.m,&sd[index].dob.y);


    save_students();
    printf("Student updated successfully!\n");
}

void save_users() {
    FILE *fp = fopen(USER_FILE, "w");
    if (fp == NULL) return;

    fprintf(fp, "Username,Password,Role\n");
    for (int i = 0; i < user_count; i++) {
        fprintf(fp, "%s,%s,%s\n", users[i].username, users[i].password, users[i].role);
    }
    fclose(fp);
}

void add_student() {
    clear_screen();
    if (student_count >= MAX_STUDENTS) {
        printf("Maximum students limit reached!\n");
        return;
    }

    Student s;
    int year;

    printf("Enter Year (1-4): ");
    scanf("%d", &year);
    getchar();

    if (year < 1 || year > 4) {
        printf("Invalid year! Must be 1-4.\n");
        return;
    }

    printf("Enter Name: ");
    fgets(s.name, MAX_NAME, stdin);
    s.name[strcspn(s.name, "\n")] = '\0';

    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].name, s.name) == 0) {
            printf("Student already exists!\n");
            return;
        }
    }
    s.year = year;

    printf("Theory Marks (0-100): ");
    scanf("%d", &s.theory);

    printf("Practical Marks (0-100): ");
    scanf("%d", &s.practical);

    printf("Viva Marks (0-100): ");
    scanf("%d", &s.viva);

    printf("Attendance Percentage (0-100): ");
    scanf("%f", &s.attendance);
    getchar();

    if (s.theory < 0 || s.theory > 100 || s.practical < 0 || s.practical > 100 ||
        s.viva < 0 || s.viva > 100 || s.attendance < 0 || s.attendance > 100) {
        printf("Invalid input! All values must be 0-100.\n");
        return;
    }
    calculate_total(&s);

    students[student_count] = s;
    student_count++;
    assign_roll_numbers(year);
    save_students();

    printf("\nStudent added successfully!\n");
    printf("Total: %d/300 | Grade: %c | Result: %s\n", s.total, s.grade, s.result);
    printf("\nStudent login created!\n");
    printf("Username: %s\n", s.roll);
    printf("Password: student123\n");
}

void assign_roll_numbers(int year) {
    for (int i = 0; i < student_count - 1; i++) {
        for (int j = i + 1; j < student_count; j++) {
            if (strcmp(students[i].name, students[j].name) > 0) {
                Student temp = students[i];
                students[i] = students[j];
                students[j] = temp;
            }
        }
    }

    char prefix[10];
    switch(year) {
        case 1:
            strcpy(prefix, "082BME");
            break;
        case 2:
            strcpy(prefix, "081BME");
            break;
        case 3:
            strcpy(prefix, "080BME");
            break;
        case 4:
            strcpy(prefix, "079BME");
            break;
        default:
            printf("Invalid year!\n");
            return;
    }

    int roll_counter = 1;
    for (int i = 0; i < student_count; i++) {
        if (students[i].year == year) {
            sprintf(students[i].roll, "%s%03d", prefix, roll_counter);
            roll_counter++;
        }
    }
}

void view_all() {
        clear_screen();
    if (student_count == 0) {
        printf("No students found!\n");
        return;
    }

    print_header("ALL STUDENTS");
    printf("%-12s %-20s %3s %3s %3s %5s %6s %5s %s\n",
           "Roll No", "Name", "Th", "Pr", "Vi", "Total", "Attend%", "Gr", "Result");
    printf("-----------------------------------------------------------------------\n");

    for (int i = 0; i < student_count; i++) {
        printf("%-12s %-20s %3d %3d %3d %5d %6.2f %4c %s\n",
               students[i].roll, students[i].name,
               students[i].theory, students[i].practical, students[i].viva,
               students[i].total, students[i].attendance,
               students[i].grade, students[i].result);
    }
    printf("\nTotal: %d students\n", student_count);
}

void search_student() {
    char search[MAX_ROLL];
    printf("Enter Roll Number or Name: ");
    scanf("%s", search);
    getchar();

    int found = 0;
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, search) == 0 ||
            strstr(students[i].name, search) != NULL) {
            found = 1;
            printf("\n----------------------------------------\n");
            printf("Roll No: %s\n", students[i].roll);
            printf("Name: %s\n", students[i].name);
            printf("Theory: %d/100\n", students[i].theory);
            printf("Practical: %d/100\n", students[i].practical);
            printf("Viva: %d/100\n", students[i].viva);
            printf("Total: %d/300\n", students[i].total);
            printf("Attendance: %.2f%%\n", students[i].attendance);
            printf("Grade: %c\n", students[i].grade);
            printf("Result: %s\n", students[i].result);
            printf("----------------------------------------\n");
        }
    }

    if (!found) printf("No student found with '%s'\n", search);
}

void update_student() {
    clear_screen();
    char roll[MAX_ROLL];
    printf("Enter Roll Number to update: ");
    scanf("%s", roll);
    getchar();

    int index = -1;
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, roll) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        printf("Student not found!\n");
        return;
    }

    printf("\nCurrent: %s | Theory:%d Practical:%d Viva:%d\n",
           students[index].name, students[index].theory,
           students[index].practical, students[index].viva);

    printf("New Theory Marks (%d): ", students[index].theory);
    int temp;
    scanf("%d", &temp);
    if (temp >= 0 && temp <= 100) students[index].theory = temp;

    printf("New Practical Marks (%d): ", students[index].practical);
    scanf("%d", &temp);
    if (temp >= 0 && temp <= 100) students[index].practical = temp;

    printf("New Viva Marks (%d): ", students[index].viva);
    scanf("%d", &temp);
    if (temp >= 0 && temp <= 100) students[index].viva = temp;
    getchar();

    calculate_total(&students[index]);
    save_students();
    printf("Student updated successfully!\n");
}

void delete_student() {
    clear_screen();
    char roll[MAX_ROLL];
    printf("Enter Roll Number to delete: ");
    scanf("%s", roll);
    getchar();

    int index = -1;
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, roll) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        printf("Student not found!\n");
        return;
    }

    printf("Delete %s? (y/n): ", students[index].name);
    char confirm;
    scanf("%c", &confirm);
    getchar();

    if (confirm == 'y' || confirm == 'Y') {
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, students[index].roll) == 0) {
                for (int j = i; j < user_count - 1; j++) {
                    users[j] = users[j + 1];
                }
                user_count--;
                break;
            }
        }

        for (int i = index; i < student_count - 1; i++) {
            students[i] = students[i + 1];
        }
        student_count--;
        save_students();
        save_users();
        printf("Student deleted successfully!\n");
    }
}

void show_statistics() {
    clear_screen();
    if (student_count == 0) {
        printf("No students found!\n");
        return;
    }
    int highest = -1, lowest = 301, total_marks = 0;
    int pass = 0, fail = 0, low_att = 0;
    int ga = 0, gb = 0, gc = 0, gd = 0, ge = 0, gf = 0;

    for (int i = 0; i < student_count; i++) {
        total_marks += students[i].total;

        if (students[i].total > highest) highest = students[i].total;
        if (students[i].total < lowest) lowest = students[i].total;

        if (strcmp(students[i].result, "PASS") == 0) pass++;
        else fail++;

        if (students[i].attendance < 75) low_att++;

        switch(students[i].grade) {
            case 'A':
               ga++;
               break;
            case 'B':
                gb++;
                break;
            case 'C':
                gc++;
                break;
            case 'D':
                gd++;
                break;
            case 'E':
                ge++;
                break;
            case 'F':
                gf++;
                break;
        } }

    float avg = (float)total_marks / student_count;
    float pass_per = (float)pass / student_count * 100;

    print_header("CLASS STATISTICS");
    printf("Total Students: %d\n", student_count);
    printf("Highest Marks: %d/300\n", highest);
    printf("Lowest Marks: %d/300\n", lowest);
    printf("Average Marks: %.2f/300\n", avg);
    printf("\n");
    printf("Passed: %d (%.2f%%)\n", pass, pass_per);
    printf("Failed: %d (%.2f%%)\n", fail, 100 - pass_per);
    printf("\n");
    printf("Low Attendance (<75%%): %d\n", low_att);
    printf("\nGrade Distribution:\n");
    printf("A: %d | B: %d | C: %d | D: %d | E: %d | F: %d\n", ga, gb, gc, gd, ge, gf);
}

void sort_by_marks() {
    clear_screen();
    if (student_count == 0) {
        printf("No students found!\n");
        return;
    }

    Student sorted[MAX_STUDENTS];
    memcpy(sorted, students, sizeof(Student) * student_count);

    for (int i = 0; i < student_count - 1; i++) {
        for (int j = i + 1; j < student_count; j++) {
            if (sorted[j].total > sorted[i].total) {
                Student temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp; }  }
    }
    print_header("RANKINGS (By Total Marks)");
    printf("%-5s %-12s %-20s %-8s %s\n", "Rank", "Roll No", "Name", "Total", "Grade");
    printf("----------------------------------------------\n");

    for (int i = 0; i < student_count; i++) {
        printf("%-5d %-12s %-20s %-8d %c\n",
               i + 1, sorted[i].roll, sorted[i].name,
               sorted[i].total, sorted[i].grade);
    } }

void show_attendance() {
    clear_screen();
    if (student_count == 0) {
        printf("No students found!\n");
        return;  }
        
    print_header("ATTENDANCE REPORT");
    printf("%-12s %-20s %-10s %s\n", "Roll No", "Name", "Attendance", "Status");
    printf("--------------------------------------------\n");

    int eligible = 0;
    for (int i = 0; i < student_count; i++) {
        char status[20];
        if (students[i].attendance >= 75) {
            strcpy(status, "Eligible");
            eligible++;
        } else {
            strcpy(status, "Not Eligible");
        }

        printf("%-12s %-20s %-10.2f %s\n",
               students[i].roll, students[i].name,
               students[i].attendance, status);
    }

    printf("\nEligible for exams: %d/%d (%.2f%%)\n",
           eligible, student_count, (float)eligible / student_count * 100);
}

void update_attendance() {
    clear_screen();
    char roll[MAX_ROLL];
    printf("Enter Roll Number: ");
    scanf("%s", roll);
    getchar();

    int index = -1;
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, roll) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        printf("Student not found!\n");
        return;
    }
    printf("Current Attendance: %.2f%%\n", students[index].attendance);
    printf("New Attendance: ");
    scanf("%f", &students[index].attendance);
    getchar();

    if (students[index].attendance >= 0 && students[index].attendance <= 100) {
        calculate_total(&students[index]);
        save_students();
        printf("Attendance updated successfully!\n");
    } else {
        printf("Invalid attendance percentage!\n");
        students[index].attendance = 0;
    }
}
void student_menu() {
    int ch;
    do {
        clear_screen();
        print_header("STUDENT PORTAL");
        printf("Welcome, %s\n", current_user);
        printf("\n1. View My Details\n");
        printf("2. Edit My Details\n");
        printf("3. View My Attendance\n");
        printf("4. View My Grade & Result\n");
        printf("5. View Class Rankings\n");
        printf("6. Logout\n");

        printf("\nChoice: ");
        scanf("%d", &ch);
        getchar();

        int student_index = -1;
        for (int i = 0; i < student_count; i++) {
            if (strcmp(students[i].roll, current_user) == 0) {
                student_index = i;
                break;
            }
        }

        if (student_index == -1 && ch != 5 && ch != 6) {
            printf("Student record not found!\n");
            pause_screen();
            continue;
        }

        switch(ch) {
            case 1:
                printf("\n========================================\n");
                printf("YOUR DETAILS\n");
                printf("========================================\n");
                printf("Roll No: %s\n", students[student_index].roll);
                printf("Name: %s\n", students[student_index].name);
                printf("Year: %d\n", students[student_index].year);
                printf("Theory Marks: %d/100\n", students[student_index].theory);
                printf("Practical Marks: %d/100\n", students[student_index].practical);
                printf("Viva Marks: %d/100\n", students[student_index].viva);
                printf("Total Marks: %d/300\n", students[student_index].total);
                printf("Attendance: %.2f%%\n", students[student_index].attendance);
                printf("Grade: %c\n", students[student_index].grade);
                printf("Result: %s\n", students[student_index].result);
                printf("Phone no: %d\n", sd[student_index].ph);
                printf("Address: %s\n", sd[student_index].add);
                printf("DOB: %d-%d-%d\n", sd[student_index].dob.d, sd[student_index].dob.m, sd[student_index].dob.y);
                printf("Gender: %c\n", sd[student_index].g);
                break;
                
            case 2:
                edit_student();
                break;

            case 3:
                printf("\n========================================\n");
                printf("YOUR ATTENDANCE\n");
                printf("========================================\n");
                printf("Roll No: %s\n", students[student_index].roll);
                printf("Name: %s\n", students[student_index].name);
                printf("Attendance: %.2f%%\n", students[student_index].attendance);
                if (students[student_index].attendance >= 75) {
                    printf("Status: Eligible for exams\n");
                } else {
                    printf("Status: NOT Eligible for exams (Low attendance)\n");
                }
                break;

            case 4:
                printf("\n========================================\n");
                printf("YOUR GRADE & RESULT\n");
                printf("========================================\n");
                printf("Roll No: %s\n", students[student_index].roll);
                printf("Name: %s\n", students[student_index].name);
                printf("Total Marks: %d/300\n", students[student_index].total);
                printf("Grade: %c\n", students[student_index].grade);
                printf("Result: %s\n", students[student_index].result);
                if (strcmp(students[student_index].result, "PASS") == 0) {
                    printf("\nCongratulations! You have passed.\n");
                } else if (strcmp(students[student_index].result, "LOW ATTEND") == 0) {
                    printf("\nYou have failed due to low attendance.\n");
                } else {
                    printf("\nYou have failed. Please work harder next time.\n");
                }
                break;

            case 5:
                sort_by_marks();
                break;

            case 6:
                logged_in = 0;
                printf("Logging out...\n");
                break;

            default:
                printf("Invalid choice!\n");
        }

        if (ch >= 1 && ch <= 5) {
            pause_screen();
        }

    } while (ch != 6);  
} 

 void login() {
    clear_screen();
    char username[30], password[30];

    print_header("LOGIN");
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);
    getchar();

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            logged_in = 1;
            strcpy(current_user, username);
            strcpy(current_role, users[i].role);
            printf("\nLogin successful! Welcome, %s\n", username);
            printf("Role: %s\n", users[i].role);
            pause_screen();
            return;
        }
    }

    printf("\nInvalid username or password!\n");
    pause_screen();
}
        
void main_menu_admin() {
    int ch;
    do {
        clear_screen();
        print_header("ADMIN PANEL");
        
        printf("1. Add Student\n");
        printf("2. View All Students\n");
        printf("3. Search Student\n");
        printf("4. Update Student\n");
        printf("5. Delete Student\n");
        printf("6. Class Statistics\n");
        printf("7. Rankings (Sort by Marks)\n");
        printf("8. Attendance Report\n");
        printf("9. Update Attendance\n");
        printf("10. Teacher Details\n");
        printf("11. Logout\n");
        printf("\nChoice: ");
        scanf("%d", &ch);
        getchar();

        switch(ch) {
            case 1:
                add_student();
                break;
            case 2:
                view_all();
                break;
            case 3:
                search_student();
                break;
            case 4:
                update_student();
                break;
            case 5:
                delete_student();
                break;
            case 6:
                show_statistics();
                break;
            case 7:
                sort_by_marks();
                break;
            case 8:
                show_attendance();
                break;
            case 9:
                update_attendance();
                break;
            case 10:
                teacher_details();
                break;
            case 11:
                logged_in = 0;
                printf("Logging out...\n");
                break;
            default:
                printf("Invalid choice!\n");
        }

        if (ch >= 1 && ch <= 10) {
            pause_screen();
        }

    } while (ch != 11);
}

void main_menu_teacher() {
    int ch;
    do {
        clear_screen();
        print_header("TEACHER PANEL");
        
        printf("1. View All Students\n");
        printf("2. Search Student\n");
        printf("3. Update Student Marks\n");
        printf("4. Update Attendance\n");
        printf("5. Class Statistics\n");
        printf("6. Rankings (Sort by Marks)\n");
        printf("7. Attendance Report\n");
        printf("8. My Profile\n");
        printf("9. Logout\n");
        printf("\nChoice: ");
        scanf("%d", &ch);
        getchar();

        switch(ch) {
            case 1:
                view_all();
                break;
            case 2:
                search_student();
                break;
            case 3:
                update_student();
                break;
            case 4:
                update_attendance();
                break;
            case 5:
                show_statistics();
                break;
            case 6:
                sort_by_marks();
                break;
            case 7:
                show_attendance();
                break;
            case 8:
                teacher_details();
                break;
            case 9:
                logged_in = 0;
                printf("Logging out...\n");
                break;
            default:
                printf("Invalid choice!\n");
        }

        if (ch >= 1 && ch <= 8) {
            pause_screen();
        }

    } while (ch != 9);
}

int main() {
    load_students();
    load_users();
    load_studentdetails();

    int ch;
    do {
        clear_screen();
        print_header_w_animation("STUDENT RECORD MANAGEMENT SYSTEM");

        if (!logged_in) {
            reenter:
            printf("[1] LOGIN\n");
            printf("[2] EXIT\n");
            printf("\nChoice: ");

            scanf("%d", &ch);
            getchar();

            if (ch == 1) {
                int choice;
                clear_screen();
                print_header("SELECT ROLE");
                printf("[1] Admin\n");
                printf("[2] Teacher\n");
                printf("[3] Student\n");
                printf("Choice: ");
                scanf("%d", &choice);
                getchar();

                login();

                if (logged_in) {
                    switch (choice) {
                        case 1:
                            main_menu_admin();
                            break;
                        case 2:
                            main_menu_teacher();
                            break;
                        case 3:
                            student_menu();
                            break;
                        default:
                            printf("Invalid role selected!\n");
                            pause_screen();
                            logged_in = 0;
                    }
                }
            }
            else if (ch == 2) {
                break;
            }
            else {
                printf("Invalid choice! Please re-enter\n");
                goto reenter;
            }
        }
    } while (1);

    save_students();
    save_users();
    print_header_w_animation("\nThank you for using the system!\n");
    return 0;
}
