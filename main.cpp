#include <curl/curl.h>
#include "PluginAPI.h"
#include "SQImports.h"
#ifdef WIN32
#include <Windows.h>
#endif
#include <string.h>
#include <string>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "ReadCFG.h"
#include <ctime>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
PluginFuncs* Server;
PluginCallbacks* Callbacks;
HSQAPI sq = NULL;
HSQUIRRELVM v = NULL;
HSQOBJECT container;

std::string Token="";
std::string channelID="";
std::string GatewayUrl = "wss://gateway.discord.gg/?v=10&encoding=json";
std::string resumeURLAppend = "/?v=10&encoding=json";
std::string HttpUrl = "https://discord.com/api/v10";
bool bDebug = false;//debug is false
int xrate_limit = 5;
std::vector<unsigned int>tickarray;
#ifdef WIN32
HANDLE hstdout = NULL;//the console handle
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004	//old sdk
#endif
#endif

#define INTENT 37377
#define RED 12
#define GREEN 10
#define WHITE 15
#define YELLOW 14
#define DISCORDSYNC_CMD_START 0x3B456AB3
#define CMD_DISCONNECT (DISCORDSYNC_CMD_START )
#define CMD_RECONNECT (DISCORDSYNC_CMD_START + 1)
#define CMD_LOGMSGEX (DISCORDSYNC_CMD_START +2)
#define CMD_SHOWUPTIME (DISCORDSYNC_CMD_START +3)
#define CMD_RESUME (DISCORDSYNC_CMD_START +4)
#define CMD_STATUS (DISCORDSYNC_CMD_START +5)
#define CMD_CHANNEL (DISCORDSYNC_CMD_START +6)
#define CMD_TOKEN (DISCORDSYNC_CMD_START +7)
#define CMD_ONMESSAGE (DISCORDSYNC_CMD_START+8)
#define CMD_SUBSCRIBE (DISCORDSYNC_CMD_START+9)
#define CMD_UNSUBSCRIBE (DISCORDSYNC_CMD_START+0xa)
#define CMD_SENDMSG (DISCORDSYNC_CMD_START+0xb)
#define CMD_ONCONNECT (DISCORDSYNC_CMD_START+0xc)
#define CMD_ON_DISCONNECT (DISCORDSYNC_CMD_START+0xd)
#define CMD_VERBOSE (DISCORDSYNC_CMD_START+0xe)
#define CMD_ONMESSAGE2 (DISCORDSYNC_CMD_START+0xf)

bool subscribed=false;//if yes, then a plugin command with message will be send when a message from discord arrives
bool verbose = false;//if on, then every "Disconnected. Reconnecting.. " messages will be shown

long heartbeat_interval = -1;
long last_hearbeat_send = 0;
float heartbeat_ack_wait_time = -1;
int sequence_number = -1;
std::string resume_gateway_url = "";
std::string session_id = "";
bool websocket_initialized = false;
bool error_msg_shown = false;
bool error_msg_shown_plgncmd = false;
bool shutting_down = false;
bool hello_received = false;
bool bsend_identity = false;
bool bsend_gateway_resume_event = false;
std::string botId = "";
CURL* gateway_easy = NULL; 
bool resuming = false;
static void websocket_close(CURL* curl);
//int resume_connection(CURL* curl);
int still_running = 0;
CURLM* multi_handle = NULL;
bool first_time = true;
unsigned int Uptime = 0;
char DS_errbuf[CURL_ERROR_SIZE];
int StartWebSocket(std::string url = GatewayUrl);
void DS_OutputMessage(int wattributes, const char* text);
#ifndef WIN32
#include <time.h>
long DS_GetTickCount()
{
	struct timespec ts;
	long theTick = 0U;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	theTick = ts.tv_nsec / 1000000;
	theTick += ts.tv_sec * 1000;
	return theTick;
}
#else
long DS_GetTickCount() { return GetTickCount(); }
#endif
size_t DS_write_data(void* buffer, size_t size, size_t nmemb, void* userp)
{
	struct curl_header* type;
	CURL* easy = (CURL*)userp;
	CURLHcode h =
		curl_easy_header(easy, "x-ratelimit-limit", 0, CURLH_HEADER, -1, &type);
	if (h == CURLHE_OK) {
		try {
			int limit = std::stoi(std::string(type->value));
			xrate_limit = limit;
		}
		catch (const std::invalid_argument& ia) {

		}
	}
	return size * nmemb;
}
//clear screen
#ifdef WIN32
void cls(HANDLE hConsole)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	SMALL_RECT scrollRect;
	COORD scrollTarget;
	CHAR_INFO fill;

	// Get the number of character cells in the current buffer.
	if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
	{
		return;
	}

	// Scroll the rectangle of the entire buffer.
	scrollRect.Left = 0;
	scrollRect.Top = 0;
	scrollRect.Right = csbi.dwSize.X;
	scrollRect.Bottom = csbi.dwSize.Y;

	// Scroll it upwards off the top of the buffer with a magnitude of the entire height.
	scrollTarget.X = 0;
	scrollTarget.Y = (SHORT)(0 - csbi.dwSize.Y);

	// Fill with empty spaces with the buffer's default text attribute.
	fill.Char.UnicodeChar = TEXT(' ');
	fill.Attributes = csbi.wAttributes;

	// Do the scroll
	ScrollConsoleScreenBuffer(hConsole, &scrollRect, NULL, scrollTarget, &fill);

	// Move the cursor to the top left corner too.
	csbi.dwCursorPosition.X = 0;
	csbi.dwCursorPosition.Y = 0;

	SetConsoleCursorPosition(hConsole, csbi.dwCursorPosition);
}
#endif
std::string getTimePrefix()
{
	std::time_t t = std::time(0);   // get time now
	std::tm* now = std::localtime(&t);
	//22:07:51
	std::string prefix = std::to_string(now->tm_hour) + ":" + std::to_string(now->tm_min) + ":" +
		std::to_string(now->tm_sec) + " ";
	return prefix;
}
void DS_LogMessageEx(const char* msg)
{
	std::string formattedMsg = std::string(msg);// +"\r" + std::string(len, '_');
	size_t len = formattedMsg.length();
#ifdef WIN32
	CONSOLE_SCREEN_BUFFER_INFO csb; 
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csb);
#else
	if(!bDebug)printf("%c[?1049h",0x1b);//enables the alternative buffer
