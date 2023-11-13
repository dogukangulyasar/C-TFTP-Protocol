#define _CRT_SECURE_NO_DEPRECATE
#include <winsock.h>
#include <iostream>
#include <string>
#include <fstream>

#define PORT 69

#define MAX_BUFFER_LENGTH 516
#define MAX_FILENAME_LENGTH 100
#define MAX_READ_LEN 516

std::string GenerateACK(char* block) {
    std::string packet = "04" + std::string(block);
    //char* packet;
    //packet = (char*)malloc(2 + strlen(block));
    //memset(packet, 0, sizeof packet);
    //strcat(packet, "04");//opcode
    //strcat(packet, block);
    return packet;
}

std::string GenereateRRQ(const char* filename) {
    /*TODO: Generate RRQ Packet*/
    std::string packet = "01" + std::string(filename);
    //char* packet = (char*)malloc(2 + strlen(filename));
    //memset(packet, 0, sizeof packet);
    //strcat(packet, "01");//opcode
    //strcat(packet, filename);
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
    memset(TFTPServerSocket.sin_zero, 0, 8);

    int server_length = sizeof(TFTPServerSocket);

    retVal = connect(clientSocket, (struct sockaddr*)&TFTPServerSocket, sizeof(TFTPServerSocket));
    if (retVal < 0) {
        std::cout << "[ERROR] Failed to connect socket on port: " << PORT << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Socket connected successfully on port: " << PORT << std::endl;
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

                // Writing file
                request_buffer.erase(0, 4);
                file_pointer.write(request_buffer.c_str(), request_count - 4); // 4 --> opcode(2bytes) + block(2bytes) size.
                //fwrite(request_buffer.c_str(), sizeof(char), request_count - 4, fp);
                last_recv_message = request_buffer;

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
            } while (request_count == MAX_READ_LEN);
            std::cout << "[CLIENT] New file " << filename << " successfully created." << std::endl;
            file_pointer.close();
        } else if((strcmp(argv[1], "PUT") == 0) || (strcmp(argv[1], "put") == 0)) { // WRQ
            // TODO: IMPLEMENT IT
        } else {
            std::cout << "[ERROR]: Invalid request." << argv[1] << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_SUCCESS);
}
