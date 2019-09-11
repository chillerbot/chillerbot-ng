/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/string.h>

#include <engine/engine.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/client/gameclient.h>

#include <game/client/components/scoreboard.h>
#include <game/localization.h>
#include <game/version.h>

#include "chat.h"


CChat::CChat()
{
	for(int i = 0; i < MAX_LINES; i++)
	{
		// reset the container indices, so the text containers can be deleted on reset
		m_aLines[i].m_TextContainerIndex = -1;
	}
	OnReset();
}

void CChat::OnWindowResize()
{
	for(int i = 0; i < MAX_LINES; i++)
	{
		m_aLines[i].m_TextContainerIndex = -1;
	}
}

void CChat::OnReset()
{
	for(int i = 0; i < MAX_LINES; i++)
	{
		m_aLines[i].m_Time = 0;
		m_aLines[i].m_aText[0] = 0;
		m_aLines[i].m_aName[0] = 0;
		m_aLines[i].m_Friend = false;
		m_aLines[i].m_TextContainerIndex = -1;
	}
	m_PrevScoreBoardShowed = false;
	m_PrevShowChat = false;

	m_ReverseTAB = false;
	m_Mode = MODE_NONE;
	m_Show = false;
	m_InputUpdate = false;
	m_ChatStringOffset = 0;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = 0x0;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;

	for(int i = 0; i < CHAT_NUM; ++i)
		m_aLastSoundPlayed[i] = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
	{
		m_Mode = MODE_NONE;
		Input()->SetIMEState(false);
		for(int i = 0; i < MAX_LINES; i++)
		{
			m_aLines[i].m_Time = 0;
		}
		m_CurrentLine = 0;
	}
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat*)pUserData)->Say(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat*)pUserData)->Say(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat*)pUserData)->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		((CChat*)pUserData)->EnableMode(1);
	else
		((CChat*)pUserData)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all or team as mode");

	if(pResult->GetString(1)[0] || g_Config.m_ClChatReset)
		((CChat*)pUserData)->m_Input.Set(pResult->GetString(1));
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->AddLine(-2, 0, pResult->GetString(0));
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT, ConEcho, this, "Echo the text in chat window");
}

bool CChat::OnInput(IInput::CEvent Event)
{
	return false;
}


void CChat::EnableMode(int Team)
{
  return;
}

