// S1.c - Main Server Side Code // 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>


#define PORT 1231  // Port number for S1 server
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

#include <errno.h>
#include <dirent.h>

/* Function prototype for client processing thread */
void *prcclient(void *socket_desc);  // Handles client requests in a separate thread

/* Create directory structure from a full file path */
void make_directory_from_path(const char *full_file_path) {
    /* Create a working copy of the path */
    char dir_path[512];  // Buffer for directory path manipulation
    snprintf(dir_path, sizeof(dir_path), "%s", full_file_path);  // Copy original path

    /* Remove filename portion if path contains a file */
    // strrchr() function accepts two argument − searches for the last occurrence of the character c (an unsigned char) in the string pointed to, by the argument str.
    char *last_slash = strrchr(dir_path, '/');  // Find last slash in path
    if (last_slash != NULL) {
        *last_slash = '\0';  // Truncate at last slash to remove filename
    }

    /* Initialize path builder */
    char current_path[512] = "";  // Will build path incrementally
    char *token = strtok(dir_path, "/");  // Start tokenizing remaining path

    /* Process each directory component */
    while (token != NULL) {
        /* Build path one component at a time */
        strcat(current_path, "/");         // Add leading slash
        strcat(current_path, token);       // Append next directory component

        /* Create directory with read/write/execute permissions for owner,
           read/execute for group and others (0755) */
        if (mkdir(current_path, 0755) == -1) {
            /* Ignore "directory already exists" errors */
            if (errno != EEXIST) {  // EEXIST means directory already exists
                perror("mkdir error");    // Print actual error for other cases
            }
        }

        /* Get next path component */
        token = strtok(NULL, "/");  // Continue tokenizing
    }
}

// Helper function to ensure complete messages are sent
void send_full_message(int sock, const char *msg) {
    int total = 0;
    int bytes_left = strlen(msg);
    int n;
    
    while(total < strlen(msg)) {
        n = send(sock, msg + total, bytes_left, 0);
        if (n == -1) break;
        total += n;
        bytes_left -= n;
    }
}

// Function to establish a connection to a server
// Parameters:
//   ip - IP address of the server to connect to
//   port - port number of the server
// Returns:
//   socket file descriptor on success, -1 on failure
int connect_to_server(const char *ip, int port) {
    int sock;  // Socket file descriptor
    struct sockaddr_in addr;  // Structure to hold server address information

    // Create a TCP socket (AF_INET for IPv4, SOCK_STREAM for TCP)
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) return -1;

    // Set up the server address structure
    addr.sin_family = AF_INET;  // IPv4 address family
    addr.sin_port = htons(port);  // Convert port to network byte order
    inet_pton(AF_INET, ip, &addr.sin_addr);  // Convert IP address string to binary form

    // Attempt to connect to the server
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);  // Close socket if connection fails
        return -1;
    }
    return sock;  // Return socket descriptor on successful connection
}

