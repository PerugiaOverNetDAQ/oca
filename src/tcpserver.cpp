/*!
  @file tcpServer.cpp
  @brief TCP server management, tx, and rx function definitions
  @author Niccolò De Paolis (niccolo.depaolis@studenti.unipg.it)
  @author Mattia Barbanera (mattia.barbanera@infn.it)
  @author Matteo Duranti (matteo.duranti@infn.it)
*/
#include "tcpServer.h"
#include "utility.h"

tcpServer::tcpServer(int port, int verb){
  kVerbosity            = verb;
  kListeningOn          = false;
  kTcpConn              = -1;
  kSockDesc             = -1;
  kPort                 = port;
  kBlocking             = false;
  
  //Reset kAddr to intended values
  memset(&kAddr, 0, sizeof(kAddr));
  kAddr.sin_family      = AF_INET;
  kAddr.sin_port        = htons(kPort);
  kAddr.sin_addr.s_addr = INADDR_ANY;
  
}

tcpServer::~tcpServer(){
  if (kVerbosity>0) {
    printf("%s) Destroying tcpServer\n", __METHOD_NAME__);
  }
  kListeningOn = false;
  close(kSockDesc);
  shutdown(kTcpConn, SHUT_RDWR);
  
  return;
}

void tcpServer::Setup(){
  int optval = 1;
  unsigned int buff_size = 16700000;
  socklen_t optLen = sizeof(optval);

  exit_if(kListeningOn == true, "%s) Socket already setup", __METHOD_NAME__);

  //Create a new socket
  printf("TCP/IP socket: Opening... ");
  kSockDesc = socket(AF_INET , SOCK_STREAM , 0);
  exit_if(kSockDesc < 0, "%s) Socket creation error", __METHOD_NAME__);

  //Set socket options
  //SO_REUSEADDR to avoid waiting for addresses already in use
  if (setsockopt(kSockDesc, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval,
                  optLen) == -1) {
    perror("setsockopt failed...\n");
    exit(1);
  }
  //Set SO_RCVBUF to maximum allowed
  buff_size = 16700000;
  if (setsockopt(kSockDesc, SOL_SOCKET, SO_RCVBUF, &buff_size,
                  optLen) == -1) {
    perror("setsockopt failed...\n");
    exit(1);
  }
  //Set SO_SNDBUF to maximum allowed
  buff_size = 16700000;
  if (setsockopt(kSockDesc, SOL_SOCKET, SO_SNDBUF, &buff_size,
                  optLen) == -1) {
    perror("setsockopt failed...\n");
    exit(1);
  }
  printf("ok\n");

  buff_size = 0;
  getsockopt(kSockDesc, SOL_SOCKET, SO_RCVBUF, &buff_size, &optLen);
  printf("%s) SO_RCVBUF: %u\n", __METHOD_NAME__, buff_size);

  buff_size = 0;
  getsockopt(kSockDesc, SOL_SOCKET, SO_SNDBUF, &buff_size, &optLen);
  printf("%s) SO_SNDBUF: %u\n", __METHOD_NAME__, buff_size);
  
  //Binding to port and address
  printf("TCP/IP socket: binding... ");
  int result = bind(kSockDesc, (struct sockaddr *) &kAddr,
                  sizeof(kAddr));
  exit_if(result < 0, "%s) Bind error", __METHOD_NAME__);
  printf("ok\n");
  fflush(stdout);

  //Listen for new connections
  printf("TCP/IP socket: listening... ");
  result = listen(kSockDesc, 100);
  exit_if(result < 0, "%s) Listening not possibile", __METHOD_NAME__);
  printf("ok\n");

  kListeningOn = true;

  return;
}

