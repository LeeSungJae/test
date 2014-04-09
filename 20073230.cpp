#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>


// ����� ��� ( ������ ��� )
//#define		DEBUG_MODE


// ������ ��Ʈ ��ȣ
#define		_SERVER_PORT_NUM_		5000
// ���� ���� ������
#define		_BUFF_SIZE_				1024
// ���ŵ� �������� �ش� ������
#define		_SOCKET_HEADERSIZE_		8
// ť�� ������
#define		_SOCKET_MAXQUEUE_		102400
// ���� ���� ���Ѽ�
#define		_MAX_CONNECT_USER_		100
// IP Address size
#define		_SOCKET_IPADDRESS_SIZE_	16
// Port 4 Byte �� IPAddress 16 Byte�� ��ģ ��
#define		_PORT_IPADDRESS_SIZE_	20
// ID�� ���̴� 100 BYTE ����
#define		_MAX_IDLENGTH_			100

// �츮 ���� ������ ���
void PrintProfile();
// ���� ���� ����
int	CreateServerSocket();
// ���� ���Ͽ� Ŭ���̾�Ʈ ���� ó��
int ConnectClient( int iServSock );
// ���� �ַ� �ڵ鸵
void error_handling(char *message);
// ������� - ������ �з�
void DevideReceiveSocketData( int iFD, char *sData, int iLength );
// ������� - ������ ó��
void ProcUserMessage( int iFD, int iHeader, int iDataSize, char *cRecvData );
// Ŭ���̾�Ʈ���� �޼��� ��Ŷ ����
int SendPacket( int iFD, int iHeader, int iDataSize, char *cData );
// ������ ���� ��ɾ �Է����� ��
void ServerCommandInput();
// ���� ����
void ServerClose();
// ��ɾ� ������ �����ش�.
void CommandHelp();
// ID List ���� ID �� ã�� ä���� �� �� �ֵ��� �޼����� ������.
void IDFindandConnect( char *cID, char *cSendData );

// ť�� ó���� ������
char	*cQueue;
int		iQueueLength;

// ���ӵ� ���� ���̵� ��� ����
char	*cUserId[_MAX_CONNECT_USER_];			// ���̵�
int		iUserIdLen[_MAX_CONNECT_USER_];			// ���̵� ����
int		iUserFD[_MAX_CONNECT_USER_];			// ���� �ѹ�
int		iIDPosition;							// �δ콺
fd_set	fdReadEvent;							// ���� �̺�Ʈ�� ó���ϱ� ����	
int		ifdMax;
int		iServSock;								// ���� ����

// ���� - Ŭ���̾�Ʈ �������� ����
typedef enum
{
	LOGIN_ID = 1,							// ID ����
	LOGIN_OK,								// ���� ����

	WANT_IDLIST,							// ID List ��û
	RECV_IDLIST,							// ID List ���
	
	CREATE_CHAT,							// ���� ����� ���� ���� ��û
	RECV_CREATE_CHAT,						// ��û ���

	JOIN_CHAT,								// �������� �ش� Ŭ���̾�Ʈ�� �����϶�� ��û
	RECV_JOIN_CHAT,							// Ŭ���̾�Ʈ���� ���� ��û�� ����

	RETURN_PORT,							// ä���� ����Ǿ� ��Ʈ��ȣ ��ȯ
	SEND_ALL_MESSAGE,						// ��ü �޼��� ������

	CONNECT_NEW_USER,						// ���ο� �����ڰ� ���ܼ� ���̵� ������
	DISCONNECT_USER,						// �����ڰ� ������ �����Ͽ��� ��
	
	SEND_ALLMESSAGE,						// ������ ��ü���� �˾��Ż����� ������.
} emStoCPROCTOCOL;

