#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

inline int lwip_socket(int d, int t, int p) { return socket(d, t | SOCK_CLOEXEC, p); }
inline int lwip_bind(int s, const sockaddr* a, socklen_t n) { return bind(s, a, n); }
inline int lwip_connect(int s, const sockaddr* a, socklen_t n) { return connect(s, a, n); }
inline int lwip_close(int s) { return close(s); }
inline int lwip_fcntl(int s, int cmd, int val) { return fcntl(s, cmd, val); }
inline int lwip_select(int n, fd_set* r, fd_set* w, fd_set* e, timeval* t) { return select(n, r, w, e, t); }
inline int lwip_setsockopt(int s, int l, int o, const void* v, socklen_t n) { return setsockopt(s, l, o, v, n); }
inline int lwip_getsockopt(int s, int l, int o, void* v, socklen_t* n) { return getsockopt(s, l, o, v, n); }
inline int lwip_recv(int s, void* b, size_t n, int f) { return static_cast<int>(recv(s, b, n, f)); }
inline int lwip_send(int s, const void* b, size_t n, int f) { return static_cast<int>(send(s, b, n, f | MSG_NOSIGNAL)); }
inline int lwip_recvfrom(int s, void* b, size_t n, int f, sockaddr* a, socklen_t* l) {
  return static_cast<int>(recvfrom(s, b, n, f, a, l));
}
inline int lwip_sendto(int s, const void* b, size_t n, int f, const sockaddr* a, socklen_t l) {
  return static_cast<int>(sendto(s, b, n, f | MSG_NOSIGNAL, a, l));
}
