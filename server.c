#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define SRC_PORT "33333"
#define DST_PORT "44444"

#define MAX_MSG_LENGTH 65535
#define HEADER_LENGTH 8

// Struct to hold the key variables to keep track of.
typedef struct {
    int src_listener;    // fd for the listening socket bound to port 33333
    int dst_listener;    // fd for the listening socket bound to port 44444
    int src_connection;  // fd for the connection on port 33333
    int fds_size;        // the size of fds (the set of file decsriptors)
    int fd_count;        // the num of file descriptors in the set
} ConnectionsInfo;

/*
 * get_listener_socket - Gets a listening socket and binds it to the relevant port.
 * @return: the file descriptor for the listening socket
 */
int get_listener_socket(char* port) {
	int listener; // Listening socket descriptor
	int yes=1; // For setsockopt() SO_REUSEADDR, below
	int rv; // Return value for getaddrinfo()

	struct addrinfo hints, *ai, *p;

	// Get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
		fprintf(stderr, "server: %s\n", gai_strerror(rv));
		exit(1);
	}
	
	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol);
		if (listener < 0) { 
			continue;
		}
		
		// Lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	// If we got here, it means we didn't get bound
	if (p == NULL) {
		return -1;
	}

	freeaddrinfo(ai); // Finished with this

	// Listen
	if (listen(listener, 10) == -1) {
		return -1;
	}

	return listener; // Return listening socket descriptor
}

/*
 * add_to_fds - Adds a new file descriptor to the set.
 */
void add_to_fds(struct pollfd **fds, int newfd, int *fd_count, int *fds_size) {
	// If we don't have room, add more space in the fds array
	if (*fd_count == *fds_size) {
		*fds_size *= 2; // Double it
		*fds = realloc(*fds, sizeof(**fds) * (*fds_size));
	}

	(*fds)[*fd_count].fd = newfd;
	(*fds)[*fd_count].events = POLLIN; // Check ready-to-read
	(*fds)[*fd_count].revents = 0;

	(*fd_count)++;
}

/*
 * del_from_fds - Removes a file descriptor at a given index from the set.
 */
void del_from_fds(struct pollfd fds[], int i, int *fd_count) {
	// Copy the one from the end over this one
	fds[i] = fds[*fd_count-1];

	(*fd_count)--;
}

/*
 * handle_new_connection - Handles new client connections to the listening sockets.
 * @is_src: set to 1 if the connection is to port 33333, 0 if it's to port 44444
 * @return: 0 if connection made successfully, -1 otherwise
 */
int handle_new_connection(ConnectionsInfo *fd_info, struct pollfd **fds, int is_src) {

    int listener;
    char* port;

    if (is_src) {
        listener = fd_info->src_listener;
        port = SRC_PORT;
    } else {
        listener = fd_info->dst_listener;
        port = DST_PORT;
    }

	struct sockaddr_storage remoteaddr; // Client address
	socklen_t addrlen;
	int newfd;  // Newly accept()ed socket descriptor

	addrlen = sizeof remoteaddr;
	newfd = accept(listener, (struct sockaddr *)&remoteaddr,
			&addrlen);

	if (newfd == -1) {
		perror("accept");
        return -1;
	} else {
		add_to_fds(fds, newfd, &fd_info->fd_count, &fd_info->fds_size);

		printf("new connection to port %s on socket %d\n", port, newfd);

		if (is_src) {
			fd_info->src_connection = newfd;
		}
	}

    return 0;
}

/*
 * close_connection - Closes a connection if the client hangs up or if there is an error.
 * Calls del_from_fds() to remove the relevant file descriptor from the set.
 */
void close_connection(int nbytes, int sender_fd, int *fd_count, struct pollfd *fds, int *fd_i) {
    if (nbytes == 0) {
		// Connection closed
		printf("socket %d hung up\n", sender_fd);
	} else {
		perror("recv");
	}

    close(fds[*fd_i].fd); // Close the connection 
	del_from_fds(fds, *fd_i, fd_count); // Remove the fd from the set
	(*fd_i)--; // reexamine the slot we just deleted    
}

