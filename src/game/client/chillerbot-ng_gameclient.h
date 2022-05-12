#ifndef GAME_CLIENT_CHILLERBOT_NG_GAMECLIENT_H
#define GAME_CLIENT_CHILLERBOT_NG_GAMECLIENT_H

#include "gameclient.h"

int m_ThreadInpState;
bool SaneTTY = true;
char m_aThreadInputBuf[2048];
enum {
	THREAD_INPUT_READY,
	THREAD_INPUT_BLOCK,
	THREAD_INPUT_DONE
};

void CGameClient::StartInputThread(int mode)
{
	m_InputMode = mode;
	if (m_ThreadInpState != THREAD_INPUT_READY)
	{
		printf("ERROR: input thread already running.\n");
		return;
	}
	if (m_InputMode == INPUT_RCON_CONSOLE)
	{

		if (!Client()->RconAuthed())
			printf("password> ");
		else
			printf("rcon> ");
	}
	else if (m_InputMode == INPUT_LOCAL_CONSOLE)
		printf("console> ");
	else if (m_InputMode == INPUT_CHILLER_CONSOLE)
		printf(":");
	m_ThreadInpState = THREAD_INPUT_BLOCK;
	thread_init(*ConsoleKeyInputThread, NULL);
}

void ConsoleKeyInputThread(void *pArg)
{
#if defined(CONF_FAMILY_UNIX)
	system("stty sane");
#endif
	m_aThreadInputBuf[0] = 0;
	fgets(m_aThreadInputBuf, sizeof(m_aThreadInputBuf), stdin);
	m_ThreadInpState = THREAD_INPUT_DONE;
	SaneTTY = true;
}

void CGameClient::ShowServerList()
{
	int j = 0;
	for (int i = 0;; i++)
	{
		const CServerInfo *pInfo = m_pServerBrowser->SortedGet(i);
		if (!pInfo)
		{
			if (!i)
				printf("press 'l' to load/refresh server browser first.\n");
			return;
		}

		j++;
		if (j > 20)
			return;

		char aPlayers[10];
		str_format(aPlayers, sizeof(aPlayers), "[%d/%d]", pInfo->m_NumClients, pInfo->m_MaxClients);
		printf("%-64s %-10s [%s]\n", pInfo->m_aName, aPlayers, pInfo->m_aAddress);
	}
}

void CGameClient::PenetrateServer()
{
	if (!g_Config.m_ClPenTest)
		return;

	if (m_RequestCmdlist < 0) // waiting to send
	{
		if (time_get() >= -m_RequestCmdlist)
		{
			m_pChat->SayChat("/cmdlist");
			m_RequestCmdlist = time_get();
		}
		// dbg_msg("pentest", "time=%lld req=%lld diff=%lld", time_get(), -m_RequestCmdlist, (-m_RequestCmdlist - time_get()) / time_freq());
		return;
	}
	else if (m_RequestCmdlist)
	{
		int64 SecsPassed = -(m_RequestCmdlist - time_get()) / time_freq();
		// str_format(aBuf, sizeof(aBuf), "sent message %lld secs ago", SecsPassed);
		// m_pChat->SayChat(aBuf);
		if (SecsPassed > 1)
		{
			char aBuf[2048];
			str_copy(aBuf, "", sizeof(aBuf));
			for(std::vector<char*>::size_type i = 0; i != m_vChatCmds.size(); i++)
			{
				str_append(aBuf, m_vChatCmds[i], sizeof(aBuf));
				// dbg_msg("pentest", "append chat cmd=%s", m_vChatCmds[i]);
			}
			// m_pChat->SayChat(aBuf);
			dbg_msg("pentest", "found chat cmds=%s", aBuf);
			// m_pChat->SayChat("stopped waiting.");
			m_RequestCmdlist = 0; // finished waiting for response
		}
		return;
	}

	m_PenDelay--;
	if (m_PenDelay > 0)
		return;
	m_PenDelay = 100 + rand() % 50;

	// chat messages
	if (rand() % 2) // parsed chat cmds
	{
		char aChatCmd[128];
		char aArg[64];
		static const char *pCharset = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"§$%&/()=?{[]}\\<>|-.,;:+#*'~'@_/";
		str_copy(aArg, "", sizeof(aArg));
		int len = rand() % 64;
		for (int i = 0; i < len; i++)
		{
			char buf[2];
			str_format(buf, sizeof(buf), "%c", pCharset[rand() % strlen(pCharset)]);
			str_append(aArg, buf, sizeof(aArg));
		}
		str_format(aChatCmd, sizeof(aChatCmd), "/%s %s", GetRandomChatCommand(), aArg);
		m_pChat->SayChat(aChatCmd);
	}
	else // file messages
	{
		const char *pMessage = GetPentestCommand(g_Config.m_ClPenTestFile);
		if (pMessage)
		{
			m_pChat->SayChat(pMessage);
		}
		else
		{
			const int NUM_CMDS = 3;
			char aaCmds[NUM_CMDS][512] = {
				"/register xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx 12831237189237189231982371938712893798",
				"todo: configure me ( pentest file not found)",
				"/login bang baz baz"
			};
			m_pChat->SayChat(aaCmds[rand() % NUM_CMDS]);
		}
	}



	// kill and reconnect and vote
	int r = rand() % 50;
	if (r == 2)
		SendKill(-1);
	if (r > 3 && r < 6)
		m_pVoting->Vote(rand() % 2 ? -1 : 1);
	if (r == 1)
		m_pClient->Connect(g_Config.m_DbgStressServer);
}

