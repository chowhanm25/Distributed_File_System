
// Client-side implementation for Distributed File System Project
// Handles commands: uploadf, downlf, removef, downltar, dispfnames
#include <stdio.h>      // For input/output functions like printf(), scanf(), fopen(), etc.
#include <stdlib.h>     // For memory allocation (malloc, free), exit(), and general utilities
#include <string.h>     // For string operations like strcpy(), strcat(), strlen(), strcmp(), memset(), etc.
#include <unistd.h>     // For POSIX functions like read(), write(), close(), fork(), sleep(), etc.
#include <sys/socket.h> // For socket programming: socket(), bind(), connect(), send(), recv(), etc.
#include <arpa/inet.h>  // For functions related to IP address conversion like inet_pton(), htons(), etc.
#include <netinet/in.h> // For sockaddr_in structure used in networking
#include <pthread.h>    // For working with threads: pthread_create(), pthread_exit(), etc.
#include <errno.h>      // For error codes like EEXIST, EINVAL, etc.
#include <sys/stat.h>   // For mkdir(), stat(), and other file/directory-related operations

#define PORT 1231

#include <dirent.h> // This header is for reading directories — listing files and folders inside a directory.

#define SERVER_IP "127.0.0.1"  // Server IP Address 
#define BUFFER_SIZE 4096
/* Send a command string to the server through the socket */
void send_command(int sock, char *command) {
    // Send the complete command string to server
    // sock: The connected socket descriptor
    // command: The command string to send
    // strlen(command): Length of the command string
    // 0: Default flags for send() 
    send(sock, command, strlen(command), 0);
}

/* Receive and print response from server until complete */
/*
  The recv() Function: receiving data from a socket
    ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    sockfd: The socket descriptor
    buf: Buffer to store received data
    len: Maximum length of data to receive (BUFFER_SIZE in your case)
    flags: Optional flags (0 means no special behavior)
*/
void receive_response(int sock) {
    char buffer[BUFFER_SIZE];  // Buffer to store received data
    int bytes;  // Number of bytes received
    int expecting_more = 1;
   
    
    // Continuously receive data until server closes connection
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes] = '\0';  // Null-terminate the received data
        printf("%s", buffer);  // Print the received data
        
        // If received less than full buffer, we got all data
        if (bytes < BUFFER_SIZE) break;
    }
    if (strstr(buffer, "ENDOFLIST")) {
        expecting_more = 0;  // No more data after this
        char *end = strstr(buffer, "ENDOFLIST");
        *end = '\0';  // Terminate string before marker
    }
}

