#include <iostream>
#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/util/log.hh>

#define DATAGRAM_SIZE 1350

uint16_t port = 12345;

using namespace seastar::net;

seastar::future<> handle_read(udp_channel &channel, udp_datagram datagram) {
    std::cout << "Read " << datagram.get_data().len() << " bytes from " << datagram.get_src() << "\n";

    char buf[DATAGRAM_SIZE];
    memcpy(buf, datagram.get_data().fragment_array()->base, datagram.get_data().len());
    buf[datagram.get_data().len()] = '\0';
    std::cout << "Message is " << buf << "\n";
    std::cout << "Actual message is " << datagram.get_data() << "\n";

    std::string s = buf;
    std::reverse(s.begin(), s.end());

    return seastar::do_with(std::move(s), [&channel, &datagram] (std::string &to_send) {
        std::cout << "Sending " << to_send << "\n";
        return channel.send(datagram.get_src(), to_send.c_str());
    });
}

seastar::future<> service_loop() {
    return seastar::do_with(seastar::make_udp_channel(port), [] (udp_channel &channel) {
        return seastar::keep_doing([&channel] {
            return channel.receive().then([&channel] (udp_datagram datagram) {
                return handle_read(channel, std::move(datagram));
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
            ("port", po::value<std::uint16_t>()->required(), "listen port");

    try {
        app.run(argc, argv, [&] () {
            auto&& config = app.configuration();
            port = config["port"].as<std::uint16_t>();
            return f();
        });
    } catch(...) {
        std::cerr << "Couldn't start application: " << std::current_exception() << '\n';
        return 1;
    }
    return 0;
}