// Function to handle file upload from client to server
// Parameters:
//   sock - socket connected to the client
//   filename - name of the file being uploaded
//   dest_path - destination path where file should be stored
// Updated handle_uploadf function in S1.c
void handle_uploadf(int sock, char *filename, char *dest_path) {
    char *home = getenv("HOME");
    if (!home) {
        send_full_message(sock, "Error: Cannot get HOME environment.\n");
        return;
    }

    // Verify file extension
    char *ext = strrchr(filename, '.');
    if (!ext) {
        send_full_message(sock, "Error: File has no extension.\n");
        return;
    }

    // Check supported file types
    if (strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && 
        strcmp(ext, ".txt") != 0 && strcmp(ext, ".zip") != 0) {
        char err_msg[100];
        snprintf(err_msg, sizeof(err_msg), 
                "Error: Unsupported file type '%s'. Only .c, .pdf, .txt, and .zip allowed.\n",
                ext);
        send_full_message(sock, err_msg);
        return;
    }

    // First, receive file size from client
    char size_buf[32];
    memset(size_buf, 0, sizeof(size_buf));
    int size_bytes = recv(sock, size_buf, sizeof(size_buf), 0);
    if (size_bytes <= 0) {
        send_full_message(sock, "Error: Failed to receive file size.\n");
        return;
    }
    
    long expected_size = atol(size_buf);
    
    // Send acknowledgment to client
    send(sock, "OK", 2, 0);

    // Determine target server
    char server_prefix[4] = "S1";
    int port = 0;
    
    if (strcmp(ext, ".pdf") == 0) {
        port = 1202;
        strcpy(server_prefix, "S2");
    }
    else if (strcmp(ext, ".txt") == 0) {
        port = 1203;
        strcpy(server_prefix, "S3");
    }
    else if (strcmp(ext, ".zip") == 0) {
        port = 1206;
        strcpy(server_prefix, "S4");
    }

    // Prepare full file path (temporary storage on S1)
    char full_file_path[512];
    if (strncmp(dest_path, "~S1", 3) == 0) {
        snprintf(full_file_path, sizeof(full_file_path), "%s/S1%s/%s", 
                home, dest_path + 3, filename);
    } else {
        snprintf(full_file_path, sizeof(full_file_path), "%s/%s", 
                home, filename);
    }

    // Create directory structure if needed
    make_directory_from_path(full_file_path);

    // Open file for writing
    FILE *file = fopen(full_file_path, "wb");
    if (!file) {
        char err_msg[100];
        snprintf(err_msg, sizeof(err_msg), "Error: Cannot create file '%s'\n", filename);
        send_full_message(sock, err_msg);
        return;
    }

    // Receive exact amount of file content
    char buffer[BUFFER_SIZE];
    long bytes_received_total = 0;
    int bytes_received;
    int transfer_ok = 1;
    
    while (bytes_received_total < expected_size) {
        long remaining = expected_size - bytes_received_total;
        int to_recv = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        
        bytes_received = recv(sock, buffer, to_recv, 0);
        if (bytes_received <= 0) {
            transfer_ok = 0;
            break;
        }
        
        if (fwrite(buffer, 1, bytes_received, file) != bytes_received) {
            transfer_ok = 0;
            break;
        }
        
        bytes_received_total += bytes_received;
    }

    fclose(file);

    if (!transfer_ok || bytes_received_total != expected_size) {
        remove(full_file_path);
        send_full_message(sock, "Error: File transfer failed.\n");
        return;
    }

    // Handle .c files (keep on S1)
    if (port == 0) {
        printf("[S1] Stored %s at %s\n", filename, full_file_path);
        send_full_message(sock, "File uploaded successfully.\n");
        return;
    }

    // For non-.c files, forward to appropriate server
    char modified_path[512];
    if (strncmp(dest_path, "~S1", 3) == 0) {
        // Special handling for .txt files to ensure S3 path is correct
        if (strcmp(ext, ".txt") == 0) {
            // Ensure path starts with ~S3
            if (dest_path[3] == '/') {
                snprintf(modified_path, sizeof(modified_path), "~S3%s", dest_path + 3);
            } else {
                snprintf(modified_path, sizeof(modified_path), "~S3/%s", dest_path + 3);
            }
        } else {
            snprintf(modified_path, sizeof(modified_path), "~%s%s", 
                    server_prefix, dest_path + 3);
        }
    } else {
        snprintf(modified_path, sizeof(modified_path), "~%s/%s", 
                server_prefix, dest_path);
    }

    // Connect to target server
    int s_sock = connect_to_server("127.0.0.1", port);
    if (s_sock == -1) {
        remove(full_file_path);
        send_full_message(sock, "Error: Could not connect to target server.\n");
        return;
    }

    // Send upload command with modified path
    char forward_cmd[512];
    snprintf(forward_cmd, sizeof(forward_cmd), "uploadf %s %s", 
            filename, modified_path);
    send(s_sock, forward_cmd, strlen(forward_cmd), 0);
    usleep(200000); // Increased delay to ensure server is ready

    // Send file content
    file = fopen(full_file_path, "rb");
    if (!file) {
        close(s_sock);
        remove(full_file_path);
        send_full_message(sock, "Error: Could not reopen file for forwarding.\n");
        return;
    }

    while ((bytes_received = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(s_sock, buffer, bytes_received, 0) != bytes_received) {
            perror("Error forwarding file to target server");
            break;
        }
    }

    fclose(file);
    close(s_sock);
    remove(full_file_path);

    printf("[S1] Forwarded %s to %s server at %s\n", filename, server_prefix, modified_path);
    send_full_message(sock, "File uploaded successfully.\n");
}

