# Distributed File System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-green.svg)](https://www.linux.org/)

A sophisticated **distributed file system** implementation using socket programming in C, designed for the COMP-8567 course. This system transparently distributes files across multiple servers based on file types while providing a unified interface to clients.

## 🏗️ System Architecture

The system consists of **four specialized servers** and supports **multiple concurrent client connections**:

- **S1 (Main Server)** - Port 1231: Primary interface for all client interactions
- **S2 (PDF Server)** - Port 1202: Handles all `.pdf` file operations  
- **S3 (Text Server)** - Port 1203: Manages all `.txt` file operations
- **S4 (Archive Server)** - Port 1206: Processes all `.zip` file operations

### 🔄 File Distribution Logic

| File Type | Storage Location | Client Perception |
|-----------|------------------|------------------|
| `.c` files | S1 (Local) | Stored on S1 |
| `.pdf` files | S2 Server | Stored on S1* |
| `.txt` files | S3 Server | Stored on S1* |
| `.zip` files | S4 Server | Stored on S1* |

*\*Clients remain unaware of the actual distributed storage*

## 📁 Project Structure

```
Distributed_File_System/
├── README.md                          # Project documentation
├── src/                               # Source code directory
│   ├── chowhan_munna_110189503_S1.c   # Main server implementation
│   ├── Chowhan_munna_110189503_S2.c   # PDF server implementation  
│   ├── Chowhan_munna_110189503_S3.c   # Text server implementation
│   ├── Chowhan_munna_110189503_S4.c   # Archive server implementation
│   └── Chowhan_munna_110189503_s25client.c # Client implementation
├── docs/                              # Documentation
│   └── Project_Requirements.pdf       # Original project specifications
├── scripts/                           # Utility scripts
│   ├── compile.sh                     # Compilation script
│   ├── start_servers.sh               # Server startup script
│   └── stop_servers.sh                # Server shutdown script
└── examples/                          # Usage examples
    └── sample_commands.txt            # Example client commands
```

## ⚡ Features

### 🌐 **Distributed Architecture**
- **Transparent file distribution** across multiple servers
- **Load balancing** based on file types
- **Fault-tolerant** communication between servers

### 🔒 **Multi-threaded Server Design**
- **Concurrent client handling** using pthread
- **Fork-based request processing** for scalability
- **Non-blocking I/O** operations

### 📤 **Comprehensive File Operations**
- **Upload**: Support for 1-3 files simultaneously
- **Download**: Retrieve 1-2 files with automatic server routing
- **Remove**: Delete files with cross-server coordination
- **Archive**: Create and download tar files by file type
- **List**: Display all files in a directory across all servers

### 🛡️ **Robust Error Handling**
- **Input validation** and syntax checking
- **Network error recovery**
- **File system error management**
- **Graceful connection handling**

## 🚀 Getting Started

### Prerequisites

- **Linux/Unix environment**
- **GCC compiler** (version 7.0 or higher)
- **POSIX threads support**
- **Network connectivity** (localhost)

### Installation

1. **Clone the repository**:
   ```bash
   git clone https://github.com/chowhanm25/Distributed_File_System.git
   cd Distributed_File_System
   ```

2. **Set up directories**:
   ```bash
   mkdir -p ~/S1 ~/S2 ~/S3 ~/S4
   chmod +x scripts/*.sh
   ```

3. **Compile all components**:
   ```bash
   ./scripts/compile.sh
   ```

### Quick Start

1. **Start all servers** (in separate terminals):
   ```bash
   ./scripts/start_servers.sh
   ```

2. **Launch client**:
   ```bash
   ./s25client
   ```

3. **Try sample commands**:
   ```bash
   s25client$ uploadf sample.c ~S1/projects/
   s25client$ downlf ~S1/projects/sample.c
   s25client$ dispfnames ~S1/projects/
   ```

## 💻 Client Commands

### 📤 `uploadf` - File Upload
**Syntax**: `uploadf filename1 [filename2] [filename3] destination_path`

