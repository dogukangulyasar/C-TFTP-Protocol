#define _CRT_SECURE_NO_DEPRECATE
#include <iostream>
#include <winsock.h>  // Add wsock32.lib to the project configuration > Linker > Input > Additional Dependencies
#include <stdio.h>
#include <fstream>
#include <string>

// TFTP Port: 69
#define PORT 69

#define MAX_BUFF_LENGTH 512
#define MAX_FILENAME_LEN 100
#define MAX_READ_LEN 512
#define MAX_TRIES 3
#define TIME_OUT 5
#define MAX_PACKETS 99

// converts block number to length-2 string
void s_to_i(char* f, int n) {
    if (n == 0) {
        f[0] = '0', f[1] = '0', f[2] = '\0';
    }
    else if (n % 10 > 0 && n / 10 == 0) {
        char c = n + '0';
        f[0] = '0', f[1] = c, f[2] = '\0';
    }
    else if (n % 100 > 0 && n / 100 == 0) {
        char c2 = (n % 10) + '0';
        char c1 = (n / 10) + '0';
        f[0] = c1, f[1] = c2, f[2] = '\0';
    }
    else {
        f[0] = '9', f[1] = '9', f[2] = '\0';
    }
}

char* GenerateAcknowledgePacket(const char* block) {
    char* packet;
    packet = (char*)malloc(2+strlen(block));
	memset(packet, 0, sizeof packet);
	strcat(packet, "04");//opcode
	strcat(packet, block);
    return packet;
}

std::string GenerateDataPacket(int block, char* data) {
    std::string packet;
    if (block <= 9) {
        packet = "030" + std::to_string(block) + std::string(data);
    } else {
        packet = "03" + std::to_string(block) + std::string(data);
    }
     
 //   char *packet;
	//char temp[3];
	//s_to_i(temp, block);
	//packet = (char*)malloc(4+strlen(data));
	//memset(packet, 0, sizeof packet);
	//strcat(packet, "03");//opcode
	//strcat(packet, temp);
	//strcat(packet, data);

    return packet;
}

std::string GenerateErrorMessage(const char* errcode, const char* errmsg) {
    std::string packet = "05" + std::string(errcode) + std::string(errmsg);
    //char* packet;
    //packet = (char*)malloc(4+strlen(errmsg)); // packet = ?
    //memset(packet, 0, sizeof(packet));
    //std::strcat(packet, "05"); // Setting opcode
    //strcat(packet, errcode); // Setting error code
    //strcat(packet, errmsg); // Setting error mesage
    return packet;
}

int CheckTimeout(int mainSocket, char* buff, struct sockaddr_in client_address, int client_length) {
    fd_set fdread;
    int n;
    struct timeval tv;

    // Setup file descriptor set
    FD_ZERO(&fdread);
    FD_SET(mainSocket, &fdread);

    // Setup struct timeval for the timeout
    tv.tv_sec = TIME_OUT;
    tv.tv_usec = 0;

    // Wait until timeout or data received
    n = select(mainSocket+1, &fdread, NULL, NULL, &tv);
    if (n == 0) {
        std::cout << "[WARNING] Timeout while waiting data received." << std::endl;
        return -2; // Opcode -2 --> Timeout
    } else if(n == -1) {
        std::cout << "[ERROR] error";
        return -1; // Opcode -1 --> Error
    }

    return recv(mainSocket, buff, MAX_BUFF_LENGTH-1, 0);
}

