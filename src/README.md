# Source Code Directory

This directory contains the complete implementation of the Distributed File System in C.

## Files Overview

### Server Components

#### 🖥️ `chowhan_munna_110189503_S1.c` - Main Server
- **Port**: 1231
- **Primary Functions**: 
  - Client connection management
  - File type routing and distribution
  - Cross-server communication coordination
  - Local storage for `.c` files

#### 📄 `Chowhan_munna_110189503_S2.c` - PDF Server  
- **Port**: 1202
- **Primary Functions**:
  - PDF file storage and retrieval
  - PDF-specific operations
  - Communication with S1 server

#### 📝 `Chowhan_munna_110189503_S3.c` - Text Server
- **Port**: 1203  
- **Primary Functions**:
  - Text file storage and management
  - Text-specific operations
  - S1 server coordination

#### 📦 `Chowhan_munna_110189503_S4.c` - Archive Server
- **Port**: 1206
- **Primary Functions**:
  - ZIP file handling
  - Archive operations
  - S1 server integration

### Client Component

#### 💻 `Chowhan_munna_110189503_s25client.c` - Client Application
- **Primary Functions**:
  - User interface for file operations
  - Command parsing and validation
  - Network communication with S1
  - File transfer handling

## Key Technical Features

### 🔄 Multi-threading Support
- **pthread implementation** for concurrent client handling
- **Fork-based processing** in S1 server using `prcclient()` function
- **Thread-safe operations** for file handling

### 🌐 Network Architecture
- **TCP socket programming** for reliable communication
- **Custom protocol** for inter-server messages
- **Error handling** and connection recovery

### 📁 File System Integration
- **Automatic directory creation** using `make_directory_from_path()`
- **File type validation** and routing logic
- **Cross-platform path handling**

## Implementation Details

### Server-to-Server Communication
```c
// Example: S1 forwarding PDF file to S2
int s2_sock = connect_to_server("127.0.0.1", 1202);
char forward_cmd[512];
snprintf(forward_cmd, sizeof(forward_cmd), "uploadf %s %s", filename, modified_path);
send(s2_sock, forward_cmd, strlen(forward_cmd), 0);
```

### Client Command Processing
```c
// Command parsing in client
char command[20];
char args[10][512];
char *token = strtok(buffer, " ");
strncpy(command, token, sizeof(command) - 1);
```

### File Distribution Logic
```c
// File type routing in S1
if (strcmp(ext, ".pdf") == 0) {
    port = 1202; // Route to S2
} else if (strcmp(ext, ".txt") == 0) {
    port = 1203; // Route to S3
} else if (strcmp(ext, ".zip") == 0) {
    port = 1206; // Route to S4
}
```

## Compilation Instructions

### Individual Compilation
```bash
# Compile servers
gcc -o S1 chowhan_munna_110189503_S1.c -lpthread
gcc -o S2 Chowhan_munna_110189503_S2.c -lpthread
gcc -o S3 Chowhan_munna_110189503_S3.c -lpthread
gcc -o S4 Chowhan_munna_110189503_S4.c -lpthread

# Compile client
gcc -o s25client Chowhan_munna_110189503_s25client.c
```

### Batch Compilation
```bash
# Use the provided script
../scripts/compile.sh
```

## Memory Management

### Dynamic Allocation
- **Client socket handling**: `malloc()` for new client connections
- **File list management**: `realloc()` for expanding arrays
- **String handling**: `strdup()` for filename storage

### Resource Cleanup
- **Socket closure**: Proper cleanup of network connections
- **File handles**: `fclose()` after operations
- **Memory deallocation**: `free()` for allocated resources

## Error Handling

### Network Errors
- **Connection failure recovery**
- **Timeout handling**
- **Socket error management**

### File System Errors
- **Permission checking**
- **Directory creation validation**
- **File existence verification**

## Security Considerations

### Input Validation
- **File extension verification**
- **Path sanitization**
- **Buffer overflow protection**

### Network Security
- **Localhost-only communication**
- **Protocol validation**
- **Connection authentication**

---

> 💡 **Tip**: For detailed function documentation, see the inline comments within each source file.