#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BLOCK_SIZE 3
#define SLEEP_DURATION 2

// Global arrays to store block state
int A[BLOCK_SIZE][BLOCK_SIZE];  // Original puzzle block
int B[BLOCK_SIZE][BLOCK_SIZE];  // Current state of block

void draw_block() {
    printf("\033[H\033[J");  // Clear screen
    printf("┌───┬───┬───┐\n");
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
            printf("├───┼───┼───┤\n");
    }
    printf("└───┴───┴───┘\n");
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
        fprintf(stderr, "Usage: %s blockno read_fd write_fd rn1_write_fd rn2_write_fd cn1_write_fd cn2_write_fd\n", argv[0]);
        exit(1);
    }

    int block_no = atoi(argv[1]);
    int read_fd = atoi(argv[2]);
    int write_fd = atoi(argv[3]);
    int row_neighbor1_wfd = atoi(argv[4]);
    int row_neighbor2_wfd = atoi(argv[5]);
    int col_neighbor1_wfd = atoi(argv[6]);
    int col_neighbor2_wfd = atoi(argv[7]);

    // Redirect stdin to read from pipe
    dup2(read_fd, STDIN_FILENO);
    close(read_fd);

    // Redirect stdout
    FILE *write_pipe = fdopen(write_fd, "w");

    char command[100];
    while (fgets(command, sizeof(command), stdin) != NULL) {
        if (command[0] == 'n') {
            int values[9];
            sscanf(command + 1, "%d %d %d %d %d %d %d %d %d", 
                   &values[0], &values[1], &values[2], 
                   &values[3], &values[4], &values[5], 
                   &values[6], &values[7], &values[8]);
            handle_new_block(values);
        }
        else if (command[0] == 'p') {
            int cell, digit;
            sscanf(command + 1, "%d %d", &cell, &digit);
            
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
            FILE* row_neighbor1 = fdopen(row_neighbor1_wfd, "w");
            FILE* row_neighbor2 = fdopen(row_neighbor2_wfd, "w");
            
            fprintf(row_neighbor1, "r %d %d %d\n", row, digit, write_fd);
            fflush(row_neighbor1);
            
            fprintf(row_neighbor2, "r %d %d %d\n", row, digit, write_fd);
            fflush(row_neighbor2);
            
            int response1, response2;
            scanf("%d %d", &response1, &response2);
            
            if (response1 != 0 || response2 != 0) {
                show_error("Row conflict");
                continue;
            }

            // Check column conflicts with neighbors
            FILE* col_neighbor1 = fdopen(col_neighbor1_wfd, "w");
            FILE* col_neighbor2 = fdopen(col_neighbor2_wfd, "w");
            
            fprintf(col_neighbor1, "c %d %d %d\n", col, digit, write_fd);
            fflush(col_neighbor1);
            
            fprintf(col_neighbor2, "c %d %d %d\n", col, digit, write_fd);
            fflush(col_neighbor2);
            
            scanf("%d %d", &response1, &response2);
            
            if (response1 != 0 || response2 != 0) {
                show_error("Column conflict");
                continue;
            }

            // If we get here, the move is valid
            B[row][col] = digit;
            draw_block();
        }
        else if (command[0] == 'r') {
            int row, digit, response_fd;
            sscanf(command + 1, "%d %d %d", &row, &digit, &response_fd);
            int conflict = check_row_conflict(row, digit);
            FILE* response_pipe = fdopen(response_fd, "w");
            fprintf(response_pipe, "%d\n", conflict);
            fflush(response_pipe);
        }
        else if (command[0] == 'c') {
            int col, digit, response_fd;
            sscanf(command + 1, "%d %d %d", &col, &digit, &response_fd);
            int conflict = check_column_conflict(col, digit);
            FILE* response_pipe = fdopen(response_fd, "w");
            fprintf(response_pipe, "%d\n", conflict);
            fflush(response_pipe);
        }
        else if (command[0] == 'q') {
            printf("\nBye...\n");
            fflush(stdout);
            sleep(SLEEP_DURATION);
            exit(0);
        }
    }

    return 0;
}