void tcpServer::AcceptConnection(){
  struct sockaddr_in clientAddr;
  socklen_t addrLen = sizeof(clientAddr);
  int flags, retflags;

  sockaddr_in peerAddr;
  socklen_t peerAddrLen = sizeof(peerAddr);

  //Close open connections, if any
  if (kTcpConn!=-1) {
    shutdown(kTcpConn, SHUT_RDWR);
    kTcpConn = -1;
  }

  if (!kBlocking) {
    //Save the current flags, if any
    flags = fcntl(kSockDesc ,F_GETFL, 0);
    exit_if(flags < 0, "%s) Fcntl failed:", __METHOD_NAME__);
    //Make the socket non-blocking
    retflags=fcntl(kSockDesc, F_SETFL, flags | O_NONBLOCK);
    exit_if(retflags < 0, "%s) Fcntl failed:", __METHOD_NAME__);
  }

  //Stop here until a connection exists
  while (kTcpConn==-1) {
    
    if (kVerbosity>1) {
      printf("%s) Waiting for connections...\n", __METHOD_NAME__);
    }
    
    //Await (blocking) or try (non-blocking) connections
    kTcpConn = accept(kSockDesc, (struct sockaddr *) &clientAddr, &addrLen);
    if (kVerbosity>1) {
      printf("%s) kTcpConn: %d, errno: %d (EAGAIN, EWOULDBLOCK: %d %d)\n",
                __METHOD_NAME__, kTcpConn, errno, EAGAIN, EWOULDBLOCK);
    }
    
    //Return if server is closing
    if (kVerbosity>2) {
      printf("%s) %d\n", __METHOD_NAME__, kListeningOn);
    }
    if (!kListeningOn){
      return;
    }
    
    //If known errors occur, retry; otherwise, exit
    if (errno == EAGAIN || errno == EWOULDBLOCK){
      usleep(100);
      continue;
    } else {
      exit_if(kTcpConn<0, "%s) Negotiation error:", __METHOD_NAME__);
    }
    
    //Setting up of the connections, not in a rush
    sleep(1);
  }
  
  //FIXME Return to blocking socket?
  if (!kBlocking) {
    //Update the connection flags to make it non-blocking
    flags=fcntl(kTcpConn ,F_GETFL, 0);
    exit_if(flags < 0, "%s) Fcntl failed:", __METHOD_NAME__);
    retflags=fcntl(kTcpConn, F_SETFL, flags | O_NONBLOCK);
    exit_if(retflags < 0, "%s) Fcntl failed:", __METHOD_NAME__);
  }
  
  //
  getpeername(kTcpConn, (sockaddr *) &peerAddr, &peerAddrLen);
  printf("%s) Connection succeded: (socket number %d, %s:%d)\n", 
            __METHOD_NAME__, kTcpConn, inet_ntoa(peerAddr.sin_addr),
            ntohs(peerAddr.sin_port));

  return;
}

void tcpServer::SockStart(){
  Setup();
  AcceptConnection();

  return;
}

void tcpServer::StopListening(){
  kListeningOn = false;
  return;
}

void tcpServer::StartListening(){
  kListeningOn = true;
  return;
}

int tcpServer::Tx(const void* msg, uint32_t len){
  int n;
  n = write(kTcpConn, msg, len);
  if(n < 0){
    fprintf(stderr, "Error in writing to the socket\n");
    std::error_code ec (errno, std::generic_category());
    std::cout << "Error: " << ec.value() << ", Message: " << ec.message() << '\n';
    //exit(1);
    return n;
  }
  if (kVerbosity > 3) printf("%s) Sent %d bytes\n", __METHOD_NAME__, n);
  return n;
}

int tcpServer::Rx(void* msg, uint32_t len){
  int n;
  n = read(kTcpConn, msg, len);
  if (n < 0) {
    fprintf(stderr, "Error in reading the socket\n");
    std::error_code ec (errno, std::generic_category());
    std::cout << "Error: " << ec.value() << ", Message: " << ec.message() << '\n';
  }
  return n;
}

int tcpServer::RxTimeout(void* msg, uint32_t len, int timeout){
  if (timeout <= 0) {
    return Rx(msg, len);
  }

  if (waitForReadEvent(timeout) == true) {
    return Rx(msg, len);
  } else {
    //return read(kTcpConn, msg, len);
    return -2;
  }
}

bool tcpServer::waitForReadEvent(int timeout){
  fd_set readSet;
  struct timeval waitTime;
  waitTime.tv_sec  = static_cast<int>(timeout/1000);
  waitTime.tv_usec = static_cast<int>((timeout%1000)*1000);

  //Initialize readSet and add the listening socket to the set readSet
  FD_ZERO(&readSet); //FIXME if kTcpConn already in the list, FD_SET returns no error
  FD_SET(kTcpConn, &readSet);
	
  //Wait the socket to be readable
  //pselect for signal capture; poll/ppoll are an upgrade
	int retSel = select(kTcpConn+1, &readSet, NULL, NULL, &waitTime);
	if(retSel > 0) {
	  return true;
	} else if(retSel < 0) {
	  printf("%s) Select returned negative value: %d\n", __METHOD_NAME__, retSel);
	} else {
    printf("%s) Timeout\n", __METHOD_NAME__);
	}
	
  return false;
}