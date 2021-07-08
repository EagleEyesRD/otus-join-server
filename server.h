#include <iostream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(boost::asio::io_service& ios)
        : socket(ios) {}

    tcp::socket& get_socket()
    {
        return socket;
    }

    void start(int pSize)
    {

        pm = new PackManager(pSize);
        std::cout << "run packetmanager" << std::endl;
        socket.async_read_some(
            boost::asio::buffer(data, max_length),
            boost::bind(&Session::handle_read, this,
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void handle_read(std::shared_ptr<Session>& s,
        const boost::system::error_code& err,
        size_t bytes_transferred)
    {
        //std::string line = "";
        if (!err) {
            line = parseStr(std::string(data));
            std::cout << "get buff:" << line << std::endl;
            socket.async_read_some(
                boost::asio::buffer(data, max_length),
                boost::bind(&Session::handle_read, this,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
        else {
            std::cerr << "err (recv): " << err.message() << std::endl;
        }
        /*std::cout << "try run commandline:" << line << std::endl;
        if (line.length() > 0)
            if (pm->isrunpacketfile(line)) {
                std::cout << "push to commandline\n";
                pm->runpacket(line);
            }*/
    }

    std::string getCommand()const { return line; }
private:
    std::string parseStr(std::string tmpstr) {
        std::string secstr = tmpstr.substr(tmpstr.find('/') + 1);
        std::string resstr = secstr.substr(0, (tmpstr.substr(tmpstr.find('/') + 1)).find(' '));
        return resstr;
    };
    tcp::socket socket;
    enum { max_length = 1024 };
    char data[max_length];
    PackManager* pm;
    std::string line = "";
};

class Server {
public:
    Server(boost::asio::io_service& ios,
        short port, int pSize) : ios(ios), acceptor(ios, tcp::endpoint(tcp::v4(), port)), packSize(pSize)
    {
        std::shared_ptr<Session> session = std::make_shared<Session>(ios);
        acceptor.async_accept(session->get_socket(),
            boost::bind(&Server::handle_accept, this,
                session,
                boost::asio::placeholders::error));
    }

    void handle_accept(std::shared_ptr<Session> session,
        const boost::system::error_code& err)
    {
        if (!err) {
            session->start(packSize);
            session = std::make_shared<Session>(ios);
            std::cout << "run session" << std::endl;
            acceptor.async_accept(session->get_socket(),
                boost::bind(&Server::handle_accept, this, session,
                    boost::asio::placeholders::error));
        }
        else {
            std::cerr << "err: " + err.message() << std::endl;
            session.reset();
        }
    }
private:
    boost::asio::io_service& ios;
    tcp::acceptor acceptor;
    int  packSize;
};
////////////////////////////////////////////////////////////////
#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
using namespace boost::asio;
using namespace boost::posix_time;
io_service service;

class talk_to_client;
typedef boost::shared_ptr<talk_to_client> client_ptr;
typedef std::vector<client_ptr> array;
array clients;

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)


void update_clients_changed();

/** simple connection to server:
    - logs in just with username (no password)
    - all connections are initiated by the client: client asks, server answers
    - server disconnects any client that hasn't pinged for 5 seconds

    Possible client requests:
    - gets a list of all connected clients
    - ping: the server answers either with "ping ok" or "ping client_list_changed"
*/
class talk_to_client : public boost::enable_shared_from_this<talk_to_client>
{// , boost::format_no_copy
    typedef talk_to_client self_type;
    talk_to_client() : sock_(service), started_(false),
        timer_(service), clients_changed_(false) {
    }
public:
    typedef boost::system::error_code error_code;
    typedef boost::shared_ptr<talk_to_client> ptr;

    void start() {
        started_ = true;
        clients.push_back(shared_from_this());
        last_ping = boost::posix_time::microsec_clock::local_time();
        // first, we wait for client to login
        do_read();
    }
    static ptr new_() {
        ptr new_(new talk_to_client);
        return new_;
    }
    void stop() {
        if (!started_) return;
        started_ = false;
        sock_.close();

        ptr self = shared_from_this();
        array::iterator it = std::find(clients.begin(), clients.end(), self);
        clients.erase(it);
        update_clients_changed();
    }
    bool started() const { return started_; }
    ip::tcp::socket& sock() { return sock_; }
    std::string username() const { return username_; }
    void set_clients_changed() { clients_changed_ = true; }
private:
    void on_read(const error_code& err, size_t bytes) {
        if (err) stop();
        if (!started()) return;
        // process the msg
        std::string msg(read_buffer_, bytes);
        if (msg.find("login ") == 0) on_login(msg);
        else if (msg.find("ping") == 0) on_ping();
        else if (msg.find("ask_clients") == 0) on_clients();
        else std::cerr << "invalid msg " << msg << std::endl;
    }

    void on_login(const std::string& msg) {
        std::istringstream in(msg);
        in >> username_ >> username_;
        std::cout << username_ << " logged in" << std::endl;
        do_write("login ok\n");
        update_clients_changed();
    }
    void on_ping() {
        do_write(clients_changed_ ? "ping client_list_changed\n" : "ping ok\n");
        clients_changed_ = false;
    }
    void on_clients() {
        std::string msg;
        for (array::const_iterator b = clients.begin(), e = clients.end(); b != e; ++b)
            msg += (*b)->username() + " ";
        do_write("clients " + msg + "\n");
    }

    void do_ping() {
        do_write("ping\n");
    }
    void do_ask_clients() {
        do_write("ask_clients\n");
    }

    void on_check_ping() {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        if ((now - last_ping).total_milliseconds() > 5000) {
            std::cout << "stopping " << username_ << " - no ping in time" << std::endl;
            stop();
        }
        last_ping = boost::posix_time::microsec_clock::local_time();
    }
    void post_check_ping() {
        timer_.expires_from_now(boost::posix_time::millisec(5000));
        timer_.async_wait(MEM_FN(on_check_ping));
    }


    void on_write(const error_code& err, size_t bytes) {
        do_read();
    }
    void do_read() {
        async_read(sock_, buffer(read_buffer_),
            MEM_FN2(read_complete, _1, _2), MEM_FN2(on_read, _1, _2));
        post_check_ping();
    }
    void do_write(const std::string& msg) {
        if (!started()) return;
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some(buffer(write_buffer_, msg.size()),
            MEM_FN2(on_write, _1, _2));
    }
    size_t read_complete(const boost::system::error_code& err, size_t bytes) {
        if (err) return 0;
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
        // we read one-by-one until we get to enter, no buffering
        return found ? 0 : 1;
    }
private:
    ip::tcp::socket sock_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
    std::string username_;
    deadline_timer timer_;
    boost::posix_time::ptime last_ping;
    bool clients_changed_;
};

void update_clients_changed() {
    for (array::iterator b = clients.begin(), e = clients.end(); b != e; ++b)
        (*b)->set_clients_changed();
}

ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 8001));

void handle_accept(talk_to_client::ptr client, const boost::system::error_code& err) {
    client->start();
    talk_to_client::ptr new_client = talk_to_client::new_();
    acceptor.async_accept(new_client->sock(), boost::bind(handle_accept, new_client, _1));
}