/*
* concat_two_bytes - Concatanates two bytes into one 16 bit word. 
* @return: the 16 bit word
*/
uint16_t concat_two_bytes(uint8_t left_byte, uint8_t right_byte) {
    uint16_t word = left_byte << 8 | right_byte;
    return word;
}

/*
* calculate_checksum - Calculates the checksum of the message by 
* performing the one's complement sum of all 16 bit words in the message.
* @return: the calculated checksum
*/
uint16_t calculate_checksum(char *message, int length) {

	uint32_t sum = 0;

	// Add together all 16 bit words in the message
	for (int i=0; i<length-1; i+=2) {
		if (i == 4) {
			sum += 0xcccc;
		} else {
            sum += concat_two_bytes((uint8_t)message[i], (uint8_t)message[i+1]);
		}
	}

	// Check if there's an extra byte at the end
	if (length % 2 != 0) {
		sum += (uint8_t)message[length-1] << 8;
	}

	// Deal with carry bit
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	// Return the one's complement of the sum
	return (uint16_t)(~sum);
}

/*
* check_header - Checks that the magic number and options fields are valid
* and that the necessary padding is present in the header. 
* @return: 0 if valid, -1 otherwise
*/
int check_header(char *header) {
    if ((unsigned char)header[0] != 0xcc) {
        fprintf(stderr, "Invalid magic number\n");
        return -1;
    } else if (header[1] != 0x40 && header[1] != 0x0){ // Options field should be 01000000 or 00000000
        fprintf(stderr, "Invalid options field\n");
        return -1;
    } else if (header[6] != 0x0 || header[7] != 0x0) { // Should be 2 bytes of padding
        fprintf(stderr, "Incorrect padding\n");
        return -1;
    }

    return 0;
}

/*
* broadcast_message - Broadcasts message to all destination clients.
*/
void broadcast_message(char *message, int message_length, ConnectionsInfo *fd_info, struct pollfd *fds) {
    for(int j = 0; j < fd_info->fd_count; j++) {
        int dest_fd = fds[j].fd;

        // Don't send to destination listener or src client
        if (dest_fd != fd_info->dst_listener && dest_fd != fd_info->src_connection) {
            if (send(dest_fd, message, message_length, 0) == -1) {
                perror("send");
            }
        }
    }
}

/*
 * handle_dst_client_data - handles data from destination clients.
 * Calls close_connection() if the client hangs up or errors; "ignores" any other data sent.
 */
void handle_dst_client_data(int *fd_count, struct pollfd *fds, int *fd_i) {
    int sender_fd = fds[*fd_i].fd;
    char buf[256];
    int nbytes = recv(fds[*fd_i].fd, buf, sizeof buf, 0);

    if (nbytes <= 0) { // Got error or connection closed by client
        close_connection(nbytes, sender_fd, fd_count, fds, fd_i);
    }
}

/*
 * handle_src_client_data - Handles data from the src client
 * Calls close_connection() if the client hangs up or errors.
 * Broadcasts the message to the destination clients if the header, length and checksum are all valid
 * Drops the message if not.
 */
