#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<openssl/aes.h>
#include<openssl/rand.h>
//#include<pthread.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

#define BUFFER_SIZE 4096

void error(char *msg);
void thread_error(char *msg);
void client();
void server();
void *sthread_execution(void *param);

typedef struct sthread_param{
  int cli_sockfd;
  struct sockaddr_in ssh_serv_addr;
}sthread_param;

typedef struct ctr_state{
  unsigned char ivec[AES_BLOCK_SIZE];
  unsigned int num;
  unsigned char ecount[AES_BLOCK_SIZE];
}ctr_state;

int init_ctr(ctr_state *state, const unsigned char iv[16])
{
  state->num = 0;
  memset(state->ecount, 0, AES_BLOCK_SIZE);
  memset(state->ivec + 8, 0, 8);
  memcpy(state->ivec, iv, 8);
}

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
  //printf("%d\n", optind);
  if (optind+2 != argc)
  {
    fprintf(stderr, "Mandatory arguments destination and port missing\n");
    return 1;
  }

  dst = argv[optind++];
  dst_port = argv[optind];
  
  //printf("%d %s %s %s %s\n", argc, argv[0], key_file, dst, dst_port);
  
  if (server_mode == 0)
    client(dst, dst_port);
  else
    server(ps_port, dst, dst_port);
}
/*
void *sthread_execution(void *param_ptr)
{
  fprintf(stdout, "started new thread\n");
  int n, ssh_sockfd, cli_sockfd, flags;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in ssh_serv_addr;
  sthread_param *param;

  param = (sthread_param *)param_ptr;
  cli_sockfd = param->clisockfd;
  ssh_serv_addr = param->ssh_serv_addr;
  bzero(buffer, BUFFER_SIZE);
   
  fprintf(stdout, "thread - step1\n");
  ssh_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (ssh_sockfd < 0)
    thread_error("Error in opening socket\n");
 
  fprintf(stdout, "thread - step2\n");
  if (connect(ssh_sockfd, (struct sockaddr *)&ssh_serv_addr, sizeof(ssh_serv_addr)) < 0)
  {
    thread_error("Error connecting to ssh server\n");
  }
  flags = fcntl(cli_sockfd, F_GETFL);
  fcntl(cli_sockfd, F_SETFL, flags | O_NONBLOCK);
  flags = fcntl(ssh_sockfd, F_GETFL);
  fcntl(ssh_sockfd, F_SETFL, flags | O_NONBLOCK);  
  fprintf(stdout, "thread - step3 cli_sockfd %d \n", cli_sockfd);
  while(1)
  {
    //fprintf(stdout, "thread - while looping...\n");
    bzero(buffer, BUFFER_SIZE);
    while((n = read(cli_sockfd, buffer, BUFFER_SIZE-1)) > 0)
    {
      fprintf(stdout, "read from cli sock %s\n", buffer);
      write(ssh_sockfd, buffer, strlen(buffer));
      if(n < BUFFER_SIZE-1)
        break;
    }
    bzero(buffer, BUFFER_SIZE);
    while((n = read(ssh_sockfd, buffer, BUFFER_SIZE-1)) > 0)
    {
      fprintf(stdout, "read from ssh sock %s\n", buffer);
      write(cli_sockfd, buffer, strlen(buffer));
      if(n < BUFFER_SIZE-1)
        break;
    }
  }
  fprintf(stdout, "thread - step4\n");
}
*/
void error(char *msg)
{
  perror(msg);
  exit(0);
}

/*void thread_error(char *msg)
{
  perror(msg);
  pthread_exit(NULL);
}*/

void client(char *dst, char *dst_port)
{
  int sockfd, portno, n1, n2;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char buffer[BUFFER_SIZE];
  
  portno = atoi(dst_port);
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

  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

  while(1)
  {
    bzero(buffer, BUFFER_SIZE);
    while((n1 = read(STDIN_FILENO, buffer, BUFFER_SIZE-1)) > 0)
    {
      //fprintf(stdout, "%s\n", buffer);
      //bzero(buffer, 256);
      //fgets(buffer, 255, stdin);
      write(sockfd, buffer, n1);
      if(n1 < BUFFER_SIZE-1)
        break;
    }
  
    bzero(buffer, BUFFER_SIZE);
    while((n2 = read(sockfd, buffer, BUFFER_SIZE-1)) > 0)
    {
      //n = read(sockfd, buffer, 255);
      //if(n < 0)
      //{
      //  error("Error in reading from socket");
      //}
      write(STDOUT_FILENO, buffer, n2);
      if(n2 < BUFFER_SIZE-1)
        break;
    }
  }
}

