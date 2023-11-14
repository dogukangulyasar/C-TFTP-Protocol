#define _CRT_SECURE_NO_DEPRECATE
#include <iostream>
#include <winsock.h>  // Add wsock32.lib to the project configuration > Linker > Input > Additional Dependencies
#include <stdio.h>
#include <fstream>
#include <string>
#include <sstream>


// TFTP Port: 69
#define PORT 69

#define MAX_BUFFER_LENGTH 516
#define MAX_FILENAME_LEN 100
#define MAX_READ_LEN 512
#define MAX_TRIES 3
#define TIME_OUT 5
#define MAX_PACKETS 99


std::string GenerateAcknowledgePacket(const char* block) {
    std::string packet = "04" + std::string(block);
    return packet;
}

std::string GenerateDataPacket(int block, std::string data) {
    std::string packet;
    if (block <= 9) {
        packet = "030" + std::to_string(block) + data;
    }
    else {
        packet = "03" + std::to_string(block) + data;
    }

    return packet;
}

std::string GenerateErrorMessage(const char* errcode, const char* errmsg) {
    std::string packet = "05" + std::string(errcode) + std::string(errmsg);
    return packet;
}

int CheckTimeout(int socketfd, char* buff, struct sockaddr_in client_address, int client_length) {
    fd_set fdread;
    int n;
    struct timeval tv;

    // Setup file descriptor set
    FD_ZERO(&fdread);
    FD_SET(socketfd, &fdread);

    // Setup struct timeval for the timeout
    tv.tv_sec = TIME_OUT;
    tv.tv_usec = 0;

    // Wait until timeout or data received
    n = select(socketfd + 1, &fdread, NULL, NULL, &tv);
    if (n == 0) {
        std::cout << "[WARNING] Timeout while waiting data received." << std::endl;
        return -2; // Opcode -2 --> Timeout
    }
    else if (n == -1) {
        std::cout << "[ERROR] error";
        return -1; // Opcode -1 --> Error
    }

    return recv(socketfd, buff, MAX_BUFFER_LENGTH - 1, 0);
}

