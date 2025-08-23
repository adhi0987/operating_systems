#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "boardgen.c"  // Include the provided board generator

#define BLOCK_COUNT 9
#define BLOCK_SIZE 3
#define BOARD_SIZE 9

// Structure to store pipe file descriptors for each block
typedef struct {
    int read_fd;   // Read end of pipe
    int write_fd;  // Write end of pipe
} PipePair;

// Get row neighbors for a block
void get_row_neighbors(int block, int* n1, int* n2) {
    int row = block / 3;
    int start = row * 3;
    *n1 = (block + 1) % 3 + start;
    *n2 = (block + 2) % 3 + start;
}

// Get column neighbors for a block
void get_column_neighbors(int block, int* n1, int* n2) {
    int col = block % 3;
    *n1 = (block + 3) % 9;
    if (*n1 / 3 == block / 3) *n1 = (block + 6) % 9;
    *n2 = (block + 6) % 9;
    if (*n2 / 3 == block / 3) *n2 = (block + 3) % 9;
}

// Launch xterm for a block process
void launch_block(int block_num, PipePair pipes[BLOCK_COUNT], int row_n1, int row_n2, int col_n1, int col_n2) {
    char geometry[50];
    int x = (block_num % 3) * 300 + 100;
    int y = (block_num / 3) * 300 + 100;
    
    snprintf(geometry, sizeof(geometry), "17x8+%d+%d", x, y);
    
    char block_title[20];
    snprintf(block_title, sizeof(block_title), "Block %d", block_num);
    
    char block_num_str[10], pipe_in_str[10], pipe_out_str[10];
    char rn1_str[10], rn2_str[10], cn1_str[10], cn2_str[10];
    
    snprintf(block_num_str, sizeof(block_num_str), "%d", block_num);
    snprintf(pipe_in_str, sizeof(pipe_in_str), "%d", pipes[block_num].read_fd);
    snprintf(pipe_out_str, sizeof(pipe_out_str), "%d", pipes[block_num].write_fd);
    snprintf(rn1_str, sizeof(rn1_str), "%d", pipes[row_n1].write_fd);
    snprintf(rn2_str, sizeof(rn2_str), "%d", pipes[row_n2].write_fd);
    snprintf(cn1_str, sizeof(cn1_str), "%d", pipes[col_n1].write_fd);
    snprintf(cn2_str, sizeof(cn2_str), "%d", pipes[col_n2].write_fd);
    
    execlp("xterm", "xterm",
           "-T", block_title,
           "-fa", "Monospace",
           "-fs", "15",
           "-geometry", geometry,
           "-bg", "gray",
           "-e", "./block",
           block_num_str, pipe_in_str, pipe_out_str,
           rn1_str, rn2_str, cn1_str, cn2_str,
           NULL);
    
    perror("execlp failed");
    exit(1);
}

void print_help() {
    printf("\nFoodoku Commands:\n");
    printf("h - Show this help message\n");
    printf("n - Start new game\n");
    printf("p b c d - Place digit d in cell c of block b\n");
    printf("s - Show solution\n");
    printf("q - Quit game\n");
    printf("\nBlocks and cells are numbered 0-8 in row-major order\n\n");
}

int main() {
    PipePair pipes[BLOCK_COUNT];
    pid_t children[BLOCK_COUNT];
    int board[BOARD_SIZE][BOARD_SIZE];
    int solution[BOARD_SIZE][BOARD_SIZE];
    char command;
    
    // Create pipes for all blocks
    for (int i = 0; i < BLOCK_COUNT; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe creation failed");
            exit(1);
        }
        pipes[i].read_fd = pipefd[0];
        pipes[i].write_fd = pipefd[1];
    }
    
    // Fork child processes for each block
    for (int i = 0; i < BLOCK_COUNT; i++) {
        children[i] = fork();
        
        if (children[i] == -1) {
            perror("fork failed");
            exit(1);
        }
        
        if (children[i] == 0) {  // Child process
            // Get row and column neighbors
            int row_n1, row_n2, col_n1, col_n2;
            get_row_neighbors(i, &row_n1, &row_n2);
            get_column_neighbors(i, &col_n1, &col_n2);
            
            // Close unused pipe ends
            for (int j = 0; j < BLOCK_COUNT; j++) {
                if (j != i) close(pipes[j].read_fd);
                if (j != i && j != row_n1 && j != row_n2 && 
                    j != col_n1 && j != col_n2) {
                    close(pipes[j].write_fd);
                }
            }
            
            launch_block(i, pipes, row_n1, row_n2, col_n1, col_n2);
        }
    }
    
    // Parent process
    // Close read ends of all pipes in parent
    for (int i = 0; i < BLOCK_COUNT; i++) {
        close(pipes[i].read_fd);
    }
    
    // Main game loop
    while (1) {
        printf("Enter command (h for help): ");
        scanf(" %c", &command);
        
        switch (command) {
            case 'h':
                print_help();
                break;
                
            case 'n': {
                // Generate new board
                newboard(board, solution);
                
                // Send board to blocks
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    // Calculate block boundaries
                    int row_start = (i / 3) * 3;
                    int col_start = (i % 3) * 3;
                    
                    // Send new board command
                    dprintf(pipes[i].write_fd, "n ");
                    
                    // Send block data
                    for (int r = 0; r < 3; r++) {
                        for (int c = 0; c < 3; c++) {
                            dprintf(pipes[i].write_fd, "%d ", 
                                    board[row_start + r][col_start + c]);
                        }
                    }
                    dprintf(pipes[i].write_fd, "\n");
                }
                break;
            }
                
            case 'p': {
                int block, cell, digit;
                scanf("%d %d %d", &block, &cell, &digit);
                
                // Validate input
                if (block < 0 || block >= 9 || cell < 0 || cell >= 9 || 
                    digit < 1 || digit > 9) {
                    printf("Invalid input parameters\n");
                    break;
                }
                
                // Send place command to appropriate block
                dprintf(pipes[block].write_fd, "p %d %d\n", cell, digit);
                break;
            }
                
            case 's': {
                // Send solution to blocks
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    int row_start = (i / 3) * 3;
                    int col_start = (i % 3) * 3;
                    
                    dprintf(pipes[i].write_fd, "n ");
                    for (int r = 0; r < 3; r++) {
                        for (int c = 0; c < 3; c++) {
                            dprintf(pipes[i].write_fd, "%d ", 
                                    solution[row_start + r][col_start + c]);
                        }
                    }
                    dprintf(pipes[i].write_fd, "\n");
                }
                break;
            }
                
            case 'q':
                // Send quit command to all blocks
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    dprintf(pipes[i].write_fd, "q\n");
                }
                
                // Wait for all children to exit
                for (int i = 0; i < BLOCK_COUNT; i++) {
                    wait(NULL);
                }
                
                printf("Game over\n");
                exit(0);
                
            default:
                printf("Invalid command\n");
        }
    }
    
    return 0;
}