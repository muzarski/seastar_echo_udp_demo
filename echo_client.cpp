#include <iostream>
#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/iostream.hh>
#include <seastar/core/sleep.hh>

using namespace seastar::net;

class Client {
private:
    udp_channel channel;
    seastar::ipv4_addr server_address;

    seastar::future<> read_stdin_and_send() {
        std::string s;
        std::cin >> s;
        return seastar::do_with(std::move(s), [this] (std::string &to_send) {
            return channel.send(server_address, to_send.c_str());
        });
    }

    seastar::future<> receive() {
        return channel.receive().then([] (udp_datagram datagram) {
            char buf[DATAGRAM_SIZE];
            memcpy(buf, datagram.get_data().fragment_array()->base, datagram.get_data().len());
            buf[datagram.get_data().len()] = '\0';
            std::cout << buf << "\n";
        });
    }

public:
    Client(const std::string &host, std::uint16_t port) :
        channel(seastar::make_udp_channel()),
        server_address(seastar::ipv4_addr(host, port)) {
    };

    seastar::future<> service_loop() {
        return seastar::keep_doing([this] () {
            return read_stdin_and_send().then([this] () {
                return receive();
            });
        });
    }
};

static seastar::future<> submit_to_cores(std::string &host, std::uint16_t port) {
    return seastar::parallel_for_each(boost::irange<unsigned>(0, seastar::smp::count),
            [&host, &port] (unsigned core) {
        return seastar::smp::submit_to(core, [&host, &port] () {
            Client _client(host, port);
            return seastar::do_with(std::move(_client), [] (Client &client) {
                return client.service_loop();
            });
        });
    });
}

int main(int argc, char **argv) {
    seastar::app_template app;

    namespace po = boost::program_options;
    app.add_options()
            ("host", po::value<std::string>()->required(), "server address")
            ("port", po::value<std::uint16_t>()->required(), "listen port");

    try {
        app.run(argc, argv, [&] () {
            auto&& config = app.configuration();
            std::string server_address = config["host"].as<std::string>();
            std::uint16_t port = config["port"].as<std::uint16_t>();
            return seastar::do_with(std::move(server_address), port,
                                    [] (std::string &addr, std::uint16_t &port) {
               return submit_to_cores(addr, port);
            });
        });
    } catch(...) {
        std::cerr << "Couldn't start application: " << std::current_exception() << '\n';
        return 1;
    }
    return 0;
}
