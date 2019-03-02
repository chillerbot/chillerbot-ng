/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/config.h>
#include <engine/demo.h>
#include <engine/friends.h>
#include <engine/ghost.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>

#include <game/localization.h>
#include <game/client/components/countryflags.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>

#include "menus.h"
#include "motd.h"
#include "voting.h"

#include <base/tl/string.h>
#include <engine/keys.h>
#include <engine/storage.h>
#include "ghost.h"


// ghost stuff
int CMenus::GhostlistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	return 0;
}

void CMenus::GhostlistPopulate()
{

}

CMenus::CGhostItem *CMenus::GetOwnGhost()
{
	return 0;
}

void CMenus::UpdateOwnGhost(CGhostItem Item)
{

}

