#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BLOCK_SIZE 3
#define SLEEP_DURATION 2

// Global arrays to store block state
int A[BLOCK_SIZE][BLOCK_SIZE];  // Original puzzle block
int B[BLOCK_SIZE][BLOCK_SIZE];  // Current state of block

void draw_block() {
    printf("\033[H\033[J");  // Clear screen
    printf("+---+---+---+\n");
    for (int i = 0; i < BLOCK_SIZE; i++) {
        printf("│");
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (B[i][j] == 0)
                printf(" _ ");
            else
                printf(" %d ", B[i][j]);
            printf("│");
        }
        printf("\n");
        if (i < BLOCK_SIZE - 1)
            printf("+---+---+---+\n");
    }
    printf("+---+---+---+\n");
    fflush(stdout);
}

void handle_new_block(int block_values[9]) {
    int idx = 0;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            A[i][j] = block_values[idx];
            B[i][j] = block_values[idx];
            idx++;
        }
    }
    draw_block();
}

int check_row_conflict(int row, int digit) {
    for (int j = 0; j < BLOCK_SIZE; j++) {
        if (B[row][j] == digit) return 1;
    }
    return 0;
}

int check_column_conflict(int col, int digit) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (B[i][col] == digit) return 1;
    }
    return 0;
}

void show_error(const char* message) {
    printf("\n%s\n", message);
    fflush(stdout);
    sleep(SLEEP_DURATION);
    draw_block();
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s blockno rd wd rn1wd rn2wd cn1wd cn2wd\n", argv[0]);
        exit(1);
    }

    int block_no = atoi(argv[1]);
    int read_fd = atoi(argv[2]);
    int write_fd = atoi(argv[3]);
    int row_neighbor1_wd = atoi(argv[4]);
    int row_neighbor2_wd = atoi(argv[5]);
    int col_neighbor1_wd = atoi(argv[6]);
    int col_neighbor2_wd = atoi(argv[7]);

    // Redirect stdin to read from pipe
    dup2(read_fd, STDIN_FILENO);
    close(read_fd);

    char command;
    while (scanf(" %c", &command) == 1) {
        if (command == 'n') {
            int values[9];
            for (int i = 0; i < 9; i++) {
                scanf("%d", &values[i]);
            }
            handle_new_block(values);
        }
        else if (command == 'p') {
            int cell, digit;
            scanf("%d %d", &cell, &digit);
            
            int row = cell / 3;
            int col = cell % 3;

            // Check if trying to modify original puzzle
            if (A[row][col] != 0) {
                show_error("Read-only cell");
                continue;
            }

            // Check block conflict
            int block_conflict = 0;
            for (int i = 0; i < BLOCK_SIZE && !block_conflict; i++) {
                for (int j = 0; j < BLOCK_SIZE; j++) {
                    if (B[i][j] == digit && (i != row || j != col)) {
                        block_conflict = 1;
                        break;
                    }
                }
            }
            if (block_conflict) {
                show_error("Block conflict");
                continue;
            }

            // Check row conflicts with neighbors
            dprintf(row_neighbor1_wd, "r %d %d %d\n", row, digit, write_fd);
            dprintf(row_neighbor2_wd, "r %d %d %d\n", row, digit, write_fd);
            
            int response;
            scanf("%d", &response);
            if (response != 0) {
                show_error("Row conflict");
                continue;
            }
            scanf("%d", &response);
            if (response != 0) {
                show_error("Row conflict");
                continue;
            }

            // Check column conflicts with neighbors
            dprintf(col_neighbor1_wd, "c %d %d %d\n", col, digit, write_fd);
            dprintf(col_neighbor2_wd, "c %d %d %d\n", col, digit, write_fd);
            
            scanf("%d", &response);
            if (response != 0) {
                show_error("Column conflict");
                continue;
            }
            scanf("%d", &response);
            if (response != 0) {
                show_error("Column conflict");
                continue;
            }

            // If we get here, the move is valid
            B[row][col] = digit;
            draw_block();
        }
        else if (command == 'r') {
            int row, digit, response_fd;
            scanf("%d %d %d", &row, &digit, &response_fd);
            int conflict = check_row_conflict(row, digit);
            dprintf(response_fd, "%d\n", conflict);
        }
        else if (command == 'c') {
            int col, digit, response_fd;
            scanf("%d %d %d", &col, &digit, &response_fd);
            int conflict = check_column_conflict(col, digit);
            dprintf(response_fd, "%d\n", conflict);
        }
        else if (command == 'q') {
            printf("\nBye...\n");
            fflush(stdout);
            sleep(SLEEP_DURATION);
            exit(0);
        }
    }

    return 0;
}