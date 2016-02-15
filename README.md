# **Check System Sockets recv() last**

Sockets implementations not always perfect and some of them may be broken in various ways. This test suite check ability to correctly retrieve last received data from system after remote side closed TCP connection.

### Detailed description of possible bug in sockets implementation
Consider following scenario: TCP connection is established between __Host A__ and __Host B__. Application on __Host A__ sends some data to __Host B__ and closed connection from side of __Host A__. On __Host B__ application send some data to __Host A__ and use `recv()` only **after** sending. With correct socket implementation, last data, received from __Host A__, must be returned with this call of `recv()`. With incorrect socket implementation, __Host B__ detects closure of connection by remote side when sending data and starts return error on **any** socket operation, including `recv()`, while received data is present in system socket buffers.

A real world example: HTTP client sending a POST request (request header + request body) to HTTP server. HTTP server detects incorrect header (or unsupported options) after HTTP header is received (request body is still transmitting). HTTP server send 400/406 or 501 error response to client and closed connection. HTTP client in the same time continue to sending request body and starts reading from system received data only after sending full request. If sockets are implemented incorrectly, server response is received by client's system but never returned to client application after system detects remote closure of connection. So application (HTTP client) got only system error "socket was closed by remote side" instead of properly received server response. Correct sockets implementation returns received data even after detection of TCP connection closure by remote side.

### Building
Build `server_part` and `clint_part` by Visual Studio 2015 or later or by system `make` (compiler and linker must be installed). Nmake is not supported - use `check_system_socket_recv_last.sln` solution or MinGW/Cygwin `make`.

### OSes tested
Several combinations of OSes were tested. Current test results:

* **FAILED** - `client_part` running on Windows; `server_part` running on any of: Windows, Linux, FreeBSD, Darwin
* **SUCCEED** - `client_part` running on any of: Linux, FreeBSD, Darwin; `server_part` running on any of: Windows, Linux, FreeBSD, Darwin