#include "server.h"

int main(int argc, char* argv[])
{
    /*
    int port = 9090;
    int packetSize = 0;
    //std::string tmpstr = "get buff:GET /dsdsa HTTP/1.1";
    //std::cout << parseStr(tmpstr);
    if (argc > 1) {
        port = std::stoi(argv[1]);
        packetSize = std::stoi(argv[2]);
        std::cout << "Local server is running on " + std::to_string(port) 
            <<" port and it work with " << std::to_string(packetSize) << " packets" << std::endl;
        try {
            boost::asio::io_service ios;
            Server s(ios, port, packetSize);
            ios.run();
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }*/

    talk_to_client::ptr client = talk_to_client::new_();
    acceptor.async_accept(client->sock(), boost::bind(handle_accept, client, _1));
    service.run();
    return 0;
}

