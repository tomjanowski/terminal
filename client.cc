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
using namespace std;
const int LEN=256*256;
termios term,oldterm;
void window_size(int);
int fd;
int main(int argc, char ** argv) try {
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
  listen(fd1,1);
  fd=accept(fd1,reinterpret_cast<sockaddr*>(&addr),&addr_len);
  if (fd<0) {
    perror("accept");
    throw "accept";
    }
  pollfd fds[2];
  fds[0].fd=0;
  fds[0].events=POLLIN|POLLHUP|POLLERR;
  fds[1].fd=fd;
  fds[1].events=POLLIN|POLLHUP|POLLERR;
  int rec=0;
  tcgetattr(0,&oldterm);
  cfmakeraw(&term);
  tcsetattr(0,TCSANOW,&term);
  winsize wins={0,0};
  ioctl(0,TIOCGWINSZ,&wins);
  ostringstream xx;
  xx << "\r~size " << wins.ws_col << " " << wins.ws_row << "~" << endl;
  write(fd,xx.str().c_str(),xx.str().size());
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
        if ((rec=read(fds[i].fd,buffer,LEN))<=0) {
          perror("recv");
          throw "recv";
          }
        write(fds[!i].fd,buffer,rec);
        }
      }
    }
//send(fd,"aaaa",4,0);
  } catch (const char * x) {
  tcsetattr(0,TCSANOW,&oldterm);
  cout << "\r" << x << endl;
  }
void window_size(int signal) {
  winsize wins={0,0};
  ioctl(0,TIOCGWINSZ,&wins);
  ostringstream xx;
  xx << "\r~size " << wins.ws_col << " " << wins.ws_row << "~";
  write(fd,xx.str().c_str(),xx.str().size());
  }
