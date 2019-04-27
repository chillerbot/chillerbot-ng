/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/sorted_array.h>

#include <math.h>
#include <limits.h>

#include <game/generated/client_data.h>

#include <base/system.h>

#include <engine/serverbrowser.h>
#include <engine/shared/ringbuffer.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/keys.h>
#include <engine/console.h>

#include <cstring>
#include <cstdio>

#include <game/version.h>

#include <game/client/lineinput.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>

#include "console.h"

CGameConsole::CInstance::CInstance(int Type)
{
	m_pHistoryEntry = 0x0;

	m_Type = Type;

	if(Type == CGameConsole::CONSOLETYPE_LOCAL)
		m_CompletionFlagmask = CFGFLAG_CLIENT;
	else
		m_CompletionFlagmask = CFGFLAG_SERVER;

	m_aCompletionBuffer[0] = 0;
	m_CompletionChosen = -1;
	m_CompletionRenderOffset = 0.0f;
	m_ReverseTAB = false;

	m_aUser[0] = '\0';
	m_UserGot = false;
	m_UsernameReq = false;

	m_IsCommand = false;
}

void CGameConsole::CInstance::Init(CGameConsole *pGameConsole)
{
	m_pGameConsole = pGameConsole;
}

void CGameConsole::CInstance::ClearBacklog()
{
	m_Backlog.Init();
	m_BacklogActPage = 0;
}

void CGameConsole::CInstance::ClearHistory()
{
	m_History.Init();
	m_pHistoryEntry = 0;
}

void CGameConsole::CInstance::ExecuteLine(const char *pLine)
{
	if(m_Type == CGameConsole::CONSOLETYPE_LOCAL)
		m_pGameConsole->m_pConsole->ExecuteLine(pLine);
	else
	{
		if(m_pGameConsole->Client()->RconAuthed())
			m_pGameConsole->Client()->Rcon(pLine);
		else
		{
			if(!m_UserGot && m_UsernameReq)
			{
				m_UserGot = true;
				str_copy(m_aUser, pLine, sizeof m_aUser);
			}
			else
			{
				m_pGameConsole->Client()->RconAuth(m_aUser, pLine);
				m_UserGot = false;
			}
		}
	}
}

void CGameConsole::CInstance::PossibleCommandsCompleteCallback(const char *pStr, void *pUser)
{
	CGameConsole::CInstance *pInstance = (CGameConsole::CInstance *)pUser;
	if(pInstance->m_CompletionChosen == pInstance->m_CompletionEnumerationCount)
		pInstance->m_Input.Set(pStr);
	pInstance->m_CompletionEnumerationCount++;
}