int main()
{
	// ���� �̺�Ʈ�� ó���ϱ� ����	
	fd_set				fdTempEvent;
	// ������ ������ Ŭ���̾�Ʈ�� �Ҵ����ִ� ����
	int					iClientSock;
	// ���� ���� ����
	char				cRecvMsg[_BUFF_SIZE_];
	int					iRecvSize;
	// �߻��� �̺�Ʈ �׸� ID
	int					iFD;
	

	// ������ ���
	PrintProfile();
	CommandHelp();
	
	// ���� ����
	iServSock = CreateServerSocket();

	// ť�� �ʱ�ȭ
	cQueue = new char[ _SOCKET_MAXQUEUE_ ];
	memset( cQueue, 0, _SOCKET_MAXQUEUE_ );
	iQueueLength = 0;

	// ID List �δ콺 �ʱ�ȭ
	memset( iUserIdLen, 0, sizeof(int) * _MAX_CONNECT_USER_ );
	memset( iUserFD, 0, sizeof(int) * _MAX_CONNECT_USER_ );
	iIDPosition = 0;
	
	// �̺�Ʈ �׸� ����
	FD_ZERO( &fdReadEvent );
	FD_SET( fileno(stdin), &fdReadEvent );
	FD_SET( iServSock, &fdReadEvent );
	ifdMax = iServSock;

	// ���η���
	while(1)
	{	
		// �̺�Ʈ�� ������ ��츦 ����Ͽ�
		fdTempEvent = fdReadEvent;

		// ����� �̺�Ʈ�� ����
		select ( ifdMax+1, &fdTempEvent, 0, 0, NULL );
		
		// ������ ���� ��� �׸��� �̺�Ʈ �߻� ���� �˻�
		for ( iFD=0;iFD<ifdMax+1;iFD++ )
		{
			// �̺�Ʈ�� �߻��ϸ� TRUE
			if ( FD_ISSET( iFD, &fdTempEvent ) )
			{
				// ������ ���� ��ɾ �Է��� ���
				if ( iFD == fileno(stdin) )
				{
					ServerCommandInput();
				}
				// ���� ��û�� ���
				else if ( iFD == iServSock ) 
				{
					// ���� ���Ͽ� Ŭ���̾�Ʈ ���� ó��
					iClientSock = ConnectClient( iFD );

					// ���� ó���� ���ϵ� �̺�Ʈ �׸� ����
					FD_SET( iClientSock, &fdReadEvent );

					// �̺�Ʈ �׸��� �ִ밪 ����
					if( ifdMax < iClientSock )
						ifdMax = iClientSock;
				}
				// �� �ܿ��� ���ӵ� Ŭ���̾�Ʈ�� ������ ��ɾ���.
				else
				{
					// ���� ���۸� �ʱ�ȭ �ϰ�
					memset( cRecvMsg, '\0', _BUFF_SIZE_ );

					// �����Ѵ�.
					iRecvSize = read( iFD, cRecvMsg, _BUFF_SIZE_ );

					// ���� ũ�Ⱑ 0 �̸� ���� �����̴�.
					if( iRecvSize <= 0 ) 
					{ 
#if defined( DEBUG_MODE )
						printf( "Close Socket %d\n", iFD );
#endif
						// ���� ���� ó��
						int		iFDPos, jFDPos;
						for ( iFDPos=0;iFDPos<iIDPosition;iFDPos++ )
						{
							if ( iUserFD[iFDPos] == iFD )
							{
								ProcUserMessage( iFD, DISCONNECT_USER, iUserIdLen[iFDPos], cUserId[iFDPos] );
								for ( jFDPos=1;jFDPos<iIDPosition - iFDPos;jFDPos++ )
								{
									iUserIdLen[iFDPos + jFDPos - 1]	= iUserIdLen[iFDPos + jFDPos];
									cUserId[iFDPos + jFDPos - 1]		= cUserId[iFDPos + jFDPos];
									iUserFD[iFDPos + jFDPos - 1]		= iUserFD[iFDPos + jFDPos];
								}
								break;
							}
						}
						
						// ���� ���� ó�� �Ǿ��ٴ� �ǹ�
						if ( iFDPos != iIDPosition )
							iIDPosition--;

						FD_CLR( iFD, &fdReadEvent );
						close( iFD );

						continue;
					}
					
#if defined( DEBUG_MODE )
					printf( "Recv Data [ Size %08d ]\n", iRecvSize );
#endif

					// ���ŵ� ������ ó��
					DevideReceiveSocketData( iFD, cRecvMsg, iRecvSize );
				}
			}
		}
#if defined( DEBUG_MODE )
		printf( "Loop End\n" );
		printf( "\n" );
#endif		
	}
	
	// ���α׷� ����� �޸� ��ȯ
	ServerClose();
}

// ���� ������ ���
void PrintProfile()
{
	printf( "Software Architecture\n" );
	printf( "\t20073230 �̼���\n" );
	//printf( "\n" );
	printf( "Make Server Prog\n" );
	printf( "\t20073223 �̱���\n" );
	//printf( "\n" );
	printf( "Prog Design & Document\n" );
	printf( "\t20103391 ����ȯ\n" );
	printf( "Make UserProg by MFC\n" );
	printf( "\tGOP ( Great Over Power )\n" );
	printf( "\n\n" );
}

