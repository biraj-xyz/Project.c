/*
 * ════════════════════════════════════════════════════════════════════
 *  STUDENT RESULT MANAGEMENT SYSTEM — ENTERPRISE EDITION  v3.0
 *  IOE Pulchowk Campus — Mechanical Engineering Department
 *
 *  PERSISTENT FILE STORAGE (data/ directory):
 *    students.dat     — binary: all student records
 *    users.dat        — binary: all user / login records
 *    results.dat      — binary: all exam result records
 *    fees.dat         — binary: fee payment records
 *    attendance.dat   — binary: staff attendance records
 *    notices.txt      — text:   notice board entries
 *    ledger.txt       — text:   financial transactions
 *    system.cfg       — text:   system configuration
 *    system.log       — text:   audit / event log (NEW)
 *    *.bak            — backup copies before every write (NEW)
 *
 *  HOW TO COMPILE:
 *    gcc pulchowk_final.c -o pulchowk_result_system
 *
 *  HOW TO RUN:
 *    Windows : pulchowk_result_system.exe
 *    Linux   : ./pulchowk_result_system
 *
 *  UPGRADE NOTES (v3.0 additions over v2.0):
 *    + system.log — full audit trail of every login / change / error
 *    + Backup system — .bak written before every binary file save
 *    + Safe-write — write to .tmp then rename, prevents corruption
 *    + Recovery — if magic/version wrong, auto-restore from .bak
 *    + Password masking — * echoed on input (Unix only; plain on Win)
 *    + XOR obfuscation — passwords stored obfuscated in binary files
 *    + Login attempt limiter — 3 strikes then locked for session
 *    + Duplicate username guard — enforced before any insert
 *    + Orphan-result guard — results must match a known student
 *    + Export to CSV — results, fees, student list
 *    + Import students from CSV — batch enrolment
 *    + Cumulative GPA across all semesters
 *    + Full attendance report per staff member
 *    + Fee reports: sorted by status / year / outstanding amount
 *    + Data-integrity check menu (admin only)
 *    + Secure admin: reset any user's password
 *    + Change password (all roles, verified)
 *    + View system log (admin only)
 *    + All existing v2.0 features preserved unchanged
 * ════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #include <conio.h>          /* _getch for password masking */
  #define MKDIR(p)   _mkdir(p)
  #define RENAME(a,b) MoveFileExA(a,b,MOVEFILE_REPLACE_EXISTING)
  #define HAS_GETCH  1
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <termios.h>
  #include <unistd.h>
  #define MKDIR(p)   mkdir(p, 0755)
  #define RENAME(a,b) rename(a,b)
  #define HAS_GETCH  0
#endif

/* ════════════════════════════════════════════════════════════════════
 *  FILE PATH CONSTANTS
 * ════════════════════════════════════════════════════════════════════ */
#define DATA_DIR           "data"
#define FILE_STUDENTS      "data/students.dat"
#define FILE_USERS         "data/users.dat"
#define FILE_RESULTS       "data/results.dat"
#define FILE_FEES          "data/fees.dat"
#define FILE_ATTENDANCE    "data/attendance.dat"
#define FILE_NOTICES       "data/notices.txt"
#define FILE_LEDGER        "data/ledger.txt"
#define FILE_CONFIG        "data/system.cfg"
#define FILE_LOG           "data/system.log"   /* NEW: audit log */

/* Temp & backup suffixes */
#define SUFFIX_TMP         ".tmp"
#define SUFFIX_BAK         ".bak"

/* Magic numbers — validate binary files */
#define MAGIC_STUDENTS     0xA1B2C3D4u
#define MAGIC_USERS        0xA2B3C4D5u
#define MAGIC_RESULTS      0xA3B4C5D6u
#define MAGIC_FEES         0xA4B5C6D7u
#define MAGIC_ATTENDANCE   0xA5B6C7D8u
#define FILE_VERSION       3              /* bumped from v2 */

/* XOR key for password obfuscation in binary files */
#define PWD_XOR_KEY        0x5Au

/* ════════════════════════════════════════════════════════════════════
 *  LIMITS & SIZES
 * ════════════════════════════════════════════════════════════════════ */
#define MAX_STUDENTS       300
#define MAX_SUBJECTS         7
#define MAX_YEAR             4
#define MAX_SEM              2
#define MAX_STAFF           20
#define MAX_ACCOUNTS         3
#define MAX_TEACHERS         2
#define MAX_NOTICES         40   /* expanded from 20 */
#define MAX_LOGIN_ATTEMPTS   3   /* NEW: lockout after 3 failures */
#define USERNAME_LEN        20
#define PASSWORD_LEN        28
#define NAME_LEN            64
#define SUBJECT_LEN         64
#define CODE_LEN            12
#define ROLE_LEN            16
#define LABEL_LEN           40
#define LOG_LINE_LEN       256
#define CSV_LINE_LEN       512

/* ════════════════════════════════════════════════════════════════════
 *  DATA STRUCTURES  (all original structures fully preserved)
 * ════════════════════════════════════════════════════════════════════ */

/* Binary file header — every .dat file starts with this */
typedef struct {
    unsigned int magic;
    int          version;
    int          count;
    char         created[32];
    char         modified[32];
} FileHeader;

/* Student record */
typedef struct {
    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
    char name[NAME_LEN];
    char role[ROLE_LEN];
    int  year;
    int  valid_sems[2];
    int  valid_sem_count;
    char prefix[16];
    char year_label[LABEL_LEN];
} Student;

/* Generic user — admin / teacher / staff / accountant */
typedef struct {
    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
    char name[NAME_LEN];
    char role[ROLE_LEN];
    char display[LABEL_LEN];
    char extra[128];
    char phone[20];
    char joined[16];
} User;

/* One semester result */
typedef struct {
    int marks[MAX_SUBJECTS];
    int count;
} SemResult;

/* All results for one student */
typedef struct {
    char      username[USERNAME_LEN];
    SemResult results[MAX_YEAR][MAX_SEM];
} StudentResult;

/* Fee record */
typedef struct {
    char username[USERNAME_LEN];
    int  year;
    int  due;
    int  paid;
    char status[12];
    char last_payment[16];
} FeeRecord;

/* Attendance record */
typedef struct {
    char username[USERNAME_LEN];
    char status[12];
    char date[16];
} AttendRecord;

/* Notice entry */
typedef struct {
    char date[16];
    char category[24];
    char text[200];
} Notice;

/* Subject information — IOE BME new syllabus 2080 */
typedef struct {
    char name[SUBJECT_LEN];
    char code[CODE_LEN];
    int  total_fm;
    int  theory_fm;
    int  internal_fm;
    int  practical_fm;
} SubjectInfo;

/* Ranking row (used by qsort) */
typedef struct {
    char   username[USERNAME_LEN];
    double pct;
    int    total;
    const char *grade;
} RankRow;

/* ════════════════════════════════════════════════════════════════════
 *  GLOBAL DATA ARRAYS  (all original globals preserved)
 * ════════════════════════════════════════════════════════════════════ */

static Student       ALL_STUDENTS[MAX_STUDENTS];
static int           STUDENT_COUNT = 0;

static User          USERS[MAX_STUDENTS + MAX_STAFF + MAX_ACCOUNTS + MAX_TEACHERS + 10];
static int           USER_COUNT = 0;

static StudentResult RESULTS[MAX_STUDENTS];
static int           RESULT_COUNT = 0;

static FeeRecord     FEES[MAX_STUDENTS];
static int           FEE_COUNT = 0;

static AttendRecord  ATTENDANCE[MAX_STAFF];
static int           ATTEND_COUNT = 0;

static Notice        NOTICES[MAX_NOTICES];
static int           NOTICE_COUNT = 0;

/* Session state */
static User    current_user;
static int     current_is_student = 0;
static Student current_student;
static int     current_sel_year   = 0;
static int     current_sel_sem    = 0;
static int     logged_in          = 0;

/* ════════════════════════════════════════════════════════════════════
 *  IOE BME NEW SYLLABUS 2080
 *  Source: SOMAES Hub (somes.ioe.edu.np) — Official Pulchowk ME Society
 * ════════════════════════════════════════════════════════════════════ */

static const int SUBJECT_COUNT[MAX_YEAR][MAX_SEM] = {
    {6, 6},   /* Year 1 */
    {6, 6},   /* Year 2 */
    {7, 7},   /* Year 3 */
    {7, 4},   /* Year 4 */
};

static const SubjectInfo SUBJECTS[MAX_YEAR][MAX_SEM][MAX_SUBJECTS] = {
/* Year 1 / Sem 1 */
{{ {"Engineering Mathematics I",                    "SH 101", 100, 80, 20,  0},
   {"Engineering Chemistry",                        "SH 103", 100, 60, 20, 25},
   {"Computer Programming",                         "CT 101", 100, 60, 20, 25},
   {"Fundamental of Electrical & Electronics Engg.","EX 101", 100, 60, 20, 25},
   {"Engineering Drawing",                          "ME 101", 100, 40, 20, 40},
   {"Engineering Mechanics I",                      "ME 103", 100, 80, 20,  0},
   {"",                                             "",         0,  0,  0,  0}},
/* Year 1 / Sem 2 */
  { {"Engineering Mathematics II",                  "SH 151", 100, 80, 20,  0},
    {"Engineering Physics",                         "SH 152", 100, 60, 20, 25},
    {"Engineering Thermodynamics I",                "ME 151", 100, 80, 20,  0},
    {"Machine Drawing",                             "ME 152", 100, 40, 20, 40},
    {"Engineering Mechanics II",                    "ME 153", 100, 80, 20,  0},
    {"Workshop Technology",                         "ME 155", 100, 60, 20, 25},
    {"",                                            "",          0,  0,  0,  0}}},
/* Year 2 / Sem 1 */
{{ {"Engineering Mathematics III",                  "SH 201", 100, 80, 20,  0},
   {"Material Science",                             "ME 201", 100, 60, 20, 25},
   {"Manufacturing & Production Processes",         "ME 202", 100, 60, 20, 25},
   {"Engineering Thermodynamics II",                "ME 203", 100, 80, 20,  0},
   {"Metrology",                                    "ME 204", 100, 60, 20, 25},
   {"Strength of Materials",                        "ME 205", 100, 80, 20,  0},
   {"",                                             "",          0,  0,  0,  0}},
/* Year 2 / Sem 2 */
  { {"Probability and Statistics",                  "SH 252", 100, 80, 20,  0},
    {"Electrical Machines",                         "EE 254", 100, 60, 20, 25},
    {"Mechanics of Solids",                         "ME 251", 100, 80, 20,  0},
    {"Instrumentation and Sensors",                 "ME 252", 100, 60, 20, 25},
    {"Computer Aided Design",                       "ME 253", 100, 40, 20, 40},
    {"Fluid Mechanics with Engineering Applications","ME 254", 100, 60, 20, 25},
    {"",                                            "",          0,  0,  0,  0}}},
/* Year 3 / Sem 1 */
{{ {"Control System and Automation",                "EE 307", 100, 60, 20, 25},
   {"Numerical Methods",                            "SH 301", 100, 60, 20, 25},
   {"Automotive Technology",                        "ME 301", 100, 60, 20, 25},
   {"Fluid Machines",                               "ME 302", 100, 60, 20, 25},
   {"Theory of Machines and Mechanism",             "ME 303", 100, 60, 20, 25},
   {"Organization and Management",                  "ME 304", 100, 80, 20,  0},
   {"Technical English",                            "SH 303", 100, 80, 20,  0}},
/* Year 3 / Sem 2 */
  { {"Professional Engineering Economics",          "ME 356", 100, 80, 20,  0},
    {"Industrial Engineering and Management",       "ME 351", 100, 80, 20,  0},
    {"Heat and Mass Transfer",                      "ME 352", 100, 60, 20, 25},
    {"Machine Design I",                            "ME 353", 100, 80, 20,  0},
    {"Machine Dynamics",                            "ME 354", 100, 60, 20, 25},
    {"Minor Project",                               "ME 355", 100,  0, 60, 40},
    {"Elective I",                                  "ME 365", 100, 80, 20,  0}}},
/* Year 4 / Sem 1 */
{{ {"Entrepreneurship Development",                 "ME 411", 100, 80, 20,  0},
   {"Finite Element Method",                        "ME 412", 100, 60, 20, 25},
   {"Applied Computational Fluid Dynamics",         "ME 413", 100, 60, 20, 25},
   {"Energy Resources and Technology",              "ME 414", 100, 80, 20,  0},
   {"Machine Design II",                            "ME 415", 100, 80, 20,  0},
   {"Elective II",                                  "ME 425", 100, 80, 20,  0},
   {"Project I",                                    "ME 416", 100,  0, 60, 40}},
/* Year 4 / Sem 2 */
  { {"Project Management and Professional Practice","ME 463",  100, 80, 20,  0},
    {"Elective III",                                "ME 465",  100, 80, 20,  0},
    {"Project II",                                  "ME 461",  100,  0, 60, 40},
    {"Industrial Attachment (8 weeks)",             "ME 462",  100,  0, 50, 50},
    {"",                                            "",           0,  0,  0,  0},
    {"",                                            "",           0,  0,  0,  0},
    {"",                                            "",           0,  0,  0,  0}}},
};

static const char STAFF_ROLES[20][32] = {
    "Lab Technician","Lab Technician","Office Assistant","Peon",
    "Security Guard","Librarian","IT Support","Maintenance",
    "Canteen Staff","Driver","Cleaner","Records Keeper",
    "Workshop Instructor","Store Keeper","Electrician","Plumber",
    "Security Guard","Office Assistant","Gardener","Sweeper"
};

static const char STAFF_PHONES[20][20] = {
    "+977-1-5521001","+977-1-5521002","+977-1-5521003","+977-1-5521004",
    "+977-1-5521005","+977-1-5521006","+977-1-5521007","+977-1-5521008",
    "+977-1-5521009","+977-1-5521010","+977-1-5521011","+977-1-5521012",
    "+977-1-5521013","+977-1-5521014","+977-1-5521015","+977-1-5521016",
    "+977-1-5521017","+977-1-5521018","+977-1-5521019","+977-1-5521020"
};

/* ════════════════════════════════════════════════════════════════════
 *  UTILITY — LCG PRNG  (original, unchanged)
 * ════════════════════════════════════════════════════════════════════ */
static unsigned int lcg_seed;
static void         lcg_init(unsigned int s) { lcg_seed = s; }
static unsigned int lcg_next(void) {
    lcg_seed = (lcg_seed * 1664525u + 1013904223u);
    return lcg_seed;
}
static double lcg_rand(void) { return (double)lcg_seed / 4294967295.0; }

static unsigned int username_seed(const char *u, int sem) {
    unsigned int s = (unsigned int)(sem * 100);
    for (const char *p = u; *p; p++) s += (unsigned char)*p;
    return s;
}

/* ════════════════════════════════════════════════════════════════════
 *  UTILITY — STRINGS / I/O  (originals + new additions)
 * ════════════════════════════════════════════════════════════════════ */
static void str_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static void read_line(char *buf, int size) {
    if (fgets(buf, size, stdin)) {
        int n = (int)strlen(buf);
        if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
        if (n > 1 && buf[n-2] == '\r') buf[n-2] = '\0';
    } else {
        buf[0] = '\0';
    }
}

static int read_int(void) {
    char buf[32];
    read_line(buf, sizeof(buf));
    if (!buf[0]) return -1;
    for (int i = 0; buf[i]; i++)
        if (!isdigit((unsigned char)buf[i]) && !(i == 0 && buf[i] == '-')) return -1;
    return atoi(buf);
}

