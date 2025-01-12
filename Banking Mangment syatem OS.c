#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>



#define MAX_PAGES 5
#define MAX_ACCOUNTS 100
#define MAX_MEMORY 5
#define QUANTUM 2
#define MAX_USERNAME_LENGTH 20
#define MAX_PASSWORD_LENGTH 20

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

typedef struct {
    int id;         // Account ID or transaction ID
    int burst_time; // Time needed for this transaction (burst time)
    int remaining_time; // Remaining time for this transaction
} Transaction;


// Account structure
typedef struct {
    int id;
    int balance;
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
} Account;

typedef struct {
    int page_id;      // Page ID (account or transaction ID)
    int last_used;    // Timestamp of last usage (for LRU)
} Page;

// Global variables
Account accounts[MAX_ACCOUNTS];
Transaction transactions[100];  // Array to store transactions
int transaction_count = 0;
int account_count = 0;
pthread_mutex_t account_mutex[MAX_ACCOUNTS];
sem_t memory_sem;
Page memory[MAX_PAGES]; 
int memory_index = 0;
int time_stamp = 0; 


// ----------------------------
// Function Prototypes
// ----------------------------
void create_account();
int login();
void deposit(int id, int amount);
void withdraw(int id, int amount);
int check_balance(int id);
void *transaction_thread(void *args);
void display_menu();
void handle_menu(int option, int account_id);
void allocate_memory(int id);
void display_memory();
void round_robin_scheduling();

void ipc_send(const char *message);
void ipc_receive();
void lru_page_replacement(int id);
void display_lru_memory();
int mainMenu();
void logout() ;
// ----------------------------
// System Call Interface
// ----------------------------
void create_account() {
    char username[MAX_USERNAME_LENGTH], password[MAX_PASSWORD_LENGTH];
    int initial_balance;

    printf("Enter a new username: ");
    scanf("%s", username);
    printf("Enter a password: ");
    scanf("%s", password);
    printf("Enter initial balance: ");
    scanf("%d", &initial_balance);

    // Create new account
    pthread_mutex_lock(&account_mutex[account_count]);
    accounts[account_count].id = account_count + 1;
    accounts[account_count].balance = initial_balance;
    strncpy(accounts[account_count].username, username, MAX_USERNAME_LENGTH);
    strncpy(accounts[account_count].password, password, MAX_PASSWORD_LENGTH);
    pthread_mutex_init(&account_mutex[account_count], NULL);
    account_count++;
    pthread_mutex_unlock(&account_mutex[account_count - 1]);

    printf(GREEN "Account created successfully. Account ID: %d\n" RESET, account_count);
   mainMenu() ;
}

int login() {
    char username[MAX_USERNAME_LENGTH], password[MAX_PASSWORD_LENGTH];

    printf("Enter username: ");
    scanf("%s", username);
    printf("Enter password: ");
    scanf("%s", password);

    for (int i = 0; i < account_count; i++) {
        if (strcmp(accounts[i].username, username) == 0 && strcmp(accounts[i].password, password) == 0) {
            printf(GREEN "Login successful for account ID: %d\n" RESET, accounts[i].id);
            return accounts[i].id;
        }
    }

    printf(RED "Login failed: Incorrect username or password\n" RESET);
    mainMenu();
    
}

void deposit(int id, int amount) {
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].id == id) {
            pthread_mutex_lock(&account_mutex[i]);
            accounts[i].balance += amount;
            printf(GREEN "Deposited %d to account %d. New balance: %d\n" RESET, amount, id, accounts[i].balance);
            pthread_mutex_unlock(&account_mutex[i]);
            // Add transaction to round robin queue
            transactions[transaction_count].id = id;
            transactions[transaction_count].burst_time = 5; // For simplicity, assume a fixed burst time for deposit
            transactions[transaction_count].remaining_time = transactions[transaction_count].burst_time;
            transaction_count++;
            
             // Function calls fixed
            allocate_memory(id);
            display_memory();
            round_robin_scheduling();
            ipc_send("money deposited");
            ipc_receive();
            lru_page_replacement(id);
            display_lru_memory();
            return;
            
        }
    }
    printf(RED "Account %d not found.\n" RESET, id);
}

