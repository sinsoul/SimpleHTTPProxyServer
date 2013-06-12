#include "HTTP_Proxy_Server.h"

int listen_port=8899;

long GetContentLength(string *m_ResponseHeader)
{
	long nFileSize=0;
	char szValue[10];
	int nPos=-1;
	nPos=m_ResponseHeader->find("Content-Length",0);
	if(nPos != -1)
	{
		nPos += 16;
		int nCr=m_ResponseHeader->find("\r\n",nPos);
		memcpy(szValue,(char *)m_ResponseHeader->c_str()+nPos,nCr-nPos);
		nFileSize=atoi(szValue);
		return nFileSize;
	}
	else
	{
		Msg("无法获取目标服务器返回内容长度\r\n");
		return -1;
	}
}


bool AnalyzeClientRequest(string *client_request,client_request_summary *crs)
{
	int startPos=-1;
	int endPos=-1;
	endPos=client_request->find(" ht",0);
	if (endPos==string::npos)
	{
	//	Msg("客户端请求头格式错误\r\n");
		return false;
	}

	startPos=0;	

	char *request_type=new char[endPos+2];
	ZeroMemory(request_type,endPos+2);
	memcpy(request_type,(char *)(*client_request).c_str(),endPos);

	crs->type=request_type;

	startPos=client_request->find("://",endPos)+3;
	//这里还应该加入获取端口号的代码

	endPos=client_request->find("/",startPos);
	char *request_host=new char[endPos-startPos+2];
	ZeroMemory(request_host,endPos-startPos+2);
	memcpy(request_host,(char *)(*client_request).c_str()+startPos,endPos-startPos);

	crs->host=request_host;

	startPos=endPos;
	endPos=client_request->find(" HTTP/1",startPos);
	char *request_url=new char[endPos-startPos+2];
	ZeroMemory(request_url,endPos-startPos+2);
	memcpy(request_url,(char *)(*client_request).c_str()+startPos,endPos-startPos);

	crs->url=request_url;

	startPos=client_request->find("Range: ",endPos);
	if (startPos==string::npos)
	{
		delete []request_host;
		delete []request_type;
		delete []request_url;
		return true;
	}
	startPos+=7;
	endPos=client_request->find("\r\n",startPos);
	char * request_range=new char[endPos-startPos+2];
	ZeroMemory(request_range,endPos-startPos+2);
	memcpy(request_range,(char *)(*client_request).c_str()+startPos,endPos-startPos);
	
	crs->range=request_range;

	delete []request_host;
	delete []request_type;
	delete []request_url;
	delete []request_range;
	return true;
}

