#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILENAME "foodep.txt"
#define ARRAY "done.txt"

// Function prototypes
void initialize_done_file(int totalNodes);
void process_node_dependencies(int rootNode);
void rebuild_node(int node, const char *dependencies);
void update_done_file(int node);
int is_node_rebuilt(int node);

void initialize_done_file(int totalNodes) {
    FILE *file = fopen(ARRAY, "w");
    if (file == NULL) {
        fprintf(stderr, "Unable to open done file for writing\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < totalNodes; i++) {
        fprintf(file, "0");
    }
    fclose(file);
}

int is_node_rebuilt(int node) {
    FILE *file = fopen(ARRAY, "r");
    if (file == NULL) {
        fprintf(stderr, "Unable to open done file for reading\n");
        exit(EXIT_FAILURE);
    }

    char rebuiltStatus[20];
    fgets(rebuiltStatus, sizeof(rebuiltStatus), file);
    fclose(file);

    return rebuiltStatus[node - 1] == '1';
}

void update_done_file(int node) {
    FILE *file = fopen(ARRAY, "r+");
    if (file == NULL) {
        fprintf(stderr, "Unable to open done file for updating\n");
        exit(EXIT_FAILURE);
    }

    char rebuiltStatus[20];
    fgets(rebuiltStatus, sizeof(rebuiltStatus), file);
    rewind(file);
    rebuiltStatus[node - 1] = '1';
    fputs(rebuiltStatus, file);
    fclose(file);
}

void rebuild_node(int node, const char *dependencies) {
    char cname[32];
    char *cp = (char *)dependencies;
    pid_t cpid;

    // Rebuild all dependencies
    while (sscanf(cp, "%s", cname) == 1) {
        if ((cpid = fork()) == 0) {
            execlp("./rebuild", "./rebuild", cname, "adithya", NULL);
            fprintf(stderr, "Failed to execute rebuild\n");
            exit(EXIT_FAILURE);
        } else {
            waitpid(cpid, NULL, 0);
        }
        cp += strlen(cname) + 1;
    }

    // Log rebuild process
    printf("foo%d rebuilt from dependencies: %s\n", node, dependencies);
}

void process_node_dependencies(int rootNode) {
    FILE *fp = fopen(FILENAME, "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open data file\n");
        exit(EXIT_FAILURE);
    }

    int totalNodes;
    char line[256];
    fgets(line, sizeof(line), fp);
    sscanf(line, "%d", &totalNodes);

    while (fgets(line, sizeof(line), fp)) {
        int node;
        char name[3], dependencies[256];
        char *dependencyList = NULL;
        char *token;
        
        sscanf(line, "%d %s %[^\n]", &node, name, dependencies);

        // Tokenizing the dependencies to properly process the list
        token = strtok(dependencies, " ");
        while (token != NULL) {
            if (dependencyList == NULL) {
                dependencyList = malloc(strlen(token) + 1);
                strcpy(dependencyList, token);
            } else {
                dependencyList = realloc(dependencyList, strlen(dependencyList) + strlen(token) + 2);
                strcat(dependencyList, " ");
                strcat(dependencyList, token);
            }
            token = strtok(NULL, " ");
        }

        if (node == rootNode) {
            if (is_node_rebuilt(node)) {
                break; // Node already rebuilt, skip processing
            }

            rebuild_node(node, dependencyList);
            update_done_file(node);
        }

        // Free allocated memory for dependency list
        if (dependencyList != NULL) {
            free(dependencyList);
        }
    }

    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        // Default to processing node 10 if no argument is provided
        process_node_dependencies(10);
    } else if (argc == 2) {
        // Initialize the done file if required and process the given node
        FILE *fp = fopen(FILENAME, "r");
        if (fp == NULL) {
            fprintf(stderr, "Unable to open data file\n");
            exit(EXIT_FAILURE);
        }

        char line[256];
        fgets(line, sizeof(line), fp);
        int totalNodes;
        sscanf(line, "%d", &totalNodes);
        fclose(fp);

        initialize_done_file(totalNodes);
        process_node_dependencies(atoi(argv[1]));
    } else {
        fprintf(stderr, "Invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