// ��ɾ� ������ �����ش�.
void CommandHelp()
{
	printf( "@help\t\t �� �׸��� �ٽ� ����.\n" );
	printf( "@quit\t\t �ý����� �����Ѵ�.\n" );
	printf( "@idlist\t\t ���̵𸮽�Ʈ�� ����Ѵ�.\n" );
	printf( "@resetidlist\t ��� ������ �����ϰ� ���̵𸮽�Ʈ�� �ʱ�ȭ�Ѵ�.\n\n" );
}

// ������ IP �� �ʿ� ����, PORT NUM�� ������ ������ �����Ѵ�. 
int	CreateServerSocket()
{
	int							iServSock;
	struct sockaddr_in			stServAddr;
	  
	iServSock					= socket( PF_INET, SOCK_STREAM, 0 );
	stServAddr.sin_family		= AF_INET;
	stServAddr.sin_addr.s_addr	= htonl( INADDR_ANY );
	stServAddr.sin_port			= htons( _SERVER_PORT_NUM_ );

	if( bind( iServSock, (struct sockaddr *)&stServAddr, sizeof(stServAddr) ) )
		error_handling( "bind() error" );
	if( listen( iServSock, 5 ) == -1 )
		error_handling( "listen() error" );

	return iServSock;
}

// ���� ���Ͽ� Ŭ���̾�Ʈ ����
int ConnectClient( int iServSock )
{
	int							iClientSock;
	int							iClientLen;
	struct sockaddr_in			stClientddr;

	iClientLen = sizeof(stClientddr);
	iClientSock = accept( iServSock, (struct sockaddr *)&stClientddr, &iClientLen );

#if defined( DEBUG_MODE )
	printf("Connect Socket %d \n", iClientSock);
#endif

	return iClientSock;
}

// �ַ� ó��
void error_handling(char *message)
{
	printf( "%s\n", message );
	exit(1);
}

// �������κ��� ���� ������ �м� 
void DevideReceiveSocketData( int iFD, char *sData, int iLength )
{
	int				iPacketHeader;
	int				iDataSize;
	char			*wkRecv;

	memcpy( &cQueue[ iQueueLength ], sData, iLength );
	iQueueLength += iLength;

	do
	{
		// ����� ����
		memcpy( &iPacketHeader, cQueue, sizeof(int) );
		// ��Ŷ�� ũ�⸦ ����
		memcpy( &iDataSize, &cQueue[sizeof(int)], sizeof(int) );

		if ( iDataSize <= iQueueLength - _SOCKET_HEADERSIZE_ ) 
		{
			wkRecv = new char[ iDataSize ];

			// ������ ��ü�� ����
			memset( wkRecv, '\0', iDataSize );
			memcpy( wkRecv, &cQueue[ _SOCKET_HEADERSIZE_ ], iDataSize );

			// ���� ��ü�� �б⿡ ������ ���
			iQueueLength -= iDataSize + _SOCKET_HEADERSIZE_;

			// �޼��� ó��
			ProcUserMessage( iFD, iPacketHeader, iDataSize, wkRecv );

			delete[] wkRecv;
#if defined( DEBUG_MODE )
			printf( "Residual [ Size %08d ]\n\n", iQueueLength );
#endif
		}
	} while ( iDataSize <= iQueueLength - _SOCKET_HEADERSIZE_ ); // end of while
}	// End of CMainFrame::DevideReceiveSocketData

