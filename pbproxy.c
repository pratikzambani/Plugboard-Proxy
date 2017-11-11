#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

#define BUFFER_SIZE 4096

void error(char *msg);
void client();
void server();

int main(int argc, char **argv)
{
  int c;
  char *ps_port;
  char *key_file;
  char *dst;
  char *dst_port;
  int server_mode = 0;
  // reading command line args
  while((c = getopt(argc, argv, "l:k:")) != -1)
  {
    switch(c)
    {
      case 'l':
        ps_port = optarg;
        server_mode = 1;
        break;
      case 'k':
        key_file = optarg;
        break;
      case '?':
        if (optopt == 'l' || optopt == 'k')
          fprintf(stderr, "Option -%c requires an argument\n",optopt);
        else
          fprintf(stderr, "Unknown option\n");
        return 1;
      default:
        abort();
    }
  }
  printf("%d\n", optind);
  if (optind+2 != argc)
  {
    fprintf(stderr, "Mandatory arguments destination and port missing\n");
    //return 1;
  }

  dst = argv[optind++];
  dst_port = argv[optind];
  
  printf("%d %s %s %s %s\n", argc, argv[0], key_file, dst, dst_port);
  
  if (server_mode == 0)
    client();
  else
    server();
}

void error(char *msg)
{
  perror(msg);
  exit(0);
}

void client()
{
  int sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char buffer[256];
  
  portno = 2222;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("Error in opening socket");

  server = gethostbyname("localhost");
  if (server == NULL)
  {
    fprintf(stderr, "Error, could not find server\n");
    exit(0);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    error("Error connecting to server");
  }

  while(1)
  {
    bzero(buffer, BUFFER_SIZE);
    while(( n = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0)
    {
      //fprintf(stdout, "%s\n", buffer);
      //bzero(buffer, 256);
      //fgets(buffer, 255, stdin);
      n = write(sockfd, buffer, strlen(buffer));
      if(n < 0)
      {
        error("Error in writing to socket");
      }
      if(n < BUFFER_SIZE)
        break;
    }
  
    bzero(buffer, BUFFER_SIZE);
    while((n = read(sockfd, buffer, BUFFER_SIZE)) > 0)
    {
      //n = read(sockfd, buffer, 255);
      //if(n < 0)
      //{
      //  error("Error in reading from socket");
      //}
      write(STDOUT_FILENO, buffer, strlen(buffer));
      if(n < BUFFER_SIZE)
        break;
    }
  }
}

void server()
{

}