#endif
	std::time_t t = std::time(0);   // get time now
	std::tm* now = std::localtime(&t);
	//22:07:51
	std::string prefix = std::to_string(now->tm_hour) + ":" + std::to_string(now->tm_min) + ":" +
		std::to_string(now->tm_sec) + " ";
	formattedMsg = prefix + formattedMsg;
#ifdef WIN32
bool bAlternateBuffer = false;
#ifndef DISABLE_ALTERNATE_BUFFER	
	DWORD dwMode = 0; 
	if (GetConsoleMode(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&dwMode))
	{
		if ((dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) && (dwMode & ENABLE_PROCESSED_OUTPUT))
		{
			printf("%c[?1049h", 0x1b);//enables the alternative buffer
			bAlternateBuffer = true;
		}
	}
#endif
#endif
#ifndef DISABLE_LOGTEXT
	Server->LogMessage(formattedMsg.c_str());
#else
	//means server logging disabled. this must be for windows xp. so nothing is written on server_log.txt.
	//however if debug is true, we want to print the messages. 
#ifdef WIN32 //because in one print statement for linux already written ago.
	if(bDebug)
		Server->LogMessage(formattedMsg.c_str());
#endif
#endif
#ifndef WIN32	
	//screen defence - moves one line up and erase entire line
	if(!bDebug)printf("%c[1A%c[2K", 0x1b, 0x1b);

	if(!bDebug)printf("%c[?1049l", 0x1b);;//disables the alternative buffer
	fflush(stdout);
#else
	if (bAlternateBuffer)
	{
		printf("%c[1A%c[2K", 0x1b, 0x1b);

		printf("%c[?1049l", 0x1b);;//disables the alternative buffer
		fflush(stdout);
	}
#ifndef DISABLE_SCREEN_ERASING
	else {
		int columns = csb.srWindow.Right - csb.srWindow.Left + 1;
		int count = csb.dwCursorPosition.X; int r = 0;
		for (int i = 0; i < len; i++)
		{
			if (formattedMsg[i] != '\n')
			{
				if (formattedMsg[i] == '\t')count += 8;//One tab is 8 in length (windows)
				else count++;
			}
			else count = 0;
			if (count > columns) { r++; count -= columns; }
		}
		int n = 1;//New line added by Server->LogMessage
		for (size_t i = 0; i < len; i++)if (formattedMsg[i] == '\n')n++;
		//int m = csb.srWindow.Bottom - csb.dwCursorPosition.Y - n ;
		//bottom =24, cursor position =23 n=1
		//bottom =24, cursor position=23, n=2

		//csb.dwCursorPosition.Y == csb.srWindow.Bottom ?
		//	csb.dwCursorPosition.Y-- : NULL; MessageBoxA(0, std::to_string(n).c_str(), "n", 0);

		if ((csb.dwCursorPosition.Y + n + r) > csb.srWindow.Bottom)
			csb.dwCursorPosition.Y -= (csb.dwCursorPosition.Y + n + r) - csb.srWindow.Bottom;
		//Little careful here.

		//    n+r  = no of scroll down happens.

		if (csb.dwCursorPosition.Y < 0)
		{
			CONSOLE_SCREEN_BUFFER_INFO csb2;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csb2);
			cls(GetStdHandle(STD_OUTPUT_HANDLE));
		}
		else {
			SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), csb.dwCursorPosition);
			std::string eraseMsg = formattedMsg;
			for (size_t i = 0; i < len; i++)
				if (eraseMsg[i] != '\n' && eraseMsg[i] != '\t')eraseMsg[i] = ' ';
			printf("%s", eraseMsg.c_str());
			SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), csb.dwCursorPosition);
		}
	}
#endif
#endif
}

void send_heartbeat(CURL* curl) {
	size_t sent; 
	char buffer[512];
	sprintf(buffer, "{\"op\":%d, \"d\":%s}", 1, sequence_number == -1 ? "null" : std::to_string(sequence_number).c_str());
	CURLcode result = curl_ws_send(curl, (const void*)buffer, strlen(buffer), &sent, 0, CURLWS_TEXT);
	//printf("result %d, sent %d\n", result, sent);
	last_hearbeat_send = DS_GetTickCount();
	heartbeat_ack_wait_time = 0;//waiting for ack. when ack received, it will be -1.
	if (result != CURLE_OK)
	{
		DS_LogMessageEx((std::string("[DiscordSync] Could not send heartbeat. ") + std::to_string(result) ).c_str());
	}
	if(bDebug)
	{
		DS_LogMessageEx(buffer);
	}
}

