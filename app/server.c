#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int send_five_hundred(int client_fd, int server_fd);

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

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

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
	if (client_fd == -1) {
		printf("Client connection failed\n");
		return 1;
	}
	printf("Client connected\n");

	char read_buffer[1024];
	char read_buffer_copy[1024]; 

	int bytes_recieved = recv(client_fd, read_buffer, 1024, 0);
	if (bytes_recieved == -1) {
		printf("Recieve failed\n");
		return 1;
	}

	printf("Read buffer is %s\n", read_buffer);
	strcpy(read_buffer_copy, read_buffer);

	//Extract path
	char *http_method = strtok(read_buffer_copy, " ");
	char *path = strtok(0, " ");

	printf("Path is %s\n", path);
	//printf("Path length is %lu\n", strlen(path));

	if (!strcmp(path, "/")) {
		int bytes_sent = send(client_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return 1;
		}

		printf("Response sent\n");

	} else if (strstr(path, "/echo/")) {
		char response[1024];
		char *content = strstr(path, "/echo/") + strlen("/echo/");

		printf("Content is %s\n", content);

		int response_len = sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s", strlen(content), content);

		if (response_len < 0) {
			printf("Sprint failed\n");
			send_five_hundred(client_fd, server_fd);
			return 1;
		}

		int bytes_sent = send(client_fd, response, response_len, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return 1;
		}

		printf("Response sent\n");
	} else if (strstr(path, "/user-agent") != 0){ 
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
			send_five_hundred(client_fd, server_fd);
			return 1;
		}

		int bytes_sent = send(client_fd, response, response_len, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return 1;
		}

		printf("Response sent\n");
	} else {
		int bytes_sent = send(client_fd, "HTTP/1.1 404 Not Found\r\n\r\n", 26, 0);
		if (bytes_sent == -1) {
			printf("Send failed\n");
			return 1;
		}

		printf("Response sent\n");
	}

	close(server_fd);

	return 0;
}

int send_five_hundred(int client_fd, int server_fd) {
	int bytes_sent = send(client_fd, "HTTP/1.1 500 Internal Server Error\r\n\r\n", 33, 0);
	if (bytes_sent == -1) {
		printf("Send failed\n");
		return 1;
	}

	printf("Response sent\n");

	close(server_fd);
	return 0;
}
