#include <iostream>
#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/util/log.hh>
#include <seastar/core/iostream.hh>

#define DATAGRAM_SIZE 1350

std::string server_address = "127.0.0.1";
uint16_t port = 12345;

using namespace seastar::net;

seastar::future<> receive(udp_channel &channel) {
    return channel.receive().then([] (udp_datagram datagram) {
        char buf[DATAGRAM_SIZE];
        memcpy(buf, datagram.get_data().fragment_array()->base, datagram.get_data().len());
        buf[datagram.get_data().len()] = '\0';
        std::cout << "Message is " << buf << "\n";
        std::cout << "Actual message is " << datagram.get_data() << "\n";
    });
}


seastar::future<> read_stdin_and_send(udp_channel &channel, seastar::ipv4_addr &addr) {
    std::string s;
    std::cin >> s;
    return seastar::do_with(std::move(s), [&channel, &addr] (std::string &to_send) {
        std::cout << "Sending " << to_send << "\n";
        return channel.send(addr, to_send.c_str());
    });
}

seastar::future<> service_loop() {
    return seastar::do_with(seastar::make_udp_channel(), seastar::ipv4_addr(server_address, port),
                            [] (udp_channel &channel, seastar::ipv4_addr &addr) {
        return seastar::keep_doing([&channel, &addr] {
            return read_stdin_and_send(channel, addr).then([&channel] () {
                return receive(channel);
            });
        });
    });
}

seastar::future<> f() {
    return seastar::parallel_for_each(boost::irange<unsigned>(0, seastar::smp::count),
                                      [] (unsigned core) {
                                          return seastar::smp::submit_to(core, service_loop);
                                      });
}

int main(int argc, char **argv) {
    seastar::app_template app;

    namespace po = boost::program_options;
    app.add_options()
            ("server", po::value<std::string>()->required(), "server address")
            ("port", po::value<std::uint16_t>()->required(), "listen port");

    try {
        app.run(argc, argv, [&] () {
            auto&& config = app.configuration();
            server_address = config["server"].as<std::string>();
            port = config["port"].as<std::uint16_t>();
            return f();
        });
    } catch(...) {
        std::cerr << "Couldn't start application: " << std::current_exception() << '\n';
        return 1;
    }
    return 0;
}