void send_identity(CURL* curl) {
	size_t sent;
	char buffer[4096];
	sprintf(buffer, "	\
    {	\
        \"op\":%d, \"d\": \
        {	\
            \"token\":\"%s\",	\
            \"intents\":%u,	\
            \"properties\":	\
            {	\
                \"os\": \"linux\",	\
                \"browser\": \"my_library\",	\
                \"device\": \"my_library\"	\
            }	\
        }	\
    }	\
    ", 2, Token.c_str(), INTENT);
	CURLcode result = curl_ws_send(curl, (const void*)buffer, strlen(buffer), &sent, 0, CURLWS_TEXT);
	if (result != CURLE_OK)
	{
		Server->LogMessage("[DiscordSync] Could not send identity. (%d)", result);
	}
	if (bDebug)
	{
		DS_LogMessageEx(buffer);
	}
}
void send_Gateway_Resume_Event(CURL* curl) {
	if (sequence_number == -1)
	{
		DS_OutputMessage(RED, "[MODULE]  ");
		DS_OutputMessage(WHITE, "Cannot Resume.\n");
	}
	size_t sent;
	char buffer[4096];
	sprintf(buffer, "	\
    {	\
        \"op\":%d, \"d\": \
        {	\
            \"token\":\"%s\",	\
            \"session_id\":\"%s\",	\
            \"seq\":	%d\
        }	\
    }	\
    ", 6, Token.c_str(), session_id.c_str(), sequence_number);
	CURLcode result = curl_ws_send(curl, (const void*)buffer, strlen(buffer), &sent, 0, CURLWS_TEXT);
	if (result != CURLE_OK)
	{
		Server->LogMessage("[Discord] Failed to resume. (%d)", result);
	}
	if (bDebug)
	{
		DS_LogMessageEx(buffer);
	}
}

const char* getANSIColorCode(int i)
{
	switch (i)
	{
	case 1:  return "34";
	case 2: return "32";
	case 3: return "36";//cyan
	case 4: return "31";//red
	case 5: return "35";//magenta
	case 6: return "33";//yellow
	case 7: return "37";//white
	default: return "";
	}
}
void DS_OutputMessage(int wattributes, const char* text)
{	
#ifdef WIN32
	if (hstdout)
	{
		//Credits: https://bitbucket.org/stormeus/0.4-squirrel/src/master/ConsoleUtils.cpp
		CONSOLE_SCREEN_BUFFER_INFO csbBefore;
		GetConsoleScreenBufferInfo(hstdout, &csbBefore);
		SetConsoleTextAttribute(hstdout, wattributes);
		fputs(text, stdout);
		SetConsoleTextAttribute(hstdout, csbBefore.wAttributes);
	}else
		printf("%s", text);
#else
	
	printf("%c[%s%sm%s%c[0m", 27, (wattributes&8)==8?"1;":"", getANSIColorCode(wattributes&(~8)), text, 27);

#endif
}



//return 0 on successfully adding easy handle to multi handle
int DS_send_message(const std::string& message) {
	//Check if rate limit is reached..?
	unsigned int dwNow = DS_GetTickCount();

	//Remove all that happened before 1000 ms.
	while (tickarray.size() > 0)
	{
		if (tickarray.at(0) < dwNow - 1000)
			tickarray.erase(tickarray.begin());
		else break;
	}

	//Now tickarray contains only that happened in last 1000 ms
	if (xrate_limit > 0 && tickarray.size() >= xrate_limit) //'>' is not necessary.
	{
		//Rate limit reached. cannot send message.
		DS_OutputMessage(YELLOW, "[MODULE] ");
		char msg[128];
		sprintf(msg, "Message not send. %d/%d already send per second.\n", (int)tickarray.size(),
			xrate_limit);
		DS_OutputMessage(WHITE, msg);

		return -1;
	}
    size_t sent;
    char buffer[4096];
	sprintf(buffer,
		"	\
    {	\
        \"content\":\"%s\" \
    }	\
    ",
		message.c_str());
	if (bDebug)
	{
		DS_LogMessageEx(buffer);
	}
	CURL* easy_handle = curl_easy_init();
	
	if(easy_handle)
	{
		std::string url = HttpUrl+"/channels/" + channelID + "/messages";
		struct curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, ("Authorization: Bot " + Token).c_str());
		curl_easy_setopt(easy_handle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(easy_handle, CURLOPT_COPYPOSTFIELDS, buffer);
		curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, DS_write_data);
		curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, easy_handle);
#ifdef DONOT_SSL_VERIFY_PEER
		curl_easy_setopt(easy_handle, CURLOPT_SSL_VERIFYPEER, FALSE);
