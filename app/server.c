#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int send_five_hundred(int client_fd);
void handle_connection(int client_fd);

int main(int argc, char **argv) {
	char *directory = ".";

	int i = 1;
	if (argc >= 3) {
		while (i < argc - 1 && strcmp(argv[i], "--directory"))
			i++;

		directory = argv[i + 1];

	} 

	printf("Settiing up directory in %s\n", directory);

	if (chdir(directory) < 0) {
		printf("Failed changing directory\n");
		return 1;
	}
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	int server_fd;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
	 	printf("Socket creation failed: %s...\n", strerror(errno));
	 	return 1;
	}

	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET ,
		.sin_port = htons(4221),
		.sin_addr = { htonl(INADDR_ANY) },
	};

	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
	 	return 1;
	}

	int connection_backlog = 12;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	while (1) {
		printf("Waiting for a client to connect...\n");

		struct sockaddr_in client_addr;
		int client_addr_len = sizeof(client_addr);

		int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
		if (client_fd == -1) {
			printf("Client connection failed\n");
			return 1;
		}
		printf("Client connected\n");
		
		if (!fork()) {
			close(server_fd);
			handle_connection(client_fd);
			close(client_fd);
			exit(0);
		}
		close(client_fd);
	}

	return 0;
}

int send_five_hundred(int client_fd) {
	int bytes_sent = send(client_fd, "HTTP/1.1 500 Internal Server Error\r\n\r\n", 33, 0);
	if (bytes_sent == -1) {
		printf("Send failed\n");
		return 1;
	}

	printf("Response sent\n");

	return 0;
}

void handle_connection(int client_fd) {
	char read_buffer[1024];
	char read_buffer_copy[1024]; 

	int bytes_recieved = recv(client_fd, read_buffer, 1024, 0);
	if (bytes_recieved == -1) {
		printf("Recieve failed\n");
		return;
	}

	//printf("Read buffer is %s\n", read_buffer);
	strcpy(read_buffer_copy, read_buffer);

	//Extract path
	char *http_method = strtok(read_buffer_copy, " ");
	char *path = strtok(0, " ");

	printf("Path is %s\n", path);
	printf("Method is %s\n", http_method);
	//printf("Path length is %lu\n", strlen(path));

	if (!strcmp(path, "/")) {
		int bytes_sent = send(client_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return;
		}

		printf("Response sent\n");

	} else if (!strncmp(path, "/echo/", 6)) {
		char response[1024];
		char *content = strstr(path, "/echo/") + strlen("/echo/");

		printf("Content is %s\n", content);

		int response_len = sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s", strlen(content), content);

		if (response_len < 0) {
			printf("Sprint failed\n");
			send_five_hundred(client_fd);
			return;
		}

		int bytes_sent = send(client_fd, response, response_len, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return;
		}

		printf("Response sent\n");

	} else if (!strcmp(path, "/user-agent")) { 
		char user_agent[128];
		char response[1024];
		char *user_agent_read = strstr(read_buffer, "User-Agent: ") + strlen("User-Agent: ");
		int i = 0;

		while (user_agent_read[i] != '\r' && user_agent_read[i + 1] != '\n')
			i++;

		strncpy(user_agent, user_agent_read, i);

		printf("User-Agent is %s\n", user_agent);

		int response_len = sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s", strlen(user_agent), user_agent);

		if (response_len < 0) {
			printf("Sprint failed\n");
			send_five_hundred(client_fd);
			return;
		}

		int bytes_sent = send(client_fd, response, response_len, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return;
		}

		printf("Response sent\n");

	} else if (!strncmp(path, "/files/", 7)) {
		if (!strcmp(http_method, "GET")) {
			char *file_name = strstr(path, "/files/") + strlen("/files/");

			FILE *file = fopen(file_name, "rb");
			if (!file) {
				printf("File not found\n");

				int bytes_sent = send(client_fd, "HTTP/1.1 404 Not Found\r\n\r\n", 26, 0);
				if (bytes_sent == -1) {
					printf("Send failed\n");
					return ;
				}

				printf("Response sent\n");
				return ;
			}

			if (fseek(file, 0, SEEK_END) < 0) {
				printf("Error reading document\n");
				send_five_hundred(client_fd);
				return ;
			}

			size_t file_size = ftell(file);

			rewind(file);

			char *file_data = (char *)malloc(file_size);
			if (!file_data) {
				printf("Memory for file data was not allocated\n");
				send_five_hundred(client_fd);
				return;
			}

			if (fread(file_data, 1, file_size, file) != file_size) {
				printf("Error reading document\n");
				send_five_hundred(client_fd);
				return ;
			}

			fclose(file);

			char response[1024];

			int response_len = sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %lu\r\n\r\n%s", file_size, file_data);
			if (response_len < 0) {
				printf("Sprint failed\n");
				send_five_hundred(client_fd);
				return;
			}
			
			int bytes_sent = send(client_fd, response, response_len, 0);
			if (bytes_sent == -1) {
				printf("Send failed\n");
				return;
			}

			printf("Response sent\n");
		} else if (!strcmp(http_method, "POST")) {
			char file_len_str[128];

			char *file_len_request = strstr(read_buffer, "Content-Length: ") + strlen("Content-Length: ");
			int i;

			i = 0;
			while (file_len_request[i] != '\r' && file_len_request[i + 1] != '\n')
				i++;

			strncpy(file_len_str, file_len_request, i);

			size_t file_len = atoi(file_len_str);
			char *file_content = strstr(read_buffer, "\r\n\r\n") + 4;
			char *file_name = strstr(path, "/files/") + strlen("/files/");

			printf("File content is:\n\n%s\n", file_content);
			FILE *file = fopen(file_name, "wb");
			if (!file) {
				printf("Can not create the file\n");
				send_five_hundred(client_fd);
				return ;
			}

			printf("File was created\n");

			if (fwrite(file_content, 1, file_len, file) != file_len) {
				printf("Error writing file\n");
				send_five_hundred(client_fd);
				return ;
			}

			fclose(file);
			
			char response[1024];
			int response_len = sprintf(response, "HTTP/1.1 201 Created\r\nContent-Type: application/octet-stream\r\nContent-Length: %lu\r\n\r\n%s", file_len, file_content);
			if (response_len < 0) {
				printf("Sprint failed\n");
				send_five_hundred(client_fd);
				return;
			}
			
			int bytes_sent = send(client_fd, response, response_len, 0);
			if (bytes_sent == -1) {
				printf("Send failed\n");
				return;
			}

			printf("Response sent\n");

		}
	} else {
		int bytes_sent = send(client_fd, "HTTP/1.1 404 Not Found\r\n\r\n", 26, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return;
		}

		printf("Response sent\n");
	}
}
