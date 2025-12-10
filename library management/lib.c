#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================== CONFIGURATION CONSTANTS ================== */

#define BOOK_TABLE_SIZE 101        // Hash table size for books
#define MAX_TITLE_LEN 100
#define MAX_AUTHOR_LEN 100
#define MAX_NAME_LEN 100

#define MAX_BORROW_DAYS 14
#define FINE_PER_DAY 5.0f
#define MAX_FINE 200.0f

#define MEMBER_TYPE_STUDENT 1
#define MEMBER_TYPE_FACULTY 2

#define MAX_BOOKS_STUDENT 3
#define MAX_BOOKS_FACULTY 5

/* ================== BASIC DATE UTILITIES ================== */

typedef struct {
    int day;
    int month;
    int year;
} Date;

int isLeap(int year) {
    return (year%4==0 && year%100!=0) || (year%400==0);
}

int daysInMonth(int month, int year) {
    static int days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && isLeap(year)) return 29;
    return days[month-1];
}

// Convert Date to "number of days since 01/01/0001" (roughly)
long dateToSerial(Date d) {
    long days = d.day;
    for (int y = 1; y < d.year; ++y) {
        days += isLeap(y) ? 366 : 365;
    }
    for (int m = 1; m < d.month; ++m) {
        days += daysInMonth(m, d.year);
    }
    return days;
}

int daysBetween(Date from, Date to) {
    long a = dateToSerial(from);
    long b = dateToSerial(to);
    return (int)(b - a);
}

// Add n days to a date (simple, but enough for project)
Date addDays(Date d, int n) {
    d.day += n;
    while (1) {
        int dim = daysInMonth(d.month, d.year);
        if (d.day <= dim) break;
        d.day -= dim;
        d.month++;
        if (d.month > 12) {
            d.month = 1;
            d.year++;
        }
    }
    return d;
}

Date getToday() {
    time_t t = time(NULL);
    struct tm *info = localtime(&t);
    Date d;
    d.day = info->tm_mday;
    d.month = info->tm_mon + 1;
    d.year = info->tm_year + 1900;
    return d;
}

void printDate(Date d) {
    printf("%02d-%02d-%04d", d.day, d.month, d.year);
}

/* ================== DATA STRUCTURES ================== */

typedef struct WaitNode {
    int memberId;
    struct WaitNode *next;
} WaitNode;

typedef struct Book {
    int id;
    char title[MAX_TITLE_LEN];
    char author[MAX_AUTHOR_LEN];
    int total_copies;
    int available_copies;

    // waitlist queue
    WaitNode *wait_front;
    WaitNode *wait_rear;

    struct Book *next;  // for hash table chaining
} Book;

typedef struct Member {
    int id;
    char name[MAX_NAME_LEN];
    int type;             // STUDENT / FACULTY
    int borrowed_count;   // active borrows

    struct Member *next;  // linked list of members
} Member;

typedef struct Transaction {
    int id;
    int bookId;
    int memberId;
    Date borrow_date;
    Date due_date;
    Date return_date;
    int is_returned;  // 0 = active, 1 = returned
    float fine;

    struct Transaction *next;
} Transaction;

/* ================== GLOBAL HEADS ================== */

Book *bookTable[BOOK_TABLE_SIZE];
Member *memberHead = NULL;
Transaction *transHead = NULL;
int nextTransId = 1;

/* ================== HASH FUNCTION ================== */

int hashBookId(int id) {
    if (id < 0) id = -id;
    return id % BOOK_TABLE_SIZE;
}

/* ================== WAITLIST QUEUE ================== */

void enqueueWait( Book *book, int memberId ) {
    WaitNode *node = (WaitNode *)malloc(sizeof(WaitNode));
    if (!node) {
        printf("Memory allocation failed for waitlist.\n");
        return;
    }
    node->memberId = memberId;
    node->next = NULL;
    if (book->wait_rear == NULL) {
        book->wait_front = book->wait_rear = node;
    } else {
        book->wait_rear->next = node;
        book->wait_rear = node;
    }
}