/* NEW: read password with masking (shows * per character typed) */
static void read_password(char *buf, int size) {
#if HAS_GETCH
    int i = 0;
    int c;
    while (i < size - 1) {
        c = _getch();
        if (c == '\r' || c == '\n') break;
        if (c == '\b' || c == 127) {
            if (i > 0) { i--; printf("\b \b"); fflush(stdout); }
            continue;
        }
        buf[i++] = (char)c;
        putchar('*'); fflush(stdout);
    }
    buf[i] = '\0';
    putchar('\n');
#else
    /* Unix: disable echo */
    struct termios old_t, new_t;
    if (tcgetattr(STDIN_FILENO, &old_t) == 0) {
        new_t = old_t;
        new_t.c_lflag &= (tcflag_t)~(ECHO | ECHOE | ECHOK | ECHONL);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
        read_line(buf, size);
        tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
        printf("\n");
    } else {
        read_line(buf, size);
    }
#endif
}

/* NEW: XOR-obfuscate / de-obfuscate a password string in place */
/* xor_password: de-obfuscates password bytes in-place (used in load/save) */
static void xor_password(char *pwd) {
    for (int i = 0; pwd[i]; i++)
        pwd[i] ^= (char)PWD_XOR_KEY;
}

static void sep(char c, int w) {
    for (int i = 0; i < w; i++) putchar(c);
    putchar('\n');
}

static void pause_enter(void) {
    printf("\n  Press Enter to continue...");
    fflush(stdout);
    char b[8]; read_line(b, sizeof(b));
}

static void clrscr(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
    fflush(stdout);
#endif
}

static void today_str(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_p = localtime(&t);
    snprintf(buf, size, "%04d-%02d-%02d",
             tm_p->tm_year + 1900, tm_p->tm_mon + 1, tm_p->tm_mday);
}

static void now_str(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_p = localtime(&t);
    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d",
             tm_p->tm_year + 1900, tm_p->tm_mon + 1, tm_p->tm_mday,
             tm_p->tm_hour, tm_p->tm_min, tm_p->tm_sec);
}

static const char *get_grade(double pct) {
    if (pct >= 90) return "A+";
    if (pct >= 80) return "A";
    if (pct >= 70) return "B+";
    if (pct >= 60) return "B";
    if (pct >= 50) return "C";
    if (pct >= 40) return "D";
    return "F";
}

static double get_gpa(double pct) {
    if (pct >= 90) return 4.0;
    if (pct >= 80) return 3.6;
    if (pct >= 70) return 3.2;
    if (pct >= 60) return 2.8;
    if (pct >= 50) return 2.4;
    if (pct >= 40) return 2.0;
    return 0.0;
}

static void print_banner(void) {
    printf("\n");
    sep('=', 72);
    printf("    IOE PULCHOWK CAMPUS — MECHANICAL ENGINEERING DEPARTMENT\n");
    printf("       STUDENT RESULT MANAGEMENT SYSTEM  v3.0 (Enterprise)\n");
    printf("        Pulchowk, Lalitpur, Nepal | +977-1-5521034\n");
    sep('=', 72);
    printf("\n");
}

static void print_panel(const char *title) {
    printf("\n");
    sep('-', 72);
    printf("  %s\n", title);
    sep('-', 72);
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  NEW: AUDIT LOG — system.log
 * ════════════════════════════════════════════════════════════════════ */
static void log_event(const char *user, const char *action) {
    FILE *f = fopen(FILE_LOG, "a");
    if (!f) return;
    char ts[32]; now_str(ts, sizeof(ts));
    fprintf(f, "[%s] USER=%-14s  %s\n", ts, user ? user : "SYSTEM", action);
    fclose(f);
}

static void log_error(const char *user, const char *msg) {
    FILE *f = fopen(FILE_LOG, "a");
    if (!f) return;
    char ts[32]; now_str(ts, sizeof(ts));
    fprintf(f, "[%s] USER=%-14s  ERROR: %s\n",
            ts, user ? user : "SYSTEM", msg);
    fclose(f);
}

/* ════════════════════════════════════════════════════════════════════
 *  NEW: BACKUP HELPERS
 * ════════════════════════════════════════════════════════════════════ */

/* Create a .bak copy of a file before overwriting */
static void make_backup(const char *path) {
    char bak[256];
    snprintf(bak, sizeof(bak), "%s%s", path, SUFFIX_BAK);
    FILE *src = fopen(path, "rb");
    if (!src) return;
    FILE *dst = fopen(bak, "wb");
    if (!dst) { fclose(src); return; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, n, dst);
    fclose(src);
    fclose(dst);
}

/* Restore .bak if present */
static int restore_backup(const char *path) {
    char bak[256];
    snprintf(bak, sizeof(bak), "%s%s", path, SUFFIX_BAK);
    FILE *f = fopen(bak, "rb");
    if (!f) return 0;
    fclose(f);
    return (RENAME(bak, path) == 0) ? 1 : 0;
}

/* Safe-write: write to .tmp then rename over original */
static int safe_write_binary(const char *path, unsigned int magic,
                              const void *data, size_t item_size, int count,
                              int use_pwd_xor, size_t pwd_off, size_t rec_size) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s%s", path, SUFFIX_TMP);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Cannot open tmp file '%s': %s", tmp, strerror(errno));
        log_error("SYSTEM", msg);
        return 0;
    }

    FileHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic   = magic;
    hdr.version = FILE_VERSION;
    hdr.count   = count;
    today_str(hdr.created,  sizeof(hdr.created));
    now_str(hdr.modified, sizeof(hdr.modified));

    fwrite(&hdr, sizeof(FileHeader), 1, f);

    if (count > 0 && data) {
        if (use_pwd_xor) {
            /* Write record by record, obfuscating password field */
            const char *ptr = (const char *)data;
            for (int i = 0; i < count; i++) {
                char rec[1024];
                if (rec_size > sizeof(rec)) rec_size = sizeof(rec);
                memcpy(rec, ptr + i * rec_size, rec_size);
                /* XOR the password bytes */
                for (size_t j = pwd_off; j < pwd_off + PASSWORD_LEN && j < rec_size; j++)
                    rec[j] ^= (char)PWD_XOR_KEY;
                fwrite(rec, rec_size, 1, f);
            }
        } else {
            fwrite(data, item_size, count, f);
        }
    }
    fclose(f);

    /* Backup current file before replacing */
    make_backup(path);

    if (RENAME(tmp, path) != 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Rename '%s' -> '%s' failed: %s",
                 tmp, path, strerror(errno));
        log_error("SYSTEM", msg);
        remove(tmp);
        return 0;
    }
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 *  ENSURE DATA DIRECTORY
 * ════════════════════════════════════════════════════════════════════ */
static void ensure_data_dir(void) {
    MKDIR(DATA_DIR);
}

/* ════════════════════════════════════════════════════════════════════
 *  SAVE FUNCTIONS  (upgraded: safe-write + backup + log)
 * ════════════════════════════════════════════════════════════════════ */

static void save_students(void) {
    if (safe_write_binary(FILE_STUDENTS, MAGIC_STUDENTS,
                          ALL_STUDENTS, sizeof(Student), STUDENT_COUNT,
                          0, 0, 0)) {
        printf("  [OK] Saved %d students -> %s\n", STUDENT_COUNT, FILE_STUDENTS);
        log_event("SYSTEM", "students.dat saved");
    }
}

static void save_users(void) {
    /* Obfuscate passwords in users file */
    if (safe_write_binary(FILE_USERS, MAGIC_USERS,
                          USERS, sizeof(User), USER_COUNT,
                          1,
                          (size_t)((char *)USERS[0].password - (char *)&USERS[0]),
                          sizeof(User))) {
        printf("  [OK] Saved %d users -> %s\n", USER_COUNT, FILE_USERS);
        log_event("SYSTEM", "users.dat saved");
    }
}

static void save_results(void) {
    if (safe_write_binary(FILE_RESULTS, MAGIC_RESULTS,
                          RESULTS, sizeof(StudentResult), RESULT_COUNT,
                          0, 0, 0)) {
        printf("  [OK] Saved %d result records -> %s\n", RESULT_COUNT, FILE_RESULTS);
        log_event("SYSTEM", "results.dat saved");
    }
}

static void save_fees(void) {
    if (safe_write_binary(FILE_FEES, MAGIC_FEES,
                          FEES, sizeof(FeeRecord), FEE_COUNT,
                          0, 0, 0)) {
        printf("  [OK] Saved %d fee records -> %s\n", FEE_COUNT, FILE_FEES);
        log_event("SYSTEM", "fees.dat saved");
    }
}

static void save_attendance(void) {
    if (safe_write_binary(FILE_ATTENDANCE, MAGIC_ATTENDANCE,
                          ATTENDANCE, sizeof(AttendRecord), ATTEND_COUNT,
                          0, 0, 0)) {
        printf("  [OK] Saved %d attendance records -> %s\n",
               ATTEND_COUNT, FILE_ATTENDANCE);
        log_event("SYSTEM", "attendance.dat saved");
    }
}

static void save_notices(void) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s%s", FILE_NOTICES, SUFFIX_TMP);
    FILE *f = fopen(tmp, "w");
    if (!f) { printf("  [ERROR] Cannot open %s\n", tmp); return; }
    fprintf(f, "# NOTICE BOARD — IOE Pulchowk Campus\n");
    fprintf(f, "# COUNT=%d\n", NOTICE_COUNT);
    for (int i = 0; i < NOTICE_COUNT; i++)
        fprintf(f, "[%s] [%s] %s\n",
                NOTICES[i].date, NOTICES[i].category, NOTICES[i].text);
    fclose(f);
    make_backup(FILE_NOTICES);
    RENAME(tmp, FILE_NOTICES);
    printf("  [OK] Saved %d notices -> %s\n", NOTICE_COUNT, FILE_NOTICES);
    log_event("SYSTEM", "notices.txt saved");
}

static void save_ledger_entry(const char *type, const char *desc, int amount) {
    FILE *f = fopen(FILE_LEDGER, "a");
    if (!f) { log_error("SYSTEM", "Cannot append to ledger.txt"); return; }
    char dt[32]; now_str(dt, sizeof(dt));
    fprintf(f, "[%s] %-8s  Rs %-12d  %s\n", dt, type, amount, desc);
    fclose(f);
}

static void save_config(void) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s%s", FILE_CONFIG, SUFFIX_TMP);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "# IOE Pulchowk Campus — System Configuration\n");
    fprintf(f, "ORGANIZATION=Pulchowk Engineering Campus\n");
    fprintf(f, "ADMIN_NAME=IOE Pulchowk Campus\n");
    fprintf(f, "DEPARTMENT=Mechanical Engineering\n");
    fprintf(f, "PROGRAM=B.E. Mechanical (BME)\n");
    fprintf(f, "SYLLABUS_VERSION=2080\n");
    fprintf(f, "ACADEMIC_YEAR=2081/2082\n");
    fprintf(f, "LOCATION=Pulchowk, Lalitpur 44700, Nepal\n");
    fprintf(f, "PHONE=+977-1-5521034\n");
    fprintf(f, "AFFILIATED_TO=Institute of Engineering, Tribhuvan University\n");
    fprintf(f, "TAGLINE=Where the art of the possible rises.\n");
    fprintf(f, "FILE_VERSION=%d\n", FILE_VERSION);
    char dt[32]; now_str(dt, sizeof(dt));
    fprintf(f, "LAST_SAVED=%s\n", dt);
    fclose(f);
    make_backup(FILE_CONFIG);
    RENAME(tmp, FILE_CONFIG);
    printf("  [OK] Config saved -> %s\n", FILE_CONFIG);
}

