#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "boardgen.c"

#define BOARD_SIZE 9
#define BLOCK_SIZE 3
#define NUM_BLOCKS 9

typedef struct {
    int row_neighbors[2];
    int col_neighbors[2];
} BlockNeighbors;

void print_help() {
    printf("\nFoodoku Commands:\n");
    printf("h - Show this help message\n");
    printf("n - Start new puzzle\n");
    printf("p b c d - Place digit d in cell c of block b\n");
    printf("s - Show solution\n");
    printf("q - Quit game\n\n");
}

BlockNeighbors get_block_neighbors(int block) {
    BlockNeighbors neighbors;
    int row = block / 3;
    int col = block % 3;
    
    // Row neighbors
    int idx = 0;
    for (int c = 0; c < 3; c++) {
        if (c != col) {
            neighbors.row_neighbors[idx++] = row * 3 + c;
        }
    }
    
    // Column neighbors
    idx = 0;
    for (int r = 0; r < 3; r++) {
        if (r != row) {
            neighbors.col_neighbors[idx++] = r * 3 + col;
        }
    }
    
    return neighbors;
}

void get_block_position(int block, int *x, int *y) {
    *x = 300 + (block % 3) * 300;
    *y = 100 + (block / 3) * 300;
}

void send_board_data(FILE* fp, int board[BOARD_SIZE][BOARD_SIZE], int block) {
    int start_row = (block / 3) * 3;
    int start_col = (block % 3) * 3;
    
    fprintf(fp, "n");
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            fprintf(fp, " %d", board[start_row + i][start_col + j]);
        }
    }
    fprintf(fp, "\n");
    fflush(fp);
}

int main() {
    int board[BOARD_SIZE][BOARD_SIZE];
    int solution[BOARD_SIZE][BOARD_SIZE];
    FILE* block_pipes[NUM_BLOCKS];
    pid_t child_pids[NUM_BLOCKS];
    
    // Fork child processes for each block
    for (int block = 0; block < NUM_BLOCKS; block++) {
        int pipes[2];
        if (pipe(pipes) == -1) {
            perror("pipe");
            exit(1);
        }
        
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork");
            exit(1);
        }
        
        if (pid == 0) {  // Child process
            close(pipes[1]);
            dup2(pipes[0], STDIN_FILENO);
            
            BlockNeighbors neighbors = get_block_neighbors(block);
            
            int x, y;
            get_block_position(block, &x, &y);
            
            char geometry[32];
            snprintf(geometry, sizeof(geometry), "17x8+%d+%d", x, y);
            
            char block_str[8], rd_str[8], wd_str[8];
            char rn1_str[8], rn2_str[8], cn1_str[8], cn2_str[8];
            
            snprintf(block_str, sizeof(block_str), "%d", block);
            snprintf(rd_str, sizeof(rd_str), "%d", pipes[0]);
            snprintf(wd_str, sizeof(wd_str), "%d", pipes[1]);
            
            // Prepare neighbor pipe descriptors
            BlockNeighbors block_neighbors = get_block_neighbors(block);
            snprintf(rn1_str, sizeof(rn1_str), "%d", block_neighbors.row_neighbors[0]);
            snprintf(rn2_str, sizeof(rn2_str), "%d", block_neighbors.row_neighbors[1]);
            snprintf(cn1_str, sizeof(cn1_str), "%d", block_neighbors.col_neighbors[0]);
            snprintf(cn2_str, sizeof(cn2_str), "%d", block_neighbors.col_neighbors[1]);
            
            char title[16];
            snprintf(title, sizeof(title), "Block %d", block);
            
            execlp("xterm", "xterm",
                   "-T", title,
                   "-fa", "Monospace",
                   "-fs", "15",
                   "-geometry", geometry,
                   "-bg", "white",
                   "-e", "./block",
                   block_str, rd_str, wd_str,
                   rn1_str, rn2_str, cn1_str, cn2_str,
                   NULL);
                   
            perror("execlp");
            exit(1);
        }
        
        // Parent process
        close(pipes[0]);
        block_pipes[block] = fdopen(pipes[1], "w");
        child_pids[block] = pid;
    }
    
    print_help();
    
    char command[100];
    while (fgets(command, sizeof(command), stdin) != NULL) {
        command[strcspn(command, "\n")] = 0;  // Remove newline
        
        if (command[0] == 'h') {
            print_help();
        }
        else if (command[0] == 'n') {
            newboard(board, solution);
            for (int block = 0; block < NUM_BLOCKS; block++) {
                send_board_data(block_pipes[block], board, block);
            }
        }
        else if (command[0] == 'p') {
            int block, cell, digit;
            if (sscanf(command+1, "%d %d %d", &block, &cell, &digit) == 3) {
                if (block >= 0 && block < NUM_BLOCKS &&
                    cell >= 0 && cell < BLOCK_SIZE * BLOCK_SIZE &&
                    digit >= 1 && digit <= 9) {
                    fprintf(block_pipes[block], "p %d %d\n", cell, digit);
                    fflush(block_pipes[block]);
                }
                else {
                    printf("Invalid input values\n");
                }
            }
        }
        else if (command[0] == 's') {
            for (int block = 0; block < NUM_BLOCKS; block++) {
                send_board_data(block_pipes[block], solution, block);
            }
        }
        else if (command[0] == 'q') {
            for (int block = 0; block < NUM_BLOCKS; block++) {
                fprintf(block_pipes[block], "q\n");
                fflush(block_pipes[block]);
            }
            
            // Wait for all children to exit
            for (int block = 0; block < NUM_BLOCKS; block++) {
                waitpid(child_pids[block], NULL, 0);
                fclose(block_pipes[block]);
            }
            
            printf("Game over. Thanks for playing!\n");
            break;
        }
    }
    
    return 0;
}