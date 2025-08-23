#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include<sys/types.h>
#include<linux/limits.h>

#define MAX_USERS 5000

// Structure to store user information
typedef struct {
    uid_t u_id;
    char user_login[256];
} Userinfo;

// Global variables
Userinfo users[MAX_USERS];
int user_count = 0;
int file_count = 0;

// Function to check if a string ends with a specific suffix
int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
    {
        return 0;
    }
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len)
    {
        return 0;
    }
    
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

// Function to build a map of user IDs to login names
void build_user_map() {
    FILE *passwd_file = fopen("/etc/passwd", "r");
    if (!passwd_file) {
        perror("Failed to open /etc/passwd");
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), passwd_file) && user_count < MAX_USERS) {
        char *username = strtok(line, ":");
        if (!username) continue;
        
        char *skip = strtok(NULL, ":");
        if (!skip) continue;
        
        char *uid_str = strtok(NULL, ":");
        if (!uid_str) continue;
        
        uid_t uid = atoi(uid_str);
        users[user_count].u_id = uid;
        strncpy(users[user_count].user_login, username, sizeof(users[user_count].user_login) - 1);
        users[user_count].user_login[sizeof(users[user_count].user_login) - 1] = '\0';
        user_count++;
    }
    
    fclose(passwd_file);
}

// Function to get login name from user ID
const char* get_login_from_uid(uid_t uid) {
    static char unknown[16];
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].u_id == uid) {
            return users[i].user_login;
        }
    }
    
    // If user not found in our cache, try to get from system
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        // Add to our cache for future lookups
        if (user_count < MAX_USERS) {
            users[user_count].u_id = uid;
            strncpy(users[user_count].user_login, pw->pw_name, sizeof(users[user_count].user_login) - 1);
            users[user_count].user_login[sizeof(users[user_count].user_login) - 1] = '\0';
            user_count++;
        }
        return pw->pw_name;
    }
    
    // User not found
    snprintf(unknown, sizeof(unknown), "%u", uid);
    return unknown;
}

// Function to recursively search directories
void search_directory(const char *dir_path, const char *extension) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char path[PATH_MAX];
    
    if ((dir = opendir(dir_path)) == NULL) {
        fprintf(stderr, "Error opening directory: %s\n", dir_path);
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Create full path
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        
        // Get file information
        if (lstat(path, &file_stat) < 0) {
            fprintf(stderr, "Error getting file stats: %s\n", path);
            continue;
        }
        
        // If it's a directory, recurse into it
        if (S_ISDIR(file_stat.st_mode)) {
            search_directory(path, extension);
        }
        // If it's a regular file with the target extension
        else if (S_ISREG(file_stat.st_mode)) {
            char dot_ext[256];
            snprintf(dot_ext, sizeof(dot_ext), ".%s", extension);
            
            if (ends_with(entry->d_name, dot_ext)) {
                file_count++;
                
                // Get owner information
                const char *owner = get_login_from_uid(file_stat.st_uid);                
                printf("|\t%-3d\t|\t%-6s\t|\t%-8ld\t|\t%-100s\t|\n", file_count,owner,(long)file_stat.st_size,path);
            }
        }
    }
    
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <extension>\n", argv[0]);
        return 1;
    }   
    const char *directory = argv[1];
    const char *extension = argv[2];
    // Build user ID to login name map
    build_user_map();
    // Print header
    printf("+---------------+-----------------------+-----------------------+-----------------------------------------------------------------------------------------------------------+\n");
    printf("|\t%-3s\t|\t%-6s\t\t|\t%-8s\t|\t\t\t\t\t\t\t\t%s\t\t\t\t\t\t\t|\n", "NO", "OWNER", "SIZE", "NAME");
    printf("+---------------+-----------------------+-----------------------+-----------------------------------------------------------------------------------------------------------+\n");    // Start searching
    search_directory(directory, extension);
    printf("+---------------+-----------------------+-----------------------+-----------------------------------------------------------------------------------------------------------+\n");    
    printf("+++ %d files match the extension %s\n", file_count, extension);
    return 0;
}