int dequeueWait(Book *book) {
    if (book->wait_front == NULL) return -1;
    WaitNode *temp = book->wait_front;
    int id = temp->memberId;
    book->wait_front = temp->next;
    if (book->wait_front == NULL)
        book->wait_rear = NULL;
    free(temp);
    return id;
}

int isWaitlistEmpty(Book *book) {
    return book->wait_front == NULL;
}

/* ================== MEMBER FUNCTIONS ================== */

Member* findMember(int id) {
    Member *curr = memberHead;
    while (curr) {
        if (curr->id == id) return curr;
        curr = curr->next;
    }
    return NULL;
}

int maxBooksAllowed(Member *m) {
    if (m->type == MEMBER_TYPE_STUDENT) return MAX_BOOKS_STUDENT;
    else if (m->type == MEMBER_TYPE_FACULTY) return MAX_BOOKS_FACULTY;
    return 0;
}

void registerMember() {
    Member *m = (Member *)malloc(sizeof(Member));
    if (!m) {
        printf("Memory allocation failed.\n");
        return;
    }
    printf("Enter Member ID: ");
    scanf("%d", &m->id);
    if (findMember(m->id)) {
        printf("Member with this ID already exists.\n");
        free(m);
        return;
    }
    printf("Enter Member Name: ");
    getchar(); // consume newline
    fgets(m->name, MAX_NAME_LEN, stdin);
    m->name[strcspn(m->name, "\n")] = '\0';

    printf("Member Type (1 = Student, 2 = Faculty): ");
    scanf("%d", &m->type);
    if (m->type != MEMBER_TYPE_STUDENT && m->type != MEMBER_TYPE_FACULTY) {
        printf("Invalid type. Setting as Student.\n");
        m->type = MEMBER_TYPE_STUDENT;
    }
    m->borrowed_count = 0;
    m->next = memberHead;
    memberHead = m;
    printf("Member registered successfully.\n");
}

int memberHasActiveBorrows(int memberId) {
    Transaction *curr = transHead;
    while (curr) {
        if (!curr->is_returned && curr->memberId == memberId)
            return 1;
        curr = curr->next;
    }
    return 0;
}

int memberInAnyWaitlist(int memberId) {
    for (int i = 0; i < BOOK_TABLE_SIZE; ++i) {
        Book *b = bookTable[i];
        while (b) {
            WaitNode *w = b->wait_front;
            while (w) {
                if (w->memberId == memberId) return 1;
                w = w->next;
            }
            b = b->next;
        }
    }
    return 0;
}

void deleteMember() {
    int id;
    printf("Enter Member ID to delete: ");
    scanf("%d", &id);

    if (memberHasActiveBorrows(id)) {
        printf("Cannot delete member. They have active borrowed books.\n");
        return;
    }
    if (memberInAnyWaitlist(id)) {
        printf("Cannot delete member. They are in a waitlist.\n");
        return;
    }

    Member *curr = memberHead, *prev = NULL;
    while (curr && curr->id != id) {
        prev = curr;
        curr = curr->next;
    }
    if (!curr) {
        printf("Member not found.\n");
        return;
    }
    if (prev) prev->next = curr->next;
    else memberHead = curr->next;

    free(curr);
    printf("Member deleted successfully.\n");
}

/* ================== BOOK FUNCTIONS ================== */

Book* findBook(int id) {
    int index = hashBookId(id);
    Book *curr = bookTable[index];
    while (curr) {
        if (curr->id == id) return curr;
        curr = curr->next;
    }
    return NULL;
}

