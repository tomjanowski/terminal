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
#include <string.h>
#include <openssl/err.h>
using namespace std;
const int LEN=256*256;
termios term,oldterm;
void window_size(int);
int fd;
SSL *ssl;
int main(int argc, char ** argv) try {
  tcgetattr(0,&oldterm);
  if (argc<4) throw string(argv[0])+" CA.pem cert.pem key.pem [peer_common_name]";
  const int MAX_NAME=100;
  unsigned char expected_common_name[MAX_NAME]={};
  bool check_name=false;
  if (argc>4) {
    strncpy(reinterpret_cast<char*>(expected_common_name),argv[4],MAX_NAME);
    check_name=true;
    }
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
  if (SSL_get_verify_result(ssl)!=X509_V_OK) {
    throw "SSL_get_verify_result";
    }
  X509 *cert=SSL_get_peer_certificate(ssl);
  if (!cert) throw "SSL_get_peer_certificate";
  X509_NAME *name=X509_get_subject_name(cert);
  int i=X509_NAME_get_index_by_NID(name,NID_commonName,-1);
  const unsigned char *common_name=ASN1_STRING_get0_data(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(name,i)));
  cout << "Peer common name: " << common_name << endl;
  if (check_name && string(reinterpret_cast<const char*>(common_name))!=string(reinterpret_cast<const char*>(expected_common_name))) {
    throw "names do not match";
    }
  cout << "current cipher: " << SSL_CIPHER_standard_name(SSL_get_current_cipher(ssl)) << endl;
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
  SSL_shutdown(ssl);
  shutdown(fd,SHUT_RDWR);
  } catch (const char * x) {
  tcsetattr(0,TCSANOW,&oldterm);
  cout << "\r" << x << endl;
  if (string(x)=="bind") exit(1);
  SSL_shutdown(ssl);
  shutdown(fd,SHUT_RDWR);
  }
void window_size(int signal) {
  winsize wins={0,0};
  ioctl(0,TIOCGWINSZ,&wins);
  ostringstream xx;
  xx << "\r~size " << wins.ws_col << " " << wins.ws_row << "~";
  SSL_write(ssl,xx.str().c_str(),xx.str().size());
  }