void CGameConsole::CInstance::OnInput(IInput::CEvent Event)
{
	bool Handled = false;

	if(m_pGameConsole->Input()->KeyIsPressed(KEY_LCTRL) && m_pGameConsole->Input()->KeyPress(KEY_V))
	{
		const char *Text = m_pGameConsole->Input()->GetClipboardText();
		if(Text)
		{
			char Line[256];
			int i, Begin = 0;
			for(i = 0; i < str_length(Text); i++)
			{
				if(Text[i] == '\n')
				{
					if(i == Begin)
					{
						Begin++;
						continue;
					}
					int max = min(i - Begin + 1, (int)sizeof(Line));
					str_copy(Line, Text + Begin, max);
					Begin = i+1;
					ExecuteLine(Line);
				}
			}
			int max = min(i - Begin + 1, (int)sizeof(Line));
			str_copy(Line, Text + Begin, max);
			Begin = i+1;
			m_Input.Add(Line);
		}
	}

	if(m_pGameConsole->Input()->KeyIsPressed(KEY_LCTRL) && m_pGameConsole->Input()->KeyPress(KEY_C))
	{
		m_pGameConsole->Input()->SetClipboardText(m_Input.GetString());
	}

	if(Event.m_Flags&IInput::FLAG_PRESS)
	{
		if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
		{
			if(m_Input.GetString()[0] || (m_UsernameReq && !m_pGameConsole->Client()->RconAuthed() && !m_UserGot))
			{
				if(m_Type == CONSOLETYPE_LOCAL || m_pGameConsole->Client()->RconAuthed())
				{
					char *pEntry = m_History.Allocate(m_Input.GetLength()+1);
					mem_copy(pEntry, m_Input.GetString(), m_Input.GetLength()+1);
				}
				ExecuteLine(m_Input.GetString());
				m_Input.Clear();
				m_pHistoryEntry = 0x0;
			}

			Handled = true;
		}
		else if (Event.m_Key == KEY_UP)
		{
			if (m_pHistoryEntry)
			{
				char *pTest = m_History.Prev(m_pHistoryEntry);

				if (pTest)
					m_pHistoryEntry = pTest;
			}
			else
				m_pHistoryEntry = m_History.Last();

			if (m_pHistoryEntry)
				m_Input.Set(m_pHistoryEntry);
			Handled = true;
		}
		else if (Event.m_Key == KEY_DOWN)
		{
			if (m_pHistoryEntry)
				m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

			if (m_pHistoryEntry)
				m_Input.Set(m_pHistoryEntry);
			else
				m_Input.Clear();
			Handled = true;
		}
		else if(Event.m_Key == KEY_TAB)
		{
			if(m_Type == CGameConsole::CONSOLETYPE_LOCAL || m_pGameConsole->Client()->RconAuthed())
			{
				if(m_ReverseTAB)
					 m_CompletionChosen--;
				else
					m_CompletionChosen++;
				m_CompletionEnumerationCount = 0;
				m_pGameConsole->m_pConsole->PossibleCommands(m_aCompletionBuffer, m_CompletionFlagmask, m_Type != CGameConsole::CONSOLETYPE_LOCAL &&
					m_pGameConsole->Client()->RconAuthed() && m_pGameConsole->Client()->UseTempRconCommands(),	PossibleCommandsCompleteCallback, this);

				// handle wrapping
				if(m_CompletionEnumerationCount && (m_CompletionChosen >= m_CompletionEnumerationCount || m_CompletionChosen <0))
				{
					m_CompletionChosen= (m_CompletionChosen + m_CompletionEnumerationCount) %  m_CompletionEnumerationCount;
					m_CompletionEnumerationCount = 0;
					m_pGameConsole->m_pConsole->PossibleCommands(m_aCompletionBuffer, m_CompletionFlagmask, m_Type != CGameConsole::CONSOLETYPE_LOCAL &&
						m_pGameConsole->Client()->RconAuthed() && m_pGameConsole->Client()->UseTempRconCommands(),	PossibleCommandsCompleteCallback, this);
				}
			}
		}
		else if(Event.m_Key == KEY_PAGEUP)
		{
			++m_BacklogActPage;
		}
		else if(Event.m_Key == KEY_PAGEDOWN)
		{
			--m_BacklogActPage;
			if(m_BacklogActPage < 0)
				m_BacklogActPage = 0;
		}
		else if(Event.m_Key == KEY_HOME)
		{
			m_BacklogActPage = INT_MAX;
		}
		else if(Event.m_Key == KEY_END)
		{
			m_BacklogActPage = 0;
		}
		else if(Event.m_Key == KEY_LSHIFT)
		{
			m_ReverseTAB = true;
			Handled = true;
		}
	}
	if(Event.m_Flags&IInput::FLAG_RELEASE && Event.m_Key == KEY_LSHIFT)
	{
		m_ReverseTAB = false;
		Handled = true;
	}

	if(!Handled)
		m_Input.ProcessInput(Event);

	if(Event.m_Flags & (IInput::FLAG_PRESS|IInput::FLAG_TEXT))
	{
		if((Event.m_Key != KEY_TAB) && (Event.m_Key != KEY_LSHIFT))
		{
			m_CompletionChosen = -1;
			str_copy(m_aCompletionBuffer, m_Input.GetString(), sizeof(m_aCompletionBuffer));
			m_CompletionRenderOffset = 0.0f;
		}

		// find the current command
		{
			char aBuf[64] = {0};
			const char *pSrc = GetString();
			int i = 0;
			for(; i < (int)sizeof(aBuf)-1 && *pSrc && *pSrc != ' '; i++, pSrc++)
				aBuf[i] = *pSrc;
			aBuf[i] = 0;

			const IConsole::CCommandInfo *pCommand = m_pGameConsole->m_pConsole->GetCommandInfo(aBuf, m_CompletionFlagmask,
				m_Type != CGameConsole::CONSOLETYPE_LOCAL && m_pGameConsole->Client()->RconAuthed() && m_pGameConsole->Client()->UseTempRconCommands());
			if(pCommand)
			{
				m_IsCommand = true;
				str_copy(m_aCommandName, pCommand->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
				str_copy(m_aCommandHelp, pCommand->m_pHelp, IConsole::TEMPCMD_HELP_LENGTH);
				str_copy(m_aCommandParams, pCommand->m_pParams, IConsole::TEMPCMD_PARAMS_LENGTH);
			}
			else
				m_IsCommand = false;
		}
	}
}

void CGameConsole::CInstance::PrintLine(const char *pLine, bool Highlighted)
{
	int Len = str_length(pLine);

	if (Len > 255)
		Len = 255;

	CBacklogEntry *pEntry = m_Backlog.Allocate(sizeof(CBacklogEntry)+Len);
	pEntry->m_YOffset = -1.0f;
	pEntry->m_Highlighted = Highlighted;
	mem_copy(pEntry->m_aText, pLine, Len);
	pEntry->m_aText[Len] = 0;
	dbg_msg("console-ng", pLine); // TODO: print only rcon to not double local
}

CGameConsole::CGameConsole()
: m_LocalConsole(CONSOLETYPE_LOCAL), m_RemoteConsole(CONSOLETYPE_REMOTE)
{
	m_ConsoleType = CONSOLETYPE_LOCAL;
	m_ConsoleState = CONSOLE_CLOSED;
	m_StateChangeEnd = 0.0f;
	m_StateChangeDuration = 0.1f;
}

float CGameConsole::TimeNow()
{
	static long long s_TimeStart = time_get();
	return float(time_get()-s_TimeStart)/float(time_freq());
}

CGameConsole::CInstance *CGameConsole::CurrentConsole()
{
	if(m_ConsoleType == CONSOLETYPE_REMOTE)
		return &m_RemoteConsole;
	return &m_LocalConsole;
}

void CGameConsole::OnReset()
{
}

// only defined for 0<=t<=1
static float ConsoleScaleFunc(float t)
{
	//return t;
	return sinf(acosf(1.0f-t));
}

struct CRenderInfo
{
	CGameConsole *m_pSelf;
	const char *m_pCurrentCmd;
	int m_WantedCompletion;
	int m_EnumCount;
	float m_Offset;
	float m_Width;
};

void CGameConsole::PossibleCommandsRenderCallback(const char *pStr, void *pUser)
{
	CRenderInfo *pInfo = static_cast<CRenderInfo *>(pUser);
}

void CGameConsole::OnRender()
{
	float ConsoleHeight;

	float Progress = (TimeNow()-(m_StateChangeEnd-m_StateChangeDuration))/float(m_StateChangeDuration);

	if (Progress >= 1.0f)
	{
		if (m_ConsoleState == CONSOLE_CLOSING)
			m_ConsoleState = CONSOLE_CLOSED;
		else if (m_ConsoleState == CONSOLE_OPENING)
			m_ConsoleState = CONSOLE_OPEN;

		Progress = 1.0f;
	}

	if (m_ConsoleState == CONSOLE_OPEN && g_Config.m_ClEditor)
		Toggle(CONSOLETYPE_LOCAL);

	if (m_ConsoleState == CONSOLE_CLOSED)
		return;

	if (m_ConsoleState == CONSOLE_OPEN)
		Input()->MouseModeAbsolute();

	float ConsoleHeightScale;

	if (m_ConsoleState == CONSOLE_OPENING)
		ConsoleHeightScale = ConsoleScaleFunc(Progress);
	else if (m_ConsoleState == CONSOLE_CLOSING)
		ConsoleHeightScale = ConsoleScaleFunc(1.0f-Progress);
	else //if (console_state == CONSOLE_OPEN)
		ConsoleHeightScale = ConsoleScaleFunc(1.0f);

	CInstance *pConsole = CurrentConsole();

	{
		float FontSize = 10.0f;
		float RowHeight = FontSize*1.25f;
		float x = 3;
		float y = ConsoleHeight - RowHeight - 5.0f;

		const char *pPrompt = "> ";
		if(m_ConsoleType == CONSOLETYPE_REMOTE)
		{
			if(Client()->State() == IClient::STATE_LOADING || Client()->State() == IClient::STATE_ONLINE)
			{
				if(Client()->RconAuthed())
					pPrompt = "rcon> ";
				else
				{
					if(pConsole->m_UsernameReq)
					{
						if(!pConsole->m_UserGot)
							pPrompt = "Enter Username> ";
						else
							pPrompt = "Enter Password> ";
					}
					else
						pPrompt = "Enter Password> ";
				}
			}
			else
				pPrompt = "NOT CONNECTED> ";
		}
    // dbg_msg("console", pPrompt);
	}
}

void CGameConsole::OnMessage(int MsgType, void *pRawMsg)
{
}

bool CGameConsole::OnInput(IInput::CEvent Event)
{
	// accept input when opening, but not at first frame to discard the input that caused the console to open
	if(m_ConsoleState != CONSOLE_OPEN && (m_ConsoleState != CONSOLE_OPENING || m_StateChangeEnd == TimeNow()+m_StateChangeDuration))
		return false;
	if((Event.m_Key >= KEY_F1 && Event.m_Key <= KEY_F12) || (Event.m_Key >= KEY_F13 && Event.m_Key <= KEY_F24))
		return false;

	if(Event.m_Key == KEY_ESCAPE && (Event.m_Flags&IInput::FLAG_PRESS))
		Toggle(m_ConsoleType);
	else
		CurrentConsole()->OnInput(Event);

	return true;
}

void CGameConsole::Toggle(int Type)
{
	if(m_ConsoleType != Type && (m_ConsoleState == CONSOLE_OPEN || m_ConsoleState == CONSOLE_OPENING))
	{
		// don't toggle console, just switch what console to use
	}
	else
	{
		if (m_ConsoleState == CONSOLE_CLOSED || m_ConsoleState == CONSOLE_OPEN)
		{
			m_StateChangeEnd = TimeNow()+m_StateChangeDuration;
		}
		else
		{
			float Progress = m_StateChangeEnd-TimeNow();
			float ReversedProgress = m_StateChangeDuration-Progress;

			m_StateChangeEnd = TimeNow()+ReversedProgress;
		}

		if (m_ConsoleState == CONSOLE_CLOSED || m_ConsoleState == CONSOLE_CLOSING)
		{
			/*Input()->MouseModeAbsolute();
			m_pClient->m_pMenus->UseMouseButtons(false);*/
			m_ConsoleState = CONSOLE_OPENING;
			/*// reset controls
			m_pClient->m_pControls->OnReset();*/

			Input()->SetIMEState(true);
		}
		else
		{
			Input()->MouseModeRelative();
			m_pClient->m_pMenus->UseMouseButtons(true);
			m_pClient->OnRelease();
			m_ConsoleState = CONSOLE_CLOSING;

			Input()->SetIMEState(false);
		}
	}

	m_ConsoleType = Type;
}

void CGameConsole::Dump(int Type)
{
	CInstance *pConsole = Type == CONSOLETYPE_REMOTE ? &m_RemoteConsole : &m_LocalConsole;
	char aFilename[128];
	char aDate[20];

	str_timestamp(aDate, sizeof(aDate));
	str_format(aFilename, sizeof(aFilename), "dumps/%s_dump_%s.txt", Type==CONSOLETYPE_REMOTE?"remote_console":"local_console", aDate);
	IOHANDLE io = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(io)
	{
		for(CInstance::CBacklogEntry *pEntry = pConsole->m_Backlog.First(); pEntry; pEntry = pConsole->m_Backlog.Next(pEntry))
		{
			io_write(io, pEntry->m_aText, str_length(pEntry->m_aText));
			io_write_newline(io);
		}
		io_close(io);
	}
}

void CGameConsole::ConToggleLocalConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->Toggle(CONSOLETYPE_LOCAL);
}