// Updated upload_file function.
void upload_file(int sock, char *filenames[], char *destination_path, int file_count) {
    // First verify all files exist
    for (int i = 0; i < file_count; i++) {
        if (access(filenames[i], F_OK) == -1) {
            printf("Error: File '%s' not found\n", filenames[i]);
            return;
        }
    }

    // Process each file individually
    for (int i = 0; i < file_count; i++) {
        // Build command for single file
        char command[512];
        snprintf(command, sizeof(command), "uploadf %s %s", filenames[i], destination_path);
        
        // Send the upload command
        send(sock, command, strlen(command), 0);
        usleep(100000); // Small delay to ensure server is ready

        // Open and get file size
        FILE *file = fopen(filenames[i], "rb");
        if (!file) {
            printf("Error opening file %s\n", filenames[i]);
            continue;
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // Send file size first
        char size_buf[32];
        snprintf(size_buf, sizeof(size_buf), "%ld", file_size);
        send(sock, size_buf, sizeof(size_buf), 0);
        
        // Wait for acknowledgment (should be "OK" or error message)
        char ack[BUFFER_SIZE];
        int ack_bytes = recv(sock, ack, sizeof(ack), MSG_PEEK);
        if (ack_bytes > 0) {
            ack[ack_bytes] = '\0';
            // If server sent an error message, display it and skip this file
            if (ack[0] == 'E' || strstr(ack, "Error")) {
                recv(sock, ack, ack_bytes, 0); // Actually read the error message
                printf("%.*s", ack_bytes, ack);
                fclose(file);
                continue;
            }
            // Otherwise read just the "OK" acknowledgment
            recv(sock, ack, 2, 0); // Read the "OK" we peeked at
        } else {
            printf("Error: No response from server\n");
            fclose(file);
            continue;
        }

        // Send file content
        char buffer[BUFFER_SIZE];
        size_t bytes;
        long total_sent = 0;
        
        while (total_sent < file_size && (bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            // Make sure we don't send more than file size
            if (total_sent + bytes > file_size) {
                bytes = file_size - total_sent;
            }
            
            int sent = send(sock, buffer, bytes, 0);
            if (sent <= 0) break;
            
            total_sent += sent;
        }

        fclose(file);
        
        // Wait for server confirmation for this file
        char response[BUFFER_SIZE];
        memset(response, 0, sizeof(response));
        int resp_bytes = recv(sock, response, sizeof(response) - 1, 0);
        if (resp_bytes > 0) {
            response[resp_bytes] = '\0';
            // Print only complete messages (ending with newline)
            if (response[strlen(response)-1] == '\n') {
                printf("%s", response);
            } else {
                // If incomplete, read more until we get the full message
                char extra[BUFFER_SIZE];
                int extra_bytes = recv(sock, extra, sizeof(extra) - 1, 0);
                if (extra_bytes > 0) {
                    extra[extra_bytes] = '\0';
                    strcat(response, extra);
                }
                printf("%s\n", response);
            }
        }
        
        printf("Uploaded: %s (%ld bytes)\n", filenames[i], file_size);
    }
}

/* Download a file from the server */
void download_file(int sock, char *filepaths[], int file_count) {
    // Process each file individually
    for (int i = 0; i < file_count; i++) {
        // Send individual request for each file
        char command[512];
        snprintf(command, sizeof(command), "downlf %s", filepaths[i]);
        send_command(sock, command);
        
        // Extract filename for local saving
        char *filename = strrchr(filepaths[i], '/');
        filename = filename ? filename + 1 : filepaths[i];
        
        // Open local file for writing
        FILE *file = fopen(filename, "wb");
        if (!file) {
            perror("Error creating file");
            continue;
        }

        // Receive file data
        char buffer[BUFFER_SIZE];
        int bytes;
        int error_flag = 0;
        
        while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
            // Check for error message
            if (bytes < BUFFER_SIZE && buffer[0] == 'E') {
                printf("%.*s", bytes, buffer);
                error_flag = 1;
                break;
            }
            
            // Write received data
            fwrite(buffer, 1, bytes, file);
            
            // If we got less than full buffer, transfer is complete
            if (bytes < BUFFER_SIZE) break;
        }
        
        fclose(file);
        
        if (error_flag) {
            remove(filename);  // Delete partial/error file
        } else {
            printf("File downloaded: %s\n", filename);
        }
        
        // Small delay between file transfers
        usleep(100000);
    }
}

// Modify to handle up to 2 files
void remove_file(int sock, char *filepaths[], int file_count) {
    char command[512];
    snprintf(command, sizeof(command), "removef");
    
    for (int i = 0; i < file_count; i++) {
        strcat(command, " ");
        strcat(command, filepaths[i]);
    }
    
    send_command(sock, command);
    
    // Receive responses for each file
    for (int i = 0; i < file_count; i++) {
        char response[BUFFER_SIZE];
        int bytes = recv(sock, response, BUFFER_SIZE, 0);
        if (bytes > 0) {
            response[bytes] = '\0';
            printf("%s", response);
        }
    }
}

/* Download a tar archive of specific file type from server */
void download_tar(int sock, char *filetype) {
    // Validate requested file type against supported types
    if (strcmp(filetype, ".c") != 0 && 
        strcmp(filetype, ".pdf") != 0 && 
        strcmp(filetype, ".txt") != 0) {
        printf("Error: Only .c, .pdf, or .txt file types are supported.\n");
        return;
    }

    // Determine output tar filename based on file type
    char tarname[20];  // Buffer for tar filename
    if (strcmp(filetype, ".c") == 0)
        strcpy(tarname, "cfiles.tar");    // C files archive
    else if (strcmp(filetype, ".pdf") == 0)
        strcpy(tarname, "pdf.tar");       // PDF files archive
    else if (strcmp(filetype, ".txt") == 0)
        strcpy(tarname, "text.tar");      // Text files archive

    // Format and send download command to server
    char command[512];
    snprintf(command, sizeof(command), "downltar %s", filetype);
    send_command(sock, command);
    sleep(1);  // Allow server time to prepare the tar file

    // Check for error response from server (peek without removing from queue)
    char response[BUFFER_SIZE];
    int bytes = recv(sock, response, BUFFER_SIZE, MSG_PEEK | MSG_DONTWAIT);
    if (bytes > 0 && response[0] == 'E') {
        // Actually read the error message to clear buffer
        bytes = recv(sock, response, BUFFER_SIZE, 0);
        response[bytes] = '\0';
        printf("%s", response);  // Display server error
        return;
    }

    // Create output file for the tar archive
    FILE *file = fopen(tarname, "wb");  // Open in binary write mode
    if (!file) {
        printf("Error: Could not create output file.\n");
        return;
    }

    // Receive and save the tar file contents
    char buffer[BUFFER_SIZE];
    int total_bytes = 0;
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        // Check if server sent an error message instead of file data
        if (bytes < BUFFER_SIZE && buffer[0] == 'E') {
            printf("%.*s", bytes, buffer);  // Print error message
            fclose(file);
            remove(tarname);  // Delete incomplete/empty tar file
            return;
        }
        
        // Write received data to file
        fwrite(buffer, 1, bytes, file);
        total_bytes += bytes;  // Track total bytes received
        
        // If received less than full buffer, transfer is complete
        if (bytes < BUFFER_SIZE) break;
    }
    fclose(file);  // Close the tar file

    // Verify we actually received data
    if (total_bytes == 0) {
        printf("Error: No files of type %s found on server.\n", filetype);
        remove(tarname);  // Remove empty tar file
        return;
    }

    // Success message with downloaded archive info
    printf("Successfully downloaded %s containing all %s files.\n", tarname, filetype);
}

