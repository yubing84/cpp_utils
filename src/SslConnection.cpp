#include "SslConnection.hpp"

#include "Logger.hpp"
#include "Common.hpp"

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>



void SslConnection::init()
{
  TRACE_STATIC;

  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
}

void SslConnection::destroy()
{
  TRACE_STATIC;

  ERR_free_strings();
}


SslConnection::SslConnection (  const int      socket,
                                Message       *message,
                                const size_t   bufferLength )
  : StreamConnection()
  , m_tcpConnection(socket, message, 0)
  , m_message(message)
  , m_buffer(0)
  , m_bufferLength(bufferLength)
  , m_sslHandle(0)
  , m_sslContext(0)
{
  TRACE;

  setHost(m_tcpConnection.getHost());
  setPort(m_tcpConnection.getPort());

  m_buffer = new unsigned char[m_bufferLength];
  m_message->setConnection(this);
}


SslConnection::SslConnection (  const std::string   host,
                                const int           port,
                                Message            *message,
                                const size_t        bufferLength )
  : StreamConnection(host, port)
  , m_tcpConnection(host, port, message, 0)
  , m_message(message)
  , m_buffer(0)
  , m_bufferLength(bufferLength)
  , m_sslHandle(0)
  , m_sslContext(0)
{
  TRACE;
  m_buffer = new unsigned char[m_bufferLength];
  m_message->setConnection(this);
}


SslConnection::~SslConnection()
{
  TRACE;
  disconnect();
  delete m_buffer;
}


Connection* SslConnection::clone(const int socket)
{
  TRACE;

  SslConnection *conn = new SslConnection( socket, m_message->clone(), m_bufferLength );
  conn->initClientContext();
  return conn;
}


bool SslConnection::connect()
{
  TRACE;

  if ( !m_tcpConnection.connect() )
    return false;

//   if ( !initHandlers() )
//     return false;

  if ( SSL_set_fd(m_sslHandle, m_tcpConnection.getSocket() ) == 0 ) {
    getSslError("SSL set connection socket failed. ");
    return -1;
  }

  LOG( Logger::INFO, "itt" );
  if ( SSL_connect (m_sslHandle) != 1 ) {
    LOG (Logger::ERR, getSslError("SSL handshake failed. ").c_str() );
    return false;
  }
  LOG( Logger::INFO, "de itt mar nem?" );

  return true;
}


bool SslConnection::bind()
{
  TRACE;

  if ( !m_tcpConnection.bind() )
    return false;

//   if ( !initHandlers() )
//     return false;


  return true;
}


bool SslConnection::listen( const int maxPendingQueueLen )
{
  TRACE;
  return m_tcpConnection.listen(maxPendingQueueLen);
}


int SslConnection::accept()
{
  TRACE;

  int client_socket = m_tcpConnection.accept();
  if ( client_socket == -1)
    return client_socket;

  LOG( Logger::INFO, "server itt");

  if ( SSL_set_fd(m_sslHandle, client_socket) == 0 ) {
    getSslError("SSL set connection socket failed. ");
    return -1;
  }

  LOG( Logger::INFO, "server itt 2");

  if ( SSL_accept(m_sslHandle) == -1 ) {
    getSslError("SSL accept failed. ");
    return -1;
  }

  LOG( Logger::INFO, "server itt 3");

  return client_socket;
}

/// @todo this function shall be refactored
bool SslConnection::disconnect()
{
  TRACE;

  /// @note do I have to call this?
  if ( m_tcpConnection.getSocket() != -1 )
    m_tcpConnection.disconnect();

  if ( m_sslHandle == 0 || m_sslContext == 0 )
    return false;

  int ret = SSL_shutdown(m_sslHandle);
  if ( ret == 0 ) {
    LOG( Logger::INFO, "\"close notify\" alert was sent and the peer's "
                       "\"close notify\" alert was received.");
  }
  else if (ret == 1 ) {
    LOG( Logger::WARNING, "The shutdown is not yet finished. "
                          "Calling SSL_shutdown() for a second time...");
    SSL_shutdown(m_sslHandle);
  }
  else if ( ret == 2 ) {
    LOG (Logger::ERR, getSslError("The shutdown was not successful. ").c_str() );
  }

  /// @note I have to check the ref count?! This stinks
  if (m_sslHandle && m_sslHandle->references > 0)
    SSL_free(m_sslHandle);

  if (m_sslHandle && m_sslContext->references > 0)
    SSL_CTX_free(m_sslContext);

  m_sslHandle = 0;
  m_sslContext = 0;

  return true;
}