int main() {
    std::cout << "Hello TFTP(UDP) server!\n";
    int retVal = 0;
    // Initialize the WSA variables. Supporting socket programiing in windows environment.
    WSAData ws;

    if (WSAStartup(MAKEWORD(2, 2), &ws) < 0) {
        std::cout << "[ERROR] Can not initalize WSA variables." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    else {
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
    }
    else {
        std::cout << "[SUCCESS] Socket successfully opened." << std::endl;
    }

    retVal = bind(mainSocket, (sockaddr*)&TFTPClient, sizeof(TFTPClient));
    if (retVal < 0) {
        std::cout << "[ERROR] Socket bind failed." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    else {
        std::cout << "[SUCCESS] Successfully bind socket." << std::endl;
    }

    std::cout << "[INFO] Server started, waiting for recvfrom..." << std::endl;

    // Holds request count
    int request_count = -1;
    while (1) {
        // Buffer to hold request message
        std::string request_buffer;
        request_buffer.resize(MAX_BUFFER_LENGTH);
        int client_length = sizeof(TFTPClient);
        // Call recvfrom() to get a request datagram from the client.
        request_count = recvfrom(mainSocket, &request_buffer[0], MAX_BUFFER_LENGTH, 0, (struct sockaddr*)&TFTPClient, &client_length);

        if (request_count == -1) {
            std::cout << "[ERROR] Error while receiving client request." << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        } else { // TODO: Make it multi thread
            if (request_buffer[0] == '0' && request_buffer[1] == '1') { // OPCODE: 01 --> RRQ
                std::cout << "REQUEST BUFFER: " << request_buffer << std::endl;
                request_buffer.erase(0, 2);

                std::ifstream file_pointer(request_buffer, std::ios::binary | std::ios::ate);

                // File is not occurs or does not have valid access, send error message.
                // TODO: Check for access in here as well.
                if (!file_pointer.is_open()) { // SENDING ERROR PACKET - FILE NOT FOUND
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

                // Thanks to use std::ios::ate, we can determine total file size. But need to clear and return to the beginning of the file.
                int remaining = file_pointer.tellg();
                file_pointer.clear();
                file_pointer.seekg(0);

                if (remaining == 0) {
                    remaining++;
                }
                else if (remaining % MAX_READ_LEN == 0) {
                    remaining--;
                }

                int last_remaining = 0;

                // ********** TODO: It reads '\0' but can't received further more from that byte in client???? **********
                while (remaining > 0 && file_pointer.is_open()) {
                    // READING FILE
                    char temp[MAX_READ_LEN + 5]; // +5?
                    if (remaining > MAX_READ_LEN) {
                        // Read from file
                        file_pointer.read(temp, MAX_READ_LEN);
                        last_remaining = MAX_READ_LEN;
                        remaining -= MAX_READ_LEN;
                    } else {
                        file_pointer.read(temp, remaining);
                        last_remaining = remaining;
                        remaining = 0;
                    }

                    // SENDING DATA PACKET
                    std::string data(temp, last_remaining);
                    std::string data_packet = GenerateDataPacket(block, data);

                    if ((request_count = sendto(mainSocket, &data_packet[0], data_packet.size(), 0, (struct sockaddr*)&TFTPClient, client_length)) == -1) {
                        std::cout << "[ERROR] Error sending data packet. Errcode: " << WSAGetLastError() << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }

                    std::cout << "[SERVER] Sent " << request_count << " bytes to client" << std::endl;

                    // WAITING FOR ACK MESSAGE
                    int time;
                    for (time = 0; time <= MAX_TRIES; time++) {
                        if (time == MAX_TRIES) {
                            std::cout << "[ERROR] Maximum number of tries reached while waiting ACK message from client." << std::endl;
                            WSACleanup();
                            exit(EXIT_FAILURE);
                        }

                        request_count = CheckTimeout(mainSocket, &request_buffer[0], TFTPClient, client_length);

                        if (request_count == -1) { // Error
                            std::cout << "[ERROR] Error occured while waiting ACK Packet from client." << std::endl;
                            WSACleanup();
                            exit(EXIT_FAILURE);
                        }
                        else if (request_count == -2) { // Timeout, try again.
                            std::cout << "[ERROR] Try to send ACK Packet to Client again." << std::endl;
                            int temp_bytes;
                            if ((temp_bytes = sendto(mainSocket, &data_packet[0], data_packet.size(), 0, (struct sockaddr*)&TFTPClient, client_length) == -1)) {
                                std::cout << "[ERROR] Error occured while trying to send Data Packet to client." << std::endl;
                                WSACleanup();
                                exit(EXIT_FAILURE);
                            }

                            std::cout << "[SERVER] Sent " << temp_bytes << " again." << std::endl;
                            continue;
                        }
                        else {
                            break;
                        }
                    }
                    std::cout << "[SERVER] Got ACK Packet " << request_count << " bytes long." << std::endl;
                    block++;
                    if (block > MAX_PACKETS)
                        block = 1;
                }

                file_pointer.close();
            } else if (request_buffer[0] == '0' && request_buffer[1] == '2') { // OPCODE: 02 --> WRQ
                std::cout << "[SERVER] Got WRQ Request." << std::endl;
                // Sending ACK Packet
                std::string message = GenerateAcknowledgePacket("00");
                std::string last_message(request_buffer);

                std::string last_sent_ack(message);

                if ((request_count = sendto(mainSocket, &message[0], message.size(), 0, (struct sockaddr*)&TFTPClient, client_length)) == -1) {
                    std::cout << "[ERROR] Error in sending ACK packet." << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                std::cout << "[SERVER] ACK packet sent." << std::endl;

                // Setting file name
                // Remove opcode from request_buffer
                std::string filename = request_buffer.substr(2, request_buffer.size());
                std::ofstream file_pointer(filename, std::ios::binary);
                
                if (file_pointer.is_open() == NULL) {
                    std::cout << "[ERROR] File " << filename << " access denied. Sending error packet" << std::endl;
                    std::string error_message = GenerateErrorMessage("05", "ERROR_ACCESS_DENIED");
                    sendto(mainSocket, &error_message[0], error_message.size(), 0, (struct sockaddr*)&TFTPClient, client_length);
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                do {
                    // Receiving file - Packet Data
                    if ((request_count = recvfrom(mainSocket, &request_buffer[0], MAX_BUFFER_LENGTH, 0, (struct sockaddr*)&TFTPClient, &client_length)) == -1) {
                        std::cout << "[ERROR] Could not receive data from client for write operation." << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }
                    std::cout << "[SERVER] Got packet " << request_count << " bytes long." << std::endl;

                    //SENDING LAST ACK AGAIN - AS IT HAS NOT REACHED
                    if (request_buffer == last_message) {
                        sendto(mainSocket, &last_sent_ack[0], last_sent_ack.size(), 0, (struct sockaddr*)&TFTPClient, client_length);
                        continue;
                    }

                    // Preparing block information
                    char block[3];
                    strncpy(block, request_buffer.c_str() + 2, 2);
                    block[2] = '\0';

                    // Writing file
                    last_message = request_buffer;
                    std::string write_buffer = request_buffer.substr(4, request_buffer.size()); // Eliminate opcode + block info
                    file_pointer.write(write_buffer.c_str(), request_count - 4);

                    // Sending ACK packet
                    std::string ack_packet = GenerateAcknowledgePacket(block);
                    int ack_count;
                    if ((ack_count = sendto(mainSocket, &ack_packet[0], ack_packet.size(), 0, (struct sockaddr*)&TFTPClient, client_length)) == -1) {
                        std::cout << "[ERROR] Could not snet ACK packet." << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }

                    std::cout << "[SERVER] ACK packet sent: " << request_count << " bytes long" << std::endl;
                    last_sent_ack = ack_packet;
                } while (request_count == MAX_BUFFER_LENGTH);
                std::cout << "[SERVER] New file " << filename << " successfully created." << std::endl;
                file_pointer.close();
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
