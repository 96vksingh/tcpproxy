/*
 * Description
 * ~~~~~~~~~~~
 * The objective of the TCP proxy server is to act as an intermediary
 * in order to 'forward' TCP based connections from external clients
 * onto a singular remote server.
 * The communication flow in the direction from the client to the proxy
 * to the server is called the upstream flow, and the communication flow
 * in the direction from the server to the proxy to the client is called
 * the downstream flow. Furthermore the up and down stream connections
 * are consolidated into a single concept known as a bridge.
 * In the event either the downstream or upstream end points disconnect,
 * the proxy server will proceed to disconnect the other end point
 * and eventually destroy the associated bridge.
 * The following is a flow and structural diagram depicting the
 * various elements (proxy, server and client) and how they connect
 * and interact with each other.
                                   ---> upstream --->           +---------------+
                                                    +---->------>               |
                              +-----------+         |           | Remote Server |
                    +--------->          [x]--->----+  +---<---[x]              |
                    |         | TCP Proxy |            |        +---------------+
+-----------+       |  +--<--[x] Server   <-----<------+
|          [x]--->--+  |      +-----------+
|  Client   |          |
|           <-----<----+
+-----------+
               <--- downstream <---
*/

#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include "tcpproxy.h"

namespace ip = boost::asio::ip;
typedef ip::tcp::socket socket_type;
using namespace tcp_proxy;

//std::vector<boost::shared_ptr<tcp_proxy::bridge> > tcp_proxy::bridge_instances = std::vector<boost::shared_ptr<tcp_proxy::bridge> >();
// uint64_t tcp_proxy::num_client_connections_ = 0;
// uint64_t tcp_proxy::num_server_connections_ = 0;
bool debug = false;
tcp_proxy::bridge::bridge(boost::asio::io_service& ios)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   csplice_ptr_ = boost::shared_ptr<client_splice>(new client_splice(ios));
   ssplice_ptr_ = boost::shared_ptr<server_splice>(new server_splice(ios));
    std::cout << "inited client and server splice " << __FUNCTION__ << std::endl;
}

void tcp_proxy::bridge::init(boost::shared_ptr<bridge> bp)
{
   boost::weak_ptr<bridge> wbp = bp;
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   csplice_ptr_->set_bridge(wbp);
   ssplice_ptr_->set_bridge(wbp);
}

void tcp_proxy::bridge::start(const std::string& upstream_host, unsigned short upstream_port)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   csplice_ptr_->upstream_socket_.async_connect(
      ip::tcp::endpoint(
         boost::asio::ip::address::from_string(upstream_host),
         upstream_port),
      boost::bind(&client_splice::handle_upstream_connect,
                  csplice_ptr_->shared_from_this(),
                  boost::asio::placeholders::error));
}

void tcp_proxy::bridge::close()
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   boost::mutex::scoped_lock lock(mutex_);
   ssplice_ptr_->close();
   csplice_ptr_->close();

   /* Iterate ande delete the current bridge instance from the bridge_isntance list */
   for(auto it = bridge_instances.begin();
       it != bridge_instances.end(); ++it)
   {
      if((*it).get() == this) {
         bridge_instances.erase(it);
      }
   }
}

tcp_proxy::client_splice::client_splice(boost::asio::io_service& ios)
   :upstream_socket_(ios)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
}

void tcp_proxy::client_splice::set_bridge(boost::weak_ptr<bridge> wbp)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   wbp_ = wbp;
}

void tcp_proxy::client_splice::set_ss(boost::weak_ptr<server_splice> wssp)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   wssp_ = wssp;
}


void tcp_proxy::client_splice::handle_upstream_connect(const boost::system::error_code& error)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   num_client_connections_++;
   std::cout << "Client Conn. " << num_client_connections_ << std::endl;
   boost::shared_ptr<bridge> bp = wbp_.lock();
   if (!error)
   {
      boost::weak_ptr<client_splice> wcsp = bp->csplice_ptr_;
      boost::weak_ptr<server_splice> wssp = bp->ssplice_ptr_;
      bp->csplice_ptr_->set_ss(wssp);
      bp->ssplice_ptr_->set_cs(wcsp);

      upstream_socket_.async_read_some(
         boost::asio::buffer(upstream_data_,max_data_length),
         boost::bind(&client_splice::handle_upstream_read,
                     shared_from_this(),
                     boost::asio::placeholders::error,
                     boost::asio::placeholders::bytes_transferred));

      bp->ssplice_ptr_->downstream_socket_.async_read_some(
         boost::asio::buffer(bp->ssplice_ptr_->downstream_data_,max_data_length),
         boost::bind(&server_splice::handle_downstream_read,
                     bp->ssplice_ptr_->shared_from_this(),
                     boost::asio::placeholders::error,
                     boost::asio::placeholders::bytes_transferred));
   } else {
      std::cerr << "Exception:" << error.message() << std::endl;
      if(bp) {
         bp->close();
      } else {
         close();
      }
   }
}
void tcp_proxy::client_splice::handle_upstream_write(const boost::system::error_code& error)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   if (!error)
   {
      boost::shared_ptr<server_splice> ssp = wssp_.lock();

      ssp->downstream_socket_.async_read_some(
         boost::asio::buffer(ssp->downstream_data_,max_data_length),
         boost::bind(&server_splice::handle_downstream_read,
                     ssp->shared_from_this(),
                     boost::asio::placeholders::error,
                     boost::asio::placeholders::bytes_transferred));
   } else {
      boost::shared_ptr<bridge> bp = wbp_.lock();
      std::cerr << "Exception:" << error.message() << std::endl;
      if(bp) {
         bp->close();
      } else {
         close();
      }
   }
}