void CGameConsole::ConToggleRemoteConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->Toggle(CONSOLETYPE_REMOTE);
}

void CGameConsole::ConClearLocalConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->m_LocalConsole.ClearBacklog();
}

void CGameConsole::ConClearRemoteConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->m_RemoteConsole.ClearBacklog();
}

void CGameConsole::ConDumpLocalConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->Dump(CONSOLETYPE_LOCAL);
}

void CGameConsole::ConDumpRemoteConsole(IConsole::IResult *pResult, void *pUserData)
{
	((CGameConsole *)pUserData)->Dump(CONSOLETYPE_REMOTE);
}

void CGameConsole::ClientConsolePrintCallback(const char *pStr, void *pUserData, bool Highlighted)
{
	((CGameConsole *)pUserData)->m_LocalConsole.PrintLine(pStr, Highlighted);
}

void CGameConsole::ConConsolePageUp(IConsole::IResult *pResult, void *pUserData)
{
	CInstance *pConsole = ((CGameConsole *)pUserData)->CurrentConsole();
	pConsole->m_BacklogActPage++;
}

void CGameConsole::ConConsolePageDown(IConsole::IResult *pResult, void *pUserData)
{
	CInstance *pConsole = ((CGameConsole *)pUserData)->CurrentConsole();
	--pConsole->m_BacklogActPage;
	if(pConsole->m_BacklogActPage < 0)
		pConsole->m_BacklogActPage = 0;
}

