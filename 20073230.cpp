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


// 디버그 모드 ( 데이터 출력 )
//#define		DEBUG_MODE


// 서버의 포트 번호
#define		_SERVER_PORT_NUM_		5000
// 수신 버퍼 사이즈
#define		_BUFF_SIZE_				1024
// 수신된 데이터의 해더 사이즈
#define		_SOCKET_HEADERSIZE_		8
// 큐딩 사이즈
#define		_SOCKET_MAXQUEUE_		102400
// 유저 접속 제한수
#define		_MAX_CONNECT_USER_		100
// IP Address size
#define		_SOCKET_IPADDRESS_SIZE_	16
// Port 4 Byte 와 IPAddress 16 Byte를 합친 값
#define		_PORT_IPADDRESS_SIZE_	20
// ID의 길이는 100 BYTE 한정
#define		_MAX_IDLENGTH_			100

// 우리 팀원 프로필 출력
void PrintProfile();
// 서버 소켓 생성
int	CreateServerSocket();
// 서버 소켓에 클라이언트 접속 처리
int ConnectClient( int iServSock );
// 각종 애러 핸들링
void error_handling(char *message);
// 소켓통신 - 데이터 분류
void DevideReceiveSocketData( int iFD, char *sData, int iLength );
// 소켓통신 - 데이터 처리
void ProcUserMessage( int iFD, int iHeader, int iDataSize, char *cRecvData );
// 클라이언트에게 메세지 패킷 전송
int SendPacket( int iFD, int iHeader, int iDataSize, char *cData );
// 서버가 직접 명령어를 입력했을 때
void ServerCommandInput();
// 서버 종료
void ServerClose();
// 명령어 종류를 보여준다.
void CommandHelp();
// ID List 에서 ID 를 찾아 채팅을 할 수 있도록 메세지를 보낸다.
void IDFindandConnect( char *cID, char *cSendData );

// 큐딩 처리할 변수들
char	*cQueue;
int		iQueueLength;

// 접속된 유저 아이디 기억 변수
char	*cUserId[_MAX_CONNECT_USER_];			// 아이디
int		iUserIdLen[_MAX_CONNECT_USER_];			// 아이디 길이
int		iUserFD[_MAX_CONNECT_USER_];			// 소켓 넘버
int		iIDPosition;							// 인댁스
fd_set	fdReadEvent;							// 각종 이벤트를 처리하기 위한	
int		ifdMax;
int		iServSock;								// 서버 소켓

// 서버 - 클라이언트 프로토콜 정의
typedef enum
{
	LOGIN_ID = 1,							// ID 전송
	LOGIN_OK,								// 접속 성공

	WANT_IDLIST,							// ID List 요청
	RECV_IDLIST,							// ID List 결과
	
	CREATE_CHAT,							// 서버 만들기 위한 정보 요청
	RECV_CREATE_CHAT,						// 요청 결과

	JOIN_CHAT,								// 서버에게 해당 클라이언트를 접속하라고 요청
	RECV_JOIN_CHAT,							// 클라이언트에게 접속 요청을 받음

	RETURN_PORT,							// 채팅이 종료되어 포트번호 반환
	SEND_ALL_MESSAGE,						// 전체 메세지 보내기

	CONNECT_NEW_USER,						// 새로운 접속자가 생겨서 아이디를 전송함
	DISCONNECT_USER,						// 접속자가 접속을 종료하였을 때
	
	SEND_ALLMESSAGE,						// 접속자 전체에게 팝업매새지를 보낸다.
} emStoCPROCTOCOL;