void withdraw(int id, int amount) {
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].id == id) {
            pthread_mutex_lock(&account_mutex[i]);
            if (accounts[i].balance >= amount) {
                accounts[i].balance -= amount;
                printf(GREEN "Withdrew %d from account %d. New balance: %d\n" RESET, amount, id, accounts[i].balance);
            } else {
                printf(RED "Insufficient funds in account %d.\n" RESET, id);
            }
            pthread_mutex_unlock(&account_mutex[i]);
            transactions[transaction_count].id = id;
            transactions[transaction_count].burst_time = 7; // Assume burst time for withdraw is 7
            transactions[transaction_count].remaining_time = transactions[transaction_count].burst_time;
            transaction_count++;
            
           // Function calls fixed
            allocate_memory(id);
            display_memory();
            round_robin_scheduling();
            ipc_send("money withdraw");
            ipc_receive();
            lru_page_replacement(id);
            display_lru_memory();

            return;
            
        }
    }
    printf(RED "Account %d not found.\n" RESET, id);
}

int check_balance(int id) {
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].id == id) {
            pthread_mutex_lock(&account_mutex[i]);
            int balance = accounts[i].balance;
            pthread_mutex_unlock(&account_mutex[i]);
            // Add transaction to round robin queue
            transactions[transaction_count].id = id;
            transactions[transaction_count].burst_time = 3; // Assume burst time for balance check is 3
            transactions[transaction_count].remaining_time = transactions[transaction_count].burst_time;
            transaction_count++;
            
           // Function calls fixed
            allocate_memory(id);
            display_memory();
            round_robin_scheduling();
            ipc_send("balance checked");
            ipc_receive();
            lru_page_replacement(id);
            display_lru_memory();
            return balance;
           
        }
    }
    printf(RED "Account %d not found.\n" RESET, id);
    return -1;
}
// ----------------------------
// Multithreading and Synchronization
// ----------------------------
void *transaction_thread(void *args) {
    int *data = (int *)args;
    int id = data[0];
    int type = data[1]; // 0 for deposit, 1 for withdraw
    int amount = data[2];
    if (type == 0) {
        deposit(id, amount);  // Deposit transaction
    } else if (type == 1) {
        withdraw(id, amount);  // Withdraw transaction
    }
    return NULL;
}

// ----------------------------
// Memory Management
// ----------------------------
 
void allocate_memory(int id) {
    sem_wait(&memory_sem);
    
    // Search for the ID in memory
    for (int i = 0; i < memory_index; i++) {
        if (memory[i].page_id == id) {
            // If the page already exists, update the last used time and return
            memory[i].last_used = time_stamp++;
            sem_post(&memory_sem);
            return;
        }
    }
    
    // If memory is full, evict the least recently used page (LRU)
    if (memory_index == MAX_PAGES) {
        lru_page_replacement(id);
    }
    
    // Add new page
    memory[memory_index].page_id = id;
    memory[memory_index].last_used = time_stamp++;
    memory_index++;
    
    sem_post(&memory_sem);
}

void lru_page_replacement(int id) {
    int lru_index = 0;
    for (int i = 1; i < memory_index; i++) {
        if (memory[i].last_used < memory[lru_index].last_used) {
            lru_index = i;
        }
    }
    // Evict the least recently used page
    printf("Evicting page %d from memory (LRU)\n", memory[lru_index].page_id);
    memory[lru_index].page_id = id;
    memory[lru_index].last_used = time_stamp++;
}

void display_memory() {
    printf("Memory (Pages): ");
    for (int i = 0; i < memory_index; i++) {
        printf("%d ", memory[i].page_id);
    }
    printf("\n");
}

void display_lru_memory() {
    printf("LRU Memory Pages: ");
    for (int i = 0; i < memory_index; i++) {
        printf("%d ", memory[i].page_id);
    }
    printf("\n");
}

// ----------------------------
// CPU Scheduling (Round Robin) & Gantt Chart
// ----------------------------
 void round_robin_scheduling() {
    int time = 0; // Current time
    int total_waiting_time = 0;
    int completed_transactions = 0;

    for (int i = 0; i < transaction_count; i++) {
        transactions[i].remaining_time = transactions[i].burst_time;
    }

    printf("\nGantt Chart:\n");
    while (completed_transactions < transaction_count) {
        for (int i = 0; i < transaction_count; i++) {
            if (transactions[i].remaining_time > 0) {
                int time_to_execute = (transactions[i].remaining_time <= QUANTUM) ? transactions[i].remaining_time : QUANTUM;
                printf("T%d [%d-%d] ", transactions[i].id, time, time + time_to_execute);
                time += time_to_execute;
                transactions[i].remaining_time -= time_to_execute;

                if (transactions[i].remaining_time == 0) {
                    completed_transactions++;
                    int waiting_time = time - transactions[i].burst_time;
                    total_waiting_time += waiting_time;
                }
            }
        }
    }
    printf("\n");

    double average_waiting_time = (double)total_waiting_time / transaction_count;
    printf("\nAverage Waiting Time: %.2f\n", average_waiting_time);
}

