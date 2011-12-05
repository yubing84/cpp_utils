// gpp sslclient_main.cpp -o sslclient -I../include ../src/Logger.cpp ../src/Thread.cpp ../src/Socket.cpp -lpthread ../src/SocketClient.cpp  ../src/Poll.cpp ../src/Connection.cpp ../src/SslConnection.cpp -lssl -lcrypto ../src/TcpConnection.cpp ../src/Addrinfo.cpp


#include "Logger.hpp"

#include "Message.hpp"
#include "SslConnection.hpp"
#include "SocketClient.hpp"


#include <iostream>
#include <string>

#include <time.h> // nanosleep
#include <Common.hpp>




class SimpleMessage : public Message
{
public:

  SimpleMessage( void *msgParam = 0)
    : Message(msgParam)
  {
    TRACE;
  }

  bool buildMessage( const void   *msgPart,
                     const size_t  msgLen )
  {
    TRACE;
    m_buffer = std::string( (const char*) msgPart, msgLen );
    onMessageReady();
    return true;
  }

  void onMessageReady()
  {
    TRACE;

    LOG( Logger::INFO, std::string("Got reply from server: ").
                        append(m_buffer).c_str() );

    *( static_cast<bool*>(m_param) ) = true;
  }

  Message* clone()
  {
    TRACE;
    return new SimpleMessage(m_param);
  }

protected:

  size_t getExpectedLength()
  {
    TRACE;
    return 0;
  }
};


int main(int argc, char* argv[] )
{
  if ( argc != 4 ) {
    std::cerr << "Usage: " << argv[0] <<  " <HOST> <PORT> <MSG>" << std::endl;
    return 1;
  }

  Logger::createInstance();
  Logger::init(std::cout);
  Logger::setLogLevel(Logger::FINEST);
  SslConnection::init();

  bool finished = false;

  SimpleMessage msg(&finished);
  SslConnection conn(argv[1], argv[2], &msg);
  conn.initClientContext();
  SocketClient socketClient(&conn);

  if ( !socketClient.connect() ) {
    LOG_STATIC( Logger::ERR, "Couldn't connect to server, exiting..." );
    SslConnection::destroy();
    Logger::destroy();
    return 1;
  }

  // wait for thread creation
  sleep(1);

  // send message to server
  std::string msg1(argv[3]);
  if ( !socketClient.send( msg1.c_str(), msg1.length()) ) {
    LOG_STATIC( Logger::ERR, "Couldn't send message to server, exiting..." );
    SslConnection::destroy();
    Logger::destroy();
    return 1;
  }

  // wait for the complate &handled reply
  struct timespec tm = {0,1000};
  while ( !finished && socketClient.isPolling() )
    nanosleep(&tm, &tm) ;

//   socketClient.disconnect();
  SslConnection::destroy();
  Logger::destroy();
  return 0;
}