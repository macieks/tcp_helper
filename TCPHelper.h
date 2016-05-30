/**
 *	@struct TCPHelper
 *	@brief TCP socket helper used to send and receive whole packets (or fail ) which is normally not supported by TCP sockets.
 *
 *	@note for all TCPHelper functions that return socketErr:
 *		- returned socketErr is always either 0 or negative
 *		- 0 indicates no critical socket error (the socket is still valid)
 *		- when TCPHelper_Send or TCPHelper_Recv fails and socketErr is 0 it means there was not enough room in either:
 *			- custom buffer
 *			- internal socket buffer (got wouldblock error from send/recv)
 *		- negative numbers are standard socket errors
 */
struct TCPHelper;

//! Creates TCP packet socket helper
TCPHelper* TCPHelper_Create(int sendBufferSize, int recvBufferSize);
//! Destroys TCP packet socket helper
void TCPHelper_Destroy(TCPHelper* socket);

//! Resets TCP packet socket helper with socket handle
void TCPHelper_Reset(TCPHelper* socket, int sock);
//! Gets socket handle
int TCPHelper_GetSocket(TCPHelper* socket);
//! Tells whether there was some critical socket error
bool TCPHelper_IsError(TCPHelper* socket, int& socketErr);

//! Sends the data; on success, whole buffer is sent, on failure nothing is sent; note, apart from calling TCPHelper_Send, it's required to call TCPHelper_TickSend regularly to make sure all (buffered) messages are actually sent
bool TCPHelper_Send(TCPHelper* socket, const void* data, int dataSize, int& socketErr);
//! Ticks internal sending of pending messages
void TCPHelper_TickSend(TCPHelper* socket, int& socketErr);

//! Tells whether TCPHelper_Recv with specific number of bytes would succeed
bool TCPHelper_WouldRecv(TCPHelper* socket, int numBytes, int& socketErr);
//! Receives the data; on success whole buffer is received, on failure nothing is received
bool TCPHelper_Recv(TCPHelper* socket, void* buffer, int numBytes, int& socketErr);