void tcp_proxy::client_splice::handle_upstream_read(const boost::system::error_code& error,
                                                    const size_t& bytes_transferred)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   if (!error)
   {
      boost::shared_ptr<server_splice> ssp = wssp_.lock();

      async_write(ssp->downstream_socket_,
                  boost::asio::buffer(upstream_data_,bytes_transferred),
                  boost::bind(&server_splice::handle_downstream_write,
                              ssp->shared_from_this(),
                              boost::asio::placeholders::error));
   } else {
      boost::shared_ptr<bridge> bp = wbp_.lock();
      std::cerr << "Exception:" << error.message() << std::endl;
      if(bp) {
         bp->close();
      } else {
         close();
      }
   }
}

void tcp_proxy::client_splice::close()
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   boost::mutex::scoped_lock lock(mutex_);
   if (upstream_socket_.is_open())
   {
      upstream_socket_.close();
   }
}

tcp_proxy::server_splice::server_splice(boost::asio::io_service& ios)
   :downstream_socket_(ios)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
}

void tcp_proxy::server_splice::set_bridge(boost::weak_ptr<bridge> wbp)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   wbp_ = wbp;
}

void tcp_proxy::server_splice::set_cs(boost::weak_ptr<client_splice> wcsp)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   wcsp_ = wcsp;
}

void tcp_proxy::server_splice::handle_downstream_write(const boost::system::error_code& error)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   if (!error)
   {
      boost::shared_ptr<client_splice> csp = wcsp_.lock();

      csp->upstream_socket_.async_read_some(
         boost::asio::buffer(csp->upstream_data_,max_data_length),
         boost::bind(&client_splice::handle_upstream_read,
                     csp->shared_from_this(),
                     boost::asio::placeholders::error,
                     boost::asio::placeholders::bytes_transferred));
   } else {
      boost::shared_ptr<bridge> bp = wbp_.lock();
      std::cerr << "Exception:" << error.message() << std::endl;
      if(bp) {
         bp->close();
      } else {
         close();
      }
   }
}

void tcp_proxy::server_splice::handle_downstream_read(const boost::system::error_code& error,
                                                      const size_t& bytes_transferred)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   if (!error)
   {
      boost::shared_ptr<client_splice> csp = wcsp_.lock();

      async_write(csp->upstream_socket_,
                  boost::asio::buffer(downstream_data_,bytes_transferred),
                  boost::bind(&client_splice::handle_upstream_write,
                              csp->shared_from_this(),
                              boost::asio::placeholders::error));
   } else {
      boost::shared_ptr<bridge> bp = wbp_.lock();
      std::cerr << "Exception:" << error.message() << std::endl;
      if(bp) {
         bp->close();
      } else {
         close();
      }
   }
}

void tcp_proxy::server_splice::close()
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;

   boost::mutex::scoped_lock lock(mutex_);

   if (downstream_socket_.is_open())
   {
      downstream_socket_.close();
   }
}

typedef boost::shared_ptr<bridge> ptr_type;
tcp_proxy::acceptor::acceptor(boost::asio::io_service& io_service,
                              const std::string& local_host, unsigned short local_port,
                              const std::string& upstream_host, unsigned short upstream_port)
   : io_service_(io_service),
     localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
     acceptor_(io_service_,ip::tcp::endpoint(localhost_address,local_port)),
     upstream_port_(upstream_port),
     upstream_host_(upstream_host)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
}

bool tcp_proxy::acceptor::accept_connections()
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;

   try
   {
      boost::shared_ptr<bridge> bp = boost::shared_ptr<bridge>(new bridge(io_service_));
      bp->init(bp);
      session_ = bp;
      tcp_proxy::bridge_instances.push_back(bp);
      std::cout << "Waiting to accept connections" << std::endl;
      acceptor_.async_accept(bp->ssplice_ptr_->downstream_socket_,
                             boost::bind(&acceptor::handle_accept,
                                         this,
                                         boost::asio::placeholders::error));
      std::cout << __FUNCTION__ << "Asynchronously accepted connections" << std::endl;
   }
   catch(std::exception& e)
   {
      std::cerr << "acceptor exception: " << e.what() << std::endl;
      return false;
   }

   return true;
}

void tcp_proxy::acceptor::handle_accept(const boost::system::error_code& error)
{
   if(debug)
      std::cout << "In " << __FUNCTION__ << std::endl;
   if (!error)
   {
      boost::shared_ptr<bridge> bp = session_.lock();
      num_server_connections_++;
      std::cout << "Server Conn. " << num_server_connections_ << std::endl;
      bp->start(upstream_host_, upstream_port_);

      if (!accept_connections())
      {
         std::cerr << "Failure during call to accept." << std::endl;
      }
   }
   else
   {
      std::cerr << "Error: " << error.message() << std::endl;
   }
}

int main(int argc, char* argv[])
{
   if (argc != 6)
   {
      std::cerr << "usage: tcpproxy_server <local host ip> <local port> <forward host ip> <forward port> <debug 0(false)/1(true)>" << std::endl;
      return 1;
   }

   const unsigned short local_port   = static_cast<unsigned short>(::atoi(argv[2]));
   const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[4]));
   const std::string local_host      = argv[1];
   const std::string forward_host    = argv[3];
   debug = boost::lexical_cast<bool>(argv[5]);

   boost::asio::io_service ios;

   try
   {
      tcp_proxy::acceptor acceptor(ios,
                                   local_host, local_port,
                                   forward_host, forward_port);
      std::cout << "Created acceptor object" << std::endl;
      acceptor.accept_connections();
      std::cout << "Acceptor- accepted connections" << std::endl;
      ios.run();
   }
   catch(std::exception& e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}
