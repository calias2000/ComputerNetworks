#include "packet-format.h"
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char *file_name = argv[1];
  char *host = argv[2];
  int port = atoi(argv[3]);
  int windows = atoi(argv[4]);
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  FILE *file = fopen(file_name, "r");
  if (!file) {
    perror("fopen");
    exit(-1);
  }

  // Prepare server host address.
  struct hostent *he;
  if (!(he = gethostbyname(host))) {
    perror("gethostbyname");
    exit(-1);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = *((struct in_addr *)he->h_addr),
  };

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(-1);
  }
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  uint32_t seq_num = 0;
  data_pkt_t data_pkt;
  ack_pkt_t ack_pkt;
  size_t data_len;
  int check = 1;
  uint32_t count = 1;
  int base = 0;
  int send = 0;

  for (int x=0; x<windows - 1; x++){
    data_pkt.seq_num = htonl(seq_num++);
    data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

    // Send segment.
    if(check == 1){
      ack_pkt.seq_num = htonl(count);
      printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
            offsetof(data_pkt_t, data) + data_len);
      ssize_t sent_len =
          sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                (struct sockaddr *)&srv_addr, sizeof(srv_addr));
      
      if (sent_len != offsetof(data_pkt_t, data) + data_len) {
        fprintf(stderr, "Truncated packet.\n");
        exit(-1);
      }
    }
    send++;
  }
  do { // Generate segments from file, until the the end of the file.
    // Prepare data segment.
    data_pkt.seq_num = htonl(seq_num++);

    // Load data from file.
    data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

    // Send segment.
    
    ack_pkt.seq_num = htonl(count);
    printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
          offsetof(data_pkt_t, data) + data_len);
    ssize_t sent_len =
        sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
              (struct sockaddr *)&srv_addr, sizeof(srv_addr));
    send++;
    if (sent_len != offsetof(data_pkt_t, data) + data_len) {
      fprintf(stderr, "Truncated packet.\n");
      exit(-1);
    }
    ssize_t len =
          recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                  (struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});
    printf("BASE %d COUNT %d ACK %d DAT %d\n", base, count, ntohl(ack_pkt.seq_num), ntohl(data_pkt.seq_num) );
    if(len >0 && ntohl(ack_pkt.seq_num) < send){
        printf("Received ack ATRASADO %d\n", ntohl(ack_pkt.seq_num));
        count++;
        base++;
        seq_num = base + windows - 1;
      }
      else if(len >0 && ntohl(ack_pkt.seq_num) == send){
        printf("Received ack EM ORDEM %d\n", ntohl(ack_pkt.seq_num));
        count++;
        base++;
        seq_num = base;
        check = 0;
      }
      else{
        count = base;
        seq_num = base;
        send = count;
        fseek(file, count * 1000, SEEK_SET);
        for(int y = 0; y<windows-1; y++){
          data_pkt.seq_num = htonl(seq_num++);
          data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
          ack_pkt.seq_num = htonl(count);
          ssize_t sent_len =
              sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                    (struct sockaddr *)&srv_addr, sizeof(srv_addr));
          printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
                offsetof(data_pkt_t, data) + data_len);
          if (sent_len != offsetof(data_pkt_t, data) + data_len) {
            fprintf(stderr, "Truncated packet.\n");
            exit(-1);
          }
        }
      }
  } while (!(feof(file) && data_len < sizeof(data_pkt.data)));
  while(check){
      
      ssize_t len =
          recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                  (struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});
      if(len >0 && ntohl(ack_pkt.seq_num) < send){
        printf("Received ack EM ORDEM %d\n", ntohl(ack_pkt.seq_num));
        count++;
        base++;
        seq_num = base;
        check = 1;
      }
      else if(len >0 && ntohl(ack_pkt.seq_num) == send){
        printf("Received ack FINAL %d\n", ntohl(ack_pkt.seq_num));
        check = 0;
      }
      else{
        send = count;
        count = base;
        seq_num = base;
        fseek(file, count * 1000, SEEK_SET);
        data_pkt.seq_num = htonl(seq_num++);
        data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
        ack_pkt.seq_num = htonl(count);
        ssize_t sent_len =
            sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                  (struct sockaddr *)&srv_addr, sizeof(srv_addr));
        printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
              offsetof(data_pkt_t, data) + data_len);
        if (sent_len != offsetof(data_pkt_t, data) + data_len) {
          fprintf(stderr, "Truncated packet.\n");
          exit(-1);
      }

      }
  }

  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return 0;
}
