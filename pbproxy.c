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

#define BUFFER_SIZE 8192

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
    client(dst, dst_port, key_file);
  else
    server(ps_port, dst, dst_port, key_file);
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

void client(char *dst, char *dst_port, char *key_file)
{

  FILE *log;
  log = fopen("1.txt", "w");

  int sockfd, portno, n1, n2;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char buffer[BUFFER_SIZE];
  
  portno = atoi(dst_port);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("Error in opening socket");

  server = gethostbyname(dst);
  if (server == NULL)
  {
    fprintf(stderr, "Error, could not find server\n");
    exit(0);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  //bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_addr.s_addr = ((struct in_addr *)(server->h_addr))->s_addr;
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    error("Error connecting to server\n");
  }
  fprintf(log, "connected to server...\n");
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

  ctr_state state;
  unsigned char iv[8];
  unsigned char enc_key[16];
  AES_KEY aes_key;

  FILE *key_f = fopen(key_file, "rb");
  if(key_f == NULL)
  {
    fclose(key_f);
    error("Error in opening key file\n");
  }

  if(fread(enc_key, 1, AES_BLOCK_SIZE, key_f) != 16)
    error("Error in reading key file\n");

  //fprintf(stdout, "enc key is %s\n", enc_key);
  if(AES_set_encrypt_key((const unsigned char *)enc_key, 128, &aes_key) < 0)
    error("Could not set encryption key.\n");
  
  while(1)
  {
    bzero(buffer, BUFFER_SIZE);
    fprintf(log, "reading from stdin...\n");
    while((n1 = read(STDIN_FILENO, buffer, BUFFER_SIZE-1)) > 0)
    {
      if(!RAND_bytes(iv, 8))
        error("Error in generating random bytes\n");
      //bzero(buffer, 256);
      //fgets(buffer, 255, stdin);
      char *ivcipher = (char *)malloc(n1+8);
      unsigned char ciphertext[n1];
      
      memcpy(ivcipher, iv, 8);
      init_ctr(&state, iv);
      AES_ctr128_encrypt(buffer, ciphertext, n1, &aes_key, state.ivec, state.ecount, &state.num);
      memcpy(ivcipher+8, ciphertext, n1);
      write(sockfd, ivcipher, n1+8);
      free(ivcipher);
      bzero(buffer, BUFFER_SIZE);
      if(n1 < BUFFER_SIZE-1)
        break;
    }  
    bzero(buffer, BUFFER_SIZE);
    fprintf(log, "reading from sockfd...\n");
    while((n2 = read(sockfd, buffer, BUFFER_SIZE-1)) > 0)
    {
      if(n2 < 8)
      {
        close(sockfd);
        error("Received buffer length less than 8\n");
      }
      //n = read(sockfd, buffer, 255);
      //if(n < 0)
      //{
      //  error("Error in reading from socket");
      //}
      unsigned char plaintext[n2-8];
      memcpy(iv, buffer, 8);
      init_ctr(&state, iv);
      AES_ctr128_encrypt(buffer+8, plaintext, n2-8, &aes_key, state.ivec, state.ecount, &state.num);
      write(STDOUT_FILENO, plaintext, n2-8);
      bzero(buffer, BUFFER_SIZE);
      if(n2 < BUFFER_SIZE-1)
        break;
    }
    if(n2 == 0)
    {
      fprintf(log, "breaking from client side\n");
      break;
    }
  }
  fclose(log);
}

