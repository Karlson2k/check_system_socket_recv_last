/*
   Client part of test programs for checking ability to retrieve from
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
#include <string.h>

#if !defined(_WIN32) || defined(__CYGWIN__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#define SYS_POSIX_SOCKETS 1
typedef int SYS_socket;
#define SYS_INVALID_SOCKET (-1)
#define SYS_sock_errno (errno)
#define SYS_reset_sock_errno() (errno=0)
#define SYS_set_sock_errno(e) (errno=(e))
#define SYS_socket_close_(fd) close((fd))
#define SYS_ECONNABORTED ECONNABORTED
#define SYS_ECONNRESET ECONNRESET
#else  /* defined(_WIN32) && !defined(__CYGWIN__) */
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#define SYS_WINSOCK_SOCKETS 1
typedef SOCKET SYS_socket;
#define SYS_INVALID_SOCKET (INVALID_SOCKET)
#define SYS_sock_errno (WSAGetLastError())
#define SYS_reset_sock_errno() (WSASetLastError(0))
#define SYS_set_sock_errno(e) (WSASetLastError((e)))
#define SYS_socket_close_(fd) closesocket((fd))
#ifndef _SSIZE_T_DEFINED
typedef ptrdiff_t ssize_t;
#define _SSIZE_T_DEFINED
#endif /* _SSIZE_T_DEFINED */
typedef unsigned long in_addr_t;
#ifdef _MSC_EXTENSIONS
#pragma comment(lib, "Ws2_32.lib")
#endif /* _MSC_EXTENSIONS */
#define SYS_ECONNABORTED WSAECONNABORTED
#define SYS_ECONNRESET WSAECONNRESET
#endif /* defined(_WIN32) && !defined(__CYGWIN__) */


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