void CChat::ChillerbotMessage(const char *pMsg, int ClientID)
{
	char aBuf[2048];
	if(str_find_nocase(pMsg, "bot?") ||
		str_find_nocase(pMsg, "info();") ||
		str_find_nocase(pMsg, "version();") ||
		str_find_nocase(pMsg, "help();") ||
		str_find_nocase(pMsg, "!help") ||
		str_find_nocase(pMsg, "!info"))
	{
		str_format(aBuf, sizeof(aBuf), "[chillerbot-ng] chillerbot.png headless (v%s)", CHILLERBOT_VERSION);
		Say(0, aBuf);
	}
	else if(str_find_nocase(pMsg, "I was a faithful friend from the start. The best friend? No. But I was a faithful friend. From map to map, I helped jao get across hard barriers. We talked and talked, about the future, what teeworlds could hold. We had wonderful moments together, ..."))
	{
		Say(0, "... memories that would last a lifetime. Whenever I would see him on a server, a new journey would spark up as we explored the wonders of each map. And now? he won't fucking transfer my points");
	}

	if(ClientID == -1) // server message
	{
		if(m_pClient->m_RequestCmdlist > 0)
		{
			// int64 SecsPassed = -(m_pClient->m_RequestCmdlist - time_get()) / time_freq();
			// str_format(aBuf, sizeof(aBuf), "sent message %lld secs ago", SecsPassed);
			// Say(0, aBuf);
			// dbg_msg("pentest", "cmdlist: %s", pMsg);
			str_copy(aBuf, pMsg, sizeof(aBuf));
			char aToken[64];
			for(const char *tok = aBuf; (tok = str_next_token(tok, ",", aToken, sizeof(aToken)));)
			{
				char *pBuf = (char*)malloc(64*sizeof(char));
				str_copy(pBuf, str_skip_whitespaces(aToken), 64);
				m_pClient->m_vChatCmds.push_back(pBuf);
				// dbg_msg("pentest", "found chat command: %s", pBuf);
			}
		}
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		AddLine(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
		ChillerbotMessage(pMsg->m_pMessage, pMsg->m_ClientID);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_find_nocase(pLine, pName);

	if(pHL)
	{
		int Length = str_length(pName);

		if((pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;

	}

	return false;
}

void CChat::AddLine(int ClientID, int Team, const char *pLine)
{
	if(*pLine == 0 ||
		(ClientID == -1 && !g_Config.m_ClShowChatSystem) ||
		(ClientID >= 0 && (m_pClient->m_aClients[ClientID].m_aName[0] == '\0' || // unknown client
		m_pClient->m_aClients[ClientID].m_ChatIgnore ||
		(m_pClient->m_Snap.m_LocalClientID != ClientID && g_Config.m_ClShowChatFriends && !m_pClient->m_aClients[ClientID].m_Friend) ||
		(m_pClient->m_Snap.m_LocalClientID != ClientID && m_pClient->m_aClients[ClientID].m_Foe))))
		return;

	// trim right and set maximum length to 256 utf8-characters
	int Length = 0;
	const char *pStr = pLine;
	const char *pEnd = 0;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = 0;
		}
		else if(pEnd == 0)
			pEnd = pStrOld;

		if(++Length >= 256)
		{
			*(const_cast<char *>(pStr)) = 0;
			break;
		}
	}
	if(pEnd != 0)
		*(const_cast<char *>(pEnd)) = 0;

	bool Highlighted = false;
	char *p = const_cast<char*>(pLine);
	while(*p)
	{
		Highlighted = false;
		pLine = p;
		// find line separator and strip multiline
		while(*p)
		{
			if(*p++ == '\n')
			{
				*(p-1) = 0;
				break;
			}
		}

		m_CurrentLine = (m_CurrentLine+1)%MAX_LINES;
		m_aLines[m_CurrentLine].m_Time = time_get();
		m_aLines[m_CurrentLine].m_YOffset[0] = -1.0f;
		m_aLines[m_CurrentLine].m_YOffset[1] = -1.0f;
		m_aLines[m_CurrentLine].m_ClientID = ClientID;
		m_aLines[m_CurrentLine].m_Team = Team;
		m_aLines[m_CurrentLine].m_NameColor = -2;

		m_aLines[m_CurrentLine].m_TextContainerIndex = -1;

		// check for highlighted name
		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		{
			if(ClientID != m_pClient->m_LocalIDs[0])
			{
				// main character
				if(LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
					Highlighted = true;
				// dummy
				if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
					Highlighted = true;
			}
		}
		else
		{
			// on demo playback use local id from snap directly,
			// since m_LocalIDs isn't valid there
			if(LineShouldHighlight(pLine, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName))
				Highlighted = true;
		}


		m_aLines[m_CurrentLine].m_Highlighted = Highlighted;

		if(ClientID < 0) // server or client message
		{
			str_copy(m_aLines[m_CurrentLine].m_aName, "*** ", sizeof(m_aLines[m_CurrentLine].m_aName));
			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), "%s", pLine);
		}
		else
		{
			if(m_pClient->m_aClients[ClientID].m_Team == TEAM_SPECTATORS)
				m_aLines[m_CurrentLine].m_NameColor = TEAM_SPECTATORS;

			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
			{
				if(m_pClient->m_aClients[ClientID].m_Team == TEAM_RED)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_RED;
				else if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_BLUE;
			}

			if(Team == 2) // whisper send
			{
				str_format(m_aLines[m_CurrentLine].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName), "→ %s", m_pClient->m_aClients[ClientID].m_aName);
				m_aLines[m_CurrentLine].m_NameColor = TEAM_BLUE;
				m_aLines[m_CurrentLine].m_Highlighted = false;
				m_aLines[m_CurrentLine].m_Team = 0;
				Highlighted = false;
			}
			else if(Team == 3) // whisper recv
			{
				str_format(m_aLines[m_CurrentLine].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName), "← %s", m_pClient->m_aClients[ClientID].m_aName);
				m_aLines[m_CurrentLine].m_NameColor = TEAM_RED;
				m_aLines[m_CurrentLine].m_Highlighted = true;
				m_aLines[m_CurrentLine].m_Team = 0;
				Highlighted = true;
			}
			else
				str_copy(m_aLines[m_CurrentLine].m_aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(m_aLines[m_CurrentLine].m_aName));

			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), ": %s", pLine);
			m_aLines[m_CurrentLine].m_Friend = m_pClient->m_aClients[ClientID].m_Friend;
		}

		m_aLines[m_CurrentLine].m_Friend = ClientID >= 0 ? m_pClient->m_aClients[ClientID].m_Friend : false;

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "%s%s", m_aLines[m_CurrentLine].m_aName, m_aLines[m_CurrentLine].m_aText);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, Team >= 2?"whisper":(m_aLines[m_CurrentLine].m_Team?"teamchat":"chat"), aBuf, Highlighted);
	}
}

void CChat::OnPrepareLines()
{
	int64 Now = time_get();
	for(int i = 0; i < MAX_LINES; i++)
	{

		int r = ((m_CurrentLine - i) + MAX_LINES) % MAX_LINES;
		if(Now > m_aLines[r].m_Time + 16 * time_freq() && !m_Show)
			break;

		char aName[64] = "";
		if(g_Config.m_ClShowIDs && m_aLines[r].m_ClientID != -1 && m_aLines[r].m_aName[0] != '\0')
		{
			if(m_aLines[r].m_ClientID >= 10)
				str_format(aName, sizeof(aName), "%d: ", m_aLines[r].m_ClientID);
			else
				str_format(aName, sizeof(aName), " %d: ", m_aLines[r].m_ClientID);
			str_append(aName, m_aLines[r].m_aName, sizeof(aName));
		}
		else
		{
			str_copy(aName, m_aLines[r].m_aName, sizeof(aName));
		}
  }
    /*
    m_aLines[r].m_ClientID 
    m_aLines[r].m_Team
    m_aLines[r].m_NameColor == TEAM_RED
    */
}

void CChat::OnRender()
{
	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend+time_freq() < time_get())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter-1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				Say(pEntry->m_Team, pEntry->m_aText);
				break;
			}
		}
		--m_PendingChatCounter;
	}
}

void CChat::Say(int Team, const char *pLine)
{
	m_LastChatSend = time_get();

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CChat::SayChat(const char *pLine)
{
	if(!pLine || str_length(pLine) < 1)
		return;

	bool AddEntry = false;

	if(m_LastChatSend+time_freq() < time_get())
	{
		Say(m_Mode == MODE_ALL ? 0 : 1, pLine);
		AddEntry = true;
	}
	else if(m_PendingChatCounter < 3)
	{
		++m_PendingChatCounter;
		AddEntry = true;
	}

	if(AddEntry)
	{
		CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry)+str_length(pLine)-1);
		pEntry->m_Team = m_Mode == MODE_ALL ? 0 : 1;
		mem_copy(pEntry->m_aText, pLine, str_length(pLine));
	}
}