#endif
		CURLMcode result=curl_multi_add_handle(multi_handle, easy_handle);
		if (result != CURLM_OK)
		{
			std::string msg = std::string("DiscordSync: Failed on ( curl_multi_add_handle ) (") + std::to_string(result) + std::string(")\n");
			DS_OutputMessage(RED, msg.c_str());
			return result;
		}
		result=curl_multi_perform(multi_handle, &still_running);
		if (result != CURLM_OK)
		{
			std::string msg = std::string("DiscordSync: Failed on ( curl_multi_perform ) (") + std::to_string(result)+std::string(")\n");
			DS_OutputMessage(RED, msg.c_str());
			return result;
		}
		tickarray.push_back(dwNow);
		return 0;
		//OnServerFrame will work the rest of code
	}else 
	{
		DS_OutputMessage(RED,"DiscordSync: Failed on ( curl_easy_init )\n");
		return -1;
	}
}
SQInteger fn_SendMessage(HSQUIRRELVM v)
{
	if (websocket_initialized)
	{
		const SQChar* message;
		sq->getstring(v, 2, &message);
		int res = DS_send_message(message);
	}
	else if (!error_msg_shown)
	{
		DS_OutputMessage(YELLOW, "[MODULE]  ");
		char msg[128];
		sprintf(msg, "Cannot send message when disconnected. Use 0x%x to reconnect\n", CMD_RECONNECT);
		DS_OutputMessage(WHITE,msg );
		error_msg_shown = true;
	}
	return 0;
}
void DiscordSync_CloseConnection()
{
	websocket_initialized = false;
	//bsend_identity = false;
	bsend_gateway_resume_event = false;
	resuming = false;
	heartbeat_interval = -1;
	hello_received = false;
	//sequence_number = -1;
	error_msg_shown = false;
	error_msg_shown_plgncmd = false;
	first_time = false;//To show 'reconnected' msg
	heartbeat_ack_wait_time = -1;
	if (gateway_easy)
	{
		curl_multi_remove_handle(multi_handle, gateway_easy);
		curl_easy_cleanup(gateway_easy);
		gateway_easy = NULL;
	}
	if (bDebug)
	{
		DS_LogMessageEx("DiscordSync_CloseConnection was called");
	}
}
static int recv_any(CURL* curl) {
	size_t rlen;size_t total_len=0;
	const struct curl_ws_frame* meta;
	char buffer[4096];std::string data="";CURLcode result;
	do
	{
	result = curl_ws_recv(curl, buffer, sizeof(buffer), &rlen, &meta);
		if (result != CURLE_OK) 
		{
		if (result == CURLE_GOT_NOTHING)
		{
			DS_LogMessageEx("[DiscordSync] Disconnected. (CURLE_GOT_NOTHING)");
			if (verbose) 
			{
				DS_OutputMessage(RED, "[MODULE] ");
				DS_OutputMessage(WHITE, "Connection closed. Disconnected. ");
			}
			DiscordSync_CloseConnection(); 
			Server->SendPluginCommand(CMD_ON_DISCONNECT, ""); 
			//Try resuming
			if (resume_gateway_url != "")
			{
				Server->SendPluginCommand(CMD_RESUME, "");
				if(verbose)
					DS_OutputMessage(YELLOW, "Trying to resume..\n");
			}
			else
			{
				Server->SendPluginCommand(CMD_RECONNECT, "");
				if(verbose)
					DS_OutputMessage(YELLOW, "Reconnecting..\n");
			}
			
			return result;//curl handle null. so return
		}
		else if (result != CURLE_AGAIN )
		{
			std::string errmsg;
			if (result == CURLE_RECV_ERROR)
				errmsg = "Failed in receiving network data. ";
			else 
				errmsg= std::string("Error occurred in recv_any. CURLcode: ") + std::to_string(result) + ". ";
			DS_OutputMessage(RED, "[MODULE]");
			DS_OutputMessage(WHITE, errmsg.c_str());
			DS_OutputMessage(WHITE, "Disconnected.\n");
			DS_LogMessageEx((std::string("[DiscordSync] Disconnected ") + std::to_string(result)).c_str());
			DiscordSync_CloseConnection(); 
			Server->SendPluginCommand(CMD_ON_DISCONNECT, ""); 
			return result;//curl handle null. so return
		}
		return result;//curle_again, so return.
	}
	data += std::string(buffer, rlen);
	total_len+=rlen;
	}
	while(meta->bytesleft>0);

	bool resumable;
	if (meta->flags & CURLWS_TEXT) {
		if (bDebug)
			DS_LogMessageEx(data.c_str());
		try {
			json jsonData = json::parse(data);
			std::string jsonString = jsonData.dump(4);
			//LogMessageEx(jsonString.c_str());
			if (jsonData.contains("op")) {
				int opcode = jsonData["op"];
				switch (opcode) {
				case 10:
					hello_received = true;

					if (jsonData.contains("d") && jsonData["d"].contains("heartbeat_interval")) {
						long intvl = jsonData["d"]["heartbeat_interval"];
						heartbeat_interval = intvl;
						send_heartbeat(curl);
					}
					/*If we have resume_connection, then gateway respond with opcode 10
					{"t":null,"s":null,"op":10,"d":{"heartbeat_interval":41250,"_trace":["[\"gateway-prd-us-east1-c-6cgd\",{\"micros\":0.0}]"]}}
					*/
					if (resuming && !bsend_gateway_resume_event)
					{
						send_Gateway_Resume_Event(curl);
						bsend_gateway_resume_event = true;
					}
					break;
				case 9://Invalid Section
					resumable = jsonData["d"]; 
					DS_LogMessageEx("[DiscordSync] Invalid Session");
					DS_OutputMessage(RED, "[MODULE]  ");
					DS_OutputMessage(WHITE, "Disconnected.");
					websocket_close(curl);
					DiscordSync_CloseConnection();
					if (resumable == true)
					{
						DS_OutputMessage(WHITE, "Resuming..\n");
						StartWebSocket(resume_gateway_url);
						resuming = true;
					}
					else
					{
						sequence_number = -1;
						printf("\n");//fflush
						Server->SendPluginCommand(CMD_ON_DISCONNECT, "");
					}
					break;
				case 7: //Reconnect
					if (verbose)
					{
//The most common message. If in windows, log it (it won't print on screen)
#ifdef WIN32
						DS_LogMessageEx("[DiscordSync]Reconnect instruction received");
#endif
						DS_OutputMessage(RED, "[MODULE]  ");
						DS_OutputMessage(WHITE, "Disconnected. Resuming..\n");
					}
					websocket_close(curl);
					DiscordSync_CloseConnection();
					StartWebSocket(resume_gateway_url);
					resuming = true;
					
					break;
				case 11:
					//printf("Heartbeat ACK (opcode 11) received\n");
					heartbeat_ack_wait_time = -1;//ack received
					break;
				case 1:
					send_heartbeat(curl);
					break;
				case 0:
					// Handle Dispatch event (various event types)
					if(jsonData.contains("s"))
						sequence_number = jsonData["s"];
					if (jsonData.contains("t")) {
						std::string eventType = jsonData["t"];
						
						if (eventType == "MESSAGE_CREATE") {
							// Handle the MESSAGE_CREATE event
							if (jsonData.contains("d") && jsonData["d"].contains("content")) {
								std::string messageContent = jsonData["d"]["content"];
								std::string authorUsername = jsonData["d"]["author"]["username"];
								std::string authorid = jsonData["d"]["author"]["id"];
								if (authorid == botId)//the message we send will come back as event
									break;
								std::string channelId = jsonData["d"]["channel_id"];
								std::string nick = "";
								if (jsonData["d"].contains("member"))
								{
									if (jsonData["d"]["member"].contains("nick"))
									{
										if (!jsonData["d"]["member"]["nick"].is_null())
											nick = jsonData["d"]["member"]["nick"];
									}
								}
								//printf("%s\n", nick.c_str());
								if (channelId == channelID)
								{
									//Call squirrel function
									//printf("%s: %s\n", authorUsername.c_str(), messageContent.c_str());
									std::string function = "onDiscordMessage";
									bool isCommand = false;
									std::string cmd = "", text = "";
									if (messageContent.length() > 0 && (messageContent[0] == '/'||
										messageContent[0]=='!'))
									{
										function = "onDiscordCommand";
										isCommand = true;
										if (messageContent.length() > 1)
										{
											size_t i = messageContent.find(" ");
											if (i != std::string::npos)
											{
												// /exec print(500)
												cmd = messageContent.substr(1, i - 1);
												if (messageContent.length() > i + 1)
													text = messageContent.substr(i + 1);
											}
											else
												cmd = messageContent.substr(1);
										}
										
									}
									if (sq && v)//only if squirrel is available
									{
										sq->pushroottable(v);
										sq->pushstring(v, function.c_str(), -1);
										if (SQ_SUCCEEDED(sq->get(v, -2)))
										{
											sq->pushroottable(v);
											//sq->pushstring(v, authorUsername.c_str(), -1);
											if(nick!="")
												sq->pushstring(v, nick.c_str(), -1);
											else
												sq->pushstring(v, authorUsername.c_str(), -1);
											if (!isCommand)
												sq->pushstring(v, messageContent.c_str(), -1);
											else
											{
												sq->pushstring(v, cmd.c_str(), -1);
												sq->pushstring(v, text.c_str(), -1);
											}
											sq->call(v, isCommand == false ? 3 : 4, 0, 1);
										}
										sq->pop(v, 1);
									}
									if (subscribed)
									{
										if (isCommand)
											Server->SendPluginCommand(CMD_ONMESSAGE, "%s:%s %s", nick != "" ? nick.c_str() : authorUsername.c_str(), cmd.c_str(), text.c_str());
										else
											Server->SendPluginCommand(CMD_ONMESSAGE2, "%s:%s", nick != "" ? nick.c_str() : authorUsername.c_str(), messageContent.c_str());
									}
								}
							}
						}
						else if (eventType == "READY") {
							//Handle the READY event
							if (jsonData.contains("d") ) {
								if (jsonData["d"].contains("user"))
								{
									botId = jsonData["d"]["user"]["id"];
								}
								resume_gateway_url = jsonData["d"]["resume_gateway_url"];
								resume_gateway_url += resumeURLAppend;
								session_id = jsonData["d"]["session_id"];
								if (!first_time)
								{
									if (verbose)
									{
										DS_OutputMessage(GREEN, "[MODULE]  ");
										DS_OutputMessage(WHITE, "Reconnected.\n");
									}
									DS_LogMessageEx("[DiscordSync]: Reconnected to Discord");
								}
								else {
									first_time = false;
									DS_LogMessageEx("[DiscordSync]: Connected to Discord");
								}
								Uptime = DS_GetTickCount();
								DS_LogMessageEx(("[DiscordSync] Resume URL: " + resume_gateway_url).c_str());
								DS_LogMessageEx(("[DiscordSync] Session id: " + session_id).c_str());
								Server->SendPluginCommand(CMD_ONCONNECT, "");
							}
						}
						else if (eventType == "RESUMED")
						{
							if (verbose)
							{
								DS_OutputMessage(GREEN, "[MODULE]  ");
								DS_OutputMessage(WHITE, "Reconnected.\n");
							}
							DS_LogMessageEx("[DiscordSync]: Resumed to Discord");
							resuming = false;//done with it
						}
						// Add more cases for other event types as needed.


					}
					break;
				default:
					// Handle other opcodes or events
					DS_LogMessageEx(("Unhandled opcode " + std::to_string(opcode) + " received").c_str());
					break;
				}
			}
		}
		catch (const json::parse_error& e) {
			// Handle the parsing error as needed
			DS_LogMessageEx((std::string("DiscordSync: JSON parsing error: ")+ e.what()).c_str());
			DS_LogMessageEx(data.c_str());			
		}
	}
	else if (meta->flags & CURLWS_CLOSE) {
		if (bDebug)
			DS_LogMessageEx((std::string("meta->flags: ") + std::to_string(meta->flags)).c_str());
	DS_OutputMessage(RED, "[MODULE]  ");
	if (bsend_identity && botId == "")
	{
		//Probably the token was invalid
		DS_OutputMessage(WHITE, "Gateway connection closed. Invalid token?\n");
	}
	else
		DS_OutputMessage(WHITE, "Gateway connection closed. Disconnected.\n");
	DS_LogMessageEx("[DiscordSync] Gateway connection closed.");
	DiscordSync_CloseConnection();
	Server->SendPluginCommand(CMD_ON_DISCONNECT, "");
		//exit(0);
	}
	else {
		//printf("Other WebSocket flags or data\n");
	}

	return 0;
}


