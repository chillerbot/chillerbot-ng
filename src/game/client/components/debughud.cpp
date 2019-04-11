/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/layers.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>

#include "debughud.h"

void CDebugHud::RenderNetCorrections()
{
	// if(!g_Config.m_Debug || g_Config.m_DbgGraphs || !m_pClient->m_Snap.m_pLocalCharacter || !m_pClient->m_Snap.m_pLocalPrevCharacter)
	if(!m_pClient->m_Snap.m_pLocalCharacter || !m_pClient->m_Snap.m_pLocalPrevCharacter)
		return;

	float Velspeed = length(vec2(m_pClient->m_Snap.m_pLocalCharacter->m_VelX/256.0f, m_pClient->m_Snap.m_pLocalCharacter->m_VelY/256.0f))*50;
	float Ramp = VelocityRamp(Velspeed, m_pClient->m_Tuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_Tuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_Tuning[g_Config.m_ClDummy].m_VelrampCurvature);

  /*
	const char *paStrings[] = {"velspeed:", "velspeed*ramp:", "ramp:", "Pos", " x:", " y:", "angle:", "netobj corrections", " num:", " on:"};
	const int Num = sizeof(paStrings)/sizeof(char *);
	const float LineHeight = 6.0f;
	const float Fontsize = 5.0f;

	float x = Width-100.0f, y = 50.0f;
	for(int i = 0; i < Num; ++i)
		dbg_msg("debughud", paStrings[i]);

	x = Width-10.0f;
  */

	char aBuf[128];
	// str_format(aBuf, sizeof(aBuf), "%.0f", Velspeed/32);
	// str_format(aBuf, sizeof(aBuf), "%.0f", Velspeed/32*Ramp);
	// str_format(aBuf, sizeof(aBuf), "%.2f", Ramp);

	// str_format(aBuf, sizeof(aBuf), "%.2f", static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f);
	// str_format(aBuf, sizeof(aBuf), "%.2f", static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f);
	// str_format(aBuf, sizeof(aBuf), "%d", m_pClient->m_Snap.m_pLocalCharacter->m_Angle);

	str_format(aBuf, sizeof(aBuf), "Pos X: %.2f Y: %.2f chillerdir: %d",
      static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_X)/32.0f,
      static_cast<float>(m_pClient->m_Snap.m_pLocalCharacter->m_Y)/32.0f,
      g_Config.m_ClChillerDir);
  dbg_msg("debughud", aBuf);
}

void CDebugHud::RenderTuning()
{
	// render tuning debugging
	if(!g_Config.m_DbgTuning)
		return;

	CTuningParams StandardTuning;

	float y = 27.0f;
	int Count = 0;
	for(int i = 0; i < m_pClient->m_Tuning[g_Config.m_ClDummy].Num(); i++)
	{
		char aBuf[128];
		float Current, Standard;
		m_pClient->m_Tuning[g_Config.m_ClDummy].Get(i, &Current);
		StandardTuning.Get(i, &Standard);

		float x = 5.0f;

		str_format(aBuf, sizeof(aBuf), "%.2f", Standard);
		x += 20.0f;

		str_format(aBuf, sizeof(aBuf), "%.2f", Current);
		x += 20.0f;

		x += 5.0f;
		Count++;
	}

	y = y+Count*6;

	float Height = 50.0f;
	float pv = 1;
	for(int i = 0; i < 100; i++)
	{
		float Speed = i/100.0f * 3000;
		float Ramp = VelocityRamp(Speed, m_pClient->m_Tuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_Tuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_Tuning[g_Config.m_ClDummy].m_VelrampCurvature);
		float RampedSpeed = (Speed * Ramp)/1000.0f;
		pv = RampedSpeed;
	}
}

void CDebugHud::OnRender()
{
	RenderTuning();
	RenderNetCorrections();
}