// Function to handle file download requests from clients
// Parameters:
//   sock - socket connected to the client
//   filepath - path of the file requested for download
void handle_downlf(int sock, char *filepath) {
    // Get the user's home directory path
    char *home = getenv("HOME");
    if (!home) {
        send(sock, "Error: Cannot get HOME environment.\n", 36, 0);
        return;
    }

    // Extract file extension from the path
    char *ext = strrchr(filepath, '.'); // find last occurence .
    if (!ext) {
        send(sock, "Error: File has no extension.\n", 30, 0);
        return;
    }

    // Determine which server contains the file based on extension
    int port = 0;  // 0 means local server (S1)
    char server_prefix[4] = "S1";  // Default to S1 server
    
    // Set port and server prefix based on file type
    if (strcmp(ext, ".pdf") == 0) {
        port = 1202; // S2's port
        strcpy(server_prefix, "S2");
    } else if (strcmp(ext, ".txt") == 0) {
        port = 1203; // S3's port
        strcpy(server_prefix, "S3");
    } else if (strcmp(ext, ".zip") == 0) {
        port = 1206; // S4's port
        strcpy(server_prefix, "S4");
    } else if (strcmp(ext, ".c") != 0) {
        // Reject unsupported file types
        send(sock, "Error: Unsupported file type.\n", 30, 0);
        return;
    }

    // Handle .c files locally (port remains 0)
    if (port == 0) {
        char full_file_path[512];
        // Construct full path, handling ~S1 prefix if present
        if (strncmp(filepath, "~S1", 3) == 0) {
            snprintf(full_file_path, sizeof(full_file_path), "%s/S1%s", home, filepath + 3); // skip first 3 characters of the string
        } else {
            snprintf(full_file_path, sizeof(full_file_path), "%s/%s", home, filepath);
        }

        // Open the file for reading in binary mode
        FILE *file = fopen(full_file_path, "rb");
        if (!file) {
            long err_size = -1;
            send(sock, &err_size, sizeof(err_size), 0); // Signal error
            return;
        }
         // Get file size
        fseek(file, 0, SEEK_END);
        long filesize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Send file size first
        send(sock, &filesize, sizeof(filesize), 0);
        // Read file contents and send to client
        // Send file data
        char buffer[BUFFER_SIZE];
        int bytes;
        while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(sock, buffer, bytes, 0);
        }
        fclose(file);
        printf("[S1] Sent .c file %s to client\n", full_file_path);
        return;
    }

    // For non-.c files, forward request to appropriate secondary server
    const char *server_ip = "127.0.0.1";  // Localhost IP
    int s_sock = connect_to_server(server_ip, port);
    if (s_sock == -1) {
        send(sock, "Error: Could not connect to secondary server.\n", 45, 0);
        return;
    }

    // Modify the path to use the correct server prefix
    char modified_path[512];
    if (strncmp(filepath, "~S1", 3) == 0) {
        // Replace ~S1 with the appropriate server prefix (~S2, ~S3, etc.)
        snprintf(modified_path, sizeof(modified_path), "~%s%s", server_prefix, filepath + 3);
    } else {
        // Keep original path if no server prefix was specified
        strcpy(modified_path, filepath);
    }

    // Send download command to secondary server
    char forward_cmd[512];
    snprintf(forward_cmd, sizeof(forward_cmd), "downlf %s", modified_path);
    send(s_sock, forward_cmd, strlen(forward_cmd), 0);

    // Relay file contents from secondary server to client
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = recv(s_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        send(sock, buffer, bytes, 0);
        if (bytes < BUFFER_SIZE) break;  // Last chunk of data received
    }

    close(s_sock);  // Close connection to secondary server
    printf("[S1] Forwarded %s file request to port %d\n", ext, port);
}