void ProcUserMessage( int iFD, int iHeader, int iDataSize, char *cRecvData )
{
	switch( iHeader )
	{
	// ID ����
	case LOGIN_ID:
		{
			// ���� ���� ���̵� ����
			cUserId[iIDPosition]		= new char[ iDataSize + 1];
			memset( cUserId[iIDPosition], 0, iDataSize + 1 );
			memcpy( cUserId[iIDPosition], cRecvData, iDataSize );
			iUserIdLen[iIDPosition]		= iDataSize + 1;
			iUserFD[iIDPosition]		= iFD;
			iIDPosition++;
			
#if defined( DEBUG_MODE )
			printf( "Insert User ID [ %s ]\n", cUserId[iIDPosition - 1] );
#endif

			// �������� ���� ���� �޼��� ����
			SendPacket( iFD, LOGIN_OK, 0, NULL );

#if defined( DEBUG_MODE )
			printf( "Send Message [ Length %08d, %s ]\n", iUserIdLen[iIDPosition-1], cUserId[iIDPosition-1] );
#endif

			// ���� ������ ���� ���̵� ��ο��� �˷��ش�.
			int				iIDPos;
			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
			{
				if ( iUserFD[iIDPos] == iFD )
					continue;

				SendPacket( iUserFD[iIDPos], CONNECT_NEW_USER, iUserIdLen[iIDPosition-1], cUserId[iIDPosition-1] );
			}
		}
		break;

	// ���� ����
	case LOGIN_OK:
		{
		}
		break;

	// ID List ��û
	case WANT_IDLIST:
		{
			int				iIDPos;
			int				iIDRealSize = 0;
			int				iBytePos = 0;
			char			*cSendMessage;

			// �����ؾߵǴ� ��ü ����� ����.
			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
				iIDRealSize += iUserIdLen[iIDPos] + sizeof(int);

			cSendMessage = new char[ iIDRealSize ];
			memset( cSendMessage, 0, iIDRealSize );

			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
			{
				memcpy( &cSendMessage[iBytePos], &iUserIdLen[iIDPos], sizeof(int) );
				iBytePos += sizeof(int);
				memcpy( &cSendMessage[iBytePos], cUserId[iIDPos], iUserIdLen[iIDPos] );
				iBytePos += iUserIdLen[iIDPos];
			}

			SendPacket( iFD, RECV_IDLIST, iIDRealSize, cSendMessage );

			delete[] cSendMessage;
		}
		break;

	// ID List ���
	case RECV_IDLIST:
		{
		}
		break;

	// ���� ����� ���� ���� ��û
	case CREATE_CHAT:
		{
		}
		break;

	// ��û ���
	case RECV_CREATE_CHAT:
		{
		}
		break;

	// �������� �ش� Ŭ���̾�Ʈ�� �����϶�� ��û
	case JOIN_CHAT:
		{
		}
		break;

	// Ŭ���̾�Ʈ���� ���� ��û�� ����
	case RECV_JOIN_CHAT:
		{
#if 0
	#if defined( DEBUG_MODE )
				printf( "\t\tRECV_JOIN_CHAT Message\n" );
	#endif
				char	cPortandIPAddress[ _PORT_IPADDRESS_SIZE_ ];
				int		iCheckByte = 0;
			
				memcpy( cPortandIPAddress, cRecvData, _PORT_IPADDRESS_SIZE_ );
				iCheckByte	+= _PORT_IPADDRESS_SIZE_;
			
	#if defined( DEBUG_MODE )
				char	cIPAddress[ _SOCKET_IPADDRESS_SIZE_ ];
				memcpy( cIPAddress, &cPortandIPAddress[ sizeof(int) ], _SOCKET_IPADDRESS_SIZE_ );
				printf( "\t\tGet IP Address [ %s ]\n", cIPAddress );
	#endif

				int		iIDLength;
				char	cGetID[100];

				do
				{
					iIDLength = 0;
					memset( cGetID, 0, 100 );

					memcpy( &iIDLength, &cRecvData[iCheckByte], sizeof(int) );
					iCheckByte += sizeof(int);
				
	#if defined( DEBUG_MODE )
					printf( "\t\tIDLength [ %08d ]\n", iIDLength );
	#endif
					memcpy( cGetID, &cRecvData[iCheckByte], iIDLength );
					iCheckByte += iIDLength;
				
	#if defined( DEBUG_MODE )
					printf( "\t\tGetID [ %s ]\n", cGetID );
	#endif

					IDFindandConnect( cGetID, cPortandIPAddress )
			} while ( iDataSize > iCheckByte );


#else


			int		iIDLength;
			char	cGetID[ _MAX_IDLENGTH_ ];
			char	cPortandIPAddress[ _PORT_IPADDRESS_SIZE_ ];
							
			iIDLength = 0;
			memset( cGetID, 0, 100 );
			memset( cPortandIPAddress, 0, _PORT_IPADDRESS_SIZE_ );

			memcpy( cPortandIPAddress, cRecvData, _PORT_IPADDRESS_SIZE_ );	
			memcpy( &iIDLength, &cRecvData[_PORT_IPADDRESS_SIZE_], sizeof(int) );
			memcpy( cGetID, &cRecvData[_PORT_IPADDRESS_SIZE_ + sizeof(int)], iIDLength );
				
#if defined( DEBUG_MODE )
			printf( "\t\tRECV_JOIN_CHAT\n" );
			printf( "\t\tID [ %s ]\n", cGetID );
#endif

			IDFindandConnect( cGetID, cPortandIPAddress );
#endif
		}
		break;

	// ä���� ����Ǿ� ��Ʈ��ȣ ��ȯ
	case RETURN_PORT:
		{
		}
		break;

	// ��ü �޼��� ������
	case SEND_ALL_MESSAGE:
		{
		}
		break;
		
	// ���ο� �����ڰ� ���ܼ� ���̵� ������
	case CONNECT_NEW_USER:
		{
		}
		break;
		
	// �����ڰ� ������ ������ ��
	case DISCONNECT_USER:
		{
			// ���� ������ ���� ���̵� ��ο��� �˷��ش�.
			int				iIDPos;
			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
				SendPacket( iUserFD[iIDPos], DISCONNECT_USER, iDataSize, cRecvData );
		}
		break;

	case SEND_ALLMESSAGE:
		{
			// ���� ������ ���� ���̵� ��ο��� �˷��ش�.
			int				iIDPos;
			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
				SendPacket( iUserFD[iIDPos], SEND_ALLMESSAGE, iDataSize, cRecvData );
		}
		break;
	}
}