int main()
{
	// 각종 이벤트를 처리하기 위한	
	fd_set				fdTempEvent;
	// 서버에 접속한 클라이언트를 할당해주는 소켓
	int					iClientSock;
	// 소켓 수신 버퍼
	char				cRecvMsg[_BUFF_SIZE_];
	int					iRecvSize;
	// 발생한 이벤트 항목 ID
	int					iFD;
	

	// 프로필 출력
	PrintProfile();
	CommandHelp();
	
	// 서버 생성
	iServSock = CreateServerSocket();

	// 큐딩 초기화
	cQueue = new char[ _SOCKET_MAXQUEUE_ ];
	memset( cQueue, 0, _SOCKET_MAXQUEUE_ );
	iQueueLength = 0;

	// ID List 인댁스 초기화
	memset( iUserIdLen, 0, sizeof(int) * _MAX_CONNECT_USER_ );
	memset( iUserFD, 0, sizeof(int) * _MAX_CONNECT_USER_ );
	iIDPosition = 0;
	
	// 이벤트 항목 설정
	FD_ZERO( &fdReadEvent );
	FD_SET( fileno(stdin), &fdReadEvent );
	FD_SET( iServSock, &fdReadEvent );
	ifdMax = iServSock;

	// 메인루프
	while(1)
	{	
		// 이벤트가 수정될 경우를 대비하여
		fdTempEvent = fdReadEvent;

		// 사용할 이벤트들 선택
		select ( ifdMax+1, &fdTempEvent, 0, 0, NULL );
		
		// 루프를 통해 모든 항목의 이벤트 발생 여부 검색
		for ( iFD=0;iFD<ifdMax+1;iFD++ )
		{
			// 이벤트가 발생하면 TRUE
			if ( FD_ISSET( iFD, &fdTempEvent ) )
			{
				// 서버가 직접 명령어를 입력한 경우
				if ( iFD == fileno(stdin) )
				{
					ServerCommandInput();
				}
				// 연결 요청인 경우
				else if ( iFD == iServSock ) 
				{
					// 서버 소켓에 클라이언트 접속 처리
					iClientSock = ConnectClient( iFD );

					// 접속 처리된 소켓도 이벤트 항목에 포함
					FD_SET( iClientSock, &fdReadEvent );

					// 이벤트 항목의 최대값 수정
					if( ifdMax < iClientSock )
						ifdMax = iClientSock;
				}
				// 그 외에는 접속된 클라이언트가 보내는 명령어임.
				else
				{
					// 수신 버퍼를 초기화 하고
					memset( cRecvMsg, '\0', _BUFF_SIZE_ );

					// 수신한다.
					iRecvSize = read( iFD, cRecvMsg, _BUFF_SIZE_ );

					// 수신 크기가 0 이면 접속 종료이다.
					if( iRecvSize <= 0 ) 
					{ 
#if defined( DEBUG_MODE )
						printf( "Close Socket %d\n", iFD );
#endif
						// 접속 종료 처리
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
						
						// 접속 종료 처리 되었다는 의미
						if ( iFDPos != iIDPosition )
							iIDPosition--;

						FD_CLR( iFD, &fdReadEvent );
						close( iFD );

						continue;
					}
					
#if defined( DEBUG_MODE )
					printf( "Recv Data [ Size %08d ]\n", iRecvSize );
#endif

					// 수신된 데이터 처리
					DevideReceiveSocketData( iFD, cRecvMsg, iRecvSize );
				}
			}
		}
#if defined( DEBUG_MODE )
		printf( "Loop End\n" );
		printf( "\n" );
#endif		
	}
	
	// 프로그램 종료시 메모리 반환
	ServerClose();
}

// 팀원 프로필 출력
void PrintProfile()
{
	printf( "Software Architecture\n" );
	printf( "\t20073230 이성제\n" );
	//printf( "\n" );
	printf( "Make Server Prog\n" );
	printf( "\t20073223 이광헌\n" );
	//printf( "\n" );
	printf( "Prog Design & Document\n" );
	printf( "\t20103391 최준환\n" );
	printf( "Make UserProg by MFC\n" );
	printf( "\tGOP ( Great Over Power )\n" );
	printf( "\n\n" );
}

// 명령어 종류를 보여준다.
void CommandHelp()
{
	printf( "@help\t\t 이 항목을 다시 본다.\n" );
	printf( "@quit\t\t 시스템을 종료한다.\n" );
	printf( "@idlist\t\t 아이디리스트를 출력한다.\n" );
	printf( "@resetidlist\t 모든 접속을 종료하고 아이디리스트를 초기화한다.\n\n" );
}

// 서버는 IP 는 필요 없고, PORT NUM은 일정한 값으로 고정한다. 
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

// 서버 소켓에 클라이언트 접속
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

// 애러 처리
void error_handling(char *message)
{
	printf( "%s\n", message );
	exit(1);
}

