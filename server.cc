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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <iomanip>
#include <openssl/ssl.h>
#include <openssl/err.h>
using namespace std;
const int LEN=256*256;
int fd;
void process_command(char*,int);
void print_hex(char *data, int len);
int master;
int main(int argc, char ** argv) try {
  int ret;
  if (argc<4) throw string(argv[0])+" CA.pem cert.pem key.pem";
  char recv_buffer[LEN];
  char send_buffer[LEN];
  char command_buffer[LEN];
  fd=socket(AF_INET,SOCK_STREAM,0);
  sockaddr_in addr;
  addr.sin_family=AF_INET;
  addr.sin_port=htons(2345);
  addr.sin_addr.s_addr=inet_addr("127.0.0.1");
  if (connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0) {
    perror("connect");
    throw "connect";
    }

//
  SSL_CTX *ctx=SSL_CTX_new(TLS_client_method());
  if (!ctx) throw "NULL CTX returned";
  if (!SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION)) throw "SSL_CTX_set_min_proto_version";
  if (SSL_CTX_load_verify_locations(ctx,argv[1],NULL)!=1) throw "SSL_CTX_load_verify_locations";
  if (SSL_CTX_use_certificate_file(ctx,argv[2],SSL_FILETYPE_PEM)!=1) throw "SSL_CTX_use_certificate_file";
  if (SSL_CTX_use_PrivateKey_file(ctx,argv[3],SSL_FILETYPE_PEM)!=1) throw "SSL_CTX_use_PrivateKey_file";
  SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,NULL);
  SSL_CTX_set_verify_depth(ctx,0);
  SSL *ssl=SSL_new(ctx);
  if (ssl==NULL) throw "SSL_new";
  if (SSL_set_fd(ssl,fd)!=1) throw "SSL_set_fd";
  if ((ret=SSL_connect(ssl))!=1) {
    cerr << SSL_get_error(ssl,ret) << endl;
    ERR_print_errors_fp(stdout);
    throw "SSL_connect";
    }
//

  master=getpt();
  if (master<0) {
    perror("getpt");
    throw "getpt";
    }
  if (grantpt(master)<0) {
    perror("grantpt");
    throw "grantpt";
    }
  if (unlockpt(master)<0) {
    perror("unlockpt");
    throw  "unlockpt";
    }
  int child=fork();
  if (child==0) {
    close(0);
    close(1);
    close(2);
    int fd=open(ptsname(master),O_RDWR);
    if (fd!=0) throw "open master";
    dup(fd);
    dup(fd);
    setsid();
    execl("/bin/bash","-bash",NULL);
    }
  pollfd fds[2];
  fds[0].fd=master;
  fds[0].events=POLLIN|POLLHUP|POLLERR;
  fds[1].fd=fd;
  fds[1].events=POLLIN|POLLHUP|POLLERR;
  int rec=0;
  winsize wins={30,100};
  ioctl(master,TIOCSWINSZ,&wins);
  bool newline=false;
  bool command=false;
  int cmd_len=0;
  for (;;) {
    rec=poll(fds,2,-1);
    for (int i=0;i<2;++i) {
      if ((fds[i].revents&POLLHUP)!=0) {
        cout << "Received POLLHUP on " << i << endl;
        exit(0);
        }
      if ((fds[i].revents&POLLIN)!=0) {
        if (i) {
          if ((rec=SSL_read(ssl,recv_buffer,LEN))<=0) {
            ERR_print_errors_fp(stdout);
            throw "SSL_read 1";
            }
          }
        else {
          if ((rec=read(fds[i].fd,recv_buffer,LEN))<=0) {
            perror("recv");
            throw "recv";
            }
          }
        if (fds[i].fd==fd) { //look for commands
          int jj=0;
          for (int j=0;j<rec;++j) {
            bool skip=false;
            if (command && recv_buffer[j]=='~') {
              command=false;
              skip=true;
              for (int k=0;k<cmd_len;++k) cout << command_buffer[k];
              process_command(command_buffer,cmd_len);
              cout << endl;
              cmd_len=0;
              }
            if (newline && recv_buffer[j]=='~') {
              command=true;
              if (jj>0) --jj;
              }
            if (recv_buffer[j]=='\r') newline=true;
            else                      newline=false;
            if (command && recv_buffer[j]!='~') command_buffer[cmd_len++]=recv_buffer[j];
            if (!command && !skip) send_buffer[jj++]=recv_buffer[j];
            }
          rec=jj;
          }
        else {
          for (int j=0;j<rec;++j) send_buffer[j]=recv_buffer[j];
          }
        if (rec>0) {
          if (i) {
            rec=write(fds[!i].fd,send_buffer,rec);
            if (rec<0) {
              perror("write");
              throw "write";
              }
            }
          else {
            rec=SSL_write(ssl,send_buffer,rec);
            if (rec<0) {
              throw "SSL_write";
              }
            }
          }
//      if (i==1) {
//        for (int j=0;j<rec;++j) cout << hex << (int)buffer[j] << " " << flush;
//        }
        }
      }
    }
//send(fd,"aaaa",4,0);
  } catch (const char * x) {
  cout << x << endl;
  shutdown(fd,SHUT_RDWR);
  } catch (const string &x) {
  cout << x << endl;
  shutdown(fd,SHUT_RDWR);
  }
//
void process_command(char* command,int len) {
  if (len>=LEN) throw "len error";
  command[len]=0;
  unsigned short a,b;
  char cmd[len];
  int ret=sscanf(command,"%s%hd%hd",cmd,&a,&b);
  if (string(cmd)=="size" && ret==3) {
    winsize wins={b,a};
    ioctl(master,TIOCSWINSZ,&wins);
    cout << "\nsize: " << a << "x" << b << endl;
    }
  }
void print_hex(char *data, int len) {
  cout << hex << setfill('0');
  cout << setw(3) << dec << 0 << hex << "  ";
  for (int i=0;i<len;++i) {
    cout << setw(2) << (int) data[i];
    if ((i+1)%16==0) { 
      cout << endl;
      cout << setw(3) << dec << i/16 << hex << "  ";
      }
    else if ((i+1)%8==0) cout << " | ";
    else cout << " ";
    }
  cout << dec << setfill(' ') << endl;
  cout << "--" << endl;
}
