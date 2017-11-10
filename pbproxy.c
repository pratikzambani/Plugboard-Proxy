#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
int main(int argc, char **argv)
{
  int c;
  char *ps_port;
  char *key_file;
  char *dest;
  char *dport;
  // reading command line args
  while((c = getopt(argc, argv, "l:k:")) != -1)
  {
    switch(c)
    {
      case 'l':
        ps_port = optarg;
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

  dest = argv[optind++];
  dport = argv[optind];
  
  printf("%s %s %s %s\n", ps_port, key_file, dest, dport);

}
