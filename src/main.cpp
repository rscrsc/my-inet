#include <asio.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <functional>

using asio::ip::tcp;

// Abstracts handling of received data
using ReceiveHandler = std::function<void(const std::string& message, std::shared_ptr<class Session>)>;
// Abstracts message to send on connection
using SendMessageProvider = std::function<std::string()>;

// Session class handling individual client communication
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, SendMessageProvider message_provider, ReceiveHandler receive_handler)
        : socket_(std::move(socket)), 
          message_provider_(std::move(message_provider)), 
          receive_handler_(std::move(receive_handler)) {}

    void start() {
        start_read();
    }

    tcp::socket& get_socket() {
        return socket_;
    }

private:
    void send_message() {
        auto self = shared_from_this();
        std::string message = message_provider_();
        asio::async_write(socket_, asio::buffer(message),
                          [this, self](std::error_code ec, std::size_t /*length*/) {
                              if (!ec) {
                                  start_read();
                              } else {
                                  std::cerr << "Error sending message: " << ec.message() << std::endl;
                              }
                          });
    }

    void start_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(data_),
                                [this, self](std::error_code ec, std::size_t length) {
                                    if (!ec) {
                                        handle_receive(std::string(data_.data(), length));
                                    } else {
                                        std::cerr << "Error reading data: " << ec.message() << std::endl;
                                    }
                                });
    }

    void handle_receive(const std::string& message) {
        // Delegate received data to the provided handler
        receive_handler_(message, shared_from_this());
        send_message();
    }

    tcp::socket socket_;
    std::array<char, 1024> data_;
    SendMessageProvider message_provider_;
    ReceiveHandler receive_handler_;
};

// Server class managing connections
class Server {
public:
    Server(asio::io_context& io_context, short port, SendMessageProvider message_provider, ReceiveHandler receive_handler)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          message_provider_(std::move(message_provider)),
          receive_handler_(std::move(receive_handler)) {
        start_accept();
    }

private:
    void start_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), message_provider_, receive_handler_)->start();
                } else {
                    std::cerr << "Error accepting connection: " << ec.message() << std::endl;
                }
                start_accept();
            });
    }

    tcp::acceptor acceptor_;
    SendMessageProvider message_provider_;
    ReceiveHandler receive_handler_;
};

// Example message provider and receive handler
std::string provide_message() {
    std::string msg = "HTTP/1.1 200 OK\r\n"
                      "Content-Length: 5\r\n"
                      "Content-Type: text/html\r\n"
                      "\r\n"
                      "hello\r\n";
    std::cout << "Sent:\n" << msg << std::endl;
    return msg;
}

void handle_received_message(const std::string& message, std::shared_ptr<Session> session) {
    std::cout << "Received:\n" << message << std::endl;
}

int main() {
    try {
        asio::io_context io_context;

        Server server(io_context, 6969, provide_message, handle_received_message);

        std::cout << "Server is running on port 6969..." << std::endl;

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}

