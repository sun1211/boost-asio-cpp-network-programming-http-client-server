#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <fstream>
#include <atomic>
#include <thread>
#include <iostream>

using namespace boost;

//provides the implementation of the HTTP protocol
class Service
{
    //static constant table containing HTTP status codes and status messages.
    static const std::map<unsigned int, std::string> http_status_table;

public:
    //constructor accepts a single parameter - shared pointer pointing to an instance of a socket connected to a client
    Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock) : m_sock(sock),
                                                                  m_request(4096),  //stream buffer member is initialized with the value of 4096, maximum size of the buffer in bytes
                                                                  m_response_status_code(200), // Assume success.
                                                                  m_resource_size_bytes(0){};

    //initiates an asynchronous communication session with the client connected to the socket
    //pointer to which was passed to the Service class' constructor
    void start_handling()
    {
        asio::async_read_until(*m_sock.get(),
                               m_request,
                               "\r\n",
                               [this](
                                   const boost::system::error_code &ec,
                                   std::size_t bytes_transferred)
                               {
                                   on_request_line_received(ec, bytes_transferred);
                               });
    }

private:
    //perform receiving and processing of the request sent by the client, parse and execute the request, and send the response back.
    //method that processes the HTTP request line
    void on_request_line_received(
        const boost::system::error_code &ec,
        std::size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();

            if (ec == asio::error::not_found)
            {
                // No delimiter has been fonud in the
                // request message.

                m_response_status_code = 413;
                send_response();

                return;
            }
            else
            {
                // In case of any other error
                // close the socket and clean up.
                on_finish();
                return;
            }
        }

        // Parse the request line.
        std::string request_line;
        std::istream request_stream(&m_request);
        std::getline(request_stream, request_line, '\r');
        // Remove symbol '\n' from the buffer.
        request_stream.get();

        // Parse the request line.
        std::string request_method;
        std::istringstream request_line_stream(request_line);
        request_line_stream >> request_method;

        // We only support GET method.
        if (request_method.compare("GET") != 0)
        {
            // Unsupported method.
            m_response_status_code = 501;
            send_response();

            return;
        }

        request_line_stream >> m_requested_resource;

        std::string request_http_version;
        request_line_stream >> request_http_version;

        if (request_http_version.compare("HTTP/1.1") != 0)
        {
            // Unsupported HTTP version or bad request.
            m_response_status_code = 505;
            send_response();

            return;
        }

        // At this point the request line is successfully
        // received and parsed. Now read the request headers.
        asio::async_read_until(*m_sock.get(),
                               m_request,
                               "\r\n\r\n",
                               [this](
                                   const boost::system::error_code &ec,
                                   std::size_t bytes_transferred)
                               {
                                   on_headers_received(ec,
                                                       bytes_transferred);
                               });

        return;
    }

    //process and store the request headers block
    void on_headers_received(const boost::system::error_code &ec,
                             std::size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();

            if (ec == asio::error::not_found)
            {
                // No delimiter has been fonud in the
                // request message.

                m_response_status_code = 413;
                send_response();
                return;
            }
            else
            {
                // In case of any other error - close the
                // socket and clean up.
                on_finish();
                return;
            }
        }

        // Parse and store headers.
        std::istream request_stream(&m_request);
        std::string header_name, header_value;

        while (!request_stream.eof())
        {
            std::getline(request_stream, header_name, ':');
            if (!request_stream.eof())
            {
                std::getline(request_stream,
                             header_value,
                             '\r');

                // Remove symbol \n from the stream.
                request_stream.get();
                m_request_headers[header_name] =
                    header_value;
            }
        }

        // Now we have all we need to process the request.
        process_request();
        send_response();

        return;
    }

    //read the contents of the requested resource from the file system
    // and store it in the buffer, ready to be sent back to the client
    void process_request()
    {
        std::cout << "process request " << m_requested_resource << "\n";
        // Read file.
        std::string resource_file_path = std::string("../root/") + m_requested_resource;

        if (!boost::filesystem::exists(resource_file_path))
        {
            // Resource not found.
            m_response_status_code = 404;

            return;
        }

        std::ifstream resource_fstream(
            resource_file_path,
            std::ifstream::binary);

        if (!resource_fstream.is_open())
        {
            // Could not open file.
            // Something bad has happened.
            m_response_status_code = 500;

            return;
        }

        // Find out file size.
        resource_fstream.seekg(0, std::ifstream::end);
        m_resource_size_bytes =
            static_cast<std::size_t>(
                resource_fstream.tellg());

        m_resource_buffer.reset(
            new char[m_resource_size_bytes]);

        resource_fstream.seekg(std::ifstream::beg);
        resource_fstream.read(m_resource_buffer.get(),
                              m_resource_size_bytes);

        m_response_headers += std::string("content-length") +
                              ": " +
                              std::to_string(m_resource_size_bytes) +
                              "\r\n";
    }

    //composes a response message and send it to the client
    void send_response()
    {
        m_sock->shutdown(asio::ip::tcp::socket::shutdown_receive);

        auto status_line = http_status_table.at(m_response_status_code);

        m_response_status_line = std::string("HTTP/1.1 ") +
                                 status_line +
                                 "\r\n";

        m_response_headers += "\r\n";

        std::vector<asio::const_buffer> response_buffers;
        response_buffers.push_back(
            asio::buffer(m_response_status_line));

        if (m_response_headers.length() > 0)
        {
            response_buffers.push_back(
                asio::buffer(m_response_headers));
        }

        if (m_resource_size_bytes > 0)
        {
            response_buffers.push_back(
                asio::buffer(m_resource_buffer.get(),
                             m_resource_size_bytes));
        }

        // Initiate asynchronous write operation.
        asio::async_write(*m_sock.get(),
                          response_buffers,
                          [this](
                              const boost::system::error_code &ec,
                              std::size_t bytes_transferred)
                          {
                              on_response_sent(ec,
                                               bytes_transferred);
                          });
    }

    //When the response sending is complete,
    // we need to shut down the socket to let the client know that a full response has been sent and no more data will be sent by the server.
    void on_response_sent(const boost::system::error_code &ec,
                          std::size_t bytes_transferred)
    {
        if (ec.value() != 0)
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();
        }

        m_sock->shutdown(asio::ip::tcp::socket::shutdown_both);

        on_finish();
    }

    // Here we perform the cleanup.
    void on_finish()
    {
        delete this;
    }