// Function to handle file removal requests
// Parameters:
//   sock - socket connected to the client
//   filepath - path of the file to be removed
void handle_removef(int sock, char *filepath) {
    // Get the user's home directory path
    char *home = getenv("HOME");
    if (!home) {
        send(sock, "Error: Cannot get HOME environment.\n", 36, 0);
        return;
    }

    // Extract file extension from the path
    char *ext = strrchr(filepath, '.');
    if (!ext) {
        send(sock, "Error: File has no extension.\n", 30, 0);
        return;
    }

    // Determine which server contains the file based on extension
    int port = 0;  // 0 means local server (S1)
    char server_prefix[4] = "S1";  // Default to S1 server
    
    // Set port and server prefix based on file type
    if (strcmp(ext, ".pdf") == 0) {
        port = 1202; // S2's port
        strcpy(server_prefix, "S2");
    } else if (strcmp(ext, ".txt") == 0) {
        port = 1203; // S3's port
        strcpy(server_prefix, "S3");
    } else if (strcmp(ext, ".zip") == 0) {
        port = 1206; // S4's port
        strcpy(server_prefix, "S4");
    }

    // Handle .c files locally (port remains 0)
    if (port == 0 && strcmp(ext, ".c") == 0) {
        char full_file_path[512];
        // Construct full path, handling ~S1 prefix if present
        if (strncmp(filepath, "~S1", 3) == 0) {
            snprintf(full_file_path, sizeof(full_file_path), "%s/S1%s", home, filepath + 3);
        } else {
            snprintf(full_file_path, sizeof(full_file_path), "%s/%s", home, filepath);
        }

        // Attempt to remove the file
        if (remove(full_file_path) == 0) {
            printf("[S1] Removed .c file: %s\n", full_file_path);
            send(sock, "File removed successfully.\n", 28, 0);
        } else {
            perror("Error removing file");
            send(sock, "Error: File could not be removed.\n", 35, 0);
        }
        return;
    } else if (port == 0) {
        // Reject unsupported file types
        send(sock, "Error: Unsupported file type.\n", 30, 0);
        return;
    }

    // For non-.c files, forward request to appropriate secondary server
    const char *server_ip = "127.0.0.1";  // Localhost IP
    int s_sock = connect_to_server(server_ip, port);
    if (s_sock == -1) {
        send(sock, "Error: Could not connect to secondary server.\n", 45, 0);
        return;
    }

    // Modify the path to use the correct server prefix
    char modified_path[512];
    if (strncmp(filepath, "~S1", 3) == 0) {
        // Replace ~S1 with the appropriate server prefix (~S2, ~S3, etc.)
        snprintf(modified_path, sizeof(modified_path), "~%s%s", server_prefix, filepath + 3);
    } else {
        // Keep original path if no server prefix was specified
        strcpy(modified_path, filepath);
    }

    // Send remove command to secondary server
    char forward_cmd[512];
    snprintf(forward_cmd, sizeof(forward_cmd), "removef %s", modified_path);
    send(s_sock, forward_cmd, strlen(forward_cmd), 0);

    // Forward the server's response back to the client
    char buffer[BUFFER_SIZE];
    int bytes = recv(s_sock, buffer, BUFFER_SIZE, 0);
    if (bytes > 0) {
        send(sock, buffer, bytes, 0);
    } else {
        send(sock, "Error: No response from secondary server.\n", 42, 0);
    }

    close(s_sock);  // Close connection to secondary server
    printf("[S1] Forwarded remove request for %s to port %d\n", ext, port);
}