**Examples**:
```bash
# Upload single file
s25client$ uploadf document.pdf ~S1/documents/

# Upload multiple files
s25client$ uploadf code.c readme.txt archive.zip ~S1/project/
```

### 📥 `downlf` - File Download  
**Syntax**: `downlf filepath1 [filepath2]`

**Examples**:
```bash
# Download single file
s25client$ downlf ~S1/documents/report.pdf

# Download multiple files
s25client$ downlf ~S1/code/main.c ~S1/docs/readme.txt
```

### 🗑️ `removef` - File Removal
**Syntax**: `removef filepath1 [filepath2]`

**Examples**:
```bash
# Remove single file
s25client$ removef ~S1/temp/old_file.txt

# Remove multiple files  
s25client$ removef ~S1/backup/data.zip ~S1/logs/debug.txt
```

### 📦 `downltar` - Archive Download
**Syntax**: `downltar filetype`

**Examples**:
```bash
# Download all C files as tar
s25client$ downltar .c

# Download all PDF files as tar
s25client$ downltar .pdf

# Download all text files as tar
s25client$ downltar .txt
```

### 📋 `dispfnames` - List Files
**Syntax**: `dispfnames pathname`

**Examples**:
```bash
# List all files in directory
s25client$ dispfnames ~S1/projects/

# List files in subdirectory
s25client$ dispfnames ~S1/documents/reports/
```

## 🔧 Technical Implementation

### Network Communication
- **TCP socket programming** for reliable data transfer
- **Multi-threaded server architecture** using pthreads
- **Custom protocol** for inter-server communication

### File Management
- **Automatic directory creation** for new paths
- **File type validation** and routing
- **Atomic file transfer** operations

### Memory Management
- **Dynamic memory allocation** for client handling
- **Buffer management** for efficient data transfer
- **Resource cleanup** and leak prevention

## 🧪 Testing

### Manual Testing
Run the provided test scenarios:
```bash
# Test file upload
echo "Hello World" > test.txt
s25client$ uploadf test.txt ~S1/test/

# Verify file distribution
s25client$ dispfnames ~S1/test/

# Test download
s25client$ downlf ~S1/test/test.txt
```

### Stress Testing
```bash
# Test concurrent clients
for i in {1..5}; do
    ./s25client &
done
```

## 🛠️ Configuration

### Port Configuration
Edit the port definitions in each server file:
```c
#define PORT_S1 1231  // Main server
#define PORT_S2 1202  // PDF server  
#define PORT_S3 1203  // Text server
#define PORT_S4 1206  // Archive server
```

### Directory Structure
Default storage locations:
- S1 files: `~/S1/`
- S2 files: `~/S2/`
- S3 files: `~/S3/`
- S4 files: `~/S4/`

## 🐛 Troubleshooting

### Common Issues

**Connection Refused**:
```bash
# Check if servers are running
ps aux | grep -E "S[1-4]"

# Verify ports are not in use
netstat -tlnp | grep -E "123[1-6]|120[2-3,6]"
```

**Permission Denied**:
```bash
# Set proper permissions
chmod 755 ~/S1 ~/S2 ~/S3 ~/S4
chmod +x scripts/*
```

**File Not Found**:
```bash
# Verify file paths
ls -la ~/S1/  # Check S1 directory
ls -la ~/S2/  # Check S2 directory
```

## 📚 Documentation

- **[Project Requirements](docs/Project_Requirements.pdf)** - Original assignment specifications
- **[API Documentation](docs/API.md)** - Detailed function documentation  
- **[Architecture Guide](docs/ARCHITECTURE.md)** - System design details

## 🏆 Academic Information

- **Course**: COMP-8567 (Advanced Operating Systems)
- **Institution**: University of Windsor
- **Semester**: Summer 2025
- **Student**: Munna Chowhan (110189503)

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

This is an academic project. While contributions are not expected, suggestions and feedback are welcome through issues.

## 📞 Contact

**Munna Chowhan**  
📧 Email: chowhanm25@uwindsor.ca  
🎓 Student ID: 110189503  
🏫 University of Windsor

---

<div align="center">

**⭐ If you find this project helpful, please consider giving it a star! ⭐**

</div>