static void local_sleep (int msec)
{
#ifdef _WIN32
  Sleep (msec);
#else  /* ! _WIN32 */
  struct timespec timeout;
  struct timespec remaining;

  timeout.tv_sec = msec / 1000;
  timeout.tv_nsec = (msec / 1000) * 1000;
  while (0 != nanosleep(&timeout, &remaining))
  {
    if (EINTR != errno)
      break;
    timeout.tv_sec = remaining.tv_sec;
    timeout.tv_nsec = remaining.tv_nsec;
  }
#endif /* ! _WIN32 */
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
      int err = SYS_sock_errno;
      fprintf (stderr, "Network error: %u\n",
               (unsigned)err);
      SYS_set_sock_errno (err);
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

enum comm_complexity
{
  COMM_SIMPLE          = 0,
  COMM_DELAY           = 1,
  COMM_ADD_DATA        = 2,
  COMM_ADD_DATA_TCPNDL = 3,
  COMM_DELAY_ADD_DATA  = 4,
  COMM_ADD_DATA_DELAY  = 5
};

const char* str_complexity (enum comm_complexity cmpl)
{
  switch (cmpl)
  {
  case COMM_SIMPLE:
    return "without additional delay or additional data";
  case COMM_DELAY:
    return "with additional delay before final receive";
  case COMM_ADD_DATA:
    return "with sending additional data before final receive";
  case COMM_ADD_DATA_TCPNDL:
    return "with sending additional data with TCP_NODELAY before final receive";
  case COMM_DELAY_ADD_DATA:
    return "with additional delay and sending additional data before final receive";
  case COMM_ADD_DATA_DELAY:
    return "with sending additional data and additional delay before final receive";
  }
  return "with some UNKNOWN method";
}

int do_communicate (const struct sockaddr_in *sock_addr_p, socklen_t addrlen, enum comm_complexity how)
{
  int result = 0;
  char *buf;
  static const size_t buf_size = 10 * 1024;

  buf = malloc (buf_size + 1); /* Extra byte for null-termination */
  if (NULL == buf)
  {
    result = 99;
    fprintf (stderr, "Error allocating memory for buffer.\n");
  }
  else
  {
    SYS_socket clnt_s;
    if (!sock_addr_p)
      return 99;
    clnt_s = socket (AF_INET, SOCK_STREAM, 0);
    if (SYS_INVALID_SOCKET == clnt_s)
    {
      fprintf (stderr, "Can't create socket: %u\n",
               (unsigned)SYS_sock_errno);
      result = 99;
    }
    else
    {
      char *str_addr;
      printf ("\nTrying communication sequence with server_part %s.\n", str_complexity (how));
      str_addr = inet_ntoa (sock_addr_p->sin_addr);
      if (!str_addr)
        str_addr = "server_part";

      printf ("Connecting to %s...\n", str_addr);
      if (connect (clnt_s, (const struct sockaddr*)sock_addr_p, addrlen) < 0)
      {
        fprintf (stderr, "Failed to connect: %u\n",
                 (unsigned)SYS_sock_errno);
        result = 99;
      }
      else
      {
        static const char data1[] = "FIRST PING\n";
        printf ("Connected. Sending \"FIRST PING\".\n");
        if (!send_str (clnt_s, data1, sizeof (data1) - 1))
        {
          result = 1;
          fprintf (stderr, "Error sending data.\n");
        }
        else
        {
          char *rcv_ptr;
          char *processed_ptr;
          const char *nl_ptr;

          rcv_ptr = buf;
          processed_ptr = buf;
          printf ("Sent. Waiting for receive \"FIRST PONG\".\n");
          nl_ptr = recv_line (clnt_s, &rcv_ptr, buf_size - (rcv_ptr - buf));
          if (!nl_ptr)
          {
            result = 1;
            fprintf (stderr, "Error receiving \"FIRST PONG\".\n");
          }
          else
          {
            rcv_ptr[0] = 0; /* Null-terminate for safety */
            printf ("Received: %*.*s\n", (int)(nl_ptr - processed_ptr), (int)(nl_ptr - processed_ptr), processed_ptr);
            if (!cmp_to_lower (processed_ptr, nl_ptr - processed_ptr, "first pong"))
            {
              result = 1;
              fprintf (stderr, "Expected marker not found in received data.\n");
            }
            else
            {
              static const char data2[] = "SECOND PING\n";
              processed_ptr = rcv_ptr;
              printf ("Sending \"SECOND PING\".\n");
              if (!send_str (clnt_s, data2, sizeof (data2) - 1))
              {
                result = 1;
                fprintf (stderr, "Error sending data.\n");
              }
              else
              {
                printf ("Sent.\n");
                if (how == COMM_DELAY || how == COMM_DELAY_ADD_DATA)
                {
                  printf ("Idling for delayed response receive...\n");
                  local_sleep (1500);
                }
                if (how == COMM_ADD_DATA || how == COMM_DELAY_ADD_DATA ||
                    how == COMM_ADD_DATA_TCPNDL || how == COMM_ADD_DATA_DELAY)
                {
                  static const char data_ign[] = "Additional data to be ignored.\n";
                  int on_val, off_val;

                  on_val = 1;
                  off_val = 0;
                  if (how == COMM_ADD_DATA_TCPNDL &&
                      0 != setsockopt (clnt_s, IPPROTO_TCP, TCP_NODELAY, (void*)&on_val, sizeof (on_val)))
                  {
                    result = 99;
                    fprintf(stderr,"Can't set TCP_ONDELAY option: %d.\n",
                            (unsigned)SYS_sock_errno);
                  }
                  printf ("Sending additional ignorable data.\n");
                  if (!send_str (clnt_s, data_ign, sizeof (data_ign) - 1))
                  {
                    printf ("Sending data failed.\n");
                  }
                  else
                  {
                    printf ("Sent.\n");
                  }
                }
                if (how == COMM_ADD_DATA_DELAY)
                {
                  printf ("Idling after additional sending for delayed response receive...\n");
                  local_sleep (1500);
                }
                printf ("Waiting for \"SECOND PONG\".\n");
                SYS_reset_sock_errno ();
                nl_ptr = recv_line (clnt_s, &rcv_ptr, buf_size - (rcv_ptr - buf));
                if (!nl_ptr)
                {
                  int err = SYS_sock_errno;
                  result = 1;
                  fprintf (stderr, "Error receiving \"SECOND PONG\".\n");
                  if (SYS_ECONNABORTED == err || SYS_ECONNRESET == err)
                    fprintf (stderr, "Looks like any received data was lost in system on this host after this system detected that connection was closed by remote side.\n");
                }
                else
                {
                  rcv_ptr[0] = 0; /* Null-terminate for safety */
                  printf ("Received: %*.*s\n", (int)(nl_ptr - processed_ptr), (int)(nl_ptr - processed_ptr), processed_ptr);
                  if (!cmp_to_lower (processed_ptr, nl_ptr - processed_ptr, "second pong"))
                  {
                    result = 1;
                    fprintf (stderr, "Expected marker not found in received data.\n");
                  }
                  else
                  {
                    printf ("Sequence completed.\n");
                  }
                }
              }
            }
          }
        }
      }
    }

    printf ("Closing connection.\n");
    SYS_socket_close_ (clnt_s);
  }

  if (result)
    fprintf (stderr, "Completed with some error.\n");
  else
    printf ("Completed without errors.\n");

  return result;
}

int do_client (in_addr_t srv_ip_addr)
{
  int result = 0;
  int total_res = 0;
  struct sockaddr_in sock_addr;
  const socklen_t addrlen = sizeof(sock_addr);
  memset (&sock_addr, 0, sizeof (sock_addr));

  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons (USE_PORT);
  sock_addr.sin_addr.s_addr = srv_ip_addr;
  result = do_communicate (&sock_addr, addrlen, COMM_SIMPLE);
  total_res += result;
  if (result <= 1)
  {
    result = do_communicate (&sock_addr, addrlen, COMM_DELAY);
    total_res += result;
    if (result <= 1)
    {
      result = do_communicate (&sock_addr, addrlen, COMM_ADD_DATA);
      total_res += result;
      if (result <= 1)
      {
        result = do_communicate (&sock_addr, addrlen, COMM_ADD_DATA_TCPNDL);
        total_res += result;
        if (result <= 1)
        {
          result = do_communicate (&sock_addr, addrlen, COMM_DELAY_ADD_DATA);
          total_res += result;
          if (result <= 1)
          {
            result = do_communicate (&sock_addr, addrlen, COMM_ADD_DATA_DELAY);
            total_res += result;
          }
        }
      }
    }
  }

  return total_res;
}

void print_usage (const char *prog_name)
{
  printf ("Usage:\n"
          "\t%s [xxx.xxx.xxx.xxx]\n"
          "where xxx.xxx.xxx.xxx is server_part IPv4 address.\n", prog_name ? prog_name : "client_part");
}

int main (int argc, char *argv[])
{
  int result = 0;
  in_addr_t srv_ip_addr = (in_addr_t)-1;

  if (!init_libs ())
  {
    fprintf (stderr, "Can't initialize libs.\n");
    return 99;
  }

  if (argc < 2)
  {
    srv_ip_addr = htonl (INADDR_LOOPBACK);
    printf ("No server_part address is specified. Using loopback address.\n");
  }
  else if ((in_addr_t)-1 == (srv_ip_addr = inet_addr (argv[1])) || (in_addr_t)0 == srv_ip_addr)
  {
    result = 99;
    fprintf (stderr, "\"%s\" is not valid IPv4 address\n", argv[1]);
    print_usage (argc > 0 ? argv[0] : NULL);
  }

  if (0 == result)
    result = do_client (srv_ip_addr);

  if (result)
    fprintf (stderr, "\nFinished with some error.\n");
  else
    printf ("\nFinished without any errors.\n");

  deinit_libs ();
  return result;
}