// 소켓으로부터 읽은 전문을 분석 
void DevideReceiveSocketData( int iFD, char *sData, int iLength )
{
	int				iPacketHeader;
	int				iDataSize;
	char			*wkRecv;

	memcpy( &cQueue[ iQueueLength ], sData, iLength );
	iQueueLength += iLength;

	do
	{
		// 헤더를 구함
		memcpy( &iPacketHeader, cQueue, sizeof(int) );
		// 패킷의 크기를 구함
		memcpy( &iDataSize, &cQueue[sizeof(int)], sizeof(int) );

		if ( iDataSize <= iQueueLength - _SOCKET_HEADERSIZE_ ) 
		{
			wkRecv = new char[ iDataSize ];

			// 전문의 본체를 구함
			memset( wkRecv, '\0', iDataSize );
			memcpy( wkRecv, &cQueue[ _SOCKET_HEADERSIZE_ ], iDataSize );

			// 전문 본체의 읽기에 성공한 경우
			iQueueLength -= iDataSize + _SOCKET_HEADERSIZE_;

			// 메세지 처리
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
	// ID 전송
	case LOGIN_ID:
		{
			// 유저 접속 아이디 저장
			cUserId[iIDPosition]		= new char[ iDataSize + 1];
			memset( cUserId[iIDPosition], 0, iDataSize + 1 );
			memcpy( cUserId[iIDPosition], cRecvData, iDataSize );
			iUserIdLen[iIDPosition]		= iDataSize + 1;
			iUserFD[iIDPosition]		= iFD;
			iIDPosition++;
			
#if defined( DEBUG_MODE )
			printf( "Insert User ID [ %s ]\n", cUserId[iIDPosition - 1] );
#endif

			// 유저에게 접속 성공 메세지 전송
			SendPacket( iFD, LOGIN_OK, 0, NULL );

#if defined( DEBUG_MODE )
			printf( "Send Message [ Length %08d, %s ]\n", iUserIdLen[iIDPosition-1], cUserId[iIDPosition-1] );
#endif

			// 새로 접속한 놈의 아이디를 모두에게 알려준다.
			int				iIDPos;
			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
			{
				if ( iUserFD[iIDPos] == iFD )
					continue;

				SendPacket( iUserFD[iIDPos], CONNECT_NEW_USER, iUserIdLen[iIDPosition-1], cUserId[iIDPosition-1] );
			}
		}
		break;

	// 접속 성공
	case LOGIN_OK:
		{
		}
		break;

	// ID List 요청
	case WANT_IDLIST:
		{
			int				iIDPos;
			int				iIDRealSize = 0;
			int				iBytePos = 0;
			char			*cSendMessage;

			// 전송해야되는 전체 사이즈를 구함.
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

	// ID List 결과
	case RECV_IDLIST:
		{
		}
		break;

	// 서버 만들기 위한 정보 요청
	case CREATE_CHAT:
		{
		}
		break;

	// 요청 결과
	case RECV_CREATE_CHAT:
		{
		}
		break;

	// 서버에게 해당 클라이언트를 접속하라고 요청
	case JOIN_CHAT:
		{
		}
		break;

	// 클라이언트에게 접속 요청을 받음
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

	// 채팅이 종료되어 포트번호 반환
	case RETURN_PORT:
		{
		}
		break;

	// 전체 메세지 보내기
	case SEND_ALL_MESSAGE:
		{
		}
		break;
		
	// 새로운 접속자가 생겨서 아이디를 전송함
	case CONNECT_NEW_USER:
		{
		}
		break;
		
	// 접속자가 접속을 끈었을 때
	case DISCONNECT_USER:
		{
			// 접속 종료한 놈의 아이디를 모두에게 알려준다.
			int				iIDPos;
			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
				SendPacket( iUserFD[iIDPos], DISCONNECT_USER, iDataSize, cRecvData );
		}
		break;

	case SEND_ALLMESSAGE:
		{
			// 접속 종료한 놈의 아이디를 모두에게 알려준다.
			int				iIDPos;
			for ( iIDPos=0;iIDPos<iIDPosition;iIDPos++ )
				SendPacket( iUserFD[iIDPos], SEND_ALLMESSAGE, iDataSize, cRecvData );
		}
		break;
	}
}

// ID List 에서 ID 를 찾아 채팅을 할 수 있도록 메세지를 보낸다.
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

// 클라이언트에게 메세지 패킷 전송
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


// 서버가 직접 명령어를 입력했을 때
// 아직 미 완성
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
	
		// 이벤트 항목 설정
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