// ID List ���� ID �� ã�� ä���� �� �� �ֵ��� �޼����� ������.
void IDFindandConnect( char *cID, char *cSendData )
{
	int	iFD;
	for ( iFD=0;iFD<iIDPosition;iFD++ )
	{
#if defined( DEBUG_MODE )
		printf( "Search ID [ %s %s ]\n", cUserId[iFD], cID );
#endif
		if ( strncmp( cUserId[iFD], cID, iUserIdLen[iFD] ) == 0 )
			SendPacket( iUserFD[iFD], RECV_JOIN_CHAT, _PORT_IPADDRESS_SIZE_, cSendData );
	}
}

// Ŭ���̾�Ʈ���� �޼��� ��Ŷ ����
int SendPacket( int iFD, int iHeader, int iDataSize, char *cData )
{
	int iRet = 0;
	
	char	*cSendData = new char[ _SOCKET_HEADERSIZE_ + iDataSize ];
	memset( cSendData, '\0', _SOCKET_HEADERSIZE_ + iDataSize );

	memcpy( cSendData, &iHeader, sizeof( int ) );
	memcpy( &cSendData[4], &iDataSize, sizeof( int ) );
	memcpy( &cSendData[8], cData, iDataSize );
	
	iRet = send( iFD, cSendData, _SOCKET_HEADERSIZE_ + iDataSize, 0 );

#if defined( DEBUG_MODE )
	printf( "Send Message = [ Header %04d, Size %08d, Send = %08d ]\n", iHeader, iDataSize, iRet );
	printf( "\t\tSend Text [ %s ]\n", cData );
#endif

	delete[] cSendData;

	return iRet;
}


// ������ ���� ��ɾ �Է����� ��
// ���� �� �ϼ�
void ServerCommandInput()
{
	char	cInputMessage[ _BUFF_SIZE_ ];
	memset( cInputMessage, 0, _BUFF_SIZE_ );
	fgets( cInputMessage, _BUFF_SIZE_, stdin );
					
	if ( strncmp( cInputMessage, "@talk", 5 ) == 0 )
	{
	}
	else if ( strncmp( cInputMessage, "@quit", 5 ) == 0 )
	{	
		ServerClose();
	}
	else if ( strncmp( cInputMessage, "@resetidlist", 12 ) == 0 )
	{
		int iIDPos;
		for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
			close( iUserFD[iIDPos] );
	
		// �̺�Ʈ �׸� ����
		FD_ZERO( &fdReadEvent );
		FD_SET( fileno(stdin), &fdReadEvent );
		FD_SET( iServSock, &fdReadEvent );
		ifdMax = iServSock;

		iIDPosition = 0;
		printf( "     Complete Reset id list...\n" );
		printf( "\n" );
	} 
	else if ( strncmp( cInputMessage, "@idlist", 7 ) == 0 )
	{
		int		iIDPos;

		if ( !iIDPosition )
			printf( "[ ]\n" );

		for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
			printf( "[ Num %04d \t Length %08d \t %s ]\n", iIDPos + 1, iUserIdLen[iIDPos], cUserId[iIDPos] );

		printf( "\n" );
	}
	else if ( strncmp( cInputMessage, "@help", 5 ) == 0 )
	{
		CommandHelp();
	}
}

void ServerClose()
{
	delete[] cQueue;

	int		iIDPos;
	for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
		delete[] cUserId[iIDPos];
	
	exit(1);
}
