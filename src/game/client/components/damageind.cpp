/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/gamecore.h> // get_angle
#include "damageind.h"

CDamageInd::CDamageInd()
{
	m_Lastupdate = 0;
	m_NumItems = 0;
}

CDamageInd::CItem *CDamageInd::CreateI()
{
	return 0;
}

void CDamageInd::DestroyI(CDamageInd::CItem *i)
{

}

void CDamageInd::Create(vec2 Pos, vec2 Dir)
{

}

void CDamageInd::OnRender()
{

}

void CDamageInd::OnInit()
{

}

void CDamageInd::Reset()
{

}
