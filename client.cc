#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sstream>
#include <signal.h>
#include <openssl/ssl.h>
#include <string>
#include <openssl/err.h>
using namespace std;
const int LEN=256*256;
termios term,oldterm;
void window_size(int);
int fd;
SSL *ssl;
int main(int argc, char ** argv) try {
  tcgetattr(0,&oldterm);
  if (argc<4) throw string(argv[0])+" CA.pem cert.pem key.pem";
  signal(SIGWINCH,window_size);
  char buffer[LEN];
  int fd1=socket(AF_INET,SOCK_STREAM,0);
  sockaddr_in addr;
  addr.sin_family=AF_INET;
  addr.sin_port=htons(2345);
  addr.sin_addr.s_addr=inet_addr("127.0.0.1");
  socklen_t addr_len=sizeof(addr);
  if (bind(fd1,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0) {
    perror("bind");
    throw "bind";
    }
//
  SSL_CTX *ctx=SSL_CTX_new(TLS_server_method());
  if (!ctx) throw "NULL CTX returned";
  if (!SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION)) throw "SSL_CTX_set_min_proto_version";
  if (SSL_CTX_load_verify_locations(ctx,argv[1],NULL)!=1) throw "SSL_CTX_load_verify_locations";
  if (SSL_CTX_use_certificate_file(ctx,argv[2],SSL_FILETYPE_PEM)!=1) throw "SSL_CTX_use_certificate_file";
  if (SSL_CTX_use_PrivateKey_file(ctx,argv[3],SSL_FILETYPE_PEM)!=1) throw "SSL_CTX_use_PrivateKey_file";
  SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,NULL);
  SSL_CTX_set_verify_depth(ctx,0);
  ssl=SSL_new(ctx);
  if (ssl==NULL) throw "SSL_new";
//
  listen(fd1,1);
  fd=accept(fd1,reinterpret_cast<sockaddr*>(&addr),&addr_len);
  if (fd<0) {
    perror("accept");
    throw "accept";
    }
  if (SSL_set_fd(ssl,fd)!=1) throw "SSL_set_fd";
  if (SSL_accept(ssl)!=1) {
    ERR_print_errors_fp(stdout);
    throw "SSL_accept";
    }
  pollfd fds[2];
  fds[0].fd=0;
  fds[0].events=POLLIN|POLLHUP|POLLERR;
  fds[1].fd=fd;
  fds[1].events=POLLIN|POLLHUP|POLLERR;
  int rec=0;
  cfmakeraw(&term);
  tcsetattr(0,TCSANOW,&term);
  winsize wins={0,0};
  ioctl(0,TIOCGWINSZ,&wins);
  ostringstream xx;
  xx << "\r~size " << wins.ws_col << " " << wins.ws_row << "~" << endl;
  SSL_write(ssl,xx.str().c_str(),xx.str().size());
  for (;;) {
    if ((rec=poll(fds,2,-1))<0) {
      if (errno==EINTR) continue;
      perror("poll");
      throw "poll";
      }
//  cout << "out of poll " << fds[0].revents << " " << fds[1].revents << endl;
    for (int i=0;i<2;++i) {
      if ((fds[i].revents&POLLHUP)!=0) {
        throw "POLLHUP";
        }
      if ((fds[i].revents&POLLERR)!=0) {
        throw "POLLERR";
        }
      if ((fds[i].revents&POLLIN)!=0) {
        if (i) {
          if ((rec=SSL_read(ssl,buffer,LEN))<=0) {
            ERR_print_errors_fp(stdout);
            throw "SSL_read";
            }
          write(fds[!i].fd,buffer,rec);
          }
        else {
          if ((rec=read(fds[i].fd,buffer,LEN))<=0) {
            perror("recv");
            throw "recv";
            }
          SSL_write(ssl,buffer,rec);
          }
        }
      }
    }
//send(fd,"aaaa",4,0);
  } catch (const string &x) {
  tcsetattr(0,TCSANOW,&oldterm);
  cout << "\r" << x << endl;
  } catch (const char * x) {
  tcsetattr(0,TCSANOW,&oldterm);
  cout << "\r" << x << endl;
  }
void window_size(int signal) {
  winsize wins={0,0};
  ioctl(0,TIOCGWINSZ,&wins);
  ostringstream xx;
  xx << "\r~size " << wins.ws_col << " " << wins.ws_row << "~";
  SSL_write(ssl,xx.str().c_str(),xx.str().size());
  }