void CGameConsole::ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		CGameConsole *pThis = static_cast<CGameConsole *>(pUserData);
		pThis->Console()->SetPrintOutputLevel(pThis->m_PrintCBIndex, pResult->GetInteger(0));
	}
}

void CGameConsole::RequireUsername(bool UsernameReq)
{
	if((m_RemoteConsole.m_UsernameReq = UsernameReq))
	{
		m_RemoteConsole.m_aUser[0] = '\0';
		m_RemoteConsole.m_UserGot = false;
	}
}

void CGameConsole::PrintLine(int Type, const char *pLine)
{
	if(Type == CONSOLETYPE_LOCAL)
		m_LocalConsole.PrintLine(pLine);
	else if(Type == CONSOLETYPE_REMOTE)
		m_RemoteConsole.PrintLine(pLine);
}

void CGameConsole::OnConsoleInit()
{
	// init console instances
	m_LocalConsole.Init(this);
	m_RemoteConsole.Init(this);

	m_pConsole = Kernel()->RequestInterface<IConsole>();

	//
	m_PrintCBIndex = Console()->RegisterPrintCallback(g_Config.m_ConsoleOutputLevel, ClientConsolePrintCallback, this);

	Console()->Register("toggle_local_console", "", CFGFLAG_CLIENT, ConToggleLocalConsole, this, "Toggle local console");
	Console()->Register("toggle_remote_console", "", CFGFLAG_CLIENT, ConToggleRemoteConsole, this, "Toggle remote console");
	Console()->Register("clear_local_console", "", CFGFLAG_CLIENT, ConClearLocalConsole, this, "Clear local console");
	Console()->Register("clear_remote_console", "", CFGFLAG_CLIENT, ConClearRemoteConsole, this, "Clear remote console");
	Console()->Register("dump_local_console", "", CFGFLAG_CLIENT, ConDumpLocalConsole, this, "Dump local console");
	Console()->Register("dump_remote_console", "", CFGFLAG_CLIENT, ConDumpRemoteConsole, this, "Dump remote console");

	Console()->Register("console_page_up", "", CFGFLAG_CLIENT, ConConsolePageUp, this, "Previous page in console");
	Console()->Register("console_page_down", "", CFGFLAG_CLIENT, ConConsolePageDown, this, "Next page in console");

	Console()->Chain("console_output_level", ConchainConsoleOutputLevelUpdate, this);
}

void CGameConsole::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE && NewState < IClient::STATE_LOADING)
	{
		m_RemoteConsole.m_UserGot = false;
		m_RemoteConsole.m_aUser[0] = '\0';
		m_RemoteConsole.m_Input.Clear();
		m_RemoteConsole.m_UsernameReq = false;
	}
}
