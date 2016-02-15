/*
   Server part of test programs for checking ability to retrieve from
   system last received data from remote host after TCP connection was
   closed by remote host.   
   Copyright (C) 2016 Karlson2k (Evgeny Grin)

   You can run, copy, modify, publish and do whatever you want with this
   program as long as this message and copyright string above are preserved.
   You are also explicitly allowed to reuse this program under any LGPL or
   GPL license or under any BSD-style license.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

#if !defined(_WIN32) || defined(__CYGWIN__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#define SYS_POSIX_SOCKETS 1
typedef int SYS_socket;
#define SYS_INVALID_SOCKET (-1)
#define SYS_sock_errno (errno)
#define SYS_socket_close_(fd) close((fd))
#else  /* defined(_WIN32) && !defined(__CYGWIN__) */
#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#define SYS_WINSOCK_SOCKETS 1
typedef SOCKET SYS_socket;
#define SYS_INVALID_SOCKET (INVALID_SOCKET)
#define SYS_sock_errno (WSAGetLastError())
#define SYS_socket_close_(fd) closesocket((fd))
#ifndef _SSIZE_T_DEFINED
typedef ptrdiff_t ssize_t;
#define _SSIZE_T_DEFINED
#endif /* _SSIZE_T_DEFINED */
#ifdef _MSC_EXTENSIONS
#pragma comment(lib, "Ws2_32.lib")
#endif /* _MSC_EXTENSIONS */
#endif /* defined(_WIN32) && !defined(__CYGWIN__) */

#ifndef SOMAXCONN
#define SOMAXCONN 127
#endif /* SOMAXCONN */


#ifndef USE_PORT
#define USE_PORT 1155
#endif /* ! USE_PORT */


#ifdef SYS_WINSOCK_SOCKETS
static int init_libs (void)
{
  WSADATA wsa_data;

  if (0 == WSAStartup (MAKEWORD (2, 2), &wsa_data))
  {
    if (MAKEWORD (2, 2) == wsa_data.wVersion)
      return !0;
    WSACleanup ();
  }
  return  0;
}

static int deinit_libs (void)
{
  return 0 == WSACleanup ();
}
#else  /* ! SYS_WINSOCK_SOCKETS */
static int init_libs (void)
{
  return !0;
}

static int deinit_libs (void)
{
  return !0;
}
#endif /* ! SYS_WINSOCK_SOCKETS */

/* returns zero on success */
int str_to_int_limit (const char *str, int *ret_val, int limit)
{
  size_t p;
  if (!ret_val)
    return -2;
  *ret_val = 0;
  for (p = 0; str[p]; ++p)
  {
    const char c = str[p];
    if (c < '0' || c > '9')
      return 1;
    if (*ret_val > INT_MAX / 10)
      return -1;
    *ret_val *= 10;
    if (((unsigned int)(*ret_val + c - '0')) > (unsigned int)limit ||
        ((unsigned int)(*ret_val + c - '0')) > (unsigned int)INT_MAX)
      return -1;
    *ret_val += c - '0';
  }
  return 0;
}

const char* find_chr (const char *str, char c, size_t strlen)
{
  size_t p;
  for (p = 0; p < strlen; p++)
  {
    if (str[p] == c)
      return str + p;
  }
  return NULL;
}

int cmp_to_lower (const char *str1, size_t str1_len, const char *str2)
{
  size_t pos;
  for (pos = 0; pos < str1_len && str2[pos]; ++pos)
  {
    const char c1 = str1[pos];
    const char c2 = str2[pos];
    if (c1 != c2 && !(c1 >= 'A' && c1 <= 'Z' && c1 - 'A' + 'a' == c2))
      return 0;
  }
  return 1;
}

/* returns pointer to the first new line char position */
const char* recv_line (SYS_socket s, char ** pbuf, size_t buf_left)
{
  while (buf_left > 0)
  {
    ssize_t rcved_size;
    char *buf_start = *pbuf;
    const char *nl_ptr;

    rcved_size = recv (s, buf_start, buf_left, 0);
    if (0 > rcved_size)
    {
      fprintf (stderr, "Network error: %u\n",
               (unsigned)SYS_sock_errno);
      return NULL;
    }
    *pbuf += rcved_size;
    buf_left -= rcved_size;
    nl_ptr = find_chr (buf_start, '\n', rcved_size);
    if (nl_ptr)
      return nl_ptr;
  }
  fprintf (stderr, "No buffer space left.\n");
  return NULL;
}