void handle_src_client_data(ConnectionsInfo *fd_info, struct pollfd *fds, int *fd_i)
{
    char message_buf[MAX_MSG_LENGTH]; // Buffer for message
    int num_message_bytes = recv(fd_info->src_connection, message_buf, sizeof message_buf, 0);

    int length_ok = 1;

	if (num_message_bytes <= 0) { // Got error or connection closed by client
        close_connection(num_message_bytes, fd_info->src_connection, &fd_info->fd_count, fds, fd_i);

		// As the source client connection is now closed, create new listener for source port
        fd_info->src_listener = get_listener_socket(SRC_PORT);
        add_to_fds(&fds, fd_info->src_listener, &fd_info->fd_count, &fd_info->fds_size); // add new listener socket to fds
        fd_info->src_connection = -1;

	} else if (num_message_bytes < HEADER_LENGTH) { // header too short
		fprintf(stderr, "Error receiving header\n");
    } else {
        uint16_t length_field = concat_two_bytes((uint8_t)message_buf[2], (uint8_t)message_buf[3]);
        uint16_t checksum = concat_two_bytes((uint8_t)message_buf[4], (uint8_t)message_buf[5]);
        
        int actual_length = num_message_bytes - HEADER_LENGTH;

        if (actual_length > length_field) {
            fprintf(stderr, "Too much data; message being dropped.\n"); // Drop the message if the data length exceeds the specified length
            length_ok = 0;
        } else if (actual_length < length_field) {
            fprintf(stderr, "Not all data received; sending what we have.\n");
        }

        if (length_ok && check_header(message_buf) == 0) {
            // Validate the checksum if the sensitive bit is set
            if ((message_buf[1] == 0x40 && calculate_checksum(message_buf, num_message_bytes) == checksum) || message_buf[1] == 0x0) {
                // Broadcast the message to all destination clients
				broadcast_message(message_buf, num_message_bytes, fd_info, fds);					
            } else {
                fprintf(stderr, "Invalid checksum\n"); // Checksum not correct so message not broadcasted
            }            
        }
    } 
}

/*
 * process_connections - Loops over the set of fds and handles any data sent to the sockets.
 * This includes new connections to the listening sockets, hangups, or messages sent.
 */
void process_connections(ConnectionsInfo *fd_info, struct pollfd **fds) {
	for(int i = 0; i < fd_info->fd_count; i++) {

		// Check if someone's ready to read
		if ((*fds)[i].revents & (POLLIN | POLLHUP)) {

			// New src connection
			if ((*fds)[i].fd == fd_info->src_listener) {
				if (handle_new_connection(fd_info, fds, 1) == 0) {
                    close(fd_info->src_listener); // Close the listener socket as we only want 1 src client
                    del_from_fds(*fds, i, &fd_info->fd_count);
                    fd_info->src_listener = -1; // Negative value so it won't have the same value as another fd
					i--;
                };
            
			// New dst connection
            } else if ((*fds)[i].fd == fd_info->dst_listener) {
				handle_new_connection(fd_info, fds, 0);
            
			// Data from source
            } else if ((*fds)[i].fd == fd_info->src_connection) {
				handle_src_client_data(fd_info, *fds, &i);

			// Data from a destination
			} else {
				handle_dst_client_data(&fd_info->fd_count, *fds, &i);
			}
		}
	}
}

/*
 * main - Creates a listener for both port 33333 and port 44444.
 * Creates a set of connections of type 'struct pollfd' called 'fds'.
 * Loops forever processing the connections.
 */
int main(void)
{

    ConnectionsInfo fd_info;

    fd_info.src_listener = get_listener_socket(SRC_PORT); // Listening socket descriptor for port 33333
    fd_info. dst_listener = get_listener_socket(DST_PORT); // Listening socket descriptor for port 44444
    fd_info.src_connection = -1; // Socket descriptor for connection to port 33333

	// Start off with room for 5 connections
	// (We'll realloc as necessary)
    fd_info.fds_size = 5;
    fd_info.fd_count = 0;
    
	struct pollfd *fds = malloc(sizeof *fds * fd_info.fds_size);

    if (fd_info.src_listener == -1) {
        fprintf(stderr, "error getting source listening socket\n");
        exit(1);
    }

    if (fd_info.dst_listener == -1) {
        fprintf(stderr, "error getting destination listening socket\n");
        exit(1);
    }    

	// Add the listeners to set
    fds[0].fd = fd_info.src_listener;
    fds[0].events = POLLIN;

    fds[1].fd = fd_info.dst_listener;
    fds[1].events = POLLIN;

    fd_info.fd_count = 2;
	
	// Report ready to read on incoming connection
	puts("waiting for connections...");

	// Main loop
	for(;;) {
		int poll_count = poll(fds, fd_info.fd_count, -1);

		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}

		// Run through connections looking for data to read
        process_connections(&fd_info, &fds);
	}

	free(fds);
}