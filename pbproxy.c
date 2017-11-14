#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<openssl/aes.h>
#include<openssl/rand.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

#define BUFFER_SIZE 8192

void error(char *msg);
void thread_error(char *msg);
void client();
void server();

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

void error(char *msg)
{
  perror(msg);
  exit(0);
}

void client(char *dst, char *dst_port, char *key_file)
{

  //FILE *log;
  //log = fopen("1.txt", "w");

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
    error("Error, could not find server\n");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = ((struct in_addr *)(server->h_addr))->s_addr;
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("Error connecting to server\n");
  
  //fprintf(log, "connected to server...\n");
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
  if(!RAND_bytes(iv, 8))
    error("Error in generating random bytes\n");
  init_ctr(&state, iv);
  //fprintf(log, "sending iv - %s\n", iv);
  write(sockfd, iv, 8); 
  while(1)
  {
    memset(buffer, 0, BUFFER_SIZE);
//    fprintf(log, "reading from stdin...\n");
    while((n1 = read(STDIN_FILENO, buffer, BUFFER_SIZE-1)) > 0)
    {
      unsigned char ciphertext[n1];
      
      AES_ctr128_encrypt(buffer, ciphertext, n1, &aes_key, state.ivec, state.ecount, &state.num);
      write(sockfd, ciphertext, n1);
      usleep(20000);
      //fprintf(log, "sending encrypted data - %s\n", ciphertext);

      memset(buffer, 0, BUFFER_SIZE);

//      if(n1 < BUFFER_SIZE-1)
//        break;
    }
    //fprintf(log, "reading from sockfd...\n");

    while((n2 = read(sockfd, buffer, BUFFER_SIZE-1)) > 0)
    {
      unsigned char plaintext[n2];

      //fprintf(log, "received encryped - %s\n", buffer);
      AES_ctr128_encrypt(buffer, plaintext, n2, &aes_key, state.ivec, state.ecount, &state.num);
      write(STDOUT_FILENO, plaintext, n2);
      usleep(10000);

      memset(buffer, 0, BUFFER_SIZE);
//      if(n2 < BUFFER_SIZE-1)
//        break;
    }
    if(n2 == 0)
    {
      //fprintf(log, "breaking from client side\n");
      break;
    }
  }
  close(sockfd);
  //fclose(log);
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
    error("Could not set encryption key.\n");

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
  ssh_serv_addr.sin_addr.s_addr = ((struct in_addr *)(ssh_server->h_addr))->s_addr;
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
    if(cli_sockfd != -1)
    {
      //fprintf(stdout, "cli_sockfd - %d\n", cli_sockfd);
      //fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
      fcntl(cli_sockfd, F_SETFL, fcntl(cli_sockfd, F_GETFL) | O_NONBLOCK);
     
      ssh_sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (ssh_sockfd < 0)
        error("Error in opening socket\n");
      if (connect(ssh_sockfd, (struct sockaddr *)&ssh_serv_addr, sizeof(ssh_serv_addr)) < 0)
        error("Error connecting to ssh server\n");

      fcntl(ssh_sockfd, F_SETFL, fcntl(ssh_sockfd, F_GETFL) | O_NONBLOCK);
    }
    int readiv=0;
//    fprintf(stdout, "going inside while loop ssh_sockfd is %d cli_sockfd is %d sockfd is %d\n", ssh_sockfd, cli_sockfd, sockfd);
    while(cli_sockfd != -1)
    { 
//      fprintf(stdout, "read from client...\n");
      memset(buffer, 0, BUFFER_SIZE);
      while((n1 = read(cli_sockfd, buffer, BUFFER_SIZE-1)) > 0)
      {
        //fprintf(stdout, "received buffer from client %s len -> %d\n", buffer, strlen(buffer));
        if(readiv == 0)
        {
          if(n1 < 8)
          {
          //fprintf(stdout, "buffer len < 8\n");
            close(cli_sockfd);
            close(ssh_sockfd);
            error("received buffer length is less than 8\n");
          }
          memcpy(iv, buffer, 8);
          //fprintf(stdout, "iv is - %s\n", iv);
          init_ctr(&state, iv);
          readiv=1;
        }
        else
        {
          unsigned char plaintext[n1];
          //fprintf(stdout, "received encryped - %s\n", buffer);
          AES_ctr128_encrypt(buffer, plaintext, n1, &aes_key, state.ivec, state.ecount, &state.num);
          //fprintf(stdout, "decryped to - %s\n", plaintext);
        //fprintf(stdout, "sending to ssh server %s len -> %d\n", plaintext, strlen(plaintext));  
          write(ssh_sockfd, plaintext, n1);
          usleep(10000);
        }
        memset(buffer, 0, BUFFER_SIZE);
//        if(n1 < BUFFER_SIZE-1)
//          break;
      }
//      memset(buffer, 0, BUFFER_SIZE);

      while((n2 = read(ssh_sockfd, buffer, BUFFER_SIZE-1)) > 0)
      {
        //fprintf(stdout, "read from ssh socket %s len -> %d\n", buffer, strlen(buffer));
        char *cipher = (char *)malloc(n2);
        unsigned char ciphertext[n2];
        //fprintf(stdout, "received from ssh - %s\n", buffer);
        AES_ctr128_encrypt(buffer, ciphertext, n2, &aes_key, state.ivec, state.ecount, &state.num);
        memcpy(cipher, ciphertext, n2);
        //fprintf(stdout, "writing to client - %s\n", cipher);
        write(cli_sockfd, cipher, n2);
        usleep(10000);
        free(cipher);

        memset(buffer, 0, BUFFER_SIZE);
//        if(n2 < BUFFER_SIZE-1)
//          break;
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

