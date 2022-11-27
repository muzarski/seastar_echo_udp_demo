#include <iostream>
#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/util/log.hh>

using namespace seastar::net;

class Server {
private:
    udp_channel channel;

    seastar::future<> handle_receive(udp_datagram &&datagram) {
        std::cout << "Read " << datagram.get_data().len() << " bytes from " << datagram.get_src() << "\n";

        char buf[DATAGRAM_SIZE];
        memcpy(buf, datagram.get_data().fragment_array()->base, datagram.get_data().len());
        buf[datagram.get_data().len()] = '\0';
        std::cout << "Message is " << buf << "\n";

        std::string s = buf;
        std::reverse(s.begin(), s.end());

        return seastar::do_with(std::move(s), [this, &datagram] (std::string &to_send) {
            std::cout << "Sending " << to_send << "\n";
            return channel.send(datagram.get_src(), to_send.c_str());
        });
    }

public:
    explicit Server(std::uint16_t listen_port) :
        channel(seastar::make_udp_channel(listen_port)) {};

    seastar::future<> service_loop() {
        return seastar::keep_doing([this] () {
            return channel.receive().then([this] (udp_datagram datagram) {
                return handle_receive(std::move(datagram));
            });
        });
    }
};

seastar::future<> submit_to_cores(uint16_t port) {
    return seastar::parallel_for_each(boost::irange<unsigned>(0, seastar::smp::count),
            [port] (unsigned core) {
        return seastar::smp::submit_to(core, [port] () {
            Server _server(port);
            return seastar::do_with(std::move(_server), [] (Server &server) {
                return server.service_loop();
            });
        });
    });
}

int main(int argc, char **argv) {
    seastar::app_template app;

    namespace po = boost::program_options;
    app.add_options()
            ("port", po::value<std::uint16_t>()->required(), "listen port");

    try {
        app.run(argc, argv, [&] () {
            auto&& config = app.configuration();
            std::uint16_t port = config["port"].as<std::uint16_t>();
            return submit_to_cores(port);
        });
    } catch(...) {
        std::cerr << "Couldn't start application: " << std::current_exception() << '\n';
        return 1;
    }
    return 0;
}
