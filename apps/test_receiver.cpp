/*
 * test_receiver.cpp
 *
 *  Created on: Aug 5, 2024
 *      Author: Nuno Barros
 */


#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>

volatile std::atomic<bool> running_flag;

template<typename T>
bool read(boost::asio::ip::tcp::socket & socket, T &obj)
{

  boost::system::error_code receiving_error;
  socket.read_some(boost::asio::buffer( &obj, sizeof(T) ),receiving_error);
  //  boost::asio::read_some( socket,
  //                     boost::asio::buffer( &obj, sizeof(T) ),
  //                     receiving_error ) ;

  if ( ! receiving_error )
  {
    return true ;
  }

  if ( receiving_error == boost::asio::error::eof)
  {
    std::string error_message = "Socket closed: " + receiving_error.message();
    std::cerr << error_message << std::endl;
    return false ;
  }

  if ( receiving_error )
  {
    std::string error_message = "Read failure: " + receiving_error.message();
    std::cerr << error_message << std::endl;
    return false ;
  }

  return true ;
}

void do_work()
{
  boost::system::error_code ec;
  unsigned short port = 8991;
  boost::asio::io_service m_receiver_ios;
  boost::asio::ip::tcp::endpoint m_endpoint;
  boost::asio::ip::tcp::socket m_receiver_socket(m_receiver_ios);

  boost::asio::io_service io_service;
  boost::asio::ip::tcp::endpoint ep( boost::asio::ip::tcp::v4(),port );
  boost::asio::ip::tcp::acceptor acceptor(io_service,ep);
  acceptor.listen(boost::asio::ip::tcp::socket::max_connections, ec);
  if (ec)
  {
   std::cout  << ": Error listening on socket: :" << port << " -- reason: '" << ec << '\'' << std::endl;
   return;
  }
  else
  {
    std::cout << ": Waiting for an incoming connection on port " << port << std::endl;
  }

  std::future<void> accepting = async( std::launch::async, [&]{ acceptor.accept(m_receiver_socket,ec) ; } ) ;
  if (ec)
  {
    std::stringstream msg;
    msg << "Socket opening failed:: " << ec.message();
    std::cerr << "Failed : " << msg.str() << std::endl;
    return;
  }
  while ( running_flag.load() )
  {
    if ( accepting.wait_for( std::chrono::milliseconds(100)) == std::future_status::ready )
    {
      break ;
    }
    else
    {
      std::cout << "Waiting..." << std::endl;
    }
  }
  std::cout << "Received...start reading" << std::endl;
  char data[1024];
  while (running_flag.load())
  {
    if (!read(m_receiver_socket,data))
    {
      std::cout << "Lost connection" << std::endl;
    }

    // read something.... print it
    std::cout << "Read [" << data << "]" << std::endl;

  }
  boost::system::error_code closing_error;

//  if ( m_error_state.load() )
//  {
//
//    m_receiver_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, closing_error);
//
//    if ( closing_error )
//    {
//      std::stringstream msg;
//      msg << "Error in shutdown " << closing_error.message();
//      ers::error(CIBCommunicationError(ERS_HERE,msg.str())) ;
//    }
//  }

  m_receiver_socket.close(closing_error) ;

  if ( closing_error )
  {
    std::stringstream msg;
    msg << "Socket closing failed:: " << closing_error.message();
    std::cerr << msg.str() << std::endl;
  }

}

int main(int argc, char** argv)
{
  running_flag.store(true);
  std::thread t1(do_work);
  std::this_thread::sleep_for(std::chrono::seconds(30));
  running_flag.store(false);
  t1.join();
  std::cout << "All done. Returning" << std::endl;
  return 0;
}

