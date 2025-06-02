
/*"Everything is a file" in Unix
A network connection effectively establishes a "file" which receives the input and
our server checks for activity in this "file" and responds accordingly.
Further, the activity of the server (receiving connections for example) itself is kept in a "file"*/

/*Also need to break some of this down into other files for clarity, maybe a check connection file/funciton
maybe a main loop funciton?*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <kernel.h> // For RISC OS specific functions
#include <swis.h>   // For RISC OS specific functions

#define PORT 12345       // Must match RISC OS client's port
#define IDLE_TIMEOUT 30  // 30 seconds
#define MAX_CLIENTS 5    // Maximum number of clients
#define BUFFER_SIZE 1024 // Size of the buffer for incoming data

void inet_ntop_riscos(int i, uint32_t *ip, char *buf, size_t len); // Convert IP address to string
int FD_ISSET_STDIN_FILENO_riscos(int i, fd_set *fds);              // Check if a file descriptor is set in the fd_set

int main()
{
    int server_fd;                               // File descriptor for the server socket
    int client_fds[MAX_CLIENTS] = {-1};          // Array holding client file descriptors/sockets
    struct sockaddr_in server_addr, client_addr; // Server and client address structures
    socklen_t client_len = sizeof(client_addr);  // Length of client address
    time_t last_activity = time(NULL);
    char buffer[BUFFER_SIZE]; // Buffer to store incoming data and storing outgoing data

    // Create socket - that is, create a file descriptor for the server "server_fd", connections can be found as activity in this file
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket() failed\n");
        exit(1);
    }

    // Set the behaviour of the socket to allow reuse of the address, opt = 1
    // This is useful when the server is restarted and the port is still in use
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections from any interface
    server_addr.sin_port = htons(PORT);       // Port number (host to network byte order)

    // Bind socket to port - register the details of the server with the file descriptor server_fd, activity on this port should go to this file descriptor
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind() failed\n");
        close(server_fd);
        exit(1);
    }

    printf("Hello World Server\n");

    // Listen for connections - that is, check for activity in the file descriptor "server_fd"
    if (listen(server_fd, MAX_CLIENTS) < 0)
    { // 5 = max pending connections
        perror("listen() failed\n");
        close(server_fd);
        exit(1);
    }

    printf("Server listening on port %d\n", PORT);
    printf("Press '\\' to shutdown the server\n");

    // Most of these variables are used with select() to monitor multiple file descriptors
    fd_set readfds;         // File descriptor set for reading, contains the file descriptors to be monitored for readyness for reading
    struct timeval tv;      // Timeout value for select()
    int active_clients = 0; // Number of active clients

    // initialize the client_fds array to -1
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_fds[i] = -1; // Initialize all client file descriptors to -1
    }

    // Loop checks for connections, when a message arrives, send a message, close connection, keep server running
    while (1)
    {

        FD_ZERO(&readfds);              // Clear the file descriptor set
        FD_SET(server_fd, &readfds);    // Add server_fd to the set
        FD_SET(STDIN_FILENO, &readfds); // Add standard input (stdin) to the set
        int max_fd = server_fd;         // Maximum file descriptor value, used in select()

        for (int i = 0; i < MAX_CLIENTS; i++) // Add all active clients to readfds
        {
            if (client_fds[i] > 0)
            {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > max_fd)
                    max_fd = client_fds[i];
            }
        }
        // Now when using readfs, we are checking for activity in both the server_fd and stdin only

        // Set timeout value for select()
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // Select() monitors the file descriptors in readfds for activity
        // If there is activity, it returns the number of file descriptors that are ready for reading
        // If there is no activity, it returns 0
        // If an error occurs, it returns -1
        // If the timeout expires, it returns 0
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0)
        {
            perror("select error\n");
            continue;
        }

        // FD_ISSET checks if the file descriptor is set in the readfds set, checks if server_fd is set in readfds
        if (FD_ISSET(server_fd, &readfds))
        {
            // Accept a new client connection
            // Loop through the client_fds array to find an empty slot for the new client
            // If an empty slot is found, accept the connection and store the client file descriptor in that slot
            // If no empty slot is found, reject the connection
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_fds[i] == -1) // Find an empty slot in the client_fds array
                {
                    client_fds[i] = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fds[i] < 0)
                    {
                        perror("accept() failed\n");
                        continue; // Skip to next iteration instead of exiting
                    }
                    active_clients++;           // Increment the number of active clients
                    last_activity = time(NULL); // Record the time of the last activity
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop_riscos(AF_INET, &client_addr.sin_addr.s_addr, client_ip, INET_ADDRSTRLEN); // Convert IP address to string
                    printf("New client connected from: %s\n Total clients: %d\n", client_ip, active_clients);
                    break; // Exit loop after accepting a new client
                }
            }
            // If no empty slot is found, reject the connection
            if (active_clients == MAX_CLIENTS)
            {
                printf("Max clients reached. Connection refused.\n");
                close(client_fds[MAX_CLIENTS - 1]); // Close the last client
                client_fds[MAX_CLIENTS - 1] = -1;   // Reset the last client slot
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_fds[i] != -1 && FD_ISSET(client_fds[i], &readfds))
            {
                // int bytes_received = recv(client_fds[i], buffer, 1, MSG_PEEK | MSG_DONTWAIT);
                int bytes_received = recv(client_fds[i], buffer, sizeof(buffer), 0);
                buffer[bytes_received] = '\0'; // Null-terminate the buffer

                switch (bytes_received)
                {
                case -1:
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // No data available (non-blocking mode) ? client is still connected
                        printf("Client %d: No data (EAGAIN)\n", client_fds[i]);
                    }
                    else
                    {
                        // Real error (e.g., ECONNRESET, ENOTCONN)
                        perror("recv failed\n");
                        close(client_fds[i]);
                        client_fds[i] = -1;
                        active_clients--;
                    }
                    break;
                case 0:
                {
                    printf("Client %d disconnected.\n", client_fds[i]);
                    close(client_fds[i]);
                    client_fds[i] = -1; // Mark slot as free
                    active_clients--;
                    break;
                }
                default:
                    printf("Client said: %s", buffer);
                    int bytes_sent = send(client_fds[i], "Hello from server\n", 18, 0);
                    continue;
                }
            }
        }

        if (FD_ISSET_STDIN_FILENO_riscos(STDIN_FILENO, &readfds))
        {
            printf("'\\' key pressed, shutting down...\n");
            break; // Exit the loop if a key is pressed
        }

        // Idle timeout check
        if (active_clients == 0 && (time(NULL) - last_activity) > IDLE_TIMEOUT)
        {
            // maybe something that clears the buffer?
            printf("No clients connected for %d seconds. Shutdown? (y/n): ", IDLE_TIMEOUT);
            char response;
            scanf("%c", &response);
            if (response == 'y')
                break;
            last_activity = time(NULL); // Reset timer
        }
    }

    // Clean up
    printf("Server shutting down...\n");
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_fds[i] > 0)
        {
            close(client_fds[i]);
            client_fds[i] = -1;
        }
    }
    close(server_fd);
    printf("Server shutdown\n");

    return 0;
}

void inet_ntop_riscos(int i, uint32_t *ip, char *buf, size_t len) // Convert IP address to string
{
    unsigned char *bytes = (unsigned char *)ip;
    snprintf(buf, len, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
}

int FD_ISSET_STDIN_FILENO_riscos(int i, fd_set *fds)
{
    _kernel_swi_regs regs;

    /* OS_Byte 129: Check key press within time limit
     * R1,R2 = Time limit ((R2 * 256) + R1) (centiseconds) */
    regs.r[0] = 129;        // OS_Byte function number for key press check
    regs.r[1] = 100 & 0xFF; // 1500 centiseconds = 15 seconds (mask the low bytes with 0xFF)
    regs.r[2] = 100 >> 8;   // High byte of 1500 (shift the high bytes right by 8 bits, to where the low bytes were)

    _kernel_swi(OS_Byte, &regs, &regs);

    if (regs.r[1] == 92) // If no key is pressed, return -1
    {
        return 1;
    }
    else if (regs.r[1] != 92 || regs.r[2] == 255) // If the time limit is reached without a key press, return -1
    {
        return 0;
    }
    else
    {
        return 0;
    };
    // printf("regs.r[1] = %d\n", regs.r[1]); //this is the key press
    // printf("regs.r[2] = %d\n", regs.r[2]); // this is 255 when timed out with no key press
}