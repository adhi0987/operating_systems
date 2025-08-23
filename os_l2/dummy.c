#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void handle_sigint(int sig) {
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);
    while (1) {
        pause();
    }
    return 0;
}