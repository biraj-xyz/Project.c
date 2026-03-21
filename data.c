#include <stdio.h>
#include <stdlib.h>
#include "student.h"

struct Student *students = NULL;
int count = 0;

/* ================= LOAD DATA (OPTIMIZED & SAFE) ================= */

void loadData() {

    FILE *fp = fopen("students.dat", "rb");
    if (!fp) {
        /* No existing file – first run */
        return;
    }

    /* Move to end to calculate file size */
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);

    if (fileSize == 0) {
        fclose(fp);
        return;
    }

    rewind(fp);

    count = fileSize / sizeof(struct Student);

    students = (struct Student *)malloc(count * sizeof(struct Student));
    if (!students) {
        printf("Memory allocation failed while loading data.\n");
        fclose(fp);
        count = 0;
        return;
    }

    size_t readCount = fread(students, sizeof(struct Student), count, fp);
    if (readCount != count) {
        printf("Error reading student records.\n");
        free(students);
        students = NULL;
        count = 0;
    }

    fclose(fp);
}

/* ================= SAVE DATA (SAFE VERSION) ================= */

void saveData() {

    FILE *fp = fopen("students.dat", "wb");
    if (!fp) {
        printf("Error: Unable to open file for saving.\n");
        return;
    }

    if (count > 0 && students != NULL) {
        size_t written = fwrite(students, sizeof(struct Student), count, fp);
        if (written != count) {
            printf("Error writing data to file.\n");
        }
    }

    fclose(fp);
}

/* ================= BACKUP SYSTEM (SAFE VERSION) ================= */

void backupData() {

    FILE *src = fopen("students.dat", "rb");
    if (!src) {
        printf("No data file found to backup.\n");
        return;
    }

    FILE *dest = fopen("backup.dat", "wb");
    if (!dest) {
        printf("Error creating backup file.\n");
        fclose(src);
        return;
    }

    struct Student temp;

    while (fread(&temp, sizeof(struct Student), 1, src) == 1) {
        fwrite(&temp, sizeof(struct Student), 1, dest);
    }

    fclose(src);
    fclose(dest);

    printf("Backup created successfully.\n");
}