// Function to handle tar file download requests
// Parameters:
//   sock - socket connected to the client
//   filetype - type of files to include in the tar archive (.c, .pdf, or .txt)
void handle_downltar(int sock, char *filetype) {
    // Verify requested filetype is supported
    if (strcmp(filetype, ".c") != 0 && 
        strcmp(filetype, ".pdf") != 0 && 
        strcmp(filetype, ".txt") != 0) {
        send(sock, "Error: Invalid filetype. Only .c, .pdf, or .txt supported.\n", 58, 0);
        return;
    }

    // Handle .c files locally
    if (strcmp(filetype, ".c") == 0) {
        char *home = getenv("HOME");
        if (!home) {
            send(sock, "Error: Cannot get HOME environment.\n", 36, 0);
            return;
        }

        // Create tar command to bundle all .c files from S1 directory
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "tar -cf - -C %s/S1 $(find %s/S1 -type f -name '*.c') 2>/dev/null", home, home);
        
        // Execute the tar command and get a file pointer to its output
        FILE *tar = popen(cmd, "r");
        if (!tar) {
            send(sock, "Error: Failed to create tar file.\n", 34, 0);
            return;
        }

        // Stream tar output directly to client
        char buffer[BUFFER_SIZE];
        int bytes;
        while ((bytes = fread(buffer, 1, BUFFER_SIZE, tar)) > 0) {
            send(sock, buffer, bytes, 0);
        }
        pclose(tar);  // Close the tar process
        printf("[S1] Created and sent cfiles.tar\n");
    } else {
        // Forward request for .pdf or .txt files to appropriate server
        int port = (strcmp(filetype, ".pdf") == 0) ? 1202 : 1203;  // S2 or S3
        
        // Connect to the secondary server
        int s_sock = connect_to_server("127.0.0.1", port);
        if (s_sock == -1) {
            send(sock, "Error: No files found.\n", 24, 0);
            return;
        }

        // Send the downltar command to the secondary server
        char forward_cmd[512];
        snprintf(forward_cmd, sizeof(forward_cmd), "downltar %s", filetype);
        send(s_sock, forward_cmd, strlen(forward_cmd), 0);

        // Check if we actually get a tar file (peek at first bytes)
        char buffer[BUFFER_SIZE];
        int bytes = recv(s_sock, buffer, BUFFER_SIZE, MSG_PEEK);
        if (bytes <= 0) {
            send(sock, "Error: No files found.\n", 24, 0);
            close(s_sock);
            return;
        }

        // Forward the actual tar data to client
        while ((bytes = recv(s_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            send(sock, buffer, bytes, 0);
            if (bytes < BUFFER_SIZE) break;  // Last chunk of data
        }
        close(s_sock);  // Close connection to secondary server
        printf("[S1] Forwarded %s tar file from port %d\n", filetype, port);
    }
}

// compares the two strings
int cmp_strings(const void *a, const void *b) {
    const char *pa = *(const char **)a;
    const char *pb = *(const char **)b;
    return strcmp(pa, pb);
}

// Helper function to get local files matching a specific extension
// Parameters:
//   base_path - directory path to search in
//   file_ext - file extension to match (without dot)
//   file_list - pointer to array of strings that will hold the results
//   count - pointer to integer tracking number of files found
void get_local_files(const char *base_path, const char *file_ext, char ***file_list, int *count) {
    // Construct find command to locate files with given extension
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "find %s -type f -name '*.%s' -printf '%%f\\n' | sort", base_path, file_ext);
    
    // Execute the command and get output stream
    FILE *fp = popen(cmd, "r");
    if (!fp) return;

    // Read command output line by line
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        
        // Expand file list array and add new filename
        *file_list = realloc(*file_list, (*count + 1) * sizeof(char*));
        (*file_list)[*count] = strdup(line);
        (*count)++;
    }
    pclose(fp);  // Close the command stream
}