void WorkThread(void *pvoid)
{
	WORKPARAM *pWork=(WORKPARAM *)pvoid;
	unsigned long recvstatus=0;
	string client_request;
	char temp[2049],c;
	ZeroMemory(temp,2049);
	for(int header_len=0;header_len<2048;header_len++)
	{
		if (recv(pWork->sckClient,&c,1,0)==0)
		{
			break;
		}
		temp[header_len]=c;
		if (temp[header_len]=='\n'&&
			temp[header_len-1]=='\r'&&
			temp[header_len-2]=='\n'&&
			temp[header_len-3]=='\r')
		{
			break;
		}
		if (recvstatus==SOCKET_ERROR)
		{
			Msg("接收客户端请求头失败\r\n");
			break;
		}

	}
	client_request+=temp;
	cout<<"客户端的请求内容："<<endl<<client_request<<endl;
	client_request_summary crs;
	if (!AnalyzeClientRequest(&client_request,&crs))
	{
		return;
	}
	cout<<"请求类型:"<<crs.type<<endl;
	cout<<"请求主机:"<<crs.host<<endl;
	cout<<"请求资源:"<<crs.url<<endl;
	if (!crs.range.empty())
	{
		cout<<"请求Range:"<<crs.range<<endl;
	}

	SOCKET m_socket;
	struct protoent *pv; 
	pv=getprotobyname("tcp"); 
	m_socket=socket(PF_INET,SOCK_STREAM,pv->p_proto);
	if(m_socket==INVALID_SOCKET)
	{
		Msg("创建套接字失败!\r\n");
		return;
	}

	hostent *m_phostip=gethostbyname(crs.host.c_str());
	if(m_phostip==NULL)
	{
		Msg("所请求的域名解析失败!\r\n");
		return;
	}
	struct in_addr ip_addr;
	memcpy(&ip_addr,m_phostip->h_addr_list[0],4);
	struct sockaddr_in destaddr;
	memset((void *)&destaddr,0,sizeof(destaddr)); 
	destaddr.sin_family=AF_INET;
	destaddr.sin_port=htons(80);
	destaddr.sin_addr=ip_addr;
	if(connect(m_socket,(struct sockaddr*)&destaddr,sizeof(destaddr))!=0)
	{
	//	Msg("连接到目标服务器失败!\r\n");
		return;
	}

	long recvlength=0;
	string m_RequestHeader;
	m_RequestHeader=m_RequestHeader+crs.type+" "+crs.url+" HTTP/1.1\r\n";
	m_RequestHeader=m_RequestHeader+"Host: "+crs.host+"\r\n";
	m_RequestHeader=m_RequestHeader+"Connection: keep-alive\r\n";
	m_RequestHeader=m_RequestHeader+"User-Agent: Novasoft NetPlayer/4.0\r\n";
//	m_RequestHeader=m_RequestHeader+"Cache-Control: max-age=0\r\n";
	m_RequestHeader=m_RequestHeader+"Accept: */*\r\n";
//	m_RequestHeader=m_RequestHeader+"Origin:  http://222.73.105.196\r\n";
//	m_RequestHeader=m_RequestHeader+"Cookie: saeut=61.188.187.53.1323685584721318\r\n";
	if (!crs.range.empty())
	{
		m_RequestHeader=m_RequestHeader+"Range: "+crs.range+"\r\n";
	}
	m_RequestHeader+="\r\n";


	if(send(m_socket,m_RequestHeader.c_str(),m_RequestHeader.length(),0)==SOCKET_ERROR)
	{
		Msg("向服务器发送请求失败!\r\n");
		return;
	}
	char buffer[512001];
	string target_response;
	ZeroMemory(temp,2049);
	unsigned int recv_sta=0,send_sta=0;

	for(int header_len=0;header_len<2048;header_len++)
	{
		if (recv(m_socket,&c,1,0)==0)
		{
			break;
		}
		temp[header_len]=c;
		if (temp[header_len]=='\n'&&
			temp[header_len-1]=='\r'&&
			temp[header_len-2]=='\n'&&
			temp[header_len-3]=='\r')
		{
			break;
		}
		if (recvstatus==SOCKET_ERROR)
		{
			Msg("接收目标服务器响应头失败\r\n");
			break;
		}
	}
	target_response=temp;
	cout<<"目标服务器响应:"<<target_response<<endl;
	long content_len=GetContentLength(&target_response);
	long n_recvd=0,n_sended=0;
	
	send(pWork->sckClient,target_response.c_str(),target_response.length(),0);
	while(1)
	{
		ZeroMemory(buffer,512001);
		recv_sta=recv(m_socket,buffer,512000,0);
		if (recv_sta==0||recv_sta==SOCKET_ERROR)
		{
			break;
		}
		n_recvd+=recv_sta;

		send_sta=send(pWork->sckClient,buffer,recv_sta,0);
		if (SOCKET_ERROR==send_sta||send_sta==0)
		{
			break;
		}
		n_sended+=send_sta;
		if (n_recvd>=content_len||n_sended>=content_len)
		{
			break;
		}
		Sleep(100);
	}
	closesocket(pWork->sckClient);
	closesocket(m_socket);

	Msg("一个传输线程结束...\r\n");
	return;
	
}