void addBook() {
    Book *b = (Book *)malloc(sizeof(Book));
    if (!b) {
        printf("Memory allocation failed for book.\n");
        return;
    }
    printf("Enter Book ID: ");
    scanf("%d", &b->id);
    if (findBook(b->id)) {
        printf("Book with this ID already exists.\n");
        free(b);
        return;
    }

    printf("Enter Title: ");
    getchar();
    fgets(b->title, MAX_TITLE_LEN, stdin);
    b->title[strcspn(b->title, "\n")] = '\0';

    printf("Enter Author: ");
    fgets(b->author, MAX_AUTHOR_LEN, stdin);
    b->author[strcspn(b->author, "\n")] = '\0';

    printf("Enter Total Copies: ");
    scanf("%d", &b->total_copies);
    if (b->total_copies < 1) b->total_copies = 1;
    b->available_copies = b->total_copies;

    b->wait_front = b->wait_rear = NULL;

    int index = hashBookId(b->id);
    b->next = bookTable[index];
    bookTable[index] = b;

    printf("Book added successfully.\n");
}

int bookHasActiveTransactions(int bookId) {
    Transaction *curr = transHead;
    while (curr) {
        if (!curr->is_returned && curr->bookId == bookId)
            return 1;
        curr = curr->next;
    }
    return 0;
}

void removeBook() {
    int id;
    printf("Enter Book ID to remove: ");
    scanf("%d", &id);

    Book *b = findBook(id);
    if (!b) {
        printf("Book not found.\n");
        return;
    }

    if (b->available_copies != b->total_copies) {
        printf("Cannot remove. Some copies are currently borrowed.\n");
        return;
    }
    if (!isWaitlistEmpty(b)) {
        printf("Cannot remove. Waitlist is not empty.\n");
        return;
    }
    if (bookHasActiveTransactions(id)) {
        printf("Cannot remove. Active transactions exist.\n");
        return;
    }

    int index = hashBookId(id);
    Book *curr = bookTable[index], *prev = NULL;
    while (curr && curr->id != id) {
        prev = curr;
        curr = curr->next;
    }
    if (!curr) return;

    if (prev) prev->next = curr->next;
    else bookTable[index] = curr->next;

    free(curr);
    printf("Book removed successfully.\n");
}

void searchBook() {
    int id;
    printf("Enter Book ID to search: ");
    scanf("%d", &id);
    Book *b = findBook(id);
    if (!b) {
        printf("Book not found.\n");
        return;
    }
    printf("Book ID: %d\nTitle: %s\nAuthor: %s\nTotal Copies: %d\nAvailable: %d\n",
           b->id, b->title, b->author, b->total_copies, b->available_copies);
    if (!isWaitlistEmpty(b)) {
        printf("Waitlist present.\n");
    }
}

/* ================== TRANSACTION FUNCTIONS ================== */

void addTransaction(int bookId, int memberId, Date borrowDate, Date dueDate) {
    Transaction *t = (Transaction *)malloc(sizeof(Transaction));
    if (!t) {
        printf("Memory allocation failed for transaction.\n");
        return;
    }
    t->id = nextTransId++;
    t->bookId = bookId;
    t->memberId = memberId;
    t->borrow_date = borrowDate;
    t->due_date = dueDate;
    t->is_returned = 0;
    t->fine = 0.0f;
    t->return_date = (Date){0,0,0};
    t->next = transHead;
    transHead = t;
}

