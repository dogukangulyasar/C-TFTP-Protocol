#define _CRT_SECURE_NO_DEPRECATE
#include <winsock.h>
#include <iostream>
#include <string>
#include <fstream>

#define PORT 69

#define MAX_BUFFER_LENGTH 516 // TODO --> 512 + 4 NOT 516.
#define MAX_FILENAME_LENGTH 100
#define MAX_READ_LEN 512

#define TIME_OUT 5
#define MAX_TRIES 3
#define MAX_PACKETS 99

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

std::string GenerateACK(char* block) {
    std::string packet = "04" + std::string(block);
    return packet;
}

std::string GenereateRRQ(const char* filename) {
    std::string opcode_for_rrq = "01";
    std::string packet = opcode_for_rrq + std::string(filename);
    return packet;
}

std::string GenerateWRQ(const char* filename) {
    std::string opcode_for_wrq = "02";
    std::string packet = opcode_for_wrq + std::string(filename);

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

int main(int argc, char* argv[]) {
    std::cout << "Hello TFTP Client!\n";

    int retVal = 0;
    struct sockaddr_in TFTPServerSocket;
    int request_count = 0;
    std::string file = argv[2]; // file name on which operation has to be done
    WSAData ws;

    if (WSAStartup(MAKEWORD(2, 2), &ws) < 0) {
        std::cout << "[ERROR] Can not initalize WSA variables." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    } else {
        std::cout << "[SUCCESS] WSA variables successfully initalized." << std::endl;
    }

    // Initialize socket as UDP & SOCK_DGRAM.
    // AF_INET --> Address familiy. Used for properly resolve addresses.
    // PF_INET --> Can be used but really, what is difference??
    int clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (clientSocket < 0) {
        std::cout << "[ERROR] Socket can not opened." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    } else {
        std::cout << "[SUCCESS] Socket successfully opened." << std::endl;
    }

    TFTPServerSocket.sin_family = AF_INET;
    TFTPServerSocket.sin_port = htons(PORT);
    TFTPServerSocket.sin_addr.s_addr = inet_addr("127.0.0.1");
    //memset(TFTPServerSocket.sin_zero, 0, 8);

    int server_length = sizeof(TFTPServerSocket);

    retVal = connect(clientSocket, (struct sockaddr*)&TFTPServerSocket, sizeof(TFTPServerSocket));
    if (retVal < 0) {
        std::cout << "[ERROR] Failed to connect socket on port: " << PORT << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    } else {
        std::cout << "[SUCCESS] Socket connected successfully on port: " << PORT << std::endl;
        // Decide what is the operation
        if ((strcmp(argv[1], "GET") == 0) || (strcmp(argv[1], "get") == 0)) { // RRQ
            // Sending RRQ
            std::string fileOpenFromServer = "C:/socket_programming/tftp_application/TFTPServer/x64/Debug/" + file + '\0';
            std::string message = GenereateRRQ(fileOpenFromServer.c_str());
            std::cout << "Message: " << message << std::endl;
            std::string last_recv_message(message);
            std::string last_sent_ack(message);
            if ((request_count = sendto(clientSocket, &message[0], message.size(), 0, (struct sockaddr*)&TFTPServerSocket, server_length)) == -1) {
                std::cout << "[ERROR] Failed to sent message on RRQ: " << std::endl;
                WSACleanup();
                exit(EXIT_FAILURE);
            }
            std::cout << "[CLIENT] RRQ message sent " << request_count << " bytes long." << std::endl;
            std::string filename(file);

            //FILE* fp = fopen(filename.c_str(), "wb");
            std::ofstream file_pointer(filename, std::ios::binary);

            if (!file_pointer.is_open()) {
                std::cout << "[ERROR] Failed to open file." << std::endl;
                WSACleanup();
                exit(EXIT_FAILURE);
            }

            // Receiving actual file
            do {
                std::string request_buffer;
                request_buffer.resize(MAX_BUFFER_LENGTH);
                if ((request_count = recvfrom(clientSocket, &request_buffer[0], MAX_BUFFER_LENGTH, 0, (struct sockaddr*)&TFTPServerSocket, &server_length)) == -1) {
                    std::cout << "[ERROR] Failed to receiving file data. Error: " << WSAGetLastError() << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }
                std::cout << "[CLIENT] Got packet " << request_count << " bytes long." << std::endl;

                // Checking if error packet
                if (request_buffer[0] == '0' && request_buffer[1] == '5') {
                    std::cout << "[ERROR] Received RRQ packet is error packet: " << request_buffer << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                // Sending last ACK again
                if (request_buffer == last_recv_message) {
                    sendto(clientSocket, &last_sent_ack[0], last_sent_ack.size(), 0, (struct sockaddr*)&TFTPServerSocket, server_length);
                    continue;
                }
                // Prepare ACK
                char block[3];
                strncpy(block, request_buffer.c_str() + 2, 2);
                block[2] = '\0';

                last_recv_message = request_buffer;
                // Erase opcode + block information from message
                request_buffer.erase(0, 4);
                // Writing file
                file_pointer.write(request_buffer.c_str(), request_count - 4); // 4 --> opcode(2bytes) + block(2bytes) size.
                //fwrite(request_buffer.c_str(), sizeof(char), request_count - 4, fp);

                // Sending ACK
                int send_count;
                std::string ack_message = GenerateACK(block);
                if ((send_count = sendto(clientSocket, &ack_message[0], ack_message.size(), 0, (struct sockaddr*)&TFTPServerSocket, server_length)) == -1) {
                    std::cout << "[ERROR] Failed to sent ACK packet." << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                std::cout << "[CLIENT] Sent ACK message " << ack_message << " bytes long" << std::endl;
                last_sent_ack = ack_message;
            } while (request_count == MAX_BUFFER_LENGTH);
            std::cout << "[CLIENT] New file " << filename << " successfully created." << std::endl;
            file_pointer.close();
        } 
        else if((strcmp(argv[1], "PUT") == 0) || (strcmp(argv[1], "put") == 0)) { // WRQ
            std::string request_buffer;
            request_buffer.resize(MAX_BUFFER_LENGTH);
            // Generating WRQ message
            std::string message = GenerateWRQ(file.c_str());
            std::string last_message;

            if (request_count = sendto(clientSocket, &message[0], message.size(), 0, (struct sockaddr*)&TFTPServerSocket, server_length) == -1) {
                std::cout << "[ERROR] Failed to sent WRQ packet." << std::endl;
                WSACleanup();
                exit(EXIT_FAILURE);
            }
            last_message = message;

            // WAITING FOR ACK MESSAGE - WRQ Request
            int time;
            for (time = 0; time < MAX_TRIES; time++) {
                // Reached max number of tries
                if (time == MAX_TRIES) {
                    std::cout << "[ERROR] Failed to get ACK packet from server within specified time." << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                // Check for timeout --> Recvfrom is inside the CheckTimeout method.
                request_count = CheckTimeout(clientSocket, &request_buffer[0], TFTPServerSocket, server_length);

                if (request_count == -1) { // Error
                    std::cout << "[ERROR] Failed to get ACK packet from server." << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                } else if (request_count == -2) { // Timeout
                    std::cout << "[WARNING] Try to send last_message to Server again. Try# " << time << std::endl;
                    int temp_bytes;
                    if (request_count = sendto(clientSocket, &last_message[0], last_message.size(), 0, (struct sockaddr*)&TFTPServerSocket, server_length) == -1) {
                        std::cout << "[ERROR] Failed to send last_message to the Server." << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }
                    std::cout << "[CLIENT] last_message sent to the server again. Bytes: " << request_count << std::endl;
                    continue;
                } else { // Valid case. Stop waiting
                    break;
                }
            }
            std::cout << "[CLIENT] Got ACK Packet for WRQ Request " << request_count << " bytes long." << std::endl;

            // Check for packet received is ACK. 04 --> opcode
            if (request_buffer.at(0) == '0' && request_buffer.at(1) == '4') {
                // It's pointer start from end thanks to std::ios::ate flag
                std::ifstream file_pointer(file, std::ios::ios_base::binary | std::ios::ate);

                // Check if file is open
                if (!file_pointer.is_open()) {
                    std::cout << "[ERROR] File not found for writing request." << std::endl;
                    WSACleanup();
                    exit(EXIT_FAILURE);
                }

                // Calculating size of file
                int remaining = file_pointer.tellg();
                // Reset pointer of file
                file_pointer.clear();
                file_pointer.seekg(0);

                int block = 1;

                int last_remaining = 0;
                while (remaining > 0 && file_pointer.is_open()) {
                    // READING FILE
                    char temp[MAX_READ_LEN + 5];
                    if (remaining > MAX_READ_LEN) {
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

                    if ((request_count = sendto(clientSocket, &data_packet[0], data_packet.size(), 0, (struct sockaddr*)&TFTPServerSocket, server_length)) == -1) {
                        std::cout << "[ERROR] Could not send data_packet" << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }

                    std::cout << "[CLIENT] Sent " << request_count << " bytes to server." << std::endl;
                    last_message = data_packet;

                    // WAITING FOR ACK - Data packet
                    int time;
                    for (time = 0; time < MAX_TRIES; time++) {
                        // Reached max number of tries
                        if (time == MAX_TRIES) {
                            std::cout << "[ERROR] Failed to get ACK packet from server within specified time." << std::endl;
                            WSACleanup();
                            exit(EXIT_FAILURE);
                        }

                        // Check for timeout --> Recvfrom is inside the CheckTimeout method.
                        request_count = CheckTimeout(clientSocket, &request_buffer[0], TFTPServerSocket, server_length);

                        if (request_count == -1) { // Error
                            std::cout << "[ERROR] Failed to get ACK packet from server." << std::endl;
                            WSACleanup();
                            exit(EXIT_FAILURE);
                        } else if (request_count == -2) { // Timeout
                            std::cout << "[WARNING] Try to send last_message to Server again. Try# " << time << std::endl;
                            int temp_bytes;
                            if (request_count = sendto(clientSocket, &last_message[0], last_message.size(), 0, (struct sockaddr*)&TFTPServerSocket, server_length) == -1) {
                                std::cout << "[ERROR] Failed to send last_message to the Server." << std::endl;
                                WSACleanup();
                                exit(EXIT_FAILURE);
                            }
                            std::cout << "[CLIENT] last_message sent to the server again. Bytes: " << request_count << std::endl;
                            continue;
                        } else { // Valid case. Stop waiting
                            break;
                        }
                    }
                    std::cout << "[CLIENT] Got ACK Packet for Data Packet " << request_count << " bytes long." << std::endl;

                    // Check if error packet received
                    if (request_buffer.at(0) == '0' && request_buffer.at(1) == '5') {
                        std::cout << "[ERROR] Error packet received instead of ACK from Server." << std::endl;
                        WSACleanup();
                        exit(EXIT_FAILURE);
                    }

                    block++;
                    if (block > MAX_PACKETS)
                        block = 1;
                }

                file_pointer.close();
            } else { // Bad packet received
                std::cout << "[ERROR] Bad packet received from server." << std::endl;
                WSACleanup();
                exit(EXIT_FAILURE);
            }
        } else {
            std::cout << "[ERROR]: Invalid request." << argv[1] << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        }
    }
    closesocket(clientSocket);
    exit(EXIT_SUCCESS);
}
