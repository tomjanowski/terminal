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
using namespace std;
const int LEN=256*256;
int fd;
int main(int argc, char ** argv) try {
  char buffer[LEN];
  fd=socket(AF_INET,SOCK_STREAM,0);
  sockaddr_in addr;
  addr.sin_family=AF_INET;
  addr.sin_port=htons(2345);
  addr.sin_addr.s_addr=inet_addr("127.0.0.1");
  if (connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0) {
    perror("connect");
    throw "connect";
    }
  int master=getpt();
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
  for (;;) {
    rec=poll(fds,2,-1);
    for (int i=0;i<2;++i) {
      if ((fds[i].revents&POLLHUP)!=0) {
        cout << "Received POLLHUP on " << i << endl;
        exit(0);
        }
      if ((fds[i].revents&POLLIN)!=0) {
        if ((rec=read(fds[i].fd,buffer,LEN))<=0) {
          perror("recv");
          throw "recv";
          }
        rec=write(fds[!i].fd,buffer,rec);
        if (rec<0) {
          perror("write");
          throw "write";
          }
        if (i==1) {
          for (int j=0;j<rec;++j) cout << hex << (int)buffer[j] << " " << flush;
          }
        }
      }
    }
//send(fd,"aaaa",4,0);
  } catch (const char * x) {
  cout << x << endl;
  shutdown(fd,SHUT_RDWR);
  }