/* close the connection */
static void websocket_close(CURL* curl)
{
	if (curl)
	{
		size_t sent;
		(void)curl_ws_send(curl, "", 0, &sent, 0, CURLWS_CLOSE);
	}
	if (bDebug)
		DS_LogMessageEx("websocket_close");
}



int StartWebSocket(std::string url) {
	CURLcode res;

	gateway_easy = curl_easy_init();
	if (gateway_easy) {
		curl_easy_setopt(gateway_easy, CURLOPT_URL, url.c_str());
		curl_easy_setopt(gateway_easy, CURLOPT_CONNECT_ONLY, 2L); /* websocket style */
		/* provide a buffer to store errors in */
		curl_easy_setopt(gateway_easy, CURLOPT_ERRORBUFFER, DS_errbuf);
#ifdef DONOT_SSL_VERIFY_PEER
		curl_easy_setopt(gateway_easy, CURLOPT_SSL_VERIFYPEER, FALSE);
#endif
		/* set the error buffer as empty before performing a request */
		DS_errbuf[0] = 0;

		if (!multi_handle)
		{
			DS_LogMessageEx("DiscordSync: Error (Multihandle not ready)");
			return -1;
		}
		CURLMcode result = curl_multi_add_handle(multi_handle, gateway_easy);
		if (result != CURLM_OK)
		{
			std::string msg = std::string("DiscordSync: Failed on ( curl_multi_add_handle ) (") + std::to_string(result) + std::string(")");
			DS_LogMessageEx(msg.c_str());
			return result;
		}
		result = curl_multi_perform(multi_handle, &still_running);
		if (result != CURLM_OK)
		{
			std::string msg = std::string("DiscordSync: Failed on ( curl_multi_perform ) (") + std::to_string(result) + std::string(")");
			DS_LogMessageEx(msg.c_str());
			return result;
		}
		if (bDebug)
			DS_LogMessageEx("StartWebsocket in progress");
		return 0;
	}
	else {
		DS_LogMessageEx("DiscordSync: Failed ( curl_easy_init ) (gateway_easy)");
		return -1;
	}
}

