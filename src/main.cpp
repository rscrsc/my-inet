#include <asio.hpp>
#include <iostream>
#include <memory>
#include <thread>

using asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket)
        : socket_(std::move(socket)) {}

    void start() {
        send_hello();
    }

private:
    void send_hello() {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer("hello\n"),
                          [this, self](std::error_code ec, std::size_t /*length*/) {
                              if (!ec) {
                                  start_read();
                              } else {
                                  std::cerr << "Error sending hello: " << ec.message() << std::endl;
                              }
                          });
    }

    void start_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(data_),
                                [this, self](std::error_code ec, std::size_t length) {
                                    if (!ec) {
                                        echo_back(length);
                                    } else {
                                        std::cerr << "Error reading data: " << ec.message() << std::endl;
                                    }
                                });
    }

    void echo_back(std::size_t length) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(data_, length),
                          [this, self](std::error_code ec, std::size_t /*length*/) {
                              if (!ec) {
                                  start_read();
                              } else {
                                  std::cerr << "Error echoing data: " << ec.message() << std::endl;
                              }
                          });
    }

    tcp::socket socket_;
    std::array<char, 1024> data_;
};

class Server {
public:
    Server(asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        start_accept();
    }

private:
    void start_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                } else {
                    std::cerr << "Error accepting connection: " << ec.message() << std::endl;
                }
                start_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        asio::io_context io_context;

        Server server(io_context, 12345); // Listening on port 12345

        std::cout << "Server is running on port 12345..." << std::endl;

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}