void server(char *ps_port, char *dst, char *dst_port)
{
  int sockfd, ssh_sockfd, cli_sockfd, portno, ssh_portno, clilen, n1, n2, flags;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in serv_addr, cli_addr, ssh_serv_addr;
  struct hostent *ssh_server;

  pthread_t sthread;
  sthread_param *param;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("Error in opening socket\n");

  fprintf(stdout, "main process - step2\n");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(ps_port);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("Error in binding port\n");
  listen(sockfd, 5);
  clilen = sizeof(cli_addr); 
 
  fprintf(stdout, "main process - step4\n");
  ssh_server = gethostbyname("localhost");
  if (ssh_server == NULL)
  {
    error("Error, could not find ssh server\n");
    fprintf(stderr, "Error, could not find server\n");
    exit(0);
  }
  
  ssh_portno = atoi(dst_port);
  ssh_serv_addr.sin_family = AF_INET;
  bcopy((char *)ssh_server->h_addr, (char *)&ssh_serv_addr.sin_addr.s_addr, ssh_server->h_length);
  //ssh_serv_addr.sin_addr.s_addr = ;
  ssh_serv_addr.sin_port = htons(ssh_portno);
  
  ssh_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (ssh_sockfd < 0)
    error("Error in opening socket\n");
	
  /*if (connect(ssh_sockfd, (struct sockaddr *)&ssh_serv_addr, sizeof(ssh_serv_addr)) < 0)
    error("Error connecting to ssh server\n");

  fcntl(ssh_sockfd, F_SETFL, fcntl(ssh_sockfd, F_GETFL) | O_NONBLOCK);  
*/
  fprintf(stdout, "main process - step6\n");
  while(1)
  {
    fprintf(stdout, "waiting for client connection...\n"); 
    cli_sockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if(cli_sockfd < 0)
      error("Error in accepting client connection\n");
    
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    fcntl(cli_sockfd, F_SETFL, fcntl(cli_sockfd, F_GETFL) | O_NONBLOCK);

    if (connect(ssh_sockfd, (struct sockaddr *)&ssh_serv_addr, sizeof(ssh_serv_addr)) < 0)
      error("Error connecting to ssh server\n");

    fcntl(ssh_sockfd, F_SETFL, fcntl(ssh_sockfd, F_GETFL) | O_NONBLOCK);  
/*    else
    {
      param = (sthread_param *)malloc(sizeof(sthread_param));
      param->clisockfd = clisockfd;
      param->ssh_serv_addr = ssh_serv_addr;
      
      fprintf(stdout, "main process - about to create new thread\n");
      pthread_create(&sthread, NULL, sthread_execution, &param);
      pthread_detach(sthread);
    }*/
    int flag=0;
    while(1)
    { 
//      fprintf(stdout, "before client read...\n");
      flag=0;   
      bzero(buffer, BUFFER_SIZE);
      //fprintf(stdout, "while loop..\n");
      while((n1 = read(cli_sockfd, buffer, BUFFER_SIZE-1)) > 0)
      {
        flag=1;
//        fprintf(stdout, "received buffer %s\n", buffer);
        write(ssh_sockfd, buffer, n1);
        if(n1 < BUFFER_SIZE-1)
          break;
      }
//      fprintf(stdout, "before ssh read...\n");
      bzero(buffer, BUFFER_SIZE);
      while((n2 = read(ssh_sockfd, buffer, BUFFER_SIZE-1)) > 0)
      {
//        fprintf(stdout, "read from ssh socket %s\n", buffer);
        write(cli_sockfd, buffer, n2);
        if(n2 < BUFFER_SIZE-1)
          break;
      }
    }
  }
  return;
}