uint8_t DiscordSync_OnServerInitialize()
{
	#ifdef WIN32 
	DWORD dwMode = 0;
	//Enable Virtual Terminal Sequences
	 bool bAlternateBuffer = false;
	 GetConsoleMode(
		 GetStdHandle(STD_OUTPUT_HANDLE),
		 &dwMode);
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), dwMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	#endif
	multi_handle=curl_multi_init();
	if (multi_handle)
	{
		int res = StartWebSocket();
		if (res == 0)
		{
			DS_OutputMessage(GREEN, "[MODULE]  ");
			DS_OutputMessage(WHITE, "Loaded DiscordSync v1.1 by habi\n");
			
			//screen (application on linux) will print whatever is in LogMessageEx
			//LogMessageEx("[MODULE]  Loaded DiscordSync v1.0 by habi");
		}
		else
		{
			DS_OutputMessage(RED, "[MODULE]  ");
			DS_OutputMessage(WHITE, "Some error occured while starting discordsync. Not connected.\n");
		}
		
	}
	else
	{
		DS_LogMessageEx("[DiscordSync] Unable to start socket");
		DS_OutputMessage(RED, "[MODULE]  ");
		DS_OutputMessage(WHITE, "Unable to start discordsync\n");
	}
	return 1;
}
//elapsedTime is around 0.0150 means 150 ms.
void DiscordSync_OnServerFrame(float elapsedTime)
{
	if (still_running)
	{
		CURLMcode mresult;
		mresult=curl_multi_perform(multi_handle, &still_running);
		if (mresult != CURLM_OK)
		{
			std::string msg = std::string("[DiscordSync] Failed ( curl_multi_perform ) Code: ") + std::to_string(mresult);
			//Server->LogMessage("DiscordSync: %s",msg.c_str()); 
			DS_LogMessageEx(msg.c_str());
		}

		struct CURLMsg* m; 
		
		bool progress = false;
		do {
			int msgq = 0;
			m = curl_multi_info_read(multi_handle, &msgq);
			if (m && (m->msg == CURLMSG_DONE)) {
				CURL* e = m->easy_handle;
				CURLcode result = m->data.result;
				if (result == CURLE_OK)
				{
					if (e == gateway_easy && !websocket_initialized)
					{
						websocket_initialized = true; 
					}
				}
				else {
	
					//Server->LogMessage("DiscordSync: failed on handle: %p CURLcode: %d\r", e, result); 
					
					char msg[256]; char msg2[256];
					sprintf(msg, "DiscordSync: failed on handle: %p CURLcode: %d %s", e, result,DS_errbuf);
					DS_LogMessageEx(msg);

					if (e == gateway_easy)
						DS_OutputMessage(RED, "[MODULE]  ");
					else
						DS_OutputMessage(YELLOW, "[MODULE]  ");
					sprintf(msg2, "%s discordsync. Status: %s.\n",
						result == CURLE_COULDNT_RESOLVE_HOST ? "Couldn't resolve host," :
						"Error occured in",
						websocket_initialized ? "Not disconnected yet" : "Disconnected");
					DS_OutputMessage(WHITE, msg2);
					//No reconnect.
				}
				//Remove the easy handle
				if (e != gateway_easy)
				{
					mresult = curl_multi_remove_handle(multi_handle, e);
					if (mresult != CURLM_OK)
					{
						std::string msg = "Error while curl_multi_remove_handle. CURLMcode: " + std::to_string(mresult);
						DS_LogMessageEx(msg.c_str());
					}
				}
				//Cleanup the easy handle. 
				if (e != gateway_easy) //we do not want to clean gateway_easy
				{
					curl_easy_cleanup(e);
				}
			}
		} while (m);

		/*if (!still_running)
		{
			//Every transfer completed. Empty the container
			
			sq->pushobject(v, container);
			if (SQ_FAILED(sq->arrayresize(v, -1, 0)))
				Server->LogMessage("DiscordSync: Warning - Array resize failed");
				
			//The userpointer is stored in the array. Resizing it will cause the
			//userpointer to be destructed and the memory it points to will be freed.
			sq->pop(v, 1);//pop the container
			
		}*/
	}
	if (websocket_initialized && gateway_easy)
	{
		recv_any(gateway_easy);

		if (heartbeat_interval != -1 && (DS_GetTickCount() - last_hearbeat_send) > heartbeat_interval) {
			send_heartbeat(gateway_easy);
		}
		if (hello_received && !bsend_identity &&!resuming) {
			send_identity(gateway_easy);
			bsend_identity = true;
		}
		if (heartbeat_ack_wait_time != -1)
		{
			//Means an ack is awaited.
			if (heartbeat_ack_wait_time > 10.000)//10 seconds
			{
				//We waited 10 seconds and no ack received.
				DS_LogMessageEx("[DiscordSync] Reconnecting since heartbeat ack was not received");
				Server->SendPluginCommand(CMD_RECONNECT, "RESUME");
				
			}
			else heartbeat_ack_wait_time += elapsedTime;
		}
		
	}
}