int main() {
    std::cout << "Hello TFTP(UDP) server!\n";
    int retVal = 0;

    // Buffer to hold request message
    std::string request_buffer;
    request_buffer.resize(MAX_BUFF_LENGTH);
    //char request_buffer[MAX_BUFF_LENGTH];

    // Initialize the WSA variables. Supporting socket programiing in windows environment.
    WSAData ws;

    if (WSAStartup(MAKEWORD(2,2), &ws) < 0) {
        std::cout << "[ERROR] Can not initalize WSA variables." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    } else {
        std::cout << "[SUCCESS] WSA variables successfully initalized." << std::endl;
    }

    // Fill client socket address struct --> sockaddr_in
    struct sockaddr_in TFTPClient;
    TFTPClient.sin_family = AF_INET;
    TFTPClient.sin_addr.s_addr = inet_addr("127.0.0.1");
    TFTPClient.sin_port = htons(PORT);

    // Initialize socket as UDP & SOCK_DGRAM.
    // AF_INET --> Address familiy. Used for properly resolve addresses.
    // PF_INET --> Can be used but really, what is difference??
    SOCKET mainSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (mainSocket < 0) {
        std::cout << "[ERROR] Socket can not opened." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    } else {
        std::cout << "[SUCCESS] Socket successfully opened." << std::endl;
    }

    retVal = bind(mainSocket, (sockaddr*)&TFTPClient, sizeof(TFTPClient));
    if (retVal < 0) {
        std::cout << "[ERROR] Socket bind failed." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    } else {
        std::cout << "[SUCCESS] Successfully bind socket." << std::endl;
    }

    std::cout << "[INFO] Server started, waiting for recvfrom..." << std::endl;

    // Holds request count
    int request_count = -1;
    while (1) {
        int client_length = sizeof(TFTPClient);
        // Call recvfrom() to get a request datagram from the client.
        request_count = recvfrom(mainSocket, &request_buffer[0], MAX_BUFF_LENGTH + 1, 0, (struct sockaddr*)&TFTPClient, &client_length);

        if (request_count == -1) {
            std::cout << "[ERROR] Error while receiving client request." << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        } else { // TODO: Make it multi thread
            if (request_buffer[0] == '0' && request_buffer[1] == '1') { // OPCODE: 01 --> RRQ
                std::cout << "REQUEST BUFFER: " << request_buffer << std::endl;
                request_buffer.erase(0, 2);

                // Open file TODO: Convert it to fstream
                FILE* fp = std::fopen(request_buffer.c_str(), "rb");

                // File is not occurs or does not have valid access, send error message.
                // TODO: Check for access in here as well.
                if (fp == NULL) { // SENDING ERROR PACKET - FILE NOT FOUND
                    std::cout << "[ERROR] file not found on server side. filename: " << request_buffer << std::endl;
                    std::string e_msg = GenerateErrorMessage("02", "ERROR_FILE_NOT_FOUND");
                    std::cout << "[ERROR]: " << e_msg << std::endl;
                    
                    // Send to client
                    sendto(mainSocket, &e_msg[0], e_msg.size(), 0, (struct sockaddr*)&TFTPClient, client_length);
                    
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                // Starting to send file
                int block = 1; // Starting block for RRQ
                
                // Put pointer at the end of the file.
                fseek(fp, 0, SEEK_END);
                // Current file position is at the end, so we can calculate total size of it.
                int total = ftell(fp);
                // Put pointer at the beginnig of the file again.
                fseek(fp, 0, SEEK_SET);
                // Calculating remaining bytes.
                int remaining = total;

                if (remaining == 0) {
                    remaining++;
                } else if(remaining % MAX_READ_LEN == 0) {
                    remaining--;
                }

                // TODO: Data packet is corrupted. Try with ifstream.
                while (remaining > 0) {
                    // READING FILE
                    char temp[MAX_READ_LEN + 5]; // +5?
                    if (remaining > MAX_READ_LEN) {
                        // Read from file
                        fread(temp, MAX_READ_LEN, sizeof(char), fp);
                        temp[MAX_READ_LEN] = '\0';
                        remaining -= MAX_READ_LEN;
                    } else {
                        fread(temp, remaining, sizeof(char), fp);
                        temp[remaining] = '\0';
                        remaining = 0;
                    }

                    // SENDING DATA PACKET
                    std::string data_packet = GenerateDataPacket(block, temp);
                    
                    if ((request_count = sendto(mainSocket, &data_packet[0], data_packet.size(), 0, (struct sockaddr*)&TFTPClient, client_length)) == -1) {
                        std::cout << "[ERROR] Error sending data packet. Errcode: " << WSAGetLastError() << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }

                    std::cout << "[SERVER] Sent " << request_count << " bytes to client" << std::endl;

                    // WAITING FOR ACK MESSAGE
                    int time;
                    for ( time = 0; time <= MAX_TRIES; time++) {
                        if (time == MAX_TRIES) {
                            std::cout << "[ERROR] Maximum number of tries reached while waiting ACK message from client." << std::endl;
                            WSACleanup();
                            exit(EXIT_FAILURE);
                        }

                        // TODO: request_count = CheckTimeout(mainSocket, request_buffer, client_address, client_length);
                        request_count = 3;
                        if (request_count == -1) { // Error
                            std::cout << "[ERROR] Error occured while waiting ACK Packet from client." << std::endl;
                            WSACleanup();
                            exit(EXIT_FAILURE);
                        } else if (request_count == -2) { // Timeout
                            std::cout << "[ERROR] Timeout occured while waiting ACK Packet from client." << std::endl;
                            // TODO: Do it again for trying times.
                            continue;
                        } else {
                            break;
                        }
                    }
                    std::cout << "[SERVER] Got ACK Packet " << request_count << " bytes long." << std::endl;
                    request_buffer[request_count] = '\0';
                    block++;
                    if (block > MAX_PACKETS)
                        block = 1;
                }

                fclose(fp);
            } else if (request_buffer[0] == '0' && request_buffer[1] == '2') { // OPCODE: 01 --> WRQ
                /*
                // Sent ACK Packet
                char* message = GenerateAcknowledgePacket("00");
                char lastrecvmessage[MAX_BUFF_LENGTH];
                strcpy(lastrecvmessage, request_buffer);
                char lastsentack[10];
                strcpy(lastsentack, message);
                
                if ((request_count = sendto(mainSocket, message, strlen(message), 0, (struct sockaddr*)&TFTPClient, client_length)) == -1) {
                    std::cout << "[ERROR] Error in sending ACK packet." << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                std::cout << "[SERVER] ACK packet sent." << std::endl;

                // Setting file name
                char filename[MAX_FILENAME_LEN];
                strcpy(filename, request_buffer + 2);
                strcat(filename, "_server");

                // TODO: Check for access & duplicate.

                FILE* fp = fopen(filename, "wb");
                // TODO: Check for access as well.
                if (fp == NULL) {
                    std::cout << "[ERROR] File " << filename << " access denied. Sending error packet" << std::endl;
                    char* errormessage = GenerateErrorMessage("05", "ERROR_ACCESS_DENIED");
                    sendto(mainSocket, errormessage, strlen(errormessage), 0, (struct sockaddr*) &TFTPClient, client_length);
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                int c_written;
                do {
                    // Receiving file - Packet Data
                    if ((request_count = recvfrom(mainSocket, request_buffer, MAX_BUFF_LENGTH - 1, 0, (struct sockaddr*)&TFTPClient, &client_length)) == -1) {
                        std::cout << "[ERROR] Could not receive data from client for write operation." << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }

                    std::cout << "[SERVER] Got packet " << request_count << " bytes long." << std::endl;
                    request_buffer[request_count] = '\0';
                    std::cout << "[SERVER] Packet contains " << request_buffer << std::endl;

                    //SENDING LAST ACK AGAIN - AS IT HAS NOT REACHED
                    if (strcmp(request_buffer, lastrecvmessage) == 0) {
                        sendto(mainSocket, lastsentack, strlen(lastsentack), 0, (struct sockaddr*)&TFTPClient, client_length);
                        continue;
                    }

                    // Writing file
                    c_written = strlen(request_buffer + 4);
                    fwrite(request_buffer + 4, sizeof(char), c_written, fp);
                    strcpy(lastrecvmessage, request_buffer);

                    // Sending ACK packet
                    char block[3];
                    strncpy(block, request_buffer+2, 2);
                    block[2] = '\0';
                    char* ack_packet = GenerateAcknowledgePacket(block);
                    if ((request_count = sendto(mainSocket, ack_packet, strlen(ack_packet), 0, (struct sockaddr*)&TFTPClient, client_length)) == -1) {
                        std::cout << "[ERROR] Could not snet ACK packet." << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }

                    std::cout << "[SERVER] ACK packet sent: " << request_count << " bytes long" << std::endl;
                    strcpy(lastsentack, ack_packet);
                } while (c_written == MAX_READ_LEN);
                std::cout << "[SERVER] New file " << filename << " successfully created." << std::endl;
                fclose(fp);
                */
            } else { // INVALID REQUEST
                std::cout << "[ERROR] Invalid request received from client" << std::endl;
                WSACleanup();
                exit(EXIT_FAILURE);
            }

            closesocket(mainSocket);
            return 0;
        }
    }

}