// Main function to handle file listing requests
// Parameters:
//   sock - client connection socket
//   pathname - directory path to list files from (may contain ~S1 prefix)
void handle_dispfnames(int sock, char *pathname) {
    // Get user's home directory
    char *home = getenv("HOME");
    if (!home) {
        send(sock, "Error: Cannot get HOME environment\n", 34, 0);
        return;
    }

    // Build full absolute path from given pathname
    char full_path[512];
    if (strncmp(pathname, "~S1", 3) == 0) {
        // Replace ~S1 with actual S1 directory path
        snprintf(full_path, sizeof(full_path), "%s/S1%s", home, pathname + 3);
    } else {
        // Use path relative to home directory
        snprintf(full_path, sizeof(full_path), "%s/%s", home, pathname);
    }

    // Verify the directory exists
    DIR *dir = opendir(full_path);
    if (!dir) {
        char err_msg1[] = "Error: Directory not found.\n";
        send(sock, err_msg1, strlen(err_msg1), 0);
        send(sock, "ENDOFLIST\n", 10, 0);  // Add end marker
        return;
    }
    closedir(dir);

    // Initialize file collection variables
    int file_count = 0;
    char *files[1024];  // Static array to hold up to 1024 filenames

    // 1. Get .c files from local S1 directory
    DIR *dp;
    struct dirent *ep;
    dp = opendir(full_path);
    if (dp) {
        while ((ep = readdir(dp)) != NULL && file_count < 1023) {
            if (ep->d_type == DT_REG) {  // Only regular files
                char *ext = strrchr(ep->d_name, '.');
                if (ext && strcmp(ext, ".c") == 0) {
                    files[file_count++] = strdup(ep->d_name);  // Copy filename
                }
            }
        }
        closedir(dp);
    }

    // 2. Get .pdf files from S2 server (port 1202)
    int s2_sock = connect_to_server("127.0.0.1", 1202);
    if (s2_sock != -1) {
        // Modify path to use S2 prefix
        char s2_path[512];
        snprintf(s2_path, sizeof(s2_path), "~S2%s", strstr(pathname, "~S1") ? pathname + 3 : pathname);
        
        // Send dispfnames command to S2
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "dispfnames %s", s2_path);
        send(s2_sock, cmd, strlen(cmd), 0);
        
        // Receive and process response
        char response[BUFFER_SIZE];
        int bytes;
        while ((bytes = recv(s2_sock, response, BUFFER_SIZE - 1, 0)) > 0 && file_count < 1023) {
            response[bytes] = '\0';  // Null-terminate
            // Parse response line by line
            char *line = strtok(response, "\n");
            while (line != NULL && file_count < 1023) {
                if (strstr(line, ".pdf")) {  // Only take PDF files
                    files[file_count++] = strdup(line); // Copy filename
                }
                line = strtok(NULL, "\n"); // split the strings into the tokens
            }
        }
        close(s2_sock);
    }

    // 3. Get .txt files from S3 server (port 1203)
    int s3_sock = connect_to_server("127.0.0.1", 1203);
    if (s3_sock != -1) {
        // Modify path to use S3 prefix
        char s3_path[512];
        snprintf(s3_path, sizeof(s3_path), "~S3%s", strstr(pathname, "~S1") ? pathname + 3 : pathname);
        
        // Send dispfnames command to S3
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "dispfnames %s", s3_path);
        send(s3_sock, cmd, strlen(cmd), 0);
        
        // Receive and process response
        char response[BUFFER_SIZE];
        int bytes;
        while ((bytes = recv(s3_sock, response, BUFFER_SIZE - 1, 0)) > 0 && file_count < 1023) {
            response[bytes] = '\0';
            char *line = strtok(response, "\n");
            while (line != NULL && file_count < 1023) {
                if (strstr(line, ".txt")) {  // Only take TXT files
                    files[file_count++] = strdup(line);
                }
                line = strtok(NULL, "\n");
            }
        }
        close(s3_sock);
    }

    // 4. Get .zip files from S4 server (port 1206)
    int s4_sock = connect_to_server("127.0.0.1", 1206);
    if (s4_sock != -1) {
        // Modify path to use S4 prefix
        char s4_path[512];
        snprintf(s4_path, sizeof(s4_path), "~S4%s", strstr(pathname, "~S1") ? pathname + 3 : pathname);
        
        // Send dispfnames command to S4
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "dispfnames %s", s4_path);
        send(s4_sock, cmd, strlen(cmd), 0);
        
        // Receive and process response
        char response[BUFFER_SIZE];
        int bytes;
        while ((bytes = recv(s4_sock, response, BUFFER_SIZE - 1, 0)) > 0 && file_count < 1023) {
            response[bytes] = '\0';
            char *line = strtok(response, "\n");
            while (line != NULL && file_count < 1023) {
                if (strstr(line, ".zip")) {  // Only take ZIP files
                    files[file_count++] = strdup(line);
                }
                line = strtok(NULL, "\n");
            }
        }
        close(s4_sock);
    }

    // Sort all collected files alphabetically
    qsort(files, file_count, sizeof(char *), (int (*)(const void *, const void *))strcmp);

    // Send sorted file list to client (one per line)
    for (int i = 0; i < file_count; i++) {
        send(sock, files[i], strlen(files[i]), 0);
        send(sock, "\n", 1, 0);
        free(files[i]);  // Free allocated filename strings
    }

    // Send end of list marker
    send(sock, "ENDOFLIST\n", 10, 0);

    // Handle empty directory case
    if (file_count == 0) {
        send(sock, "(No files found)\n", 17, 0);
    }
}

