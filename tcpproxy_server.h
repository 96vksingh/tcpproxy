// Copyright (c) 2007 Arash Partow (http://www.partow.net)
// URL: http://www.partow.net/programming/tcpproxy/index.html
//
// Distributed under the Boost Software License, Version 1.0.
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>


namespace tcp_proxy
{
   namespace ip = boost::asio::ip;
   typedef ip::tcp::socket socket_type;

   class bridge;
   class client_splice;
   class server_splice;

   class client_splice : public boost::enable_shared_from_this<client_splice>
   {
   public :
       client_splice(boost::asio::io_service& ios);
       void set_bridge(boost::weak_ptr<bridge> wbp);
       void set_ss(boost::weak_ptr<server_splice> wssp);
       void handle_upstream_connect(const boost::system::error_code& error);
       void handle_upstream_write(const boost::system::error_code& error);
       void handle_upstream_read(const boost::system::error_code& error,
                                 const size_t& bytes_transferred);
       void close();
       socket_type upstream_socket_;
       boost::weak_ptr<bridge> wbp_;
       boost::weak_ptr<server_splice> wssp_;
       enum { max_data_length = 8192 }; //8KB
       unsigned char upstream_data_[max_data_length];
       boost::mutex mutex_;
   };

   class server_splice : public boost::enable_shared_from_this<server_splice>
   {
   public:
       server_splice(boost::asio::io_service& ios);
       void set_bridge(boost::weak_ptr<bridge> wbp);
       void set_cs(boost::weak_ptr<client_splice> wcsp);
       void handle_downstream_write(const boost::system::error_code& error);
       void handle_downstream_read(const boost::system::error_code& error,
                                   const size_t& bytes_transferred);
       void close();
       socket_type downstream_socket_;
       boost::weak_ptr<bridge> wbp_;
       boost::weak_ptr<client_splice> wcsp_;
       enum { max_data_length = 8192 }; //8KB
       unsigned char downstream_data_[max_data_length];
       boost::mutex mutex_;
   };

   class bridge
   {
   public:
       bridge(boost::asio::io_service& ios);
       void init(boost::shared_ptr<bridge> bp);
       void start(const std::string& upstream_host, unsigned short upstream_port);
       void close();
       boost::shared_ptr<client_splice> csplice_ptr_;
       boost::shared_ptr<server_splice> ssplice_ptr_;
       enum { max_data_length = 8192 }; //8KB
       unsigned char downstream_data_[max_data_length];
       unsigned char upstream_data_[max_data_length];
       boost::mutex mutex_;
   };

   class acceptor
   {
   public:
       typedef boost::shared_ptr<bridge> ptr_type;
      acceptor(boost::asio::io_service& io_service,
               const std::string& local_host, unsigned short local_port,
               const std::string& upstream_host, unsigned short upstream_port);
      bool accept_connections();

   private:
      void handle_accept(const boost::system::error_code& error);
      boost::asio::io_service& io_service_;
      ip::address_v4 localhost_address;
      ip::tcp::acceptor acceptor_;
      boost::weak_ptr<bridge> session_;
      unsigned short upstream_port_;
      std::string upstream_host_;
   };
   uint64_t num_server_connections_;
   uint64_t num_client_connections_;
   std::vector<boost::shared_ptr<tcp_proxy::bridge> > bridge_instances;
}