// ----------------------------
// Inter-Process Communication
// ----------------------------
void ipc_send(const char *message) {
    printf("Message Sent: %s\n", message);
}

void ipc_receive() {
    printf("Message Received: Transaction Completed\n");
}

// ----------------------------
// Menu and User Interaction
// ----------------------------
void display_menu() {
    printf(BLUE "\n===== MENU =====\n" RESET);
    printf("1. Deposit\n");
    printf("2. Withdraw\n");
    printf("3. Check Balance\n");
    printf("4. Exit\n");
    printf("Enter your choice: ");
}

void handle_menu(int option, int account_id) {
    int amount;
    switch (option) {
        case 1:
            printf("Enter amount to deposit: ");
            scanf("%d", &amount);
            deposit(account_id, amount);
            break;
        case 2:
            printf("Enter amount to withdraw: ");
            scanf("%d", &amount);
            withdraw(account_id, amount);
            break;
        case 3:
            printf("Balance: %d\n", check_balance(account_id));
            break;
        case 4:
            printf(GREEN "Exiting...\n" RESET);
            break;
        default:
            printf(RED "Invalid choice. Please try again.\n" RESET);
    }
}
// Main Menu
int mainMenu() {
     // User login or account creation
    int account_id = -1;
    int option;
  
    while (account_id == -1) {
        printf("\n===== Welcome to the Banking System =====\n");
        printf("1. Login\n");
        printf("2. Create New Account\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &option);

        switch (option) {
            case 1:
                account_id = login();
                break;
            case 2:
                create_account();
                account_id = account_count;  // Automatically log in after account creation
                break;
            case 3:
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
}

// ----------------------------
// Main Function
// ----------------------------
int main() {
    // Initialize memory semaphore
    sem_init(&memory_sem, 0, 1);
    int account_id = -1;
    int option;
   while (account_id == -1) {
        printf("\n===== Welcome to the Banking System =====\n");
        printf("1. Login\n");
        printf("2. Create New Account\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &option);

        switch (option) {
            case 1:
                account_id = login();
                break;
            case 2:
                create_account();
                account_id = account_count;  // Automatically log in after account creation
                break;
            case 3:
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
  
    
    // After login or account creation, display menu for transactions
    do {
        printf("\n===== Main Menu =====\n");
        printf("1. Deposit\n");
        printf("2. Withdraw\n");
        printf("3. Check Balance\n");
        printf("4. logout\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &option);

        switch (option) {
            case 1: {
                int amount;
                printf("Enter amount to deposit: ");
                scanf("%d", &amount);
                
                // Prepare data to be passed to transaction_thread
                int transaction_data[3] = {account_id, 0, amount}; // 0 indicates deposit

                // Create and start the transaction thread
                pthread_t transaction_tid;
                if (pthread_create(&transaction_tid, NULL, transaction_thread, (void *)transaction_data) != 0) {
                    printf("Error: Failed to create thread for deposit.\n");
                    continue;
                }

                // Wait for the transaction thread to finish
                pthread_join(transaction_tid, NULL);
                printf("Deposit completed.\n");
            }
            break;
            case 2: {
                int amount;
                printf("Enter amount to withdraw: ");
                scanf("%d", &amount);
                
                // Prepare data to be passed to transaction_thread
                int transaction_data[3] = {account_id, 1, amount}; // 1 indicates withdraw

                // Create and start the transaction thread
                pthread_t transaction_tid;
                if (pthread_create(&transaction_tid, NULL, transaction_thread, (void *)transaction_data) != 0) {
                    printf("Error: Failed to create thread for withdraw.\n");
                    continue;
                }

                // Wait for the transaction thread to finish
                pthread_join(transaction_tid, NULL);
                printf("Withdrawal completed.\n");
            }
            break;
            case 3:
                printf("Balance: %d\n", check_balance(account_id));
                break;
                
           case 4:
                 logout();
                 break;
            case 5:
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }

    } while (option != 5);

    // Cleanup and exit
    sem_destroy(&memory_sem);
    return 0;
}
// Logout
void logout() {
    
    printf("\nYou have logged out successfully.\n");
    mainMenu();
}