uint8_t DiscordSync_OnPluginCommand(uint32_t commandIdentifier, const char* message)
{
	if (commandIdentifier == 0x7D6E22D8)
	{
		int32_t id =Server->FindPlugin("SQHost2");
		if (id != -1)
		{
			size_t size;
			const void** exports= Server->GetPluginExports(id, &size);
			if (Server->GetLastError() == vcmpErrorNone)
			{
				if (exports != NULL && size > 0)
				{
					SquirrelImports** s = (SquirrelImports**)exports;
					SquirrelImports* f = (SquirrelImports*)(*s);
					if (f)
					{
						sq=*(f->GetSquirrelAPI());
						v=*(f->GetSquirrelVM());
						if (sq && v)
						{
							//Create an array
							/*sq->newarray(v, 0);
							if (SQ_FAILED(sq->getstackobj(v, -1, &container)))
							{
								Server->LogMessage("DiscordSync: failed getting array");
								sq->pop(v, 1); return 0;
							}
							sq->addref(v, &container);
							sq->poptop(v);//pops the array!
							*/

							
							sq->pushroottable(v);
							sq->pushstring(v, "SendMessage", -1);
							sq->newclosure(v, fn_SendMessage, 0);
							sq->setparamscheck(v, 2, "ts");
							sq->newslot(v, -3, SQFalse);
							sq->pop(v, 1);

						}

					}
					
				}
			}
		}
	}
	else if (commandIdentifier == CMD_DISCONNECT)
	{
		if (gateway_easy)
		{
			websocket_close(gateway_easy);
			DiscordSync_CloseConnection();
			DS_OutputMessage(RED, "[MODULE]  ");
			DS_OutputMessage(WHITE, "Disconnected.\n");
			DS_LogMessageEx("[DiscordSync] Disconnected by self.");
			Server->SendPluginCommand(CMD_ON_DISCONNECT, "");
		}
		else if(!error_msg_shown_plgncmd) {
			DS_OutputMessage(YELLOW, "[MODULE]  ");
			DS_OutputMessage(WHITE, "Not connected\n"); 
			error_msg_shown_plgncmd = true;
		}
	}
	else if (commandIdentifier == CMD_RECONNECT)
	{
		DiscordSync_CloseConnection();
		if (multi_handle)
		{
			sequence_number = -1;
			bsend_identity = false;
			if (strcmp(message, "RESUME") == 0 && resume_gateway_url != "")
				StartWebSocket(resume_gateway_url);
			else
				StartWebSocket();
		}
	}
	else if (commandIdentifier == CMD_LOGMSGEX)
	{
		DS_LogMessageEx(message);
	}
	else if (commandIdentifier == CMD_SHOWUPTIME)
	{
		if (websocket_initialized)
		{
			unsigned int now = DS_GetTickCount();
			long elapsed = (long)now - (long)Uptime;
			if (elapsed > 0)
			{
				int days = (int)floor(elapsed / (1000 * 60 * 60 * 24));
				int hours = (int)floor((elapsed % (1000 * 60 * 60 * 24))/(1000*60*60));
				int minutes = (int)floor((elapsed % (1000 * 60 * 60)) / (1000 * 60));
				int seconds = (int)floor((elapsed % (1000 * 60 )) / (1000 ));
				std::string str = days > 0 ? (std::to_string(days) + std::string(" days, ")) :"";
				str += (days > 0 || hours > 0) ? (std::to_string(hours) + std::string(" hours, ")) : "";
				str += std::to_string(minutes) + " minutes, ";
				str += std::to_string(seconds) + " seconds.\n";

				DS_OutputMessage(GREEN, "[MODULE]  ");
				DS_OutputMessage(WHITE, "Uptime: ");
				DS_OutputMessage(WHITE, str.c_str());
			}
		}
		else
		{
			DS_OutputMessage(YELLOW, "[MODULE]  ");
			DS_OutputMessage(WHITE, "Not connected\n");
		}
	}
	else if (commandIdentifier == CMD_RESUME)
	{
		if (!websocket_initialized)
		{
			if (resume_gateway_url != "")
			{
				DiscordSync_CloseConnection();
				StartWebSocket(resume_gateway_url);
				resuming = true;
			}
			else {
				char msg[128];
				sprintf(msg, "Resume not possible. Try 0x%x\n", CMD_RECONNECT);
				DS_OutputMessage(YELLOW, "[MODULE]  ");
				DS_OutputMessage(WHITE, msg);
			}
		}
		else {
			DS_OutputMessage(YELLOW, "[MODULE]  ");
			DS_OutputMessage(WHITE, "First disconnect to resume.\n");
		}
	}
	else if (commandIdentifier == CMD_STATUS)
	{
		DS_OutputMessage(websocket_initialized?GREEN:RED, "[MODULE]  ");
		DS_OutputMessage(WHITE, "Status: ");
		websocket_initialized ? DS_OutputMessage(WHITE, "Connected\n") : DS_OutputMessage(WHITE, "Disconnected\n");
	}
	else if (commandIdentifier == CMD_CHANNEL)
	{
		channelID = std::string(message);
	}
	else if (commandIdentifier == CMD_TOKEN)
	{
	Token = std::string(message);
	}
	else if (commandIdentifier == CMD_SUBSCRIBE)
	{
	subscribed = true;
	}
	else if (commandIdentifier == CMD_UNSUBSCRIBE)
	{
	subscribed = false;
	}
	else if (commandIdentifier == CMD_SENDMSG)
	{
		if (websocket_initialized)
		{
			DS_send_message(message);
		}
		else if (!error_msg_shown &&!shutting_down)
		{
			DS_OutputMessage(YELLOW, "[MODULE]  ");
			char msg[128];
			sprintf(msg, "Status: Disconnected. Use 0x%x to reconnect\n", CMD_RECONNECT);
			DS_OutputMessage(WHITE, msg);
		}
	}
	else if (commandIdentifier == CMD_VERBOSE)
	{
		verbose = true;
		if (strlen(message) > 0 && strcmp(message, "off") == 0)
			verbose = false;
	}
	return 1;
}
void DiscordSync_OnServerShutdown(void)
{
	shutting_down = true;
	websocket_close(gateway_easy);
	DiscordSync_CloseConnection();
	if (multi_handle)
	{
		curl_multi_cleanup(multi_handle);
		multi_handle = NULL;
	}
}
#ifdef WIN32
extern "C" __declspec(dllexport) unsigned int VcmpPluginInit(PluginFuncs * pluginFuncs, PluginCallbacks * pluginCallbacks, PluginInfo * pluginInfo) {
#else
extern "C"  unsigned int VcmpPluginInit(PluginFuncs * pluginFuncs, PluginCallbacks * pluginCallbacks, PluginInfo * pluginInfo) {
#endif
	Server=pluginFuncs;
	Callbacks = pluginCallbacks;
	// Plugin information
	pluginInfo->pluginVersion = 0x1;
	pluginInfo->apiMajorVersion = PLUGIN_API_MAJOR;
	pluginInfo->apiMinorVersion = PLUGIN_API_MINOR;
	memcpy(pluginInfo->name, "DiscordSync", strlen("DiscordSync")+1);
	//pluginCallbacks->OnServerInitialise = DiscordSync_OnServerInitialize;
	pluginCallbacks->OnPluginCommand = DiscordSync_OnPluginCommand;
	pluginCallbacks->OnServerFrame = DiscordSync_OnServerFrame;
	pluginCallbacks->OnServerInitialise = DiscordSync_OnServerInitialize;
	pluginCallbacks->OnServerShutdown = DiscordSync_OnServerShutdown;

#ifdef WIN32
	//Get console handl
	hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
	//Read CFG Values  -First bot token
	char* szValue;
	cfg botToken;
	botToken.read("server.cfg", "token");

	if(botToken.argc > 0 )
	{
		szValue = botToken.ptr[0];
		Token=std::string(szValue);
	}else 
	{
		DS_OutputMessage(RED, "[MODULE]  ");
		DS_OutputMessage(WHITE, "DiscordSync : token not found in server.cfg\n");
		return 0;
	}
	botToken.freememory();
	
	//Channel ID
	cfg channel;
	channel.read("server.cfg", "channel");

	if(channel.argc > 0 )
	{
		szValue = channel.ptr[0];
		channelID=std::string(szValue);
	}else
	{
		DS_OutputMessage(RED, "[MODULE]  ");
		DS_OutputMessage(WHITE, "DiscordSync : channel not found in server.cfg\n");
		return 0;
	}
	channel.freememory();

	//Gateway URL
	cfg gateway;
	gateway.read("server.cfg", "gateway");

	if (gateway.argc > 0)
	{
		szValue = gateway.ptr[0];
		GatewayUrl = std::string(szValue);
	}
	gateway.freememory();
	
	//Http URL
	cfg discordhttp;
	discordhttp.read("server.cfg", "discordhttp");

	if (discordhttp.argc > 0)
	{
		szValue = discordhttp.ptr[0];
		HttpUrl = std::string(szValue);
	}
	discordhttp.freememory();
	
	cfg debug;
	debug.read("server.cfg", "debug");
	if (debug.argc > 0)
	{
		szValue = debug.ptr[0];
		std::string szdebug = std::string(szValue);
		if (szdebug == "true" || szdebug == "1")
		{
			bDebug = true;
			DS_LogMessageEx("[DiscordSync]debug true in server.cfg. Will print additional messages");
		}
	}
	return 1;
}