const char *CGameClient::GetRandomChatCommand()
{
	if (!m_vChatCmds.size())
		return 0;
	return m_vChatCmds[rand() % m_vChatCmds.size()];
}

const char *CGameClient::GetPentestCommand(char const *pFileName)
{
	IOHANDLE File = m_pStorage->OpenFile(pFileName, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
		return 0;

	std::vector<char*> v;
	char *pLine;
	CLineReader *lr = new CLineReader();
	lr->Init(File);
	while((pLine = lr->Get()))
		if(str_length(pLine))
			if(pLine[0]!='#')
				v.push_back(pLine);
	io_close(File);
	static const char *pCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"§$%&/()=?{[]}\\<>|-.,;:+#*'~'@_/";
	char *pMessage = v[rand() % v.size()];
	for(int i = 0; pMessage[i] != 0; i++)
	{
		if(pMessage[i] == '?')
			pMessage[i] = pCharset[rand() % strlen(pCharset)];
	}
	return pMessage;
}

void CGameClient::ChillerCommands(const char *pCmd)
{
	if (!str_comp_nocase("help", pCmd))
	{
		printf(":q to quit\n");
		printf(":pentest to spam server with commands\n");
	}
	else if (!str_comp_nocase("q", pCmd) || !str_comp_nocase("quit", pCmd))
		m_pClient->Quit();
	else if (!str_comp_nocase("pen", pCmd) || !str_comp_nocase("pentest", pCmd) || !str_comp_nocase("penetrate", pCmd))
		g_Config.m_ClPenTest ^= 1;
	else
		printf("unkown png cmd try :help\n");
}

void CGameClient::ConsoleKeyInput()
{
	if (!g_Config.m_ClChillerInput)
		return;
	if (m_ThreadInpState == THREAD_INPUT_BLOCK)
		return;

	char key = '0';
#if defined(CONF_PLATFORM_MACOSX)
	key = getch();
#elif defined(CONF_FAMILY_UNIX)
	if (SaneTTY)
	{
		system("stty -icanon time 1 min 0"); // make sure to never get stuck on getchar()
		SaneTTY = false;
	}
	key = getchar();
#elif defined _WIN32
	if (_kbhit() == 1)
		key = getch();
#endif

	if (key == 'a')
		g_Config.m_ClChillerDir = g_Config.m_ClChillerDir == -1 ? 0 : -1;
	else if (key == 'd')
		g_Config.m_ClChillerDir = g_Config.m_ClChillerDir == 1 ? 0 : 1;
	else if (key == ' ')
		g_Config.m_ClChillerJmp = 5; // jump 5 ticks
	else if (key == 'k')
		SendKill(-1);
	else if (key == 'l')
		m_pServerBrowser->Refresh(1); // 1 = TYPE_INTERNET
	else if (key == 'b')
		ShowServerList();
	else if (key == 9)
		m_pScoreboard->DoChillerRender();
	else if (key == 'x')
		m_pChat->SayChat("xd");
	else if (key == 'v') // view
		g_Config.m_ClChillerRender = g_Config.m_ClChillerRender ? 0 : 1;
	else if (key == 't')
		StartInputThread(INPUT_CHAT);
	else if (key == 'c')
		StartInputThread(INPUT_LOCAL_CONSOLE);
	else if (key == 'r')
		StartInputThread(INPUT_RCON_CONSOLE);
	else if (key == ':')
		StartInputThread(INPUT_CHILLER_CONSOLE);
	else if (key != 48 && key != -1) // empty key
		printf("key %d not found\n", key);

	if (m_ThreadInpState == THREAD_INPUT_DONE)
	{
		m_ThreadInpState = THREAD_INPUT_READY;
		strip_last_char(m_aThreadInputBuf);
		if (m_InputMode == INPUT_CHAT)
			m_pChat->Say(0, m_aThreadInputBuf);
		else if (m_InputMode == INPUT_RCON_CONSOLE)
		{
			if (!Client()->RconAuthed())
				Client()->RconAuth(m_aThreadInputBuf, m_aThreadInputBuf);
			else
				Client()->Rcon(m_aThreadInputBuf);
		}
		else if (m_InputMode == INPUT_LOCAL_CONSOLE)
			Console()->ExecuteLine(m_aThreadInputBuf);
		else if (m_InputMode == INPUT_CHILLER_CONSOLE)
			ChillerCommands(m_aThreadInputBuf);
		else
			printf("invalid input mode %d your msg from thread: %s\n", m_InputMode, m_aThreadInputBuf);
	}
}

void CGameClient::ChillerBotTick()
{
	ConsoleKeyInput();
	PenetrateServer();
	if (g_Config.m_ClShutdownSrv[0] && m_EnterGameTime)
	{
		if (!Client()->RconAuthed() && (time_get() - m_EnterGameTime) / time_freq() > 10) // wait 10 sec to authenticate
		{
			Client()->RconAuth(g_Config.m_ClShutdownSrv, g_Config.m_ClShutdownSrv);
			dbg_msg("shutdown", "authed in rcon with password='%s'", g_Config.m_ClShutdownSrv);
		}
		if ((time_get() - m_EnterGameTime) / time_freq() > 40) // wait 40 sec to shutdown server
		{
			Client()->Rcon("shutdown");
			g_Config.m_ClShutdownSrv[0] = '\0';
			dbg_msg("shutdown", "executed shutdown server");
		}
		/*
		if (time_get() % time_freq() == 0)
			dbg_msg("shutdown", "on server since %lld seconds.", (time_get() - m_EnterGameTime) / time_freq());
		*/
	}
}

#endif