int send_str (SYS_socket s, const char *str, size_t str_len)
{
  while (str_len > 0)
  {
    size_t send_size = str_len;
    ssize_t sent_size;
#ifdef SYS_WINSOCK_SOCKETS
    if (send_size > INT_MAX)
      send_size = INT_MAX;
#endif /* SYS_WINSOCK_SOCKETS */
    sent_size = send (s, str, send_size, 0);
    if (0 > sent_size)
    {
      fprintf (stderr, "Network error: %u\n",
               (unsigned)SYS_sock_errno);
      return 0;
    }
    str += sent_size;
    str_len -= sent_size;
  }
  return 1;
}

int do_respond (SYS_socket s)
{
  int result;
  char *buf;
  static const size_t buf_size = 10 * 1024;

  result = 0;
  buf = malloc (buf_size + 1); /* Extra byte for null-termination */
  if (NULL == buf)
  {
    result = 99;
    fprintf (stderr, "Error allocating memory for buffer.\n");
  }
  else
  {
    char *rcv_ptr;
    char *processed_ptr;
    const char *nl_ptr;

    rcv_ptr = buf;
    processed_ptr = buf;
    printf ("Waiting for receive \"FIRST PING\".\n");
    nl_ptr = recv_line (s, &rcv_ptr, buf_size - (rcv_ptr - buf));
    if (!nl_ptr)
    {
      result = 1;
      fprintf(stderr, "Error receiving \"FIRST PING\".\n");
    }
    else
    {
      rcv_ptr[0] = 0; /* Null-terminate for safety */
      printf ("Received: %*.*s\n", (int)(nl_ptr - processed_ptr), (int)(nl_ptr - processed_ptr), processed_ptr);
      if (!cmp_to_lower (processed_ptr, nl_ptr - processed_ptr, "first ping"))
      {
        result = 1;
        fprintf (stderr, "Expected marker not found in received data.\n");
      }
      else
      {
        static const char resp1[] = "FIRST PONG\n";
        processed_ptr = rcv_ptr;
        printf ("Sending \"FIRST PONG\".\n");
        if (!send_str (s, resp1, sizeof (resp1) - 1))
        {
          result = 1;
          fprintf (stderr, "Error sending response.\n");
        }
        else
        {
          printf ("Sent. Waiting for \"SECOND PING\".\n");
          nl_ptr = recv_line (s, &rcv_ptr, buf_size - (rcv_ptr - buf));
          if (!nl_ptr)
          {
            result = 1;
            fprintf (stderr, "Error receiving \"SECOND PING\".\n");
          }
          else
          {
            rcv_ptr[0] = 0; /* Null-terminate for safety */
            printf ("Received: %*.*s\n", (int)(nl_ptr - processed_ptr), (int)(nl_ptr - processed_ptr), processed_ptr);
            if (!cmp_to_lower (processed_ptr, nl_ptr - processed_ptr, "second ping"))
            {
              result = 1;
              fprintf (stderr, "Expected marker not found in received data.\n");
            }
            else
            {
              static const char resp2[] = "SECOND PONG\n";
              processed_ptr = rcv_ptr;
              printf ("Sending \"SECOND PONG\".\n");
              if (!send_str (s, resp2, sizeof (resp2) - 1))
              {
                result = 1;
                fprintf (stderr, "Error sending response.\n");
              }
              else
              {
                printf ("Sent. Sequence completed.\n");
              }
            }
          }
        }
      }
    }
    free (buf);
  }
  if (result)
    fprintf (stderr, "Completed with some error.\n");
  else
    printf ("Completed without errors.\n");

  printf ("Closing connection.\n");
  SYS_socket_close_ (s);
  return result;
}