Transaction* findActiveTransaction(int bookId, int memberId) {
    Transaction *curr = transHead;
    while (curr) {
        if (!curr->is_returned && curr->bookId == bookId && curr->memberId == memberId)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/* ================== BORROW / RETURN LOGIC ================== */

void borrowBook() {
    int bookId, memberId;
    printf("Enter Member ID: ");
    scanf("%d", &memberId);
    Member *m = findMember(memberId);
    if (!m) {
        printf("Member not found.\n");
        return;
    }

    printf("Enter Book ID: ");
    scanf("%d", &bookId);
    Book *b = findBook(bookId);
    if (!b) {
        printf("Book not found.\n");
        return;
    }

    if (m->borrowed_count >= maxBooksAllowed(m)) {
        printf("Borrow limit reached for this member.\n");
        return;
    }

    if (b->available_copies > 0) {
        b->available_copies--;
        m->borrowed_count++;

        Date today = getToday();
        Date due = addDays(today, MAX_BORROW_DAYS);
        addTransaction(bookId, memberId, today, due);

        printf("Book issued successfully.\n");
        printf("Borrow Date: ");
        printDate(today);
        printf("\nDue Date: ");
        printDate(due);
        printf("\n");
    } else {
        printf("No copies available. Adding member to waitlist.\n");
        enqueueWait(b, memberId);
    }
}

void autoAssignFromWaitlist(Book *b) {
    if (b->available_copies <= 0) return;
    if (isWaitlistEmpty(b)) return;

    int nextMemberId = dequeueWait(b);
    Member *m = findMember(nextMemberId);
    if (!m) {
        printf("Waitlisted member (ID %d) not found. Skipping.\n", nextMemberId);
        return;
    }
    if (m->borrowed_count >= maxBooksAllowed(m)) {
        printf("Waitlisted member (ID %d) has reached borrow limit. Skipping.\n", nextMemberId);
        return;
    }

    b->available_copies--;
    m->borrowed_count++;
    Date today = getToday();
    Date due = addDays(today, MAX_BORROW_DAYS);
    addTransaction(b->id, nextMemberId, today, due);

    printf("Book auto-assigned from waitlist to Member ID %d.\n", nextMemberId);
    printf("Borrow Date: ");
    printDate(today);
    printf("\nDue Date: ");
    printDate(due);
    printf("\n");
}

void returnBook() {
    int bookId, memberId;
    printf("Enter Member ID: ");
    scanf("%d", &memberId);
    Member *m = findMember(memberId);
    if (!m) {
        printf("Member not found.\n");
        return;
    }

    printf("Enter Book ID: ");
    scanf("%d", &bookId);
    Book *b = findBook(bookId);
    if (!b) {
        printf("Book not found.\n");
        return;
    }

    Transaction *t = findActiveTransaction(bookId, memberId);
    if (!t) {
        printf("Active transaction not found for this member and book.\n");
        return;
    }

    Date today = getToday();
    t->return_date = today;
    t->is_returned = 1;

    int diff = daysBetween(t->due_date, today);
    if (diff > 0) {
        float fine = diff * FINE_PER_DAY;
        if (fine > MAX_FINE) fine = MAX_FINE;
        t->fine = fine;
    } else {
        t->fine = 0.0f;
    }

    b->available_copies++;
    if (m->borrowed_count > 0) m->borrowed_count--;

    printf("Book returned successfully.\n");
    printf("Return Date: ");
    printDate(today);
    printf("\nFine: Rs %.2f\n", t->fine);

    // Auto assign to next in waitlist if any
    autoAssignFromWaitlist(b);
}

/* ================== REPORTING FUNCTIONS ================== */

void listAllBooks() {
    printf("===== ALL BOOKS =====\n");
    for (int i = 0; i < BOOK_TABLE_SIZE; ++i) {
        Book *b = bookTable[i];
        while (b) {
            printf("ID: %d | Title: %s | Author: %s | Total: %d | Available: %d\n",
                   b->id, b->title, b->author, b->total_copies, b->available_copies);
            b = b->next;
        }
    }
}

void listAllMembers() {
    printf("===== ALL MEMBERS =====\n");
    Member *m = memberHead;
    while (m) {
        printf("ID: %d | Name: %s | Type: %s | Active borrows: %d\n",
               m->id, m->name,
               (m->type == MEMBER_TYPE_STUDENT ? "Student" : "Faculty"),
               m->borrowed_count);
        m = m->next;
    }
}

void listActiveTransactions() {
    printf("===== ACTIVE TRANSACTIONS =====\n");
    Transaction *t = transHead;
    while (t) {
        if (!t->is_returned) {
            printf("TID: %d | BookID: %d | MemberID: %d | Borrow: ",
                   t->id, t->bookId, t->memberId);
            printDate(t->borrow_date);
            printf(" | Due: ");
            printDate(t->due_date);
            printf("\n");
        }
        t = t->next;
    }
}

void listOverdueTransactions() {
    Date today = getToday();
    printf("===== OVERDUE TRANSACTIONS =====\n");
    Transaction *t = transHead;
    int any = 0;
    while (t) {
        if (!t->is_returned && daysBetween(t->due_date, today) > 0) {
            any = 1;
            printf("TID: %d | BookID: %d | MemberID: %d | Borrow: ",
                   t->id, t->bookId, t->memberId);
            printDate(t->borrow_date);
            printf(" | Due: ");
            printDate(t->due_date);
            printf("\n");
        }
        t = t->next;
    }
    if (!any) printf("No overdue books.\n");
}

void memberTransactions() {
    int memberId;
    printf("Enter Member ID: ");
    scanf("%d", &memberId);

    Member *m = findMember(memberId);
    if (!m) {
        printf("Member not found.\n");
        return;
    }
    printf("===== TRANSACTIONS FOR MEMBER %d (%s) =====\n", m->id, m->name);
    Transaction *t = transHead;
    int any = 0;
    while (t) {
        if (t->memberId == memberId) {
            any = 1;
            printf("TID: %d | BookID: %d | Borrow: ", t->id, t->bookId);
            printDate(t->borrow_date);
            printf(" | Due: ");
            printDate(t->due_date);
            if (t->is_returned) {
                printf(" | Returned: ");
                printDate(t->return_date);
                printf(" | Fine: Rs %.2f", t->fine);
            } else {
                printf(" | Not yet returned");
            }
            printf("\n");
        }
        t = t->next;
    }
    if (!any) printf("No transactions found.\n");
}

/* ================== CLEANUP ================== */

void freeAll() {
    // free books and waitlists
    for (int i = 0; i < BOOK_TABLE_SIZE; ++i) {
        Book *b = bookTable[i];
        while (b) {
            Book *nextB = b->next;
            WaitNode *w = b->wait_front;
            while (w) {
                WaitNode *nextW = w->next;
                free(w);
                w = nextW;
            }
            free(b);
            b = nextB;
        }
    }

    // free members
    Member *m = memberHead;
    while (m) {
        Member *nextM = m->next;
        free(m);
        m = nextM;
    }

    // free transactions
    Transaction *t = transHead;
    while (t) {
        Transaction *nextT = t->next;
        free(t);
        t = nextT;
    }
}

/* ================== MENU ================== */

void printMenu() {
    printf("\n===== LIBRARY MANAGEMENT SYSTEM =====\n");
    printf("1. Add Book\n");
    printf("2. Search Book\n");
    printf("3. Remove Book\n");
    printf("4. Register Member\n");
    printf("5. Delete Member\n");
    printf("6. Borrow Book\n");
    printf("7. Return Book\n");
    printf("8. List All Books\n");
    printf("9. List All Members\n");
    printf("10. List Active Transactions\n");
    printf("11. List Overdue Transactions\n");
    printf("12. Show Member Transactions\n");
    printf("0. Exit\n");
    printf("Enter your choice: ");
}

/* ================== MAIN ================== */

int main() {
    int choice;

    // initialize hash table
    for (int i = 0; i < BOOK_TABLE_SIZE; ++i)
        bookTable[i] = NULL;

    do {
        printMenu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting.\n");
            break;
        }

        switch (choice) {
            case 1: addBook(); break;
            case 2: searchBook(); break;
            case 3: removeBook(); break;
            case 4: registerMember(); break;
            case 5: deleteMember(); break;
            case 6: borrowBook(); break;
            case 7: returnBook(); break;
            case 8: listAllBooks(); break;
            case 9: listAllMembers(); break;
            case 10: listActiveTransactions(); break;
            case 11: listOverdueTransactions(); break;
            case 12: memberTransactions(); break;
            case 0: printf("Exiting...\n"); break;
            default: printf("Invalid choice. Try again.\n");
        }

    } while (choice != 0);

    freeAll();
    return 0;
}