private:
    //shared pointer to a TCP socket object connected to the client
    std::shared_ptr<boost::asio::ip::tcp::socket> m_sock;
    //buffer into which the request message is read
    boost::asio::streambuf m_request;
    //a map where request headers are put when the HTTP request headers block is parsed
    std::map<std::string, std::string> m_request_headers;
    //URI of the resource requested by the client
    std::string m_requested_resource;

    //a buffer where the contents of a requested resource is stored before being sent to the client as a part of the response message
    std::unique_ptr<char[]> m_resource_buffer;
    //HTTP response status code
    unsigned int m_response_status_code;
    //the size of the contents of the requested resource
    std::size_t m_resource_size_bytes;
    //string containing a properly formatted response headers block
    std::string m_response_headers;
    //response status line
    std::string m_response_status_line;
};

//definition of the class representing a service is to define the http_status_table static member declared
//before and fill it with data—HTTP status code and corresponding status messages:
const std::map<unsigned int, std::string>
    Service::http_status_table =
        {
            {200, "200 OK"},
            {404, "404 Not Found"},
            {413, "413 Request Entity Too Large"},
            {500, "500 Server Error"},
            {501, "501 Not Implemented"},
            {505, "505 HTTP Version Not Supported"}};

class Acceptor
{
public:
    Acceptor(asio::io_service &ios, unsigned short port_num) : m_ios(ios),
                                                               m_acceptor(m_ios,
                                                                          asio::ip::tcp::endpoint(
                                                                              asio::ip::address_v4::any(),
                                                                              port_num)),
                                                               m_isStopped(false)
    {
    }

    // Start accepting incoming connection requests.
    void Start()
    {
        m_acceptor.listen();
        InitAccept();
    }

    // Stop accepting incoming connection requests.
    void Stop()
    {
        m_isStopped.store(true);
    }

private:
    void InitAccept()
    {
        std::shared_ptr<asio::ip::tcp::socket>
            sock(new asio::ip::tcp::socket(m_ios));

        m_acceptor.async_accept(*sock.get(),
                                [this, sock](
                                    const boost::system::error_code &error)
                                {
                                    onAccept(error, sock);
                                });
    }

    void onAccept(const boost::system::error_code &ec,
                  std::shared_ptr<asio::ip::tcp::socket> sock)
    {
        if (ec.value() == 0)
        {
            //After an instance of the Service class has been constructed, its start_handling() method is called by the Acceptor class
            (new Service(sock))->start_handling();
        }
        else
        {
            std::cout << "Error occured! Error code = "
                      << ec.value()
                      << ". Message: " << ec.message();
        }

        // Init next async accept operation if
        // acceptor has not been stopped yet.
        if (!m_isStopped.load())
        {
            InitAccept();
        }
        else
        {
            // Stop accepting incoming connections
            // and free allocated resources.
            m_acceptor.close();
        }
    }

private:
    asio::io_service &m_ios;
    asio::ip::tcp::acceptor m_acceptor;
    std::atomic<bool> m_isStopped;
};

class Server
{
public:
    Server()
    {
        m_work.reset(new asio::io_service::work(m_ios));
    }

    // Start the server.
    void Start(unsigned short port_num,
               unsigned int thread_pool_size)
    {

        assert(thread_pool_size > 0);

        // Create and strat Acceptor.
        acc.reset(new Acceptor(m_ios, port_num));
        acc->Start();

        // Create specified number of threads and
        // add them to the pool.
        for (unsigned int i = 0; i < thread_pool_size; i++)
        {
            std::unique_ptr<std::thread> th(
                new std::thread([this]()
                                { m_ios.run(); }));

            m_thread_pool.push_back(std::move(th));
        }
    }

    // Stop the server.
    void Stop()
    {
        acc->Stop();
        m_ios.stop();

        for (auto &th : m_thread_pool)
        {
            th->join();
        }
    }

private:
    asio::io_service m_ios;
    std::unique_ptr<asio::io_service::work> m_work;
    std::unique_ptr<Acceptor> acc;
    std::vector<std::unique_ptr<std::thread>> m_thread_pool;
};

const unsigned int DEFAULT_THREAD_POOL_SIZE = 2;

int main()
{
    unsigned short port_num = 3333;

    try
    {
        Server srv;

        unsigned int thread_pool_size =
            std::thread::hardware_concurrency() * 2;

        if (thread_pool_size == 0)
            thread_pool_size = DEFAULT_THREAD_POOL_SIZE;

        //client sends a TCP connection request and this request is accepted on the server
        //an instance of the Service class is created
        // and its constructor is passed a shared pointer pointing to the TCP socket object, connected to that client
        srv.Start(port_num, thread_pool_size);

        std::this_thread::sleep_for(std::chrono::seconds(60));

        srv.Stop();
    }
    catch (system::system_error &e)
    {
        std::cout << "Error occured! Error code = "
                  << e.code() << ". Message: "
                  << e.what();
    }

    return 0;
}