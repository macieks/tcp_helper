#include "TCPHelper.h"

bool socket_iswouldblock(int socketError)
{
#if defined(_WIN32)
	return socketError == WSAEWOULDBLOCK;
#else
	return socketError == EWOULDBLOCK || socketError == EAGAIN;
#endif
}

struct TCPHelper
{
	int m_socket;
	int m_lastError;

	int m_sendBufferSize;
	int m_sendBufferCapacity;
	char* m_sendBuffer;

	int m_recvBufferSize;
	int m_recvBufferCapacity;
	char* m_recvBuffer;
};

TCPHelper* TCPHelper_Create(int sendBufferSize, int recvBufferSize)
{
	char* memory = (char*) malloc(sizeof(TCPHelper) + sendBufferSize + recvBufferSize);
	if (!memory)
		return NULL;

	TCPHelper* socket = (TCPHelper*) memory;
	socket->m_socket = 0;
	socket->m_lastError = 0;
	memory += sizeof(TCPHelper);

	socket->m_sendBufferCapacity = sendBufferSize;
	socket->m_sendBufferSize = 0;
	socket->m_sendBuffer = sendBufferSize ? memory : NULL;
	memory += sendBufferSize;

	socket->m_recvBufferCapacity = recvBufferSize;
	socket->m_recvBufferSize = 0;
	socket->m_recvBuffer = recvBufferSize ? memory : NULL;

	return socket;
}

void TCPHelper_Destroy(TCPHelper* socket)
{
	free(socket);
}

void TCPHelper_Reset(TCPHelper* socket, int sock)
{
	socket->m_socket = sock;
	socket->m_lastError = 0;

	socket->m_sendBufferSize = 0;
	socket->m_recvBufferSize = 0;
}

int TCPHelper_GetSocket(TCPHelper* socket)
{
	return socket->m_socket;
}

bool TCPHelper_IsError(TCPHelper* socket, int& socketErr)
{
	socketErr = socket->m_lastError;
	return socketErr != 0;
}

bool TCPHelper_BufferSend(TCPHelper* socket, const void* data, int dataSize, int& socketErr)
{
	if (socket->m_sendBufferSize + dataSize > socket->m_sendBufferCapacity)
		return false;

	memcpy(socket->m_sendBuffer + socket->m_sendBufferSize, data, dataSize);
	return true;
}

bool TCPHelper_SendImmediately(TCPHelper* socket, const void* data, int dataSize, int& socketErr)
{
	// Send

	socketErr = send(socket->m_socket, data, dataSize, 0);
	if (socketErr < 0)
	{
		if (socket_iswouldblock(socketErr))
			socketErr = 0;
		else
			socket->m_lastError = socketErr;
		return false;
	}

	// If failed to send everything, put the rest into buffer

	if (socketErr < dataSize)
		memcpy(socket->m_sendBuffer + socket->m_sendBufferSize, (char*) data + socketErr, dataSize - socketErr);

	// Indicate no error

	socketErr = 0;
	return true;
}

bool TCPHelper_Send(TCPHelper* socket, const void* data, int dataSize, int& socketErr)
{
	assert(dataSize <= socket->m_sendBufferCapacity);

	if (socket->m_lastError)
	{
		socketErr = socket->m_lastError;
		return false;
	}

	// Send old messages

	TCPHelper_TickSend(socket, socketErr);
	if (socketErr < 0)
		return false;

	// Try to send immediately (only if there's no pending data)

	if (socket->m_sendBufferSize == 0)
		return TCPHelper_SendImmediately(socket, data, dataSize, socketErr);

	// Otherwise store data in intermediate buffer

	if (!TCPHelper_BufferSend(socket, data, dataSize, socketErr))
		return false;

	// Try sending again

	TCPHelper_TickSend(socket, socketErr);
	if (socketErr < 0)
		return false;

	return true;
}

void TCPHelper_TickSend(TCPHelper* socket, int& socketErr)
{
	if (socket->m_lastError)
	{
		socketErr = socket->m_lastError;
		return;
	}

	// Nothing to send?

	if (socket->m_sendBufferSize == 0)
	{
		socketErr = 0;
		return;
	}

	// Send

	socketErr = send(socket->m_socket, socket->m_sendBuffer, socket->m_sendBufferSize, 0);
	if (socketErr < 0)
	{
		if (socket_iswouldblock(socketErr))
			socketErr = 0;
		else
			socket->m_lastError = socketErr;
		return;
	}

	// Pop the rest of pending send data to the beginning of the buffer

	socket->m_sendBufferSize -= socketErr;
	if (socket->m_sendBufferSize > 0)
		memmove(socket->m_sendBuffer, socket->m_sendBuffer + socketErr, socket->m_sendBufferSize);

	// Indicate no error

	socketErr = 0;
}

bool TCPHelper_WouldRecv(TCPHelper* socket, int numBytes, int& socketErr)
{
	assert(numBytes <= socket->m_recvBufferCapacity);

	if (socket->m_lastError)
	{
		socketErr = socket->m_lastError;
		return false;
	}

	// Check if we've already received enough data

	if (numBytes <= socket->m_recvBufferSize)
		return true;

	// Attempt to receive the data

	socketErr = recv(socket->m_socket, socket->m_recvBuffer + socket->m_recvBufferSize, numBytes - socket->m_recvBufferSize, 0);
	if (socketErr < 0)
	{
		if (socket_iswouldblock(socketErr))
			socketErr = 0;
		else
			socket->m_lastError = socketErr;
		return false;
	}

	// Received some data

	socket->m_recvBufferSize += socketErr;
	return numBytes <= socket->m_recvBufferSize;
}

bool TCPHelper_Recv(TCPHelper* socket, void* buffer, int numBytes, int& socketErr)
{
	if (socket->m_lastError)
	{
		socketErr = socket->m_lastError;
		return false;
	}

	// Check if we'd receive all requested bytes

	if (!TCPHelper_WouldRecv(socket, numBytes, socketErr))
		return false;

	// Copy result

	memcpy(buffer, socket->m_recvBuffer, numBytes);

	// Pop the rest of pending received data to the beginning of the buffer

	socket->m_recvBufferSize -= numBytes;
	if (socket->m_recvBufferSize > 0)
		memmove(socket->m_recvBuffer, socket->m_recvBuffer + numBytes, socket->m_recvBufferSize);

	return true;
}