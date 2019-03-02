/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/localization.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/motd.h>
#include <game/client/components/statboard.h>

#include "scoreboard.h"

#include <base/tl/string.h>
#include <engine/serverbrowser.h>

CScoreboard::CScoreboard()
{
	OnReset();
}

void CScoreboard::ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData)
{
	CScoreboard *pSelf = (CScoreboard *)pUserData;
	CServerInfo Info;

	pSelf->Client()->GetServerInfo(&Info);
	pSelf->m_IsGameTypeRace = IsRace(&Info);
	pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CScoreboard::OnReset()
{
	m_Active = false;
	m_ServerRecord = -1.0f;
}

void CScoreboard::OnRelease()
{
	m_Active = false;
}

void CScoreboard::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_RECORD)
	{
		CNetMsg_Sv_Record *pMsg = (CNetMsg_Sv_Record *)pRawMsg;
		m_ServerRecord = (float)pMsg->m_ServerTimeBest/100;
		//m_PlayerRecord = (float)pMsg->m_PlayerTimeBest/100;
	}
}

void CScoreboard::OnConsoleInit()
{
	Console()->Register("+scoreboard", "", CFGFLAG_CLIENT, ConKeyScoreboard, this, "Show scoreboard");
}

void CScoreboard::RenderGoals(float x, float y, float w)
{
	// render goals
	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit);
		}
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit);
		}
		if(m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum && m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s %d/%d", Localize("Round"), m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent, m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum);
		}
	}
}

void CScoreboard::RenderSpectators(float x, float y, float w)
{
	// spectator names
	char aBuffer[1024*4];
	aBuffer[0] = 0;
	bool Multiple = false;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByName[i];
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;

		if(Multiple)
			str_append(aBuffer, ", ", sizeof(aBuffer));
		if(g_Config.m_ClShowIDs)
		{
			char aId[5];
			str_format(aId,sizeof(aId),"%d: ",pInfo->m_ClientID);
			str_append(aBuffer, aId, sizeof(aBuffer));
		}
		str_append(aBuffer, m_pClient->m_aClients[pInfo->m_ClientID].m_aName, sizeof(aBuffer));
		Multiple = true;
	}
}

void CScoreboard::RenderScoreboard(float x, float y, float w, int Team, const char *pTitle)
{
	if(Team == TEAM_SPECTATORS)
		return;

	bool lower16 = false;
	bool upper16 = false;
	bool lower24 = false;
	bool upper24 = false;
	bool lower32 = false;
	bool upper32 = false;

	if(Team == -3)
		upper16 = true;
	else if(Team == -4)
		lower32 = true;
	else if(Team == -5)
		upper32 = true;
	else if(Team == -6)
		lower16 = true;
	else if(Team == -7)
		lower24 = true;
	else if(Team == -8)
		upper24 = true;

	if(Team < -1)
		Team = 0;

	// render title
	if(!pTitle)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
			pTitle = Localize("Game over");
		else
			pTitle = Localize("Score board");
	}

	char aBuf[128] = {0};

	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
	{
		if(m_pClient->m_Snap.m_pGameDataObj)
		{
			int Score = Team == TEAM_RED ? m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed : m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
	}
	else
	{
		if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW &&
			m_pClient->m_Snap.m_paPlayerInfos[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID])
		{
			int Score = m_pClient->m_Snap.m_paPlayerInfos[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID]->m_Score;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
		else if(m_pClient->m_Snap.m_pLocalInfo)
		{
			int Score = m_pClient->m_Snap.m_pLocalInfo->m_Score;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
	}

	if(m_IsGameTypeRace && g_Config.m_ClDDRaceScoreBoard && m_pClient->m_AllowTimeScore[g_Config.m_ClDummy])
	{
		if (m_ServerRecord > 0)
		{
			str_format(aBuf, sizeof(aBuf), "%02d:%02d", ((int)m_ServerRecord)/60, ((int)m_ServerRecord)%60);
		}
		else
			aBuf[0] = 0;
	}

	// render player entries
	float FontSize = 24.0f;
	if(m_pClient->m_Snap.m_aTeamSize[Team] > 48)
		FontSize = 16.0f;
	else if(m_pClient->m_Snap.m_aTeamSize[Team] > 32)
		FontSize = 20.0f;

	int rendered = 0;
	if (upper16)
		rendered = -16;
	if (upper32)
		rendered = -32;
	if (upper24)
		rendered = -24;

	int OldDDTeam = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// make sure that we render the correct team
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByDDTeam[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if (rendered++ < 0) continue;

		int DDTeam = ((CGameClient *) m_pClient)->m_Teams.Team(pInfo->m_ClientID);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeam[j];

			if(!pInfo2 || pInfo2->m_Team != Team)
				continue;

			NextDDTeam = ((CGameClient *) m_pClient)->m_Teams.Team(pInfo2->m_ClientID);
			break;
		}

		if (OldDDTeam == -1)
		{
			for (int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeam[j];

				if(!pInfo2 || pInfo2->m_Team != Team)
					continue;

				OldDDTeam = ((CGameClient *) m_pClient)->m_Teams.Team(pInfo2->m_ClientID);
				break;
			}
		}

		OldDDTeam = DDTeam;

		// score
		if(m_IsGameTypeRace && g_Config.m_ClDDRaceScoreBoard && m_pClient->m_AllowTimeScore[g_Config.m_ClDummy])
		{
			if (pInfo->m_Score == -9999)
				aBuf[0] = 0;
			else
			{
				int Time = abs(pInfo->m_Score);
				str_format(aBuf, sizeof(aBuf), "%02d:%02d", Time/60, Time%60);
			}
		}
		else
			str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Score, -999, 99999));

		// name
		if(g_Config.m_ClShowIDs)
		{
			char aId[64] = "";
			if (pInfo->m_ClientID >= 10)
				str_format(aId, sizeof(aId),"%d: ", pInfo->m_ClientID);
			else
				str_format(aId, sizeof(aId)," %d: ", pInfo->m_ClientID);
			str_append(aId, m_pClient->m_aClients[pInfo->m_ClientID].m_aName,sizeof(aId));
		}
	}
}