void ListenThread(void *pvoid)
{
	int iRet=0, addrLen=0;
	sockaddr_in local_addr,accept_addr;
	SOCKET sckListen, sckAccept;
	int nErrCount;

	sckListen = socket(AF_INET, SOCK_STREAM, 0);
	if(sckListen == INVALID_SOCKET)
	{
		Msg("创建代理服务器的监听Socket失败\r\n");
		return;
	}
	hostent* pEnt = gethostbyname("");
	if(!pEnt)
	{
		Msg("创建代理服务器的gethostbyname()失败\r\n");
		return;
	}
	memcpy(&(local_addr.sin_addr), pEnt->h_addr, pEnt->h_length);
	local_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(listen_port);

	string strLocalAddr;
	strLocalAddr=inet_ntoa(local_addr.sin_addr);

	iRet = bind(sckListen, (const sockaddr*)&local_addr, sizeof(local_addr));
	if(iRet==SOCKET_ERROR)
	{
		Msg("创建代理服务器时绑定出错.\r\n");
		return;
	}

	nErrCount = 0;
	Msg("启动下载防封代理服务功能....\r\n代理服务器:127.0.0.1 端口:8899\r\n你可以手动对下载工具进行代理配置,以绕过视频服务器的下载限制...\r\n");
	iRet = listen(sckListen, SOMAXCONN);
	if(iRet==SOCKET_ERROR)
	{
		Msg("代理服务器监听失败\r\n");
		nErrCount++;
		if(nErrCount>=10)
		{
			Msg("nErrCount>=10, listening thread terminated.\r\n");
			return;
		}
	}
	nErrCount = 0;

	while(1)
	{
		addrLen = sizeof(accept_addr);
		sckAccept = accept(sckListen, (struct sockaddr*)&accept_addr, &addrLen);
		if(sckAccept==INVALID_SOCKET)
		{
			Msg("接受客户端连接失败\r\n");
			return;
		}
		Msg("创建一个传输线程...\r\n");
//		b_Proxy=true;
		WORKPARAM *pWorkParam = (WORKPARAM*)malloc(sizeof(WORKPARAM)) ;
		pWorkParam->sckClient = sckAccept;
		pWorkParam->client_addr = accept_addr;
		_beginthread(WorkThread,0,(void *)pWorkParam);
	}

	return;
}


 int main(_In_ int _Argc, char **argv)
 {
		 WORD wVersionRequested;
		 WSADATA wsaData;
		 int err;
		 wVersionRequested=MAKEWORD( 2, 2 );
		 err=WSAStartup( wVersionRequested, &wsaData );
		 if ( err != 0 ) 
		 {
			 return false;
		 }

		 if ( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 2 ) 
		 {
			 WSACleanup( );
			 return false; 
		 }
	 _beginthread(ListenThread,0,NULL);
	 while (true)
	 {
		 Sleep(10000000);
	 }
	 
 }

 //void AnalyzeJumpPage(HttpMessenger *hm_cmd_receiver)
 //{
	// if (hm_cmd_receiver->m_ResponseText.find("<html>")==0)
	// {
	//	 Msg("可能收到跳转页面.开始解析.\r\n");
	//	 char jmp_host[255];
	//	 char jmp_port[10];
	//	 char jmp_resource[1024];
	//	 int start_pos=-1,end_pos=-1;
	//	 string ResponseText=hm_cmd_receiver->m_ResponseText;
	//	 ZeroMemory(jmp_resource,1024);
	//	 ZeroMemory(jmp_port,10);
	//	 ZeroMemory(jmp_host,255);
	//	 start_pos=ResponseText.find("url=http://");
	//	 if (start_pos!=string::npos)
	//	 {
	//		 start_pos+=11;
	//		 end_pos=ResponseText.find("\"",start_pos);
	//		 ResponseText.erase(end_pos,ResponseText.length());
	//		 end_pos=ResponseText.find(":",start_pos);
	//		 if (end_pos!=string::npos)
	//		 {
	//			 //如果找到冒号表示包含端口号
	//			 memcpy(jmp_host,ResponseText.c_str()+start_pos,end_pos-start_pos);
	//			 start_pos=end_pos+1;
	//			 end_pos=ResponseText.find("/",start_pos);
	//			 if (end_pos==string::npos)
	//			 {
	//				 end_pos=ResponseText.length();
	//				 memcpy(jmp_port,ResponseText.c_str()+start_pos,end_pos-start_pos);
	//				 memcpy(jmp_resource,"/",1);
	//			 }
	//			 else
	//			 {
	//				 memcpy(jmp_port,ResponseText.c_str()+start_pos,end_pos-start_pos);
	//				 start_pos=end_pos;
	//				 end_pos=ResponseText.length();
	//				 memcpy(jmp_resource,ResponseText.c_str()+start_pos,end_pos-start_pos);
	//			 }
	//		 }
	//		 else
	//		 {
	//			 memcpy(jmp_port,"80",2);
	//			 //查找主机地址尾部
	//			 end_pos=ResponseText.find("/",start_pos);
	//			 if (end_pos!=string::npos)
	//			 {
	//				 if (end_pos-start_pos<255)
	//				 {
	//					 memcpy(jmp_host,ResponseText.c_str()+start_pos,end_pos-start_pos);
	//					 start_pos=end_pos;
	//					 end_pos=ResponseText.length();
	//					 memcpy(jmp_resource,ResponseText.c_str()+start_pos,end_pos-start_pos);
	//				 }
	//			 }
	//			 else
	//			 {
	//				 //没有找到资源
	//				 end_pos=ResponseText.length();
	//				 memcpy(jmp_host,ResponseText.c_str()+start_pos,end_pos-start_pos);
	//				 memcpy(jmp_resource,"/",1);
	//			 }
	//		 }
	//		 Msg("跳转主机:%s\r\n跳转端口:%s\r\n跳转资源:%s\r\n",jmp_host,jmp_port,jmp_resource);
	//		 HttpMessenger hm_jmp(jmp_host,atoi(jmp_port));
	//		 hm_jmp.CreateConnection();
	//		 hm_jmp.CreateAndSendRequest("GET",jmp_resource,jmp_host,0,false,0);
	//		 AnalyzeJumpPage(&hm_jmp);
	//		 hm_cmd_receiver->m_ResponseText.erase();
	//		 hm_cmd_receiver->m_ResponseText=hm_jmp.m_ResponseText;
	//	 }
	//	 else
	//	 {
	//		 Msg("分析跳转页面出错\r\n");
	//	 }
	// }
 //}