/* Display list of files in specified directory from server */
void display_filenames(int sock, char *pathname) {
    // Send directory listing request to server
    char command[512];
    snprintf(command, sizeof(command), "dispfnames %s", pathname);
    send(sock, command, strlen(command), 0);

    // Print directory header
    printf("Files in %s:\n", pathname);

    // Receive and process server response
    char buffer[BUFFER_SIZE];
    int bytes;
    int expecting_more = 1;  // Flag to track if more data is coming
    
    while (expecting_more) {
        bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) break;  // Connection closed or error
        
        buffer[bytes] = '\0';  // Null-terminate received data
        
        // Check for end-of-transmission marker
        if (strstr(buffer, "ENDOFLIST")) {
            expecting_more = 0;  // No more data after this
            char *end = strstr(buffer, "ENDOFLIST");
            *end = '\0';  // Terminate string before marker
        }
        
        printf("%s", buffer);  // Print received filenames
        fflush(stdout);       // Ensure immediate display
    }
    
}

/* Main client program entry point */
int main() {
    int sock;
    struct sockaddr_in server_addr;

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    /* Configure server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    /* Connect to server */
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to S1 server. Enter commands below:\n");

    char input[1024];  // Increased buffer size for multiple files
    char *args[10];    // Increased to handle up to 3 files + command + destination
    char *token;
    int arg_count;

    while (1) {
        printf("s25client$ ");
        fflush(stdout);
        
        /* Get user input */
        if (!fgets(input, sizeof(input), stdin)) {
            break;  // Exit on EOF (Ctrl+D)
        }
        input[strcspn(input, "\n")] = 0;  // Remove newline

        /* Parse input into tokens */
        arg_count = 0;
        token = strtok(input, " ");
        while (token != NULL && arg_count < 10) {  // Increased limit
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }

        if (arg_count == 0) continue;  // Empty input

        /* Handle commands */
        if (strcmp(args[0], "uploadf") == 0 && arg_count >= 3) {
            // Last arg is destination, others are files (1-3 files)
            char *filenames[3];
            int file_count = arg_count - 2;
            
            // Verify we don't exceed max files
            if (file_count > 3) {
                printf("Error: Maximum 3 files can be uploaded at once\n");
                continue;
            }
            
            // Extract filenames (all args except command and destination)
            for (int i = 0; i < file_count; i++) {
                filenames[i] = args[i+1];
            }
            
            // Upload each file individually
            for (int i = 0; i < file_count; i++) {
                upload_file(sock, &filenames[i], args[arg_count-1], 1);
            }
        }
        else if (strcmp(args[0], "downlf") == 0 && arg_count >= 2) {
            // All args after command are files (1-2 files)
            char *filepaths[2];
            int file_count = arg_count - 1;
            
            if (file_count > 2) {
                printf("Error: Maximum 2 files can be downloaded at once\n");
                continue;
            }
            
            for (int i = 0; i < file_count; i++) {
                filepaths[i] = args[i+1];
            }
            download_file(sock, filepaths, file_count);
        }
        else if (strcmp(args[0], "removef") == 0 && arg_count >= 2) {
            // All args after command are files (1-2 files)
            char *filepaths[2];
            int file_count = arg_count - 1;
            
            if (file_count > 2) {
                printf("Error: Maximum 2 files can be removed at once\n");
                continue;
            }
            
            for (int i = 0; i < file_count; i++) {
                filepaths[i] = args[i+1];
            }
            remove_file(sock, filepaths, file_count);
        }
        else if (strcmp(args[0], "downltar") == 0 && arg_count == 2) {
            download_tar(sock, args[1]);
        }
        else if (strcmp(args[0], "dispfnames") == 0 && arg_count == 2) {
            display_filenames(sock, args[1]);
        }
        else if (strcmp(args[0], "exit") == 0) {
            break;
        }
        else {
            printf("Invalid command or arguments.\n");
            printf("Usage examples:\n");
            printf("  uploadf file1 [file2] [file3] destination_path\n");
            printf("  downlf file1 [file2]\n");
            printf("  removef file1 [file2]\n");
            printf("  downltar .c|.pdf|.txt\n");
            printf("  dispfnames path\n");
        }
    }

    close(sock);
    return 0;
}