void CScoreboard::RenderRecordingNotification(float x)
{
	if(!m_pClient->DemoRecorder(RECORDER_MANUAL)->IsRecording() &&
	   !m_pClient->DemoRecorder(RECORDER_AUTO)->IsRecording() &&
	   !m_pClient->DemoRecorder(RECORDER_RACE)->IsRecording())
	{
		return;
	}

	//draw the text
	char aBuf[64] = "\0";
	char aBuf2[64];
	int Seconds;

	if(m_pClient->DemoRecorder(RECORDER_MANUAL)->IsRecording())
	{
		Seconds = m_pClient->DemoRecorder(RECORDER_MANUAL)->Length();
		str_format(aBuf2, sizeof(aBuf2), Localize("Manual %3d:%02d  "), Seconds/60, Seconds%60);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	if(m_pClient->DemoRecorder(RECORDER_RACE)->IsRecording())
	{
		Seconds = m_pClient->DemoRecorder(RECORDER_RACE)->Length();
		str_format(aBuf2, sizeof(aBuf2), Localize("Race %3d:%02d  "), Seconds/60, Seconds%60);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	if(m_pClient->DemoRecorder(RECORDER_AUTO)->IsRecording())
	{
		Seconds = m_pClient->DemoRecorder(RECORDER_AUTO)->Length();
		str_format(aBuf2, sizeof(aBuf2), Localize("Auto %3d:%02d  "), Seconds/60, Seconds%60);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
}


void CScoreboard::OnRender()
{
	if(!Active())
		return;

	// if the score board is active, then we should clear the motd message as well
	if(m_pClient->m_pMotd->IsActive())
		m_pClient->m_pMotd->Clear();

	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS))
		{
			if(m_pClient->m_Snap.m_aTeamSize[0] > 48)
			{
				RenderScoreboard(2, 150.0f, 2, -4, 0);
				RenderScoreboard(2, 150.0f, 2, -5, "");
			} else if(m_pClient->m_Snap.m_aTeamSize[0] > 32)
			{
				RenderScoreboard(2, 150.0f, 2, -7, 0);
				RenderScoreboard(2, 150.0f, 2, -8, "");
			} else if(m_pClient->m_Snap.m_aTeamSize[0] > 16)
			{
				RenderScoreboard(2, 150.0f, 2, -6, 0);
				RenderScoreboard(2, 150.0f, 2, -3, "");
			} else
			{
				RenderScoreboard(2, 150.0f, 2, 0, 0);
			}
		}
		else
		{
			const char *pRedClanName = GetClanName(TEAM_RED);
			const char *pBlueClanName = GetClanName(TEAM_BLUE);

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER && m_pClient->m_Snap.m_pGameDataObj)
			{
				char aText[256];
				str_copy(aText, Localize("Draw!"), sizeof(aText));

				if(m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed > m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue)
				{
					if(pRedClanName)
						str_format(aText, sizeof(aText), Localize("%s wins!"), pRedClanName);
					else
						str_copy(aText, Localize("Red team wins!"), sizeof(aText));
				}
				else if(m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue > m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed)
				{
					if(pBlueClanName)
						str_format(aText, sizeof(aText), Localize("%s wins!"), pBlueClanName);
					else
						str_copy(aText, Localize("Blue team wins!"), sizeof(aText));
				}
			}

			RenderScoreboard(5.0f, 150.0f, 2, TEAM_RED, pRedClanName ? pRedClanName : Localize("Red team"));
			RenderScoreboard(5.0f, 150.0f, 2, TEAM_BLUE, pBlueClanName ? pBlueClanName : Localize("Blue team"));
		}
	}

	RenderGoals(2, 150+760+10, 2);
	RenderSpectators(2, 150+760+10+50+10, 2);
	RenderRecordingNotification((7)*4);
}

bool CScoreboard::Active()
{
	// if statboard is active don't show scoreboard
	if(m_pClient->m_pStatboard->IsActive())
		return false;

	if(m_Active)
		return true;

	if(m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
	{
		// we are not a spectator, check if we are dead
		if(!m_pClient->m_Snap.m_pLocalCharacter && g_Config.m_ClScoreboardOnDeath)
			return true;
	}

	// if the game is over
	if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
		return true;

	return false;
}

const char *CScoreboard::GetClanName(int Team)
{
	int ClanPlayers = 0;
	const char *pClanName = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if(!pClanName)
		{
			pClanName = m_pClient->m_aClients[pInfo->m_ClientID].m_aClan;
			ClanPlayers++;
		}
		else
		{
			if(str_comp(m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, pClanName) == 0)
				ClanPlayers++;
			else
				return 0;
		}
	}

	if(ClanPlayers > 1 && pClanName[0])
		return pClanName;
	else
		return 0;
}