int do_server (SYS_socket listen_s, int num_set_seq)
{
  int result = 0;
  int num_in_seq = 0;
  int seq_completed = 0;
  if (num_set_seq)
    printf ("Waiting for incoming connection (%d sets of sequences left)...\n", num_set_seq);
  else
    printf ("Waiting for incoming connection (enter key to exit)...\n");

  while (1)
  {
    struct timeval *tp;
    fd_set rset;
#ifdef SYS_WINSOCK_SOCKETS
    struct timeval timeout;
#endif /* SYS_WINSOCK_SOCKETS */

    FD_ZERO (&rset);
    FD_SET (listen_s, &rset);
    tp = NULL;
    if (!num_set_seq)
    {
#ifdef SYS_POSIX_SOCKETS
      FD_SET (STDIN_FILENO, &rset);
#else  /* SYS_WINSOCK_SOCKETS */
      timeout.tv_usec = 0;
      timeout.tv_sec = 1;
      tp = &timeout;
#endif /* SYS_WINSOCK_SOCKETS */
    }
    if (0 > select ((int)listen_s + 1, &rset, NULL, NULL, tp))
    {
      fprintf (stderr, "select() error: %u\n",
               (unsigned)SYS_sock_errno);
      return 99;
    }
    if (!num_set_seq)
    {
#ifdef SYS_POSIX_SOCKETS
      if (FD_ISSET (STDIN_FILENO, &rset))
      {
        (void)getchar ();
        return result;
      }
#else  /* SYS_WINSOCK_SOCKETS */
      if (_kbhit ())
      {
        (void)_getch ();
        return result;
      }
#endif /* SYS_WINSOCK_SOCKETS */
    }
    if (FD_ISSET (listen_s, &rset))
    {
      struct sockaddr_in sock_addr;
      SYS_socket acp;
      socklen_t addr_len;

      addr_len = (socklen_t) sizeof (sock_addr);
      printf ("\nIncoming connection. Accepting...\n");
      acp = accept (listen_s, (struct sockaddr*) &sock_addr, &addr_len);
      if (SYS_INVALID_SOCKET == acp)
      {
        fprintf (stderr, "Error accepting incoming connection: %u",
                 (unsigned)SYS_sock_errno);
        return 99;
      }
      else
      {
        int res;
        printf ("Incoming connection accepted.\n");
        res = do_respond (acp);
        if (res)
        {
          result = res;
          fprintf (stderr, "Error communication with client\n");
        }
        if (res > 1)
          return result;
      }
      if (num_set_seq)
      {
        if (num_in_seq >= 5)
        {
          num_in_seq = 0;
          seq_completed++;
          if (seq_completed >= num_set_seq)
            return result;
        }
        else
          num_in_seq++;
        printf ("\nWaiting for incoming connection (%d sets of sequences left)...\n", num_set_seq - seq_completed);
      }
      else
        printf ("\nWaiting for incoming connection (enter key to exit)...\n");
    }
  }
}

void print_usage (const char *prog_name)
{
  printf ("Usage:\n"
          "\t%s [NUMBER]\n"
          "where NUMBER is number of sets of communication sequences.\n"
          "If NUMBER is not specified or zero then server_part will run until interrupted.\n",
          prog_name ? prog_name : "server_part");
}

int main(int argc, char *argv[])
{
  SYS_socket lstn_s;
  int result = 0;
  int num_set_seq;

  if (argc < 2)
  {
    printf ("Number of pairs of sequences is not specified. Will run until interrupted.\n");
    num_set_seq = 0;
  }
  else
  {
    if (0 != str_to_int_limit (argv[1], &num_set_seq, 1000))
    {
      fprintf (stderr, "\"%s\" is not valid number of pairs of sequences.\n", argv[1]);
      print_usage (argc > 0 ? argv[0] : NULL);
      return 99;
    }
  }

  if (!init_libs ())
  {
    fprintf (stderr, "Can't initialize libs.\n");
    return 99;
  }

  lstn_s = socket (AF_INET, SOCK_STREAM, 0);
  if (SYS_INVALID_SOCKET == lstn_s)
  {
    fprintf (stderr, "Can't create socket: %u\n",
             (unsigned)SYS_sock_errno);
    result = 99;
  }
  else
  {
    struct sockaddr_in sock_addr;
    socklen_t addrlen;
    memset (&sock_addr, 0, sizeof (sock_addr));
    
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons (USE_PORT);
    addrlen = sizeof (sock_addr);
    if (bind (lstn_s, (const struct sockaddr*)&sock_addr, addrlen) < 0)
    {
      fprintf (stderr, "Failed to bind socket: %u\n",
               (unsigned)SYS_sock_errno);
      result = 99;
    }
    else
    {
      if (listen (lstn_s, SOMAXCONN) < 0)
      {
        fprintf (stderr, "Failed to listen on socket: %u\n",
                 (unsigned)SYS_sock_errno);
        result = 99;
      }
      else
        result = do_server (lstn_s, num_set_seq);
    }
    SYS_socket_close_ (lstn_s);
  }
  if (result)
    fprintf (stderr, "\nFinished with some error.\n");
  else
    printf ("\nFinished without any errors.\n");

  deinit_libs ();
  return result;
}