bool SslConnection::initServerContext( const std::string certificateFile,
                                       const std::string privateKeyFile )
{
  TRACE;

  m_sslContext = SSL_CTX_new (SSLv2_server_method ());
  if ( m_sslContext == NULL ) {
    LOG (Logger::ERR, getSslError("Creating SSL context failed. ").c_str() );
    return false;
  }

  if ( !loadCertificates(certificateFile, privateKeyFile) )
    return false;

  return initHandle();
}


bool SslConnection::initClientContext()
{
  TRACE;

  m_sslContext = SSL_CTX_new (SSLv23_client_method ());
  if ( m_sslContext == NULL ) {
    LOG (Logger::ERR, getSslError("Creating SSL context failed. ").c_str() );
    return false;
  }

  return initHandle();
}


bool SslConnection::send( const void* message, const size_t length )
{
  TRACE;

  int ret = SSL_write(m_sslHandle, message, length);

  if ( ret > 0 )
    return true;

  unsigned long sslErrNo = ERR_peek_error();
  if ( ret == 0 && sslErrNo == SSL_ERROR_ZERO_RETURN ) {
    LOG( Logger::INFO, "Underlying connection has been closed.");
    return true;
  }

  LOG (Logger::ERR, getSslError("SSL write failed. ").c_str() );
  return false;
}


bool SslConnection::receive()
{
  TRACE;

  int length = SSL_read(m_sslHandle, m_buffer, m_bufferLength);

  if ( length > 0 )
    return m_message->buildMessage( (void*)m_buffer, (size_t)length);

  unsigned long sslErrNo = ERR_peek_error();
  if ( length == 0 && sslErrNo == SSL_ERROR_ZERO_RETURN ) {
    LOG( Logger::INFO, "Underlying connection has been closed.");
    return true;
  }

  LOG (Logger::ERR, getSslError("SSL read failed. ").c_str() );
  return false;
}


int SslConnection::getSocket() const
{
  TRACE;
  return m_tcpConnection.getSocket();
}


bool SslConnection::initHandle()
{
  TRACE;

  m_sslHandle = SSL_new (m_sslContext);
  if ( m_sslHandle == NULL ) {
    LOG (Logger::ERR, getSslError("Creating SSL structure for connection failed. ").c_str() );
    return false;
  }


  if ( !SSL_set_fd (m_sslHandle, m_tcpConnection.getSocket()) ) {
    LOG (Logger::ERR, getSslError("Connect the SSL object with a file descriptor failed. ").c_str() );
    return false;
  }

  return true;
}


std::string SslConnection::getSslError(const std::string &msg)
{
  char buffer[130];
  unsigned long sslErrNo = ERR_get_error();

  ERR_error_string(sslErrNo, buffer);

  return std::string(msg).append(buffer);
}


bool SslConnection::loadCertificates( const std::string certificateFile,
                                      const std::string privateKeyFile )
{
  if ( SSL_CTX_use_certificate_file(m_sslContext, certificateFile.c_str(), SSL_FILETYPE_PEM) != 1 )
  {
    getSslError("SSL certificate file loading failed. ");
    return false;
  }

  if ( SSL_CTX_use_PrivateKey_file(m_sslContext, privateKeyFile.c_str(), SSL_FILETYPE_PEM) != 1 )
  {
    getSslError("SSL private Key file loading failed. ");
    return false;
  }

  if ( SSL_CTX_check_private_key(m_sslContext) != 1 )
  {
    LOG( Logger::ERR, "Private key does not match the public certificate\n");
    return false;
  }

  return true;
}

/*---------------------------------------------------------------------*/
/*--- ShowCerts - print out certificates.                           ---*/
/*---------------------------------------------------------------------*/
// void showCertificates(SSL* ssl)
// {   X509 *cert;
//     char *line;
//
//     cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */
//     if ( cert != NULL )
//     {
//         printf("Server certificates:\n");
//         line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
//         printf("Subject: %s\n", line);
//         free(line);
//         line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
//         printf("Issuer: %s\n", line);
//         free(line);
//         X509_free(cert);
//     }
//     else
//         printf("No certificates.\n");
// }