/* Save everything */
static void save_all(void) {
    printf("\n  Saving all data to disk...\n");
    ensure_data_dir();
    save_students();
    save_users();
    save_results();
    save_fees();
    save_attendance();
    save_notices();
    save_config();
    log_event("SYSTEM", "Full save_all() completed");
    printf("  All data saved successfully.\n\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  LOAD FUNCTIONS  (upgraded: header validation + backup recovery)
 * ════════════════════════════════════════════════════════════════════ */

/* Generic binary loader — validates header, recovers from backup on failure */
static int load_binary_file(const char *path, unsigned int expected_magic,
                             void *data, size_t item_size, int max_items,
                             int use_pwd_xor, size_t pwd_off, size_t rec_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    FileHeader hdr;
    int ok = (fread(&hdr, sizeof(FileHeader), 1, f) == 1);
    if (ok && hdr.magic != expected_magic) {
        printf("  [WARN] '%s': wrong magic (0x%08X). Attempting recovery...\n",
               path, hdr.magic);
        fclose(f);
        log_error("SYSTEM", "Bad magic — restoring backup");
        if (restore_backup(path)) {
            printf("  [OK] Backup restored for '%s'.\n", path);
            f = fopen(path, "rb");
            if (!f) return -1;
            ok = (fread(&hdr, sizeof(FileHeader), 1, f) == 1);
        } else {
            log_error("SYSTEM", "No usable backup found");
            return -1;
        }
    }
    if (!ok || hdr.version != FILE_VERSION) {
        if (ok)
            printf("  [WARN] '%s': version mismatch (%d vs %d).\n",
                   path, hdr.version, FILE_VERSION);
        fclose(f);
        return -1;
    }

    int n = (hdr.count < max_items) ? hdr.count : max_items;
    if (n <= 0) { fclose(f); return 0; }

    if (use_pwd_xor) {
        char *ptr = (char *)data;
        int read_cnt = 0;
        for (int i = 0; i < n; i++) {
            char rec[1024];
            if (rec_size > sizeof(rec)) rec_size = sizeof(rec);
            if (fread(rec, rec_size, 1, f) != 1) break;
            /* De-obfuscate password */
            for (size_t j = pwd_off; j < pwd_off + PASSWORD_LEN && j < rec_size; j++)
                rec[j] ^= (char)PWD_XOR_KEY;
            memcpy(ptr + i * rec_size, rec, rec_size);
            read_cnt++;
        }
        fclose(f);
        return read_cnt;
    } else {
        int read_cnt = (int)fread(data, item_size, n, f);
        fclose(f);
        return read_cnt;
    }
}

static int load_students(void) {
    int n = load_binary_file(FILE_STUDENTS, MAGIC_STUDENTS,
                             ALL_STUDENTS, sizeof(Student), MAX_STUDENTS,
                             0, 0, 0);
    if (n >= 0) { STUDENT_COUNT = n; return 1; }
    return 0;
}

static int load_users(void) {
    size_t pwd_off = (size_t)((char *)USERS[0].password - (char *)&USERS[0]);
    int max = MAX_STUDENTS + MAX_STAFF + MAX_ACCOUNTS + MAX_TEACHERS + 10;
    int n = load_binary_file(FILE_USERS, MAGIC_USERS,
                             USERS, sizeof(User), max,
                             1, pwd_off, sizeof(User));
    if (n >= 0) { USER_COUNT = n; return 1; }
    return 0;
}

static int load_results(void) {
    int n = load_binary_file(FILE_RESULTS, MAGIC_RESULTS,
                             RESULTS, sizeof(StudentResult), MAX_STUDENTS,
                             0, 0, 0);
    if (n >= 0) { RESULT_COUNT = n; return 1; }
    return 0;
}

static int load_fees(void) {
    int n = load_binary_file(FILE_FEES, MAGIC_FEES,
                             FEES, sizeof(FeeRecord), MAX_STUDENTS,
                             0, 0, 0);
    if (n >= 0) { FEE_COUNT = n; return 1; }
    return 0;
}

static int load_attendance(void) {
    int n = load_binary_file(FILE_ATTENDANCE, MAGIC_ATTENDANCE,
                             ATTENDANCE, sizeof(AttendRecord), MAX_STAFF,
                             0, 0, 0);
    if (n >= 0) { ATTEND_COUNT = n; return 1; }
    return 0;
}

static int load_notices(void) {
    FILE *f = fopen(FILE_NOTICES, "r");
    if (!f) return 0;
    NOTICE_COUNT = 0;
    char line[300];
    while (fgets(line, sizeof(line), f) && NOTICE_COUNT < MAX_NOTICES) {
        if (line[0] == '#') continue;
        Notice *no = &NOTICES[NOTICE_COUNT];
        char *p = line;
        if (*p != '[') continue;
        p++;
        int di = 0;
        while (*p && *p != ']' && di < 15) no->date[di++] = *p++;
        no->date[di] = '\0';
        if (*p == ']') p++;
        while (*p == ' ') p++;
        if (*p == '[') {
            p++;
            int ci = 0;
            while (*p && *p != ']' && ci < 23) no->category[ci++] = *p++;
            no->category[ci] = '\0';
            if (*p == ']') p++;
            while (*p == ' ') p++;
        }
        int ti = 0;
        while (*p && *p != '\n' && ti < 199) no->text[ti++] = *p++;
        no->text[ti] = '\0';
        if (no->text[0]) NOTICE_COUNT++;
    }
    fclose(f);
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 *  DATA GENERATION  (original logic, fully preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void make_students(void) {
    STUDENT_COUNT = 0;
    struct {
        int year; int sem1; int sem2;
        const char *prefix; const char *year_label;
    } batches[] = {
        {1, 1, 1, "082BME", "1st Year"},
        {2, 1, 1, "081BME", "2nd Year"},
        {3, 1, 0, "080BME", "3rd Year"},
        {3, 0, 1, "079BME", "3rd Year"},
        {4, 1, 1, "078BME", "4th Year"},
    };
    int nb = (int)(sizeof(batches) / sizeof(batches[0]));
    for (int b = 0; b < nb; b++) {
        for (int i = 1; i <= 48; i++) {
            Student *s = &ALL_STUDENTS[STUDENT_COUNT++];
            char tmp[USERNAME_LEN];
            snprintf(tmp, USERNAME_LEN, "%s%02d", batches[b].prefix, i);
            strncpy(s->username, tmp, USERNAME_LEN - 1);
            snprintf(s->password, PASSWORD_LEN, "%s123", tmp);
            strncpy(s->name,      tmp,                   NAME_LEN - 1);
            strncpy(s->role,      "student",              ROLE_LEN - 1);
            s->year = batches[b].year;
            strncpy(s->prefix,     batches[b].prefix,     15);
            strncpy(s->year_label, batches[b].year_label, LABEL_LEN - 1);
            int sc = 0;
            if (batches[b].sem1) s->valid_sems[sc++] = 1;
            if (batches[b].sem2) s->valid_sems[sc++] = 2;
            s->valid_sem_count = sc;
        }
    }
}

static void make_users(void) {
    USER_COUNT = 0;

    User *u = &USERS[USER_COUNT++];
    snprintf(u->username, USERNAME_LEN, "Pulchowk");
    snprintf(u->password, PASSWORD_LEN, "Pulchowk123");
    snprintf(u->name,     NAME_LEN,     "IOE Pulchowk Campus");
    strncpy(u->role,    "admin",         ROLE_LEN - 1);
    strncpy(u->display, "Administrator", LABEL_LEN - 1);
    strncpy(u->phone,   "+977-1-5521034", 19);
    strncpy(u->joined,  "2066-04-01",    15);

    u = &USERS[USER_COUNT++];
    snprintf(u->username, USERNAME_LEN, "Biraj");
    snprintf(u->password, PASSWORD_LEN, "Biraj123");
    snprintf(u->name,     NAME_LEN,     "Biraj Sharma");
    strncpy(u->role,    "teacher",       ROLE_LEN - 1);
    strncpy(u->display, "Teacher",       LABEL_LEN - 1);
    strncpy(u->extra,   "Thermodynamics & Fluid Mechanics", 127);
    strncpy(u->phone,   "+977-1-5521050", 19);
    strncpy(u->joined,  "2074-07-15",    15);

    u = &USERS[USER_COUNT++];
    snprintf(u->username, USERNAME_LEN, "teacher123");
    snprintf(u->password, PASSWORD_LEN, "teacher123");
    snprintf(u->name,     NAME_LEN,     "Biraj Sharma");
    strncpy(u->role,    "teacher",       ROLE_LEN - 1);
    strncpy(u->display, "Teacher",       LABEL_LEN - 1);
    strncpy(u->extra,   "Thermodynamics & Fluid Mechanics", 127);
    strncpy(u->phone,   "+977-1-5521051", 19);
    strncpy(u->joined,  "2074-07-15",    15);

    for (int i = 1; i <= 20; i++) {
        u = &USERS[USER_COUNT++];
        snprintf(u->username, USERNAME_LEN, "staff%d",  i);
        snprintf(u->password, PASSWORD_LEN, "staff%d123", i);
        snprintf(u->name,     NAME_LEN,     "Staff Member %d", i);
        strncpy(u->role,    "staff",   ROLE_LEN - 1);
        strncpy(u->display, "Staff",   LABEL_LEN - 1);
        strncpy(u->extra,   STAFF_ROLES[i-1], 127);
        strncpy(u->phone,   STAFF_PHONES[i-1], 19);
        snprintf(u->joined, 15, "207%d-0%d-01", (i % 5) + 1, (i % 9) + 1);
    }

    const char *an[] = {"Ram Prasad Shrestha","Sita Kumari Joshi","Hari Bahadur Thapa"};
    const char *ad[] = {"Senior Accountant","Accountant","Junior Accountant"};
    const char *ap[] = {"+977-1-5521034","+977-1-5521035","+977-1-5521036"};
    const char *aj[] = {"2067-04-12","2070-07-03","2074-02-18"};
    for (int i = 1; i <= 3; i++) {
        u = &USERS[USER_COUNT++];
        snprintf(u->username, USERNAME_LEN, "accountant%d",   i);
        snprintf(u->password, PASSWORD_LEN, "accountant%d123", i);
        snprintf(u->name,     NAME_LEN,     "%s", an[i-1]);
        strncpy(u->role,    "accountant",   ROLE_LEN - 1);
        strncpy(u->display, "Accountant",   LABEL_LEN - 1);
        strncpy(u->extra,   ad[i-1],        127);
        strncpy(u->phone,   ap[i-1],        19);
        strncpy(u->joined,  aj[i-1],        15);
    }

    for (int i = 0; i < STUDENT_COUNT; i++) {
        u = &USERS[USER_COUNT++];
        strncpy(u->username, ALL_STUDENTS[i].username, USERNAME_LEN - 1);
        strncpy(u->password, ALL_STUDENTS[i].password, PASSWORD_LEN - 1);
        strncpy(u->name,     ALL_STUDENTS[i].username, NAME_LEN - 1);
        strncpy(u->role,     "student",                ROLE_LEN - 1);
        strncpy(u->display,  "Student",                LABEL_LEN - 1);
    }
}

static void generate_results(void) {
    RESULT_COUNT = 0;
    for (int i = 0; i < STUDENT_COUNT; i++) {
        StudentResult *r = &RESULTS[RESULT_COUNT++];
        strncpy(r->username, ALL_STUDENTS[i].username, USERNAME_LEN - 1);
        memset(r->results, 0, sizeof(r->results));
        for (int vs = 0; vs < ALL_STUDENTS[i].valid_sem_count; vs++) {
            int sem  = ALL_STUDENTS[i].valid_sems[vs];
            int year = ALL_STUDENTS[i].year;
            int nsub = SUBJECT_COUNT[year-1][sem-1];
            lcg_init(username_seed(ALL_STUDENTS[i].username, sem));
            lcg_next();
            SemResult *sr = &r->results[year-1][sem-1];
            sr->count = nsub;
            for (int s = 0; s < nsub; s++) {
                int fm  = SUBJECTS[year-1][sem-1][s].total_fm;
                lcg_next();
                int min_m = (int)(fm * 0.40);
                int range = (int)(fm * 0.55);
                sr->marks[s] = min_m + (int)(lcg_rand() * range);
                if (sr->marks[s] > fm) sr->marks[s] = fm;
            }
        }
    }
}

static void generate_fees(void) {
    FEE_COUNT = 0;
    char dt[16]; today_str(dt, sizeof(dt));
    for (int i = 0; i < STUDENT_COUNT; i++) {
        FeeRecord *f = &FEES[FEE_COUNT++];
        strncpy(f->username, ALL_STUDENTS[i].username, USERNAME_LEN - 1);
        f->year = ALL_STUDENTS[i].year;
        lcg_init(username_seed(ALL_STUDENTS[i].username, 99));
        lcg_next();
        f->due  = 45000 + (int)(lcg_rand() * 20000);
        lcg_next();
        double r = lcg_rand();
        f->paid = (r > 0.85) ? f->due : (int)(r * f->due);
        if (f->paid >= f->due)  strncpy(f->status, "PAID",    11);
        else if (f->paid > 0)   strncpy(f->status, "PARTIAL", 11);
        else                    strncpy(f->status, "DUE",     11);
        strncpy(f->last_payment, dt, 15);
    }
}

static void generate_attendance(void) {
    ATTEND_COUNT = 0;
    char dt[16]; today_str(dt, sizeof(dt));
    const char *statuses[] = {
        "Present","Present","Present","Absent","Present",
        "Leave","Present","Present","Absent","Present",
        "Present","Present","Leave","Present","Present",
        "Present","Absent","Present","Present","Present"
    };
    int sc = 0;
    for (int i = 0; i < USER_COUNT && ATTEND_COUNT < MAX_STAFF; i++) {
        if (strcmp(USERS[i].role, "staff") == 0 && sc < 20) {
            AttendRecord *a = &ATTENDANCE[ATTEND_COUNT++];
            strncpy(a->username, USERS[i].username, USERNAME_LEN - 1);
            strncpy(a->status,   statuses[sc++],    11);
            strncpy(a->date,     dt,                15);
        }
    }
}

static void generate_default_notices(void) {
    NOTICE_COUNT = 0;
    struct { const char *d; const char *cat; const char *t; } nd[] = {
        {"2082-02-01","EXAM",    "Semester exam schedule released for all years"},
        {"2082-01-28","ADMIN",   "Staff meeting scheduled — Friday 3:00 PM"},
        {"2082-01-25","GENERAL", "Annual sports day preparations underway"},
        {"2082-01-20","ACADEMIC","Library extended hours during exam week"},
        {"2082-01-15","EXAM",    "Exam form submission deadline: 2082-01-25"},
        {"2082-01-10","ACADEMIC","New IOE syllabus 2080 fully implemented this semester"},
        {"2082-01-05","ADMIN",   "Scholarship applications open — apply at admin office"},
    };
    int nn = (int)(sizeof(nd) / sizeof(nd[0]));
    for (int i = 0; i < nn && i < MAX_NOTICES; i++) {
        strncpy(NOTICES[i].date,     nd[i].d,   15);
        strncpy(NOTICES[i].category, nd[i].cat, 23);
        strncpy(NOTICES[i].text,     nd[i].t,   199);
        NOTICE_COUNT++;
    }
}

static void generate_default_ledger(void) {
    FILE *f = fopen(FILE_LEDGER, "r");
    if (f) { fclose(f); return; }
    f = fopen(FILE_LEDGER, "w");
    if (!f) return;
    fprintf(f, "# FINANCIAL LEDGER — IOE Pulchowk Campus\n");
    fprintf(f, "# Format: [DATETIME] TYPE  Rs AMOUNT  Description\n");
    fprintf(f, "# ============================================================\n");
    struct { const char *type; const char *desc; int amt; } entries[] = {
        {"CREDIT","Tuition Fee Collection — Year 1 Batch",  21600000},
        {"CREDIT","Tuition Fee Collection — Year 2 Batch",  19800000},
        {"CREDIT","Tuition Fee Collection — Year 3 Batch",  14400000},
        {"CREDIT","Tuition Fee Collection — Year 4 Batch",   9600000},
        {"CREDIT","Examination Fee — All Years",             5760000},
        {"CREDIT","Library Fund Collection",                  480000},
        {"DEBIT", "Salary — Teaching Staff",                1200000},
        {"DEBIT", "Salary — Administrative Staff",           600000},
        {"DEBIT", "Lab Equipment Purchase",                  350000},
        {"DEBIT", "Library Subscription Renewal",             85000},
        {"DEBIT", "Building Maintenance",                    420000},
        {"DEBIT", "Examination Expenses",                    250000},
    };
    int n = (int)(sizeof(entries) / sizeof(entries[0]));
    for (int i = 0; i < n; i++)
        fprintf(f, "[2082-01-%02d 10:00:00] %-8s  Rs %-12d  %s\n",
                i + 1, entries[i].type, entries[i].amt, entries[i].desc);
    fclose(f);
}

/* ════════════════════════════════════════════════════════════════════
 *  LOOKUP HELPERS  (originals preserved)
 * ════════════════════════════════════════════════════════════════════ */

static User *find_user(const char *un) {
    for (int i = 0; i < USER_COUNT; i++)
        if (strcmp(USERS[i].username, un) == 0) return &USERS[i];
    return NULL;
}

static Student *find_student(const char *un) {
    for (int i = 0; i < STUDENT_COUNT; i++)
        if (strcmp(ALL_STUDENTS[i].username, un) == 0) return &ALL_STUDENTS[i];
    return NULL;
}

static StudentResult *find_result(const char *un) {
    for (int i = 0; i < RESULT_COUNT; i++)
        if (strcmp(RESULTS[i].username, un) == 0) return &RESULTS[i];
    return NULL;
}

static FeeRecord *find_fee(const char *un) {
    for (int i = 0; i < FEE_COUNT; i++)
        if (strcmp(FEES[i].username, un) == 0) return &FEES[i];
    return NULL;
}

static int student_valid_sem(const Student *s, int sem) {
    for (int i = 0; i < s->valid_sem_count; i++)
        if (s->valid_sems[i] == sem) return 1;
    return 0;
}

/* NEW: check duplicate username in USERS */
static int username_exists(const char *un) {
    return find_user(un) != NULL;
}

/* ════════════════════════════════════════════════════════════════════
 *  CALCULATION HELPERS  (originals preserved)
 * ════════════════════════════════════════════════════════════════════ */

static int compute_total(const SemResult *sr) {
    int t = 0;
    for (int i = 0; i < sr->count; i++) t += sr->marks[i];
    return t;
}

static double compute_pct(const int year, const int sem, const SemResult *sr) {
    if (!sr || sr->count == 0) return 0.0;
    int grand = 0;
    for (int i = 0; i < sr->count; i++)
        grand += SUBJECTS[year-1][sem-1][i].total_fm;
    if (grand == 0) return 0.0;
    return (double)compute_total(sr) / (double)grand * 100.0;
}

/* NEW: cumulative GPA across all valid semesters */
static double compute_cumulative_gpa(const Student *s) {
    double sum = 0.0; int cnt = 0;
    StudentResult *r = find_result(s->username);
    if (!r) return 0.0;
    for (int v = 0; v < s->valid_sem_count; v++) {
        int sem = s->valid_sems[v];
        SemResult *sr = &r->results[s->year-1][sem-1];
        if (sr && sr->count > 0) {
            sum += get_gpa(compute_pct(s->year, sem, sr));
            cnt++;
        }
    }
    return cnt ? sum / cnt : 0.0;
}

/* ════════════════════════════════════════════════════════════════════
 *  RESULT CARD PRINTER  (original, preserved exactly)
 * ════════════════════════════════════════════════════════════════════ */

static void print_result_card(const Student *s, int year, int sem) {
    StudentResult *r = find_result(s->username);
    if (!r) { printf("  No result data found.\n"); return; }

    SemResult *sr = &r->results[year-1][sem-1];
    if (!sr || sr->count == 0) {
        printf("  No result for Year %d Sem %d.\n", year, sem);
        return;
    }

    int total = compute_total(sr);
    int grand = 0;
    for (int i = 0; i < sr->count; i++)
        grand += SUBJECTS[year-1][sem-1][i].total_fm;
    double pct   = (grand > 0) ? ((double)total / grand * 100.0) : 0.0;
    double gpa   = get_gpa(pct);
    const char *grade = get_grade(pct);
    char dt[32]; today_str(dt, sizeof(dt));

    printf("\n");
    sep('=', 76);
    printf("           TRIBHUVAN UNIVERSITY — IOE PULCHOWK CAMPUS\n");
    printf("              MECHANICAL ENGINEERING DEPARTMENT\n");
    printf("              OFFICIAL MARK SHEET — Academic Year 2081/2082\n");
    sep('=', 76);
    printf("  Roll No.     : %-20s   Year     : %s (Year %d)\n",
           s->username, s->year_label, year);
    printf("  Name         : %-20s   Semester : %d\n", s->name, sem);
    printf("  Batch        : %-20s   Program  : B.E. Mechanical (BME)\n", s->prefix);
    printf("  Date Issued  : %-20s   Max Marks: %d\n", dt, grand);
    sep('-', 76);
    printf("  %-3s  %-4s  %-44s  %3s  %3s  %3s  %3s  %-4s  %-7s\n",
           "SN", "Code", "Subject Name", "T.F", "I.F", "P.F", "Obt", "Full", "Grade");
    sep('-', 76);

    int pass_all = 1;
    for (int i = 0; i < sr->count; i++) {
        const SubjectInfo *si = &SUBJECTS[year-1][sem-1][i];
        if (si->total_fm == 0) break;
        int    m    = sr->marks[i];
        double sp   = (si->total_fm > 0) ? (double)m / si->total_fm * 100.0 : 0.0;
        int    pass = (m >= (int)(si->total_fm * 0.40));
        if (!pass) pass_all = 0;
        printf("  %-3d  %-7s  %-40s  %3d  %3d  %3d  %3d  %4d  %-6s  %s\n",
               i + 1, si->code,
               (strlen(si->name) > 40) ? "..." : si->name,
               si->theory_fm, si->internal_fm, si->practical_fm,
               m, si->total_fm, get_grade(sp),
               pass ? "[P]" : "[F]");
    }
    sep('-', 76);
    printf("  %-3s  %-7s  %-40s  %3s  %3s  %3s  %3d  %4d  %-6s  %s\n",
           " ", " ", "TOTAL", " ", " ", " ", total, grand, grade,
           pass_all ? "[PASS]" : "[FAIL]");
    sep('=', 76);
    printf("  Percentage : %6.2f%%     Grade : %-4s     GPA : %.2f / 4.00\n",
           pct, grade, gpa);
    printf("  Result     : %s\n",
           (pct >= 40.0 && pass_all) ? "  *** PASS ***  " : "  --- FAIL ---  ");
    sep('-', 76);
    printf("  Exam Controller             Head of Dept.            Principal\n");
    printf("  _____________________       _________________        _________________\n");
    sep('=', 76);
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  RANKING HELPERS  (originals preserved)
 * ════════════════════════════════════════════════════════════════════ */

static int rank_cmp(const void *a, const void *b) {
    const RankRow *ra = (const RankRow *)a;
    const RankRow *rb = (const RankRow *)b;
    if (rb->pct > ra->pct) return  1;
    if (rb->pct < ra->pct) return -1;
    return 0;
}

static void build_rankings(int year, int sem, RankRow *rows, int *cnt) {
    *cnt = 0;
    for (int i = 0; i < STUDENT_COUNT; i++) {
        Student *s = &ALL_STUDENTS[i];
        if (s->year != year || !student_valid_sem(s, sem)) continue;
        StudentResult *r = find_result(s->username);
        if (!r) continue;
        SemResult *sr = &r->results[year-1][sem-1];
        if (!sr || sr->count == 0) continue;
        rows[*cnt].pct   = compute_pct(year, sem, sr);
        rows[*cnt].total = compute_total(sr);
        rows[*cnt].grade = get_grade(rows[*cnt].pct);
        strncpy(rows[*cnt].username, s->username, USERNAME_LEN - 1);
        (*cnt)++;
    }
    qsort(rows, *cnt, sizeof(RankRow), rank_cmp);
}

static void print_ranking_table(int year, int sem) {
    static RankRow rows[MAX_STUDENTS];
    int cnt = 0;
    build_rankings(year, sem, rows, &cnt);
    if (cnt == 0) { printf("  No students found.\n"); return; }

    const char *medals[] = {"[GOLD]","[SILV]","[BRNZ]"};
    printf("\n");
    sep('=', 72);
    printf("  RANKINGS — Year %d  Semester %d  (%d students)\n", year, sem, cnt);
    sep('=', 72);
    printf("  %-6s  %-14s  %6s  %8s  %5s\n",
           "Rank", "Roll No.", "Total", "  %  ", "Grade");
    sep('-', 72);
    for (int i = 0; i < cnt; i++) {
        if (i < 3)
            printf("  %-6s  %-14s  %6d  %7.2f%%  %-5s\n",
                   medals[i], rows[i].username,
                   rows[i].total, rows[i].pct, rows[i].grade);
        else
            printf("  #%-5d  %-14s  %6d  %7.2f%%  %-5s\n",
                   i + 1, rows[i].username,
                   rows[i].total, rows[i].pct, rows[i].grade);
    }
    sep('=', 72);
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  STUDENT TABLE  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void print_student_table(int filter_year, const char *search) {
    char sl[USERNAME_LEN] = "";
    if (search && search[0]) {
        strncpy(sl, search, USERNAME_LEN - 1);
        str_lower(sl);
    }
    int count = 0;
    printf("\n");
    sep('=', 72);
    printf("  STUDENT DIRECTORY — BE Mechanical Engineering\n");
    sep('=', 72);
    printf("  %-4s  %-14s  %-12s  %-8s  %-12s\n",
           "#", "Roll No.", "Year", "Sem(s)", "Batch");
    sep('-', 72);
    for (int i = 0; i < STUDENT_COUNT; i++) {
        Student *s = &ALL_STUDENTS[i];
        if (filter_year > 0 && s->year != filter_year) continue;
        if (sl[0]) {
            char ul[USERNAME_LEN];
            strncpy(ul, s->username, USERNAME_LEN - 1);
            str_lower(ul);
            if (!strstr(ul, sl)) continue;
        }
        count++;
        char sems[8] = "";
        for (int v = 0; v < s->valid_sem_count; v++) {
            if (v) strcat(sems, "&");
            char tmp[4];
            snprintf(tmp, sizeof(tmp), "%d", s->valid_sems[v]);
            strcat(sems, tmp);
        }
        printf("  %-4d  %-14s  %-12s  %-8s  %-12s\n",
               count, s->username, s->year_label, sems, s->prefix);
    }
    sep('=', 72);
    printf("  Total shown: %d\n\n", count);
}

/* ════════════════════════════════════════════════════════════════════
 *  SEARCH  (original preserved + enhanced output)
 * ════════════════════════════════════════════════════════════════════ */

static void search_student(void) {
    print_panel("SEARCH STUDENT");
    printf("  Enter Roll Number (or part): "); fflush(stdout);
    char q[USERNAME_LEN];
    read_line(q, sizeof(q));
    if (!q[0]) { printf("  No input.\n"); return; }

    char ql[USERNAME_LEN];
    strncpy(ql, q, USERNAME_LEN - 1);
    str_lower(ql);

    int found = 0;
    for (int i = 0; i < STUDENT_COUNT; i++) {
        Student *s = &ALL_STUDENTS[i];
        char ul[USERNAME_LEN];
        strncpy(ul, s->username, USERNAME_LEN - 1);
        str_lower(ul);
        if (!strstr(ul, ql)) continue;
        found++;
        printf("\n");
        sep('-', 60);
        printf("  Roll No.     : %s\n",     s->username);
        printf("  Year         : %s (Year %d)\n", s->year_label, s->year);
        printf("  Semesters    : ");
        for (int v = 0; v < s->valid_sem_count; v++) {
            if (v) printf(" & ");
            printf("Sem %d", s->valid_sems[v]);
        }
        printf("\n  Batch        : %s\n", s->prefix);
        double cgpa = compute_cumulative_gpa(s);
        printf("  Cumul. GPA   : %.2f / 4.00\n", cgpa);

        StudentResult *r = find_result(s->username);
        if (r) {
            for (int vs = 0; vs < s->valid_sem_count; vs++) {
                int sem = s->valid_sems[vs];
                SemResult *sr = &r->results[s->year-1][sem-1];
                if (sr && sr->count > 0) {
                    double pct = compute_pct(s->year, sem, sr);
                    printf("  Sem %d        : %d marks  %.2f%%  Grade: %s\n",
                           sem, compute_total(sr), pct, get_grade(pct));
                }
            }
        }
        FeeRecord *f = find_fee(s->username);
        if (f)
            printf("  Fee Status   : Rs %d / %d  [%s]\n",
                   f->paid, f->due, f->status);
        sep('-', 60);
    }
    if (!found) printf("  No student found matching '%s'.\n", q);
}

/* ════════════════════════════════════════════════════════════════════
 *  BATCH TOPPERS  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void print_batch_toppers(void) {
    print_panel("BATCH TOPPERS — Hall of Fame");

    struct { int year; int sem; char label[LABEL_LEN]; } cfg[] = {
        {1,1,"1st Year — Semester 1"},   {1,2,"1st Year — Semester 2"},
        {2,1,"2nd Year — Semester 1"},   {2,2,"2nd Year — Semester 2"},
        {3,1,"3rd Year — Sem 1 (080BME)"},{3,2,"3rd Year — Sem 2 (079BME)"},
        {4,1,"4th Year — Semester 1"},   {4,2,"4th Year — Semester 2"},
    };
    int nc = (int)(sizeof(cfg) / sizeof(cfg[0]));

    typedef struct { char u[USERNAME_LEN]; double pct; const char *lbl; } TE;
    static TE all_rows[MAX_STUDENTS * 2];
    int all_cnt = 0;

    for (int c = 0; c < nc; c++) {
        for (int i = 0; i < STUDENT_COUNT; i++) {
            Student *s = &ALL_STUDENTS[i];
            if (s->year != cfg[c].year || !student_valid_sem(s, cfg[c].sem)) continue;
            StudentResult *r = find_result(s->username);
            if (!r) continue;
            SemResult *sr = &r->results[cfg[c].year-1][cfg[c].sem-1];
            if (!sr || sr->count == 0) continue;
            strncpy(all_rows[all_cnt].u, s->username, USERNAME_LEN - 1);
            all_rows[all_cnt].pct = compute_pct(cfg[c].year, cfg[c].sem, sr);
            all_rows[all_cnt].lbl = cfg[c].label;
            all_cnt++;
        }
    }
    for (int i = 0; i < all_cnt - 1; i++)
        for (int j = i + 1; j < all_cnt; j++)
            if (all_rows[j].pct > all_rows[i].pct) {
                TE tmp = all_rows[i]; all_rows[i] = all_rows[j]; all_rows[j] = tmp;
            }

    const char *medals[] = {"[GOLD] #1","[SILV] #2","[BRNZ] #3"};
    printf("  *** OVERALL TOP 3 ACROSS ALL BATCHES ***\n\n");
    for (int i = 0; i < 3 && i < all_cnt; i++) {
        printf("  %s  %-14s  %.2f%%  Grade: %s\n  Batch: %s\n\n",
               medals[i], all_rows[i].u, all_rows[i].pct,
               get_grade(all_rows[i].pct), all_rows[i].lbl);
    }

    sep('=', 72);
    printf("  PER-SEMESTER TOPPERS (Top 3 per group)\n");
    sep('=', 72);

    for (int c = 0; c < nc; c++) {
        static RankRow rows[MAX_STUDENTS];
        int cnt = 0;
        build_rankings(cfg[c].year, cfg[c].sem, rows, &cnt);
        printf("\n  %s\n", cfg[c].label);
        sep('-', 60);
        if (cnt == 0) { printf("  No data.\n"); continue; }
        int show = cnt < 3 ? cnt : 3;
        for (int i = 0; i < show; i++)
            printf("  %s  %-14s  %.2f%%  %s\n",
                   medals[i], rows[i].username, rows[i].pct, rows[i].grade);
    }
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  ADD / EDIT RESULT  (original, preserved + log)
 * ════════════════════════════════════════════════════════════════════ */

static void edit_result(void) {
    print_panel("ADD / EDIT STUDENT RESULT");
    printf("  Enter Roll Number: "); fflush(stdout);
    char uname[USERNAME_LEN];
    read_line(uname, sizeof(uname));

    Student *s = find_student(uname);
    if (!s) { printf("  ERROR: Student '%s' not found.\n", uname); return; }
    StudentResult *r = find_result(uname);
    if (!r) { printf("  ERROR: Result record missing.\n"); return; }

    printf("  Student: %s | Year: %d | Valid Sem(s): ", s->username, s->year);
    for (int v = 0; v < s->valid_sem_count; v++) {
        if (v) printf(", ");
        printf("%d", s->valid_sems[v]);
    }
    printf("\n  Select Semester [");
    for (int v = 0; v < s->valid_sem_count; v++) {
        if (v) printf("/");
        printf("%d", s->valid_sems[v]);
    }
    printf("]: "); fflush(stdout);
    int sem = read_int();
    if (!student_valid_sem(s, sem)) {
        printf("  ERROR: Semester %d not valid.\n", sem); return;
    }

    SemResult *sr = &r->results[s->year-1][sem-1];
    int nsub = SUBJECT_COUNT[s->year-1][sem-1];
    sr->count = nsub;

    printf("\n  Enter marks for Year %d Sem %d (0 – Full Marks):\n", s->year, sem);
    for (int i = 0; i < nsub; i++) {
        const SubjectInfo *si = &SUBJECTS[s->year-1][sem-1][i];
        printf("  %-44s [/%d] (current: %d): ",
               si->name, si->total_fm, sr->marks[i]);
        fflush(stdout);
        int m = read_int();
        if (m < 0 || m > si->total_fm) {
            printf("  Invalid — keeping %d\n", sr->marks[i]);
        } else {
            sr->marks[i] = m;
        }
    }
    printf("\n  Result updated successfully!\n");
    save_results();

    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg),
             "Result edited: student=%s year=%d sem=%d", uname, s->year, sem);
    log_event(current_user.username, logmsg);

    print_result_card(s, s->year, sem);
}

/* ════════════════════════════════════════════════════════════════════
 *  RESULTS OVERVIEW  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void results_overview(void) {
    print_panel("RESULTS OVERVIEW");
    printf("  Select Year [1-4]: "); fflush(stdout);
    int year = read_int();
    if (year < 1 || year > 4) { printf("  Invalid year.\n"); return; }
    printf("  Select Semester [1/2]: "); fflush(stdout);
    int sem = read_int();
    if (sem < 1 || sem > 2) { printf("  Invalid semester.\n"); return; }

    static RankRow rows[MAX_STUDENTS];
    int cnt = 0;
    build_rankings(year, sem, rows, &cnt);
    if (cnt == 0) { printf("  No data for Year %d Sem %d.\n", year, sem); return; }

    int nsub = SUBJECT_COUNT[year-1][sem-1];
    printf("\n");
    sep('=', 80);
    printf("  YEAR %d — SEMESTER %d   (%d students)\n", year, sem, cnt);
    sep('=', 80);
    printf("  %-5s  %-14s", "Rank", "Roll No.");
    for (int i = 0; i < nsub; i++) {
        const char *last = strrchr(SUBJECTS[year-1][sem-1][i].name, ' ');
        printf("  %-9s", last ? last + 1 : SUBJECTS[year-1][sem-1][i].name);
    }
    printf("  %6s  %7s  %5s\n", "Total", "  %  ", "Grade");
    sep('-', 80);

    for (int i = 0; i < cnt; i++) {
        StudentResult *r = find_result(rows[i].username);
        printf("  %-5d  %-14s", i + 1, rows[i].username);
        if (r) {
            SemResult *sr = &r->results[year-1][sem-1];
            for (int s2 = 0; s2 < nsub; s2++)
                printf("  %-9d", sr->marks[s2]);
        }
        printf("  %6d  %6.2f%%  %-5s\n",
               rows[i].total, rows[i].pct, rows[i].grade);
    }
    sep('=', 80);
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  REPORT CARDS BROWSER  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void browse_report_cards(void) {
    print_panel("REPORT CARDS");
    printf("  Enter Roll Number (e.g. 082BME01): "); fflush(stdout);
    char uname[USERNAME_LEN];
    read_line(uname, sizeof(uname));
    Student *s = find_student(uname);
    if (!s) { printf("  Student not found.\n"); return; }
    for (int v = 0; v < s->valid_sem_count; v++)
        print_result_card(s, s->year, s->valid_sems[v]);
}

/* ════════════════════════════════════════════════════════════════════
 *  TEACHERS / STAFF / ACCOUNTS VIEWS  (originals preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void show_teachers(void) {
    print_panel("TEACHERS");
    sep('=', 72);
    printf("  %-3s  %-12s  %-24s  %-20s  %-16s\n",
           "#", "Username", "Name", "Subject/Specialization", "Phone");
    sep('-', 72);
    int cnt = 0;
    for (int i = 0; i < USER_COUNT; i++) {
        if (strcmp(USERS[i].role, "teacher") == 0)
            printf("  %-3d  %-12s  %-24s  %-20s  %-16s\n",
                   ++cnt, USERS[i].username, USERS[i].name,
                   USERS[i].extra, USERS[i].phone);
    }
    sep('=', 72);
    printf("\n");
}

static void show_staff_and_accounts(void) {
    print_panel("STAFF & ACCOUNTANTS");
    printf("  STAFF MEMBERS (20)\n\n");
    sep('-', 72);
    printf("  %-3s  %-12s  %-24s  %-22s  %-16s\n",
           "#", "Username", "Name", "Job Title", "Phone");
    sep('-', 72);
    int cnt = 0;
    for (int i = 0; i < USER_COUNT; i++) {
        if (strcmp(USERS[i].role, "staff") == 0)
            printf("  %-3d  %-12s  %-24s  %-22s  %-16s\n",
                   ++cnt, USERS[i].username, USERS[i].name,
                   USERS[i].extra, USERS[i].phone);
    }
    sep('-', 72);
    printf("\n  ACCOUNTANTS (3)\n\n");
    sep('-', 72);
    printf("  %-3s  %-14s  %-24s  %-22s  %-16s\n",
           "#", "Username", "Name", "Designation", "Phone");
    sep('-', 72);
    cnt = 0;
    for (int i = 0; i < USER_COUNT; i++) {
        if (strcmp(USERS[i].role, "accountant") == 0)
            printf("  %-3d  %-14s  %-24s  %-22s  %-16s\n",
                   ++cnt, USERS[i].username, USERS[i].name,
                   USERS[i].extra, USERS[i].phone);
    }
    sep('-', 72);
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  FEE OVERVIEW  (original preserved + sort by outstanding)
 * ════════════════════════════════════════════════════════════════════ */

static void fee_overview(void) {
    print_panel("FEE OVERVIEW");
    printf("  [1]  View all fees\n");
    printf("  [2]  View by Year\n");
    printf("  [3]  Update payment for a student\n");
    printf("  [4]  Fee summary statistics\n");
    printf("  [5]  View sorted by outstanding (highest first)\n");
    printf("  Choice: "); fflush(stdout);
    int ch = read_int();

    if (ch == 1 || ch == 2) {
        int fy = 0;
        if (ch == 2) {
            printf("  Filter Year [1-4]: "); fflush(stdout);
            fy = read_int();
        }
        sep('=', 76);
        printf("  %-14s  %-6s  %-12s  %-12s  %-8s  %-12s\n",
               "Roll No.", "Year", "Due (Rs)", "Paid (Rs)", "Status", "Last Payment");
        sep('-', 76);
        int total_due = 0, total_paid = 0;
        for (int i = 0; i < FEE_COUNT; i++) {
            if (fy > 0 && FEES[i].year != fy) continue;
            printf("  %-14s  %-6d  %-12d  %-12d  %-8s  %-12s\n",
                   FEES[i].username, FEES[i].year,
                   FEES[i].due, FEES[i].paid,
                   FEES[i].status, FEES[i].last_payment);
            total_due  += FEES[i].due;
            total_paid += FEES[i].paid;
        }
        sep('=', 76);
        printf("  Total Due: Rs %d   Total Paid: Rs %d   Balance: Rs %d\n\n",
               total_due, total_paid, total_due - total_paid);

    } else if (ch == 3) {
        printf("  Enter Roll Number: "); fflush(stdout);
        char uname[USERNAME_LEN]; read_line(uname, sizeof(uname));
        FeeRecord *f = find_fee(uname);
        if (!f) { printf("  Record not found.\n"); return; }
        printf("  Due: Rs %d  |  Current Paid: Rs %d  |  Status: %s\n",
               f->due, f->paid, f->status);
        printf("  Enter new paid amount (Rs): "); fflush(stdout);
        int np = read_int();
        if (np < 0 || np > f->due) { printf("  Invalid amount.\n"); return; }
        f->paid = np;
        if (f->paid >= f->due) strncpy(f->status, "PAID",    11);
        else if (f->paid > 0)  strncpy(f->status, "PARTIAL", 11);
        else                   strncpy(f->status, "DUE",     11);
        char dt[16]; today_str(dt, sizeof(dt));
        strncpy(f->last_payment, dt, 15);
        save_fees();
        char desc[128];
        snprintf(desc, sizeof(desc), "Fee Payment — %s (Rs %d)", uname, np);
        save_ledger_entry("CREDIT", desc, np);
        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg), "Fee updated: %s paid Rs %d status=%s",
                 uname, np, f->status);
        log_event(current_user.username, logmsg);
        printf("  Payment updated and saved.\n");

    } else if (ch == 4) {
        int paid_cnt = 0, partial_cnt = 0, due_cnt = 0;
        long long total_d = 0, total_p = 0;
        for (int i = 0; i < FEE_COUNT; i++) {
            total_d += FEES[i].due; total_p += FEES[i].paid;
            if      (!strcmp(FEES[i].status, "PAID"))    paid_cnt++;
            else if (!strcmp(FEES[i].status, "PARTIAL")) partial_cnt++;
            else                                         due_cnt++;
        }
        printf("\n  Total Students : %d\n", FEE_COUNT);
        printf("  Fully Paid     : %d (%.1f%%)\n",
               paid_cnt, FEE_COUNT ? 100.0 * paid_cnt / FEE_COUNT : 0.0);
        printf("  Partial        : %d (%.1f%%)\n",
               partial_cnt, FEE_COUNT ? 100.0 * partial_cnt / FEE_COUNT : 0.0);
        printf("  Due/Pending    : %d (%.1f%%)\n",
               due_cnt, FEE_COUNT ? 100.0 * due_cnt / FEE_COUNT : 0.0);
        printf("  Total Due      : Rs %lld\n",  total_d);
        printf("  Total Paid     : Rs %lld\n",  total_p);
        printf("  Outstanding    : Rs %lld\n\n", total_d - total_p);

    } else if (ch == 5) {
        /* Sort fee records by outstanding (due - paid) descending */
        static FeeRecord sorted[MAX_STUDENTS];
        int sn = FEE_COUNT < MAX_STUDENTS ? FEE_COUNT : MAX_STUDENTS;
        memcpy(sorted, FEES, sn * sizeof(FeeRecord));
        /* Bubble sort by outstanding */
        for (int i = 0; i < sn - 1; i++)
            for (int j = i + 1; j < sn; j++) {
                int oi = sorted[i].due - sorted[i].paid;
                int oj = sorted[j].due - sorted[j].paid;
                if (oj > oi) {
                    FeeRecord tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
                }
            }
        sep('=', 72);
        printf("  FEE OUTSTANDING — Sorted Highest to Lowest\n");
        sep('=', 72);
        printf("  %-14s  %-6s  %-12s  %-12s  %-12s  %-8s\n",
               "Roll No.", "Year", "Due (Rs)", "Paid (Rs)", "Outstandng", "Status");
        sep('-', 72);
        for (int i = 0; i < sn; i++) {
            int out = sorted[i].due - sorted[i].paid;
            if (out <= 0) break;
            printf("  %-14s  %-6d  %-12d  %-12d  %-12d  %-8s\n",
                   sorted[i].username, sorted[i].year,
                   sorted[i].due, sorted[i].paid, out, sorted[i].status);
        }
        sep('=', 72);
        printf("\n");
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  LEDGER VIEWER  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void show_ledger(void) {
    print_panel("FINANCIAL LEDGER");
    FILE *f = fopen(FILE_LEDGER, "r");
    if (!f) { printf("  Ledger file not found.\n"); return; }
    char line[300];
    int count = 0;
    sep('=', 76);
    while (fgets(line, sizeof(line), f)) {
        int n = (int)strlen(line);
        if (n > 0 && line[n-1] == '\n') line[n-1] = '\0';
        printf("  %s\n", line);
        count++;
    }
    fclose(f);
    sep('=', 76);
    printf("  Total entries: %d\n\n", count);
}

/* ════════════════════════════════════════════════════════════════════
 *  NOTICE BOARD  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void show_notices(void) {
    print_panel("NOTICE BOARD");
    if (NOTICE_COUNT == 0) { printf("  No notices.\n"); return; }
    sep('=', 72);
    printf("  %-12s  %-12s  %s\n", "Date", "Category", "Notice");
    sep('-', 72);
    for (int i = 0; i < NOTICE_COUNT; i++)
        printf("  %-12s  %-12s  %s\n",
               NOTICES[i].date, NOTICES[i].category, NOTICES[i].text);
    sep('=', 72);
    printf("\n");
}

static void add_notice(void) {
    if (NOTICE_COUNT >= MAX_NOTICES) {
        printf("  Notice board full (max %d).\n", MAX_NOTICES); return;
    }
    print_panel("ADD NOTICE");
    Notice *no = &NOTICES[NOTICE_COUNT];
    today_str(no->date, sizeof(no->date));
    printf("  Category [ACADEMIC/ADMIN/EXAM/GENERAL]: "); fflush(stdout);
    read_line(no->category, sizeof(no->category));
    if (!no->category[0]) strncpy(no->category, "GENERAL", 23);
    printf("  Notice text: "); fflush(stdout);
    read_line(no->text, sizeof(no->text));
    if (!no->text[0]) { printf("  Empty notice — not added.\n"); return; }
    NOTICE_COUNT++;
    save_notices();
    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "Notice added: [%.20s] %.200s", no->category, no->text);
    log_event(current_user.username, logmsg);
    printf("  Notice added successfully.\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  SCHEDULE / ATTENDANCE  (originals preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void show_staff_schedule(void) {
    print_panel("SCHEDULE & DUTIES");
    const char *days[]  = {"Sunday","Monday","Tuesday","Wednesday","Thursday"};
    const char *duties[] = {
        "General Maintenance & Lab Preparation",
        "Office Administration & Records Management",
        "Lab Supervision & Equipment Check",
        "Security Rotation & Campus Patrol",
        "Library Management & IT Support"
    };
    sep('-', 60);
    printf("  %-12s  |  %s\n", "Day", "Duty");
    sep('-', 60);
    for (int i = 0; i < 5; i++)
        printf("  %-12s  |  %s\n", days[i], duties[i]);
    sep('-', 60);
    printf("  Check-In : 10:00 AM  |  Break : 1:00 PM  |  Check-Out : 5:00 PM\n");
    sep('-', 60);
    printf("\n");
}

static void show_attendance(void) {
    print_panel("STAFF ATTENDANCE LOG");
    printf("  [1]  View today's attendance\n");
    printf("  [2]  Update attendance for a staff member\n");
    printf("  [3]  Full attendance report (all staff)\n");
    printf("  Choice: "); fflush(stdout);
    int ch = read_int();

    if (ch == 1 || ch == 3) {
        sep('=', 72);
        printf("  %-14s  %-24s  %-14s  %-8s  %-12s\n",
               "Username", "Name", "Job Title", "Status", "Date");
        sep('-', 72);
        int p = 0, a = 0, l = 0;
        for (int i = 0; i < ATTEND_COUNT; i++) {
            for (int j = 0; j < USER_COUNT; j++) {
                if (strcmp(USERS[j].username, ATTENDANCE[i].username) == 0) {
                    printf("  %-14s  %-24s  %-14s  %-8s  %-12s\n",
                           ATTENDANCE[i].username, USERS[j].name,
                           USERS[j].extra, ATTENDANCE[i].status, ATTENDANCE[i].date);
                    break;
                }
            }
            if (!strcmp(ATTENDANCE[i].status, "Present")) p++;
            else if (!strcmp(ATTENDANCE[i].status, "Absent")) a++;
            else l++;
        }
        sep('=', 72);
        printf("  Present: %d  |  Absent: %d  |  On Leave: %d  |"
               "  Attendance Rate: %.1f%%\n\n",
               p, a, l,
               ATTEND_COUNT ? 100.0 * p / ATTEND_COUNT : 0.0);

    } else if (ch == 2) {
        printf("  Enter staff username (e.g. staff1): "); fflush(stdout);
        char uname[USERNAME_LEN]; read_line(uname, sizeof(uname));
        int found = 0;
        for (int i = 0; i < ATTEND_COUNT; i++) {
            if (strcmp(ATTENDANCE[i].username, uname) == 0) {
                printf("  Current status: %s\n", ATTENDANCE[i].status);
                printf("  New status [Present/Absent/Leave]: "); fflush(stdout);
                char ns[16]; read_line(ns, sizeof(ns));
                if (ns[0]) {
                    strncpy(ATTENDANCE[i].status, ns, 11);
                    char dt[16]; today_str(dt, sizeof(dt));
                    strncpy(ATTENDANCE[i].date, dt, 15);
                    save_attendance();
                    char logmsg[256];
                    snprintf(logmsg, sizeof(logmsg),
                             "Attendance updated: %s -> %s", uname, ns);
                    log_event(current_user.username, logmsg);
                    printf("  Attendance updated.\n");
                }
                found = 1; break;
            }
        }
        if (!found) printf("  Staff member '%s' not found.\n", uname);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  DASHBOARDS  (originals preserved, enhanced with CGPA / extras)
 * ════════════════════════════════════════════════════════════════════ */

static void admin_dashboard(void) {
    print_panel("ADMIN DASHBOARD — IOE Pulchowk Campus");
    int by_year[4] = {0};
    for (int i = 0; i < STUDENT_COUNT; i++)
        by_year[ALL_STUDENTS[i].year - 1]++;

    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║  SYSTEM OVERVIEW                         ║\n");
    printf("  ╠══════════════════════════════════════════╣\n");
    printf("  ║  Organization : Pulchowk Engineering Campus\n");
    printf("  ║  Department   : Mechanical Engineering   ║\n");
    printf("  ║  Program      : B.E. Mechanical (BME)    ║\n");
    printf("  ║  Syllabus     : IOE New Curriculum 2080  ║\n");
    printf("  ║  Acad. Year   : 2081/2082                ║\n");
    printf("  ║  Location     : Pulchowk, Lalitpur, Nepal║\n");
    printf("  ╠══════════════════════════════════════════╣\n");
    printf("  ║  Total Students     : %-4d              ║\n", STUDENT_COUNT);
    printf("  ║  ├─ Year 1 (082BME) : %-4d              ║\n", by_year[0]);
    printf("  ║  ├─ Year 2 (081BME) : %-4d              ║\n", by_year[1]);
    printf("  ║  ├─ Year 3 (080+079): %-4d              ║\n", by_year[2]);
    printf("  ║  └─ Year 4 (078BME) : %-4d              ║\n", by_year[3]);
    int sc = 0, ac = 0, tc = 0;
    for (int i = 0; i < USER_COUNT; i++) {
        if (!strcmp(USERS[i].role, "staff"))      sc++;
        if (!strcmp(USERS[i].role, "accountant")) ac++;
        if (!strcmp(USERS[i].role, "teacher"))    tc++;
    }
    printf("  ║  Teachers           : %-4d              ║\n", tc);
    printf("  ║  Staff              : %-4d              ║\n", sc);
    printf("  ║  Accountants        : %-4d              ║\n", ac);
    printf("  ╚══════════════════════════════════════════╝\n\n");

    double sum_pct = 0.0; int n_pct = 0;
    for (int i = 0; i < STUDENT_COUNT; i++) {
        Student *s = &ALL_STUDENTS[i];
        StudentResult *r = find_result(s->username);
        if (!r) continue;
        for (int v = 0; v < s->valid_sem_count; v++) {
            int sem = s->valid_sems[v];
            SemResult *sr = &r->results[s->year-1][sem-1];
            if (sr && sr->count > 0) { sum_pct += compute_pct(s->year, sem, sr); n_pct++; }
        }
    }
    if (n_pct > 0)
        printf("  Overall Avg Percentage: %.2f%%\n\n", sum_pct / n_pct);

    long long total_d = 0, total_p = 0;
    for (int i = 0; i < FEE_COUNT; i++) { total_d += FEES[i].due; total_p += FEES[i].paid; }
    if (FEE_COUNT > 0)
        printf("  Fee Collection: Rs %lld / Rs %lld  (%.1f%% collected)\n\n",
               total_p, total_d,
               total_d ? 100.0 * total_p / total_d : 0.0);
}

static void teacher_dashboard(void) {
    print_panel("TEACHER DASHBOARD");
    printf("  Welcome, %s\n", current_user.name);
    printf("  Subject Expertise : %s\n", current_user.extra);
    printf("  Phone             : %s\n", current_user.phone);
    printf("  Joined            : %s\n\n", current_user.joined);
    printf("  Total Students : %d  |  Years: 1–4  |  Semesters: 8\n\n", STUDENT_COUNT);
    printf("  IOE BME New Syllabus 2080 — Subjects Overview:\n");
    sep('-', 60);
    const char *yr_labels[] = {"I/I","I/II","II/I","II/II","III/I","III/II","IV/I","IV/II"};
    int k = 0;
    for (int y = 0; y < 4; y++)
        for (int s = 0; s < 2; s++)
            printf("  Sem %-5s : %d subjects\n", yr_labels[k++], SUBJECT_COUNT[y][s]);
    printf("\n");
}

static void student_dashboard(void) {
    print_panel("MY DASHBOARD");
    printf("  Roll No.     : %s\n", current_student.username);
    printf("  Year         : %s (Year %d)\n", current_student.year_label, current_student.year);
    printf("  Batch        : %s\n", current_student.prefix);
    printf("  Logged In    : Semester %d\n\n", current_sel_sem);

    double cgpa = compute_cumulative_gpa(&current_student);
    printf("  Cumulative GPA: %.2f / 4.00\n\n", cgpa);

    StudentResult *r = find_result(current_student.username);
    if (r) {
        SemResult *sr = &r->results[current_student.year-1][current_sel_sem-1];
        if (sr && sr->count > 0) {
            int grand = 0;
            for (int i = 0; i < sr->count; i++)
                grand += SUBJECTS[current_student.year-1][current_sel_sem-1][i].total_fm;
            double pct = compute_pct(current_student.year, current_sel_sem, sr);
            printf("  Sem %d Performance:\n", current_sel_sem);
            printf("  ┌─ Total      : %d / %d\n", compute_total(sr), grand);
            printf("  ├─ Percentage : %.2f%%\n", pct);
            printf("  ├─ Grade      : %s\n", get_grade(pct));
            printf("  └─ GPA        : %.2f / 4.00\n\n", get_gpa(pct));
        }
    }

    static RankRow rows[MAX_STUDENTS];
    int cnt = 0;
    build_rankings(current_student.year, current_sel_sem, rows, &cnt);
    for (int i = 0; i < cnt; i++) {
        if (!strcmp(rows[i].username, current_student.username)) {
            printf("  Class Rank   : #%d / %d\n\n", i + 1, cnt); break;
        }
    }

    FeeRecord *f = find_fee(current_student.username);
    if (f)
        printf("  Fee Status   : Rs %d / Rs %d  [%s]\n\n",
               f->paid, f->due, f->status);

    int nsub = SUBJECT_COUNT[current_student.year-1][current_sel_sem-1];
    printf("  Subjects — Year %d Sem %d (%s):\n",
           current_student.year, current_sel_sem, current_student.prefix);
    sep('-', 60);
    for (int i = 0; i < nsub; i++) {
        const SubjectInfo *si = &SUBJECTS[current_student.year-1][current_sel_sem-1][i];
        if (!si->total_fm) break;
        printf("  %-7s  %s  (%d marks)\n", si->code, si->name, si->total_fm);
    }
    sep('-', 60);
    printf("\n");
}

static void accountant_dashboard(void) {
    print_panel("ACCOUNTANT DASHBOARD");
    printf("  Welcome, %s\n",    current_user.name);
    printf("  Designation : %s\n", current_user.extra);
    printf("  Phone       : %s\n", current_user.phone);
    printf("  Joined      : %s\n\n", current_user.joined);
    long long total_d = 0, total_p = 0;
    int paid_c = 0, partial_c = 0, due_c = 0;
    for (int i = 0; i < FEE_COUNT; i++) {
        total_d += FEES[i].due; total_p += FEES[i].paid;
        if      (!strcmp(FEES[i].status, "PAID"))    paid_c++;
        else if (!strcmp(FEES[i].status, "PARTIAL")) partial_c++;
        else                                         due_c++;
    }
    printf("  Total Students  : %d\n",  FEE_COUNT);
    printf("  Annual Fee Due  : Rs %lld\n", total_d);
    printf("  Fee Collected   : Rs %lld  (%.1f%%)\n",
           total_p, total_d ? 100.0 * total_p / total_d : 0.0);
    printf("  Outstanding     : Rs %lld\n\n", total_d - total_p);
    printf("  Fully Paid  : %d  |  Partial: %d  |  Pending: %d\n\n",
           paid_c, partial_c, due_c);
}

static void staff_dashboard(void) {
    print_panel("STAFF DASHBOARD");
    printf("  Welcome, %s\n",    current_user.name);
    printf("  Job Title : %s\n", current_user.extra);
    printf("  Phone     : %s\n", current_user.phone);
    printf("  Joined    : %s\n\n", current_user.joined);
    printf("  Total Students on Campus: %d\n", STUDENT_COUNT);
    printf("  Department: Administrative\n\n");
    if (NOTICE_COUNT > 0) {
        printf("  Latest Notice:\n");
        printf("  [%s] %s\n\n", NOTICES[0].date, NOTICES[0].text);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  STUDENT RANKINGS  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void student_rankings(void) {
    print_ranking_table(current_student.year, current_sel_sem);
    static RankRow rows[MAX_STUDENTS];
    int cnt = 0;
    build_rankings(current_student.year, current_sel_sem, rows, &cnt);
    for (int i = 0; i < cnt; i++) {
        if (!strcmp(rows[i].username, current_student.username)) {
            printf("  >>> YOUR RANK: #%d / %d  (%.2f%%  Grade: %s)\n\n",
                   i + 1, cnt, rows[i].pct, rows[i].grade);
            break;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  SUBJECTS VIEWER  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void view_subjects(void) {
    print_panel("IOE BME SUBJECTS — New Syllabus 2080");
    printf("  Select Year [1-4]: "); fflush(stdout);
    int y = read_int();
    if (y < 1 || y > 4) { printf("  Invalid year.\n"); return; }
    printf("  Select Semester [1/2]: "); fflush(stdout);
    int s = read_int();
    if (s < 1 || s > 2) { printf("  Invalid semester.\n"); return; }

    int nsub = SUBJECT_COUNT[y-1][s-1];
    printf("\n");
    sep('=', 76);
    printf("  YEAR %d — SEMESTER %d   (%d subjects)\n", y, s, nsub);
    sep('=', 76);
    printf("  %-3s  %-8s  %-44s  %4s  %3s  %3s  %3s\n",
           "SN", "Code", "Subject Name", "FM", "Th", "In", "Pr");
    sep('-', 76);
    int grand = 0;
    for (int i = 0; i < nsub; i++) {
        const SubjectInfo *si = &SUBJECTS[y-1][s-1][i];
        if (!si->total_fm) break;
        grand += si->total_fm;
        printf("  %-3d  %-8s  %-44s  %4d  %3d  %3d  %3d\n",
               i + 1, si->code, si->name,
               si->total_fm, si->theory_fm, si->internal_fm, si->practical_fm);
    }
    sep('-', 76);
    printf("  %-3s  %-8s  %-44s  %4d\n", " ", " ", "GRAND TOTAL", grand);
    sep('=', 76);
    printf("  FM=Full Marks  Th=Theory  In=Internal  Pr=Practical\n\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  CHANGE PASSWORD  (original preserved + masking + log)
 * ════════════════════════════════════════════════════════════════════ */

static void change_password(void) {
    print_panel("CHANGE PASSWORD");
    printf("  Current password: "); fflush(stdout);
    char old[PASSWORD_LEN]; read_password(old, sizeof(old));

    if (strcmp(current_user.password, old) != 0) {
        printf("  Incorrect current password.\n");
        log_error(current_user.username, "Failed password change attempt");
        return;
    }
    printf("  New password     : "); fflush(stdout);
    char np[PASSWORD_LEN]; read_password(np, sizeof(np));
    if (!np[0]) { printf("  Empty — cancelled.\n"); return; }
    printf("  Confirm password : "); fflush(stdout);
    char cp[PASSWORD_LEN]; read_password(cp, sizeof(cp));
    if (strcmp(np, cp) != 0) { printf("  Passwords do not match.\n"); return; }

    User *u = find_user(current_user.username);
    if (u) {
        strncpy(u->password,           np, PASSWORD_LEN - 1);
        strncpy(current_user.password, np, PASSWORD_LEN - 1);
        /* Also update in students array if needed */
        if (!strcmp(u->role, "student")) {
            Student *s = find_student(u->username);
            if (s) strncpy(s->password, np, PASSWORD_LEN - 1);
        }
        save_users();
        log_event(current_user.username, "Password changed successfully");
        printf("  Password changed and saved.\n");
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  EXPORT TO CSV  (NEW — all roles with access)
 * ════════════════════════════════════════════════════════════════════ */

static void export_results_csv(void) {
    print_panel("EXPORT RESULTS TO CSV");
    printf("  Select Year [1-4]: "); fflush(stdout);
    int year = read_int();
    if (year < 1 || year > 4) { printf("  Invalid.\n"); return; }
    printf("  Select Semester [1/2]: "); fflush(stdout);
    int sem = read_int();
    if (sem < 1 || sem > 2) { printf("  Invalid.\n"); return; }

    char fname[64];
    snprintf(fname, sizeof(fname), "data/results_Y%d_S%d.csv", year, sem);
    FILE *f = fopen(fname, "w");
    if (!f) { printf("  Cannot create file.\n"); return; }

    int nsub = SUBJECT_COUNT[year-1][sem-1];
    fprintf(f, "Rank,Roll No.,Batch");
    for (int i = 0; i < nsub; i++)
        fprintf(f, ",%s", SUBJECTS[year-1][sem-1][i].code);
    fprintf(f, ",Total,Percentage,Grade,GPA\n");

    static RankRow rows[MAX_STUDENTS];
    int cnt = 0;
    build_rankings(year, sem, rows, &cnt);
    for (int i = 0; i < cnt; i++) {
        Student *s = find_student(rows[i].username);
        StudentResult *r = find_result(rows[i].username);
        fprintf(f, "%d,%s,%s", i + 1, rows[i].username,
                s ? s->prefix : "?");
        if (r) {
            SemResult *sr = &r->results[year-1][sem-1];
            for (int s2 = 0; s2 < nsub; s2++)
                fprintf(f, ",%d", sr->marks[s2]);
        }
        fprintf(f, ",%d,%.2f,%s,%.2f\n",
                rows[i].total, rows[i].pct, rows[i].grade,
                get_gpa(rows[i].pct));
    }
    fclose(f);
    printf("  Exported %d records to '%s'\n\n", cnt, fname);
    log_event(current_user.username, fname);
}

static void export_fees_csv(void) {
    const char *fname = "data/fees_export.csv";
    FILE *f = fopen(fname, "w");
    if (!f) { printf("  Cannot create file.\n"); return; }
    fprintf(f, "Roll No.,Year,Due (Rs),Paid (Rs),Outstanding,Status,Last Payment\n");
    for (int i = 0; i < FEE_COUNT; i++) {
        fprintf(f, "%s,%d,%d,%d,%d,%s,%s\n",
                FEES[i].username, FEES[i].year,
                FEES[i].due, FEES[i].paid,
                FEES[i].due - FEES[i].paid,
                FEES[i].status, FEES[i].last_payment);
    }
    fclose(f);
    printf("  Exported %d fee records to '%s'\n\n", FEE_COUNT, fname);
    log_event(current_user.username, "fees_export.csv exported");
}

static void export_students_csv(void) {
    const char *fname = "data/students_export.csv";
    FILE *f = fopen(fname, "w");
    if (!f) { printf("  Cannot create file.\n"); return; }
    fprintf(f, "Roll No.,Year,Year Label,Batch,Valid Semesters,Cumul GPA\n");
    for (int i = 0; i < STUDENT_COUNT; i++) {
        Student *s = &ALL_STUDENTS[i];
        char sems[8] = "";
        for (int v = 0; v < s->valid_sem_count; v++) {
            if (v) strcat(sems, "&");
            char tmp[4]; snprintf(tmp, sizeof(tmp), "%d", s->valid_sems[v]);
            strcat(sems, tmp);
        }
        fprintf(f, "%s,%d,%s,%s,%s,%.2f\n",
                s->username, s->year, s->year_label, s->prefix, sems,
                compute_cumulative_gpa(s));
    }
    fclose(f);
    printf("  Exported %d student records to '%s'\n\n", STUDENT_COUNT, fname);
    log_event(current_user.username, "students_export.csv exported");
}

/* ════════════════════════════════════════════════════════════════════
 *  IMPORT STUDENTS FROM CSV  (NEW — admin only)
 *  CSV format: prefix,year,sem1_valid,sem2_valid,count
 *  Example line: 083BME,1,1,1,48
 * ════════════════════════════════════════════════════════════════════ */

static void import_students_csv(void) {
    print_panel("IMPORT STUDENTS FROM CSV");
    printf("  CSV format per line: prefix,year,sem1(0/1),sem2(0/1),count\n");
    printf("  Example: 083BME,1,1,1,48\n\n");
    printf("  Enter CSV filename (in data/ dir): "); fflush(stdout);
    char fname_in[80]; read_line(fname_in, sizeof(fname_in));
    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "data/%s", fname_in);

    FILE *f = fopen(fullpath, "r");
    if (!f) { printf("  Cannot open '%s'.\n", fullpath); return; }

    char line[CSV_LINE_LEN];
    int imported = 0, skipped = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char prefix[16] = ""; int year = 0, s1 = 0, s2 = 0, count = 0;
        if (sscanf(line, "%15[^,],%d,%d,%d,%d", prefix, &year, &s1, &s2, &count) != 5)
            continue;
        if (year < 1 || year > 4 || count < 1 || count > 60) continue;

        for (int i = 1; i <= count; i++) {
            char tmp[USERNAME_LEN];
            { char nbuf[4]; snprintf(nbuf,sizeof(nbuf),"%02d",i%100);
              snprintf(tmp, USERNAME_LEN, "%.15s%s", prefix, nbuf); }
            if (STUDENT_COUNT >= MAX_STUDENTS || username_exists(tmp)) {
                skipped++; continue;
            }
            Student *s = &ALL_STUDENTS[STUDENT_COUNT++];
            strncpy(s->username, tmp, USERNAME_LEN - 1);
            snprintf(s->password, PASSWORD_LEN, "%s123", tmp);
            strncpy(s->name,      tmp,     NAME_LEN - 1);
            strncpy(s->role,      "student", ROLE_LEN - 1);
            s->year = year;
            strncpy(s->prefix, prefix, 15);
            static const char *labels[] = {"1st Year","2nd Year","3rd Year","4th Year"};
            strncpy(s->year_label, labels[year-1], LABEL_LEN - 1);
            int sc = 0;
            if (s1) s->valid_sems[sc++] = 1;
            if (s2) s->valid_sems[sc++] = 2;
            s->valid_sem_count = sc;

            /* Add to USERS */
            if (USER_COUNT < (int)(sizeof(USERS) / sizeof(USERS[0]))) {
                User *u = &USERS[USER_COUNT++];
                strncpy(u->username, tmp, USERNAME_LEN - 1);
                strncpy(u->password, s->password, PASSWORD_LEN - 1);
                strncpy(u->name,     tmp, NAME_LEN - 1);
                strncpy(u->role,     "student", ROLE_LEN - 1);
                strncpy(u->display,  "Student", LABEL_LEN - 1);
            }

            /* Add empty result */
            if (RESULT_COUNT < MAX_STUDENTS) {
                StudentResult *sr = &RESULTS[RESULT_COUNT++];
                strncpy(sr->username, tmp, USERNAME_LEN - 1);
                memset(sr->results, 0, sizeof(sr->results));
            }

            /* Add fee record */
            if (FEE_COUNT < MAX_STUDENTS) {
                FeeRecord *fr = &FEES[FEE_COUNT++];
                strncpy(fr->username, tmp, USERNAME_LEN - 1);
                fr->year = year;
                fr->due  = 50000; fr->paid = 0;
                strncpy(fr->status, "DUE", 11);
                char dt[16]; today_str(dt, sizeof(dt));
                strncpy(fr->last_payment, dt, 15);
            }
            imported++;
        }
    }
    fclose(f);
    printf("  Imported: %d   Skipped (duplicate/overflow): %d\n", imported, skipped);
    if (imported > 0) {
        save_students(); save_users(); save_results(); save_fees();
        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg), "Imported %d students from %s", imported, fname_in);
        log_event(current_user.username, logmsg);
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  EXPORT READABLE TEXT REPORT  (original, preserved)
 * ════════════════════════════════════════════════════════════════════ */

static void export_results_txt(void) {
    print_panel("EXPORT RESULTS (TXT)");
    printf("  Select Year [1-4]: "); fflush(stdout);
    int year = read_int();
    if (year < 1 || year > 4) { printf("  Invalid.\n"); return; }
    printf("  Select Semester [1/2]: "); fflush(stdout);
    int sem = read_int();
    if (sem < 1 || sem > 2) { printf("  Invalid.\n"); return; }

    char fname[64];
    snprintf(fname, sizeof(fname), "data/results_Y%d_S%d.txt", year, sem);
    FILE *f = fopen(fname, "w");
    if (!f) { printf("  Cannot create file.\n"); return; }

    int nsub = SUBJECT_COUNT[year-1][sem-1];
    fprintf(f, "IOE Pulchowk Campus — Results Year %d Sem %d\n", year, sem);
    fprintf(f, "Mechanical Engineering | New Syllabus 2080\n");
    char dt[32]; now_str(dt, sizeof(dt));
    fprintf(f, "Generated: %s\n\n", dt);

    fprintf(f, "%-14s", "Roll No.");
    for (int i = 0; i < nsub; i++)
        fprintf(f, "  %-8s", SUBJECTS[year-1][sem-1][i].code);
    fprintf(f, "  %6s  %8s  %5s\n", "Total", "Percent", "Grade");

    static RankRow rows[MAX_STUDENTS];
    int cnt = 0;
    build_rankings(year, sem, rows, &cnt);
    for (int i = 0; i < cnt; i++) {
        StudentResult *r = find_result(rows[i].username);
        fprintf(f, "%-14s", rows[i].username);
        if (r) {
            SemResult *sr = &r->results[year-1][sem-1];
            for (int s2 = 0; s2 < nsub; s2++)
                fprintf(f, "  %-8d", sr->marks[s2]);
        }
        fprintf(f, "  %6d  %7.2f%%  %-5s\n",
                rows[i].total, rows[i].pct, rows[i].grade);
    }
    fclose(f);
    printf("  Exported %d records to '%s'\n\n", cnt, fname);
    log_event(current_user.username, fname);
}

/* ════════════════════════════════════════════════════════════════════
 *  DATA INTEGRITY CHECK  (NEW — admin only)
 * ════════════════════════════════════════════════════════════════════ */

static void data_integrity_check(void) {
    print_panel("DATA INTEGRITY CHECK");
    int errors = 0;

    printf("  Checking students...\n");
    for (int i = 0; i < STUDENT_COUNT; i++) {
        Student *s = &ALL_STUDENTS[i];
        /* Check for duplicate usernames */
        for (int j = i + 1; j < STUDENT_COUNT; j++) {
            if (!strcmp(s->username, ALL_STUDENTS[j].username)) {
                printf("  [WARN] Duplicate student username: %s (rows %d & %d)\n",
                       s->username, i, j);
                errors++;
            }
        }
        /* Validate year range */
        if (s->year < 1 || s->year > 4) {
            printf("  [WARN] Student %s has invalid year: %d\n", s->username, s->year);
            errors++;
        }
        /* Validate sem count */
        if (s->valid_sem_count < 1 || s->valid_sem_count > 2) {
            printf("  [WARN] Student %s has invalid sem_count: %d\n",
                   s->username, s->valid_sem_count);
            errors++;
        }
    }

    printf("  Checking results vs students...\n");
    for (int i = 0; i < RESULT_COUNT; i++) {
        if (!find_student(RESULTS[i].username)) {
            printf("  [WARN] Orphan result: username '%s' has no student record.\n",
                   RESULTS[i].username);
            errors++;
        }
    }

    printf("  Checking fees vs students...\n");
    for (int i = 0; i < FEE_COUNT; i++) {
        if (!find_student(FEES[i].username)) {
            printf("  [WARN] Orphan fee record: username '%s'.\n", FEES[i].username);
            errors++;
        }
        if (FEES[i].paid < 0 || FEES[i].due < 0 || FEES[i].paid > FEES[i].due) {
            printf("  [WARN] Invalid fee amounts for %s: due=%d paid=%d.\n",
                   FEES[i].username, FEES[i].due, FEES[i].paid);
            errors++;
        }
    }

    printf("  Checking attendance vs staff...\n");
    for (int i = 0; i < ATTEND_COUNT; i++) {
        User *u = find_user(ATTENDANCE[i].username);
        if (!u || strcmp(u->role, "staff") != 0) {
            printf("  [WARN] Attendance for non-staff: '%s'.\n",
                   ATTENDANCE[i].username);
            errors++;
        }
    }

    printf("  Checking users for duplicates...\n");
    for (int i = 0; i < USER_COUNT; i++) {
        for (int j = i + 1; j < USER_COUNT; j++) {
            if (!strcmp(USERS[i].username, USERS[j].username)) {
                printf("  [WARN] Duplicate user: '%s' (rows %d & %d).\n",
                       USERS[i].username, i, j);
                errors++;
            }
        }
    }

    sep('-', 60);
    if (errors == 0)
        printf("  [OK] All integrity checks passed. No issues found.\n");
    else
        printf("  [!!] Found %d integrity issue(s). Review above.\n", errors);
    sep('-', 60);

    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "Integrity check: %d issue(s) found", errors);
    log_event(current_user.username, logmsg);
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  ADMIN: RESET ANY USER'S PASSWORD  (NEW)
 * ════════════════════════════════════════════════════════════════════ */

static void admin_reset_password(void) {
    print_panel("ADMIN — RESET USER PASSWORD");
    printf("  Enter username to reset: "); fflush(stdout);
    char uname[USERNAME_LEN]; read_line(uname, sizeof(uname));

    User *u = find_user(uname);
    if (!u) { printf("  User '%s' not found.\n", uname); return; }
    if (!strcmp(uname, current_user.username)) {
        printf("  Use 'Change Password' to change your own password.\n"); return;
    }

    printf("  New password for '%s': ", uname); fflush(stdout);
    char np[PASSWORD_LEN]; read_password(np, sizeof(np));
    if (!np[0]) { printf("  Empty — cancelled.\n"); return; }
    printf("  Confirm           : "); fflush(stdout);
    char cp[PASSWORD_LEN]; read_password(cp, sizeof(cp));
    if (strcmp(np, cp) != 0) { printf("  Passwords do not match.\n"); return; }

    strncpy(u->password, np, PASSWORD_LEN - 1);
    /* Also update student array if this is a student */
    Student *s = find_student(uname);
    if (s) strncpy(s->password, np, PASSWORD_LEN - 1);

    save_users();
    if (s) save_students();

    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "Admin reset password for user: %s", uname);
    log_event(current_user.username, logmsg);
    printf("  Password for '%s' reset and saved.\n", uname);
}

/* ════════════════════════════════════════════════════════════════════
 *  VIEW SYSTEM LOG  (NEW — admin only)
 * ════════════════════════════════════════════════════════════════════ */

static void view_system_log(void) {
    print_panel("SYSTEM AUDIT LOG");
    FILE *f = fopen(FILE_LOG, "r");
    if (!f) { printf("  Log file not found (system.log).\n"); return; }

    printf("  [1]  View last 50 entries\n");
    printf("  [2]  View full log\n");
    printf("  [3]  Search by keyword\n");
    printf("  Choice: "); fflush(stdout);
    int ch = read_int();

    if (ch == 3) {
        printf("  Keyword: "); fflush(stdout);
        char kw[64]; read_line(kw, sizeof(kw));
        char kl[64]; strncpy(kl, kw, 63); str_lower(kl);
        char line[LOG_LINE_LEN];
        int found = 0;
        rewind(f);
        while (fgets(line, sizeof(line), f)) {
            char ll[LOG_LINE_LEN]; strncpy(ll, line, LOG_LINE_LEN - 1); str_lower(ll);
            if (strstr(ll, kl)) {
                int n = (int)strlen(line);
                if (n > 0 && line[n-1] == '\n') line[n-1] = '\0';
                printf("  %s\n", line);
                found++;
            }
        }
        if (!found) printf("  No entries matching '%s'.\n", kw);
        printf("  Found: %d\n\n", found);
    } else {
        /* Count total lines */
        int total = 0;
        char line[LOG_LINE_LEN];
        while (fgets(line, sizeof(line), f)) total++;
        rewind(f);

        int skip = (ch == 1 && total > 50) ? total - 50 : 0;
        int cur = 0;
        sep('=', 76);
        while (fgets(line, sizeof(line), f)) {
            cur++;
            if (cur <= skip) continue;
            int n = (int)strlen(line);
            if (n > 0 && line[n-1] == '\n') line[n-1] = '\0';
            printf("  %s\n", line);
        }
        sep('=', 76);
        printf("  Total log entries: %d  |  Shown: %d\n\n", total, total - skip);
    }
    fclose(f);
}

/* ════════════════════════════════════════════════════════════════════
 *  SAVE ALL — menu trigger
 * ════════════════════════════════════════════════════════════════════ */

static void menu_save_all(void) {
    clrscr(); save_all(); pause_enter();
}

/* ════════════════════════════════════════════════════════════════════
 *  LOGIN SYSTEM  (original + masking + attempt limiter + log)
 * ════════════════════════════════════════════════════════════════════ */

static void login_screen(void) {
    clrscr(); print_banner();
    printf("  Select Role:\n");
    printf("  [1] Admin       [2] Teacher    [3] Student\n");
    printf("  [4] Staff       [5] Accountant\n");
    printf("\n  Role: "); fflush(stdout);

    int rc = read_int();
    const char *roles[] = {"","admin","teacher","student","staff","accountant"};
    if (rc < 1 || rc > 5) {
        printf("  ERROR: Invalid role.\n"); pause_enter(); return;
    }
    const char *sel_role = roles[rc];

    int sel_year = 0, sel_sem = 0;
    if (rc == 3) {
        printf("\n  Select Year [1-4]: "); fflush(stdout);
        sel_year = read_int();
        if (sel_year < 1 || sel_year > 4) {
            printf("  ERROR: Invalid year.\n"); pause_enter(); return;
        }
        printf("  Select Semester [1/2]: "); fflush(stdout);
        sel_sem = read_int();
        if (sel_sem < 1 || sel_sem > 2) {
            printf("  ERROR: Invalid semester.\n"); pause_enter(); return;
        }
    }

    printf("\n  Username: "); fflush(stdout);
    char uname[USERNAME_LEN]; read_line(uname, sizeof(uname));

    /* Login attempt limiter */
    int attempt;
    for (attempt = 1; attempt <= MAX_LOGIN_ATTEMPTS; attempt++) {
        printf("  Password [attempt %d/%d]: ", attempt, MAX_LOGIN_ATTEMPTS);
        fflush(stdout);
        char pass[PASSWORD_LEN]; read_password(pass, sizeof(pass));

        if (!uname[0] || !pass[0]) {
            printf("  ERROR: Credentials cannot be empty.\n");
            log_error(uname, "Empty credential attempt");
            continue;
        }

        User *u = find_user(uname);
        if (!u) {
            printf("  ERROR: User '%s' not found.\n", uname);
            char logmsg[256];
            snprintf(logmsg, sizeof(logmsg), "Login failed: unknown user '%s'", uname);
            log_error("UNKNOWN", logmsg);
            pause_enter(); return;
        }
        if (strcmp(u->password, pass) != 0) {
            printf("  ERROR: Wrong password (%d/%d attempts).\n",
                   attempt, MAX_LOGIN_ATTEMPTS);
            char logmsg[256];
            snprintf(logmsg, sizeof(logmsg),
                     "Login failed: wrong password for '%s' (attempt %d)",
                     uname, attempt);
            log_error(uname, logmsg);
            if (attempt == MAX_LOGIN_ATTEMPTS) {
                printf("  LOCKED: Maximum attempts reached. Login blocked.\n");
                log_error(uname, "Account locked after 3 failed attempts");
                pause_enter(); return;
            }
            continue;
        }
        if (strcmp(u->role, sel_role) != 0) {
            printf("  ERROR: Account '%s' is '%s', not '%s'.\n",
                   uname, u->role, sel_role);
            char logmsg[256];
            snprintf(logmsg, sizeof(logmsg),
                     "Login role mismatch: user=%s is '%s', attempted '%s'",
                     uname, u->role, sel_role);
            log_error(uname, logmsg);
            pause_enter(); return;
        }

        /* Student validation */
        if (rc == 3) {
            Student *s = find_student(uname);
            if (!s) {
                printf("  ERROR: Student record missing.\n");
                pause_enter(); return;
            }
            if (s->year != sel_year) {
                printf("  ERROR: Year mismatch — your account is Year %d, not Year %d.\n",
                       s->year, sel_year);
                log_error(uname, "Year mismatch during login");
                pause_enter(); return;
            }
            if (!student_valid_sem(s, sel_sem)) {
                printf("  ERROR: Semester %d not valid for roll %s.\n  Valid: ",
                       sel_sem, uname);
                for (int v = 0; v < s->valid_sem_count; v++) {
                    if (v) printf(", ");
                    printf("%d", s->valid_sems[v]);
                }
                printf("\n");
                log_error(uname, "Semester mismatch during login");
                pause_enter(); return;
            }
            current_student    = *s;
            current_is_student = 1;
            current_sel_year   = sel_year;
            current_sel_sem    = sel_sem;
        } else {
            current_is_student = 0;
            current_sel_year   = 0;
            current_sel_sem    = 0;
        }

        current_user = *u;
        logged_in    = 1;

        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg),
                 "Login successful: role=%s", sel_role);
        log_event(uname, logmsg);

        printf("\n  Login successful! Welcome, %s  [%s]\n", u->name, u->display);
        pause_enter();
        return;  /* success */
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  ADMIN MENU  (original + new options)
 * ════════════════════════════════════════════════════════════════════ */

static void run_admin_menu(void) {
    int running = 1;
    while (running) {
        clrscr(); print_banner();
        printf("  Logged in as: %s  [Administrator]\n\n", current_user.name);
        printf("  ADMIN PANEL\n");
        sep('-', 54);
        printf("  [1]  Dashboard              [2]  All Students\n");
        printf("  [3]  Results Overview       [4]  Rankings\n");
        printf("  [5]  Batch Toppers          [6]  Teachers\n");
        printf("  [7]  Staff & Accounts       [8]  Add / Edit Result\n");
        printf("  [9]  Report Cards           [10] Search Student\n");
        printf("  [11] Filter by Year         [12] View Subjects\n");
        printf("  [13] Fee Overview           [14] Financial Ledger\n");
        printf("  [15] Notice Board           [16] Add Notice\n");
        printf("  [17] Export Results (CSV)   [18] Export Results (TXT)\n");
        printf("  [19] Export Fees (CSV)      [20] Export Students (CSV)\n");
        printf("  [21] Import Students (CSV)  [22] Data Integrity Check\n");
        printf("  [23] Reset User Password    [24] View System Log\n");
        printf("  [25] Change My Password     [26] Save All Data Now\n");
        printf("  [0]  Logout\n");
        sep('-', 54);
        printf("  Choice: "); fflush(stdout);

        int ch = read_int();
        switch (ch) {
            case 1:  clrscr(); admin_dashboard(); pause_enter(); break;
            case 2:  clrscr(); print_student_table(0, ""); pause_enter(); break;
            case 3:  clrscr(); results_overview(); pause_enter(); break;
            case 4: {
                clrscr();
                printf("  Year [1-4]: "); fflush(stdout); int y = read_int();
                printf("  Sem [1/2]: ");  fflush(stdout); int s = read_int();
                if (y >= 1 && y <= 4 && s >= 1 && s <= 2) print_ranking_table(y, s);
                else printf("  Invalid.\n");
                pause_enter(); break;
            }
            case 5:  clrscr(); print_batch_toppers(); pause_enter(); break;
            case 6:  clrscr(); show_teachers(); pause_enter(); break;
            case 7:  clrscr(); show_staff_and_accounts(); pause_enter(); break;
            case 8:  clrscr(); edit_result(); pause_enter(); break;
            case 9:  clrscr(); browse_report_cards(); pause_enter(); break;
            case 10: clrscr(); search_student(); pause_enter(); break;
            case 11: {
                clrscr();
                printf("  Filter Year [1-4, 0=All]: "); fflush(stdout);
                int fy = read_int();
                print_student_table(fy, "");
                pause_enter(); break;
            }
            case 12: clrscr(); view_subjects(); pause_enter(); break;
            case 13: clrscr(); fee_overview(); pause_enter(); break;
            case 14: clrscr(); show_ledger(); pause_enter(); break;
            case 15: clrscr(); show_notices(); pause_enter(); break;
            case 16: clrscr(); add_notice(); pause_enter(); break;
            case 17: clrscr(); export_results_csv(); pause_enter(); break;
            case 18: clrscr(); export_results_txt(); pause_enter(); break;
            case 19: clrscr(); export_fees_csv(); pause_enter(); break;
            case 20: clrscr(); export_students_csv(); pause_enter(); break;
            case 21: clrscr(); import_students_csv(); pause_enter(); break;
            case 22: clrscr(); data_integrity_check(); pause_enter(); break;
            case 23: clrscr(); admin_reset_password(); pause_enter(); break;
            case 24: clrscr(); view_system_log(); pause_enter(); break;
            case 25: clrscr(); change_password(); pause_enter(); break;
            case 26: menu_save_all(); break;
            case 0:
                log_event(current_user.username, "Logout");
                running = 0; logged_in = 0; break;
            default: printf("  Invalid choice.\n"); pause_enter();
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  TEACHER MENU  (original + new options)
 * ════════════════════════════════════════════════════════════════════ */

static void run_teacher_menu(void) {
    int running = 1;
    while (running) {
        clrscr(); print_banner();
        printf("  Logged in as: %s  [Teacher]\n\n", current_user.name);
        printf("  TEACHER PANEL\n");
        sep('-', 54);
        printf("  [1]  Dashboard              [2]  Students\n");
        printf("  [3]  Results Overview       [4]  Rankings\n");
        printf("  [5]  Batch Toppers          [6]  Enter / Edit Marks\n");
        printf("  [7]  Report Cards           [8]  Search Student\n");
        printf("  [9]  View Subjects          [10] Notice Board\n");
        printf("  [11] Export Results (CSV)   [12] Export Results (TXT)\n");
        printf("  [13] Change Password\n");
        printf("  [0]  Logout\n");
        sep('-', 54);
        printf("  Choice: "); fflush(stdout);

        int ch = read_int();
        switch (ch) {
            case 1:  clrscr(); teacher_dashboard(); pause_enter(); break;
            case 2: {
                clrscr();
                printf("  Filter Year [1-4, 0=All]: "); fflush(stdout);
                int fy = read_int();
                print_student_table(fy, "");
                pause_enter(); break;
            }
            case 3:  clrscr(); results_overview(); pause_enter(); break;
            case 4: {
                clrscr();
                printf("  Year [1-4]: "); fflush(stdout); int y = read_int();
                printf("  Sem [1/2]: ");  fflush(stdout); int s = read_int();
                if (y >= 1 && y <= 4 && s >= 1 && s <= 2) print_ranking_table(y, s);
                else printf("  Invalid.\n");
                pause_enter(); break;
            }
            case 5:  clrscr(); print_batch_toppers(); pause_enter(); break;
            case 6:  clrscr(); edit_result(); pause_enter(); break;
            case 7:  clrscr(); browse_report_cards(); pause_enter(); break;
            case 8:  clrscr(); search_student(); pause_enter(); break;
            case 9:  clrscr(); view_subjects(); pause_enter(); break;
            case 10: clrscr(); show_notices(); pause_enter(); break;
            case 11: clrscr(); export_results_csv(); pause_enter(); break;
            case 12: clrscr(); export_results_txt(); pause_enter(); break;
            case 13: clrscr(); change_password(); pause_enter(); break;
            case 0:
                log_event(current_user.username, "Logout");
                running = 0; logged_in = 0; break;
            default: printf("  Invalid choice.\n"); pause_enter();
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  STUDENT MENU  (original + new options)
 * ════════════════════════════════════════════════════════════════════ */

static void run_student_menu(void) {
    int running = 1;
    while (running) {
        clrscr(); print_banner();
        printf("  Logged in as: %s  [Year %d — Sem %d]\n\n",
               current_student.username, current_sel_year, current_sel_sem);
        printf("  STUDENT PANEL\n");
        sep('-', 50);
        printf("  [1]  My Dashboard\n");
        printf("  [2]  My Results\n");
        printf("  [3]  My Report Card\n");
        printf("  [4]  Class Rankings\n");
        printf("  [5]  Batch Toppers\n");
        printf("  [6]  View Subjects\n");
        printf("  [7]  Notice Board\n");
        printf("  [8]  Change Password\n");
        printf("  [0]  Logout\n");
        sep('-', 50);
        printf("  Choice: "); fflush(stdout);

        int ch = read_int();
        switch (ch) {
            case 1: clrscr(); student_dashboard(); pause_enter(); break;
            case 2:
                clrscr();
                for (int v = 0; v < current_student.valid_sem_count; v++) {
                    int sem = current_student.valid_sems[v];
                    print_result_card(&current_student, current_student.year, sem);
                }
                pause_enter(); break;
            case 3:
                clrscr();
                print_result_card(&current_student, current_student.year, current_sel_sem);
                pause_enter(); break;
            case 4: clrscr(); student_rankings(); pause_enter(); break;
            case 5: clrscr(); print_batch_toppers(); pause_enter(); break;
            case 6: clrscr(); view_subjects(); pause_enter(); break;
            case 7: clrscr(); show_notices(); pause_enter(); break;
            case 8: clrscr(); change_password(); pause_enter(); break;
            case 0:
                log_event(current_student.username, "Logout");
                running = 0; logged_in = 0; break;
            default: printf("  Invalid choice.\n"); pause_enter();
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  STAFF MENU  (original + new options)
 * ════════════════════════════════════════════════════════════════════ */

static void run_staff_menu(void) {
    int running = 1;
    while (running) {
        clrscr(); print_banner();
        printf("  Logged in as: %s  [%s]\n\n", current_user.name, current_user.extra);
        printf("  STAFF PANEL\n");
        sep('-', 50);
        printf("  [1]  Dashboard\n");
        printf("  [2]  Student List\n");
        printf("  [3]  Schedule & Duties\n");
        printf("  [4]  Attendance Log\n");
        printf("  [5]  Notice Board\n");
        printf("  [6]  Change Password\n");
        printf("  [0]  Logout\n");
        sep('-', 50);
        printf("  Choice: "); fflush(stdout);

        int ch = read_int();
        switch (ch) {
            case 1: clrscr(); staff_dashboard(); pause_enter(); break;
            case 2: clrscr(); print_student_table(0, ""); pause_enter(); break;
            case 3: clrscr(); show_staff_schedule(); pause_enter(); break;
            case 4: clrscr(); show_attendance(); pause_enter(); break;
            case 5: clrscr(); show_notices(); pause_enter(); break;
            case 6: clrscr(); change_password(); pause_enter(); break;
            case 0:
                log_event(current_user.username, "Logout");
                running = 0; logged_in = 0; break;
            default: printf("  Invalid choice.\n"); pause_enter();
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  ACCOUNTANT MENU  (original + new options)
 * ════════════════════════════════════════════════════════════════════ */

static void run_accountant_menu(void) {
    int running = 1;
    while (running) {
        clrscr(); print_banner();
        printf("  Logged in as: %s  [%s]\n\n", current_user.name, current_user.extra);
        printf("  ACCOUNTANT PANEL\n");
        sep('-', 50);
        printf("  [1]  Dashboard\n");
        printf("  [2]  Fee Overview\n");
        printf("  [3]  Student List\n");
        printf("  [4]  Financial Ledger\n");
        printf("  [5]  Notice Board\n");
        printf("  [6]  Export Fees (CSV)\n");
        printf("  [7]  Change Password\n");
        printf("  [0]  Logout\n");
        sep('-', 50);
        printf("  Choice: "); fflush(stdout);

        int ch = read_int();
        switch (ch) {
            case 1: clrscr(); accountant_dashboard(); pause_enter(); break;
            case 2: clrscr(); fee_overview(); pause_enter(); break;
            case 3: clrscr(); print_student_table(0, ""); pause_enter(); break;
            case 4: clrscr(); show_ledger(); pause_enter(); break;
            case 5: clrscr(); show_notices(); pause_enter(); break;
            case 6: clrscr(); export_fees_csv(); pause_enter(); break;
            case 7: clrscr(); change_password(); pause_enter(); break;
            case 0:
                log_event(current_user.username, "Logout");
                running = 0; logged_in = 0; break;
            default: printf("  Invalid choice.\n"); pause_enter();
        }
    }
}

/* ════════════════════════════════════════════════════════════════════
 *  STARTUP  (original logic preserved + log init)
 * ════════════════════════════════════════════════════════════════════ */

static void startup(void) {
    ensure_data_dir();

    /* Initialize log file header if new */
    {
        FILE *lf = fopen(FILE_LOG, "a");
        if (lf) {
            char ts[32]; now_str(ts, sizeof(ts));
            fprintf(lf, "# ════ System started at %s ════\n", ts);
            fclose(lf);
        }
    }

    printf("\n  Loading data from disk...\n");

    int students_loaded   = load_students();
    int users_loaded      = load_users();
    int results_loaded    = load_results();
    int fees_loaded       = load_fees();
    int attendance_loaded = load_attendance();
    int notices_loaded    = load_notices();

    if (!students_loaded || STUDENT_COUNT == 0) {
        printf("  No existing data. Generating initial dataset...\n");
        make_students();
        make_users();
        generate_results();
        generate_fees();
        generate_attendance();
        generate_default_notices();
        generate_default_ledger();
        save_all();
        log_event("SYSTEM", "Initial data generated and saved");
        printf("  Initial data created and saved.\n");
    } else {
        if (!users_loaded || USER_COUNT == 0) {
            printf("  Rebuilding users...\n");
            make_users(); save_users();
        }
        if (!results_loaded || RESULT_COUNT == 0) {
            printf("  Rebuilding results...\n");
            generate_results(); save_results();
        }
        if (!fees_loaded || FEE_COUNT == 0) {
            printf("  Rebuilding fees...\n");
            generate_fees(); save_fees();
        }
        if (!attendance_loaded || ATTEND_COUNT == 0) {
            printf("  Rebuilding attendance...\n");
            generate_attendance(); save_attendance();
        }
        if (!notices_loaded || NOTICE_COUNT == 0) {
            printf("  Loading default notices...\n");
            generate_default_notices(); save_notices();
        }
        generate_default_ledger();
        printf("  Data loaded: %d students, %d users, %d results.\n",
               STUDENT_COUNT, USER_COUNT, RESULT_COUNT);
    }
    log_event("SYSTEM", "Startup complete");
    printf("\n");
}

/* ════════════════════════════════════════════════════════════════════
 *  MAIN  (original logic preserved)
 * ════════════════════════════════════════════════════════════════════ */

int main(void) {
    startup();

    while (1) {
        clrscr();
        print_banner();

        if (!logged_in) {
            printf("  [1]  Login\n");
            printf("  [0]  Exit\n");
            sep('-', 40);
            printf("  Choice: "); fflush(stdout);
            int ch = read_int();
            if (ch == 0) break;
            if (ch == 1) login_screen();
            else { printf("  Invalid choice.\n"); pause_enter(); }
        } else {
            if      (!strcmp(current_user.role, "admin"))      run_admin_menu();
            else if (!strcmp(current_user.role, "teacher"))    run_teacher_menu();
            else if (!strcmp(current_user.role, "student"))    run_student_menu();
            else if (!strcmp(current_user.role, "staff"))      run_staff_menu();
            else if (!strcmp(current_user.role, "accountant")) run_accountant_menu();
            else {
                printf("  Unknown role. Logging out.\n");
                log_error(current_user.username, "Unknown role on dispatch");
                logged_in = 0;
            }
        }
    }

    printf("\n  Saving all data before exit...\n");
    save_all();
    log_event("SYSTEM", "Clean shutdown");
    printf("\n  Thank you for using IOE Pulchowk Result System. Goodbye!\n\n");
    return 0;
}