// Thread function to handle client connections
// Parameters:
//   socket_desc - pointer to client socket file descriptor
void *prcclient(void *socket_desc) {
    int sock = *(int *)socket_desc;
    free(socket_desc);

    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);  // Leave space for null terminator
        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        
        // Remove any trailing newlines or carriage returns
        while (bytes > 0 && (buffer[bytes-1] == '\n' || buffer[bytes-1] == '\r')) {
            buffer[--bytes] = '\0';
        }
        
        // Skip empty commands
        if (strlen(buffer) == 0) continue;
        
        char command[20];
        char args[10][512];  // Allow up to 10 arguments (9 files + destination)
        int arg_count = 0;
        
        // Parse command and arguments
        char *token = strtok(buffer, " ");
        if (token) {
            strncpy(command, token, sizeof(command) - 1);
            command[sizeof(command) - 1] = '\0';
        } else {
            continue;  // No command found
        }
        
        while ((token = strtok(NULL, " ")) != NULL && arg_count < 10) {
            strncpy(args[arg_count], token, sizeof(args[0])-1);
            args[arg_count][sizeof(args[0])-1] = '\0';
            arg_count++;
        }

        // Process commands
        if (strcmp(command, "uploadf") == 0 && arg_count >= 2) {
            // Single file upload: uploadf filename destination
            handle_uploadf(sock, args[0], args[1]);
        }
        else if (strcmp(command, "downlf") == 0 && arg_count >= 1) {
            for (int i = 0; i < arg_count; i++) {
                handle_downlf(sock, args[i]);
            }
        } 
        else if (strcmp(command, "removef") == 0 && arg_count >= 1) {
            for (int i = 0; i < arg_count; i++) {
                handle_removef(sock, args[i]);
            }
        } 
        else if (strcmp(command, "downltar") == 0 && arg_count == 1) {
            handle_downltar(sock, args[0]);
        } 
        else if (strcmp(command, "dispfnames") == 0 && arg_count == 1) {
            handle_dispfnames(sock, args[0]);
        } 
        else {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), 
                    "Invalid command: '%s' or incorrect arguments (%d provided).\n", 
                    command, arg_count);
            send_full_message(sock, error_msg);
        }
    }

    close(sock);
    pthread_exit(NULL);
}

// Main server function
int main() {
    int server_fd, client_sock, *new_sock;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(client);

    // Create S1 directory in user's home folder at startup
    char *home = getenv("HOME");  // get environment
    if (home) {
        char s1_folder[512];
        snprintf(s1_folder, sizeof(s1_folder), "%s/S1", home);
        mkdir(s1_folder, 0755);  // Create with read/write/execute permissions for owner
    }

    // Create server socket (IPv4, TCP)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address structure
    server.sin_family = AF_INET;          // IPv4
    server.sin_port = htons(PORT);        // Port number (converted to network byte order)
    server.sin_addr.s_addr = INADDR_ANY;  // Accept connections on any interface

    // Bind socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // Start listening for incoming connections
    listen(server_fd, MAX_CLIENTS);
    printf("S1 server running on port %d...\n", PORT);

    // Main server loop - accept incoming connections
    while ((client_sock = accept(server_fd, (struct sockaddr *)&client, &client_len))) {
        pthread_t t;
        // Allocate memory for new client socket (passed to thread)
        new_sock = malloc(sizeof(int));
        *new_sock = client_sock;
        
        // Create new thread to handle client connection
        pthread_create(&t, NULL, prcclient, (void *)new_sock);
        
        // Detach thread so resources are automatically freed on exit
        pthread_detach(t);
    }

    // Close server socket (unreachable in normal operation)
    close(server_fd);
    return 0;
}