void server(char *ps_port, char *dst, char *dst_port, char *key_file)
{
  int sockfd, ssh_sockfd, cli_sockfd, portno, ssh_portno, clilen, n1, n2, flags;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in serv_addr, cli_addr, ssh_serv_addr;
  struct hostent *ssh_server;
  
  ctr_state state;
  unsigned char iv[8];
  unsigned char enc_key[16];
  AES_KEY aes_key;

  FILE *key_f = fopen(key_file, "rb");
  if(key_f == NULL)
  {
    fclose(key_f);
    error("Error in opening key file\n");
  }

  if(fread(enc_key, 1, AES_BLOCK_SIZE, key_f) != 16)
    error("Error in reading key file\n");

  //fprintf(stdout, "enc key is %s\n", enc_key);
  if(AES_set_encrypt_key(enc_key, 128, &aes_key) < 0)
  {
    error("Could not set encryption key.\n");
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("Error in opening socket\n");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(ps_port);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("Error in binding port\n");
  listen(sockfd, 5);
  clilen = sizeof(cli_addr); 
 
  ssh_server = gethostbyname(dst);
  if (ssh_server == NULL)
    error("Error, could not find ssh server\n");
  
  ssh_portno = atoi(dst_port);
  ssh_serv_addr.sin_family = AF_INET;
  bcopy((char *)ssh_server->h_addr, (char *)&ssh_serv_addr.sin_addr.s_addr, ssh_server->h_length);
  //ssh_serv_addr.sin_addr.s_addr = ;
  ssh_serv_addr.sin_port = htons(ssh_portno);
  
  /*ssh_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (ssh_sockfd < 0)
    error("Error in opening socket\n");
*/	
  /*if (connect(ssh_sockfd, (struct sockaddr *)&ssh_serv_addr, sizeof(ssh_serv_addr)) < 0)
    error("Error connecting to ssh server\n");

  fcntl(ssh_sockfd, F_SETFL, fcntl(ssh_sockfd, F_GETFL) | O_NONBLOCK);  
*/
  while((cli_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen)))
  {
    //fprintf(stdout, "waiting for client connection...\n"); 
    //cli_sockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    //fprintf(stdout, "cli_sockfd - %d\n", cli_sockfd);
    //if(cli_sockfd < 0)
      //error("Error in accepting client connection\n");
    
//    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
//    fcntl(cli_sockfd, F_SETFL, fcntl(cli_sockfd, F_GETFL) | O_NONBLOCK);

    if(cli_sockfd != -1)
    {
      fprintf(stdout, "cli_sockfd - %d\n", cli_sockfd);
      //fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
      fcntl(cli_sockfd, F_SETFL, fcntl(cli_sockfd, F_GETFL) | O_NONBLOCK);
     
      ssh_sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (ssh_sockfd < 0)
        error("Error in opening socket\n");
      if (connect(ssh_sockfd, (struct sockaddr *)&ssh_serv_addr, sizeof(ssh_serv_addr)) < 0)
        error("Error connecting to ssh server\n");

      fcntl(ssh_sockfd, F_SETFL, fcntl(ssh_sockfd, F_GETFL) | O_NONBLOCK);
    }
/*    else
    {
      param = (sthread_param *)malloc(sizeof(sthread_param));
      param->clisockfd = clisockfd;
      param->ssh_serv_addr = ssh_serv_addr;
      
      fprintf(stdout, "main process - about to create new thread\n");
      pthread_create(&sthread, NULL, sthread_execution, &param);
      pthread_detach(sthread);
    }*/
//    fprintf(stdout, "going inside while loop ssh_sockfd is %d cli_sockfd is %d sockfd is %d\n", ssh_sockfd, cli_sockfd, sockfd);
    while(cli_sockfd != -1)
    { 
      //fprintf(stdout, "read from client...\n");
      bzero(buffer, BUFFER_SIZE);
//      fprintf(stdout, "while loop..\n");
      while((n1 = read(cli_sockfd, buffer, BUFFER_SIZE-1)) > 0)
      {
        //fprintf(stdout, "received buffer from client %s len -> %d\n", buffer, strlen(buffer));
        if(n1 < 8)
        {
          //fprintf(stdout, "buffer len < 8\n");
          close(cli_sockfd);
          close(ssh_sockfd);
          error("received buffer length is less than 8\n");
        }
        unsigned char plaintext[n1-8];

        memcpy(iv, buffer, 8);
        init_ctr(&state, iv);
        AES_ctr128_encrypt(buffer+8, plaintext, n1-8, &aes_key, state.ivec, state.ecount, &state.num);
        //fprintf(stdout, "sending to ssh server %s len -> %d\n", plaintext, strlen(plaintext));  
        write(ssh_sockfd, plaintext, n1-8);
        bzero(buffer, BUFFER_SIZE);
        if(n1 < BUFFER_SIZE-1)
          break;
      }
//      if(n1 != 0 && n1 != -1)
//        fprintf(stdout, "before ssh read... n1 = %d\n", n1);
      //fprintf(stdout, "read from ssh...\n");
      bzero(buffer, BUFFER_SIZE);
      while((n2 = read(ssh_sockfd, buffer, BUFFER_SIZE-1)) > 0)
      {
        if(!RAND_bytes(iv, 8))
          error("Error in generating random bytes\n");
        //fprintf(stdout, "read from ssh socket %s len -> %d\n", buffer, strlen(buffer));
        char *ivcipher = (char *)malloc(n2+8);
        unsigned char ciphertext[n2];

        memcpy(ivcipher, iv, 8);
        init_ctr(&state, iv);
        AES_ctr128_encrypt(buffer, ciphertext, n2, &aes_key, state.ivec, state.ecount, &state.num);
        memcpy(ivcipher+8, ciphertext, n2);
        //fprintf(stdout, "writing to client %s len -> %d\n", ivcipher, strlen(ivcipher));
        write(cli_sockfd, ivcipher, n2+8);
        free(ivcipher);
        bzero(buffer, BUFFER_SIZE);
        if(n2 < BUFFER_SIZE-1)
          break;
      }
      if(n1 == 0)
      {
        //close(sockfd);
        //close(cli_sockfd);
	close(ssh_sockfd);
        break;
      }
    }
  }
  return;
}

