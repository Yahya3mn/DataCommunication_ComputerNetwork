#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define LENGTH 2048 /*Maximum length for messages.*/


// Global variables
volatile sig_atomic_t flag = 0; /*Signal flag to manage Ctrl+C operation.*/

int sockfd = 0; /*Socket file identifier.*/

char name[32]; /*User's name.*/

/*Prints the current line on the screen.*/
void str_overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

/************************************************************************************************************************************************/


/*This section contains the CRC and Checksum functions. It uses these values to check the correctness of the messages received by the client.*/

/*Checksum Error Check*/
uint16_t calculateChecksum(const char *data, size_t length) {
    uint16_t sum = 0;

    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }

    return sum;
}

// Cyclic Redundancy Check (CRC) Error Check
uint16_t calculateCRC(const char *data, size_t length) {
    uint16_t crc = 0xFFFF; // Initial value

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001; // XOR with polynomial 0xA001
            } else {
                crc = crc >> 1;
            }
        }
    }

    return crc;
}


/********************************************************************************************************************************************/

/*Truncates the newline character from an array.*/
void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

/*Signal handler for Ctrl+C.*/
void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}


//Parse Message
/*Manages sending messages to the server.*/
void send_msg_handler() {
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};
    FILE *fp = fopen("message_log.txt", "a"); //Open File

    while (1) {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);

        if (strcmp(message, "exit") == 0) {
            break;
        } else {
            uint16_t checksum = calculateChecksum(message, strlen(message));
            uint16_t crc = calculateCRC(message, strlen(message));

            

            time_t now = time(NULL);
            struct tm *local_time = localtime(&now);
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

            sprintf(buffer, "[%s] %s: %s\n Checksum : %04X  CRC : %04X\n", time_str, name, message, checksum, crc);
            send(sockfd, buffer, strlen(buffer), 0);

            fprintf(fp, "[%s] From %s: %s CRC:%04X checksum: %04X\n ", time_str, name, message, crc, checksum); // Add sender's name to the log
            fflush(fp);
        }

        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 32);
    }
    catch_ctrl_c_and_exit(2);
    fclose(fp);
}

/*Manages receiving messages from the server.*/
void recv_msg_handler() {
    char message[LENGTH] = {};
    FILE *fp = fopen("message_log.txt", "a"); // Open or create files (additive mode if available)

    while (1) {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0) {
            printf("%s\n", message);
            fprintf(fp, "Received: %s\n", message); // Write messages to file
            fflush(fp); // Write buffer to file
            str_overwrite_stdout();
        } else if (receive == 0) {
            break;
        } else {
            perror("Error receiving message");
            break;
        }
        memset(message, 0, sizeof(message));
    }

    fclose(fp); // Close the file
}


/*Takes the IP address and port of the server as command line arguments.
  Connects to the server, sends the user's name and creates threads for sending and receiving messages.
  Manages Ctrl+C for an elegant exit.*/
int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("Please enter you user name: ");
  fgets(name, 32, stdin);
  str_trim_lf(name, strlen(name));


	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 32 characters and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);


  // Connect to Server
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);

	

	pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
    return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(flag){
			printf("\nGule Gule\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}