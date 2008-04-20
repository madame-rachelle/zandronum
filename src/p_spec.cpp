// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Implements special effects:
//		Texture animation, height or lighting changes
//		 according to adjacent sectors, respective
//		 utility functions, etc.
//		Line Tag handling. Line and Sector triggers.
//		Implements donut linedef triggers
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
//			Friction
//			Wind/Current
//
//-----------------------------------------------------------------------------


#include "m_alloc.h"

#include <stdlib.h>

#include "templates.h"
#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"

#include "i_system.h"
#include "m_argv.h"
#include "m_random.h"
#include "m_bbox.h"
#include "w_wad.h"

#include "r_local.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_terrain.h"
#include "p_acs.h"

#include "g_game.h"

#include "s_sound.h"
#include "sc_man.h"
#include "gi.h"
#include "statnums.h"

// State.
#include "r_state.h"

#include "c_console.h"

// [RH] Needed for sky scrolling
#include "r_sky.h"
#include "announcer.h"
#include "deathmatch.h"
#include "duel.h"
#include "network.h"
#include "team.h"
#include "lastmanstanding.h"
#include "sbar.h"
#include "sv_commands.h"
#include "cl_demo.h"
#include "possession.h"
#include "cooperative.h"
#include "survival.h"
#include "gamemode.h"

static FRandom pr_playerinspecialsector ("PlayerInSpecialSector");

// [GrafZahl] Make this message changable by the user! ;)
CVAR(String, secretmessage, "A Secret is revealed!", CVAR_ARCHIVE)

IMPLEMENT_CLASS (DScroller)

IMPLEMENT_POINTY_CLASS (DPusher)
 DECLARE_POINTER (m_Source)
END_POINTERS

DScroller::DScroller ()
{
}

void DScroller::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_dx << m_dy
		<< m_Affectee
		<< m_Control
		<< m_LastHeight
		<< m_vdx << m_vdy
		<< m_Accel;
}

DPusher::DPusher ()
{
}

void DPusher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_Source
		<< m_Xmag
		<< m_Ymag
		<< m_Magnitude
		<< m_Radius
		<< m_X
		<< m_Y
		<< m_Affectee;
}

// killough 3/7/98: Initialize generalized scrolling
static void P_SpawnScrollers();
static void P_SpawnFriction ();		// phares 3/16/98
static void P_SpawnPushers ();		// phares 3/20/98


// [RH] Check dmflags for noexit and respond accordingly
bool CheckIfExitIsGood (AActor *self, level_info_t *info)
{
	cluster_info_t *cluster;

	// The world can always exit itself.
	if (self == NULL)
		return true;

	// Is this a deathmatch game and we're not allowed to exit?
	// [BC] Teamgame, too.
	if ((deathmatch || teamgame || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
	{
		P_DamageMobj (self, self, self, 1000000, NAME_Exit);
		return false;
	}
	// Is this a singleplayer game and the next map is part of the same hub and we're dead?
	if (self->health <= 0 &&
		( NETWORK_GetState( ) == NETSTATE_SINGLE ) &&
		info != NULL &&
		info->cluster == level.cluster &&
		(cluster = FindClusterInfo(level.cluster)) != NULL &&
		cluster->flags & CLUSTER_HUB)
	{
		return false;
	}
	// [BC] Instead of displaying this message in deathmatch only, display it any
	// time we're not in single player mode (it can be annoying when people exit
	// the map in cooperative, and it's nice to know who's doing it).
//	if (deathmatch || teamgame)
	if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
	{
		// [BB] It's possible, that a monster exits the level, so self->player can be 0.
		if( self->player != 0 )
			Printf ("%s \\c-exited the level.\n", self->player->userinfo.netname);
	}
	return true;
}


//
// UTILITIES
//



//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//

// Find the next sector with a specified tag.
// Rewritten by Lee Killough to use chained hashing to improve speed

int P_FindSectorFromTag (int tag, int start)
{
	start = start >= 0 ? sectors[start].nexttag :
		sectors[(unsigned) tag % (unsigned) numsectors].firsttag;
	while (start >= 0 && sectors[start].tag != tag)
		start = sectors[start].nexttag;
	return start;
}

// killough 4/16/98: Same thing, only for linedefs

int P_FindLineFromID (int id, int start)
{
	start = start >= 0 ? lines[start].nextid :
		lines[(unsigned) id % (unsigned) numlines].firstid;
	while (start >= 0 && lines[start].id != id)
		start = lines[start].nextid;
	return start;
}




//============================================================================
//
// P_ActivateLine
//
//============================================================================

bool P_ActivateLine (line_t *line, AActor *mo, int side, int activationType)
{
	int lineActivation;
	INTBOOL repeat;
	INTBOOL buttonSuccess;
	BYTE special;

	// [BC] Lines are server side. However, allow spectators to cross teleports.
	if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying ( ))) &&
		(( mo == NULL ) ||
		( mo->player == NULL ) ||
		( mo->player->bSpectating == false ) ||
		((( line->special == Teleport ) || ( line->special == Teleport_NoFog ) || ( line->special == Teleport_Line )) == false )
		))
	{ 
		return ( false );
	}

	if (!P_TestActivateLine (line, mo, side, activationType))
	{
		return false;
	}
	lineActivation = GET_SPAC(line->flags);
	if (lineActivation == SPAC_PTOUCH)
	{
		lineActivation = activationType;
	}
	repeat = line->flags & ML_REPEAT_SPECIAL;
	buttonSuccess = false;
	buttonSuccess = LineSpecials[line->special]
					(line, mo, side == 1, line->args[0],
					line->args[1], line->args[2],
					line->args[3], line->args[4]);

	special = line->special;
	if (!repeat && buttonSuccess)
	{ // clear the special on non-retriggerable lines
		line->special = 0;
	}
// Graf Zahl says: "If you check out the WolfenDoom WAD Operation Rheingold 2
// you will find that there are lots of shoot triggers that don't have any
// attached sector. In Doom2.exe such switches are changed and this WAD uses
// this to create a lot of shootable stuff on walls (like clocks that get
// destroyed etc.) None of those work in ZDoom. Interestingly this works in
// almost no source port."
// begin of changed code
	if (buttonSuccess)
	{
		if (lineActivation == SPAC_USE || lineActivation == SPAC_IMPACT || lineActivation == SPAC_USETHROUGH)
		{
			P_ChangeSwitchTexture (&sides[line->sidenum[0]], repeat, special);

			// [BC] Tell the clients of the switch texture change.
//			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
//				SERVERCOMMANDS_ToggleLine( line - lines, !!repeat );
		}
	}

	// some old WADs use this method to create walls that change the texture when shot.
	else if (lineActivation == SPAC_IMPACT &&					// only for shootable triggers
		!(level.flags & LEVEL_HEXENFORMAT) &&					// only in Doom-format maps
		!repeat &&												// only non-repeatable triggers
		(special<Generic_Floor || special>Generic_Crusher) &&	// not for Boom's generalized linedefs
		special &&												// not for lines without a special
		line->id &&												// only if there's a tag (which is stored in the id field)
		P_FindSectorFromTag (line->args[0], -1) == -1)			// only if no sector is tagged to this linedef
	{
		P_ChangeSwitchTexture (&sides[line->sidenum[0]], repeat, special);
		line->special = 0;

		// [BC] Tell the clients of the switch texture change.
//		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
//			SERVERCOMMANDS_ToggleLine( line - lines, !!repeat );
	}
// end of changed code
	if (developer && buttonSuccess)
	{
		Printf ("Line special %d activated\n", special);
	}
	return true;
}

//============================================================================
//
// P_TestActivateLine
//
//============================================================================

bool P_TestActivateLine (line_t *line, AActor *mo, int side, int activationType)
{
	int lineActivation;

	lineActivation = GET_SPAC(line->flags);
	if (lineActivation == SPAC_PTOUCH &&
		(activationType == SPAC_PCROSS || activationType == SPAC_IMPACT))
	{
		lineActivation = activationType;
	}
	else if (lineActivation == SPAC_USETHROUGH)
	{
		lineActivation = SPAC_USE;
	}
	else if (line->special == Teleport &&
		lineActivation == SPAC_CROSS &&
		activationType == SPAC_PCROSS &&
		mo != NULL &&
		mo->flags & MF_MISSILE)
	{ // Let missiles use regular player teleports
		lineActivation = SPAC_PCROSS;
	}
	// BOOM's generalized line types that allow monster use can actually be
	// activated by anything!
	if (activationType == SPAC_OTHERCROSS)
	{
		if (lineActivation == SPAC_CROSS && line->special >= Generic_Floor &&
			line->special <= Generic_Crusher && !(mo->flags2&MF2_NOTELEPORT))
		{
			return (line->flags & ML_MONSTERSCANACTIVATE) != 0;
		}
		return false;
	}
	if (lineActivation != activationType &&
		!(activationType == SPAC_MCROSS && lineActivation == SPAC_CROSS))
	{ 
		return false;
	}
	if (mo && !mo->player &&
		!(mo->flags & MF_MISSILE) &&
		!(line->flags & ML_MONSTERSCANACTIVATE) &&
		(activationType != SPAC_MCROSS || lineActivation != SPAC_MCROSS))
	{ // [RH] monsters' ability to activate this line depends on its type
		// In Hexen, only MCROSS lines could be activated by monsters. With
		// lax activation checks, monsters can also activate certain lines
		// even without them being marked as monster activate-able. This is
		// the default for non-Hexen maps in Hexen format.
		if (!(level.flags & LEVEL_LAXMONSTERACTIVATION))
		{
			return false;
		}
		if ((activationType == SPAC_USE || activationType == SPAC_PUSH)
			&& (line->flags & ML_SECRET))
			return false;		// never open secret doors

		bool noway = true;

		switch (lineActivation)
		{
		case SPAC_IMPACT:
		case SPAC_PCROSS:
			// shouldn't really be here if not a missile
		case SPAC_MCROSS:
			noway = false;
			break;

		case SPAC_CROSS:
			switch (line->special)
			{
			case Door_Raise:
				if (line->args[1] >= 64)
				{
					break;
				}
			case Teleport:
			case Teleport_NoFog:
			case Teleport_Line:
			case Plat_DownWaitUpStayLip:
			case Plat_DownWaitUpStay:
				noway = false;
			}
			break;

		case SPAC_USE:
		case SPAC_PUSH:
			switch (line->special)
			{
			case Door_Raise:
				if (line->args[0] == 0 && line->args[1] < 64)
					noway = false;
				break;
			case Teleport:
			case Teleport_NoFog:
				noway = false;
			}
			break;
		}
		return !noway;
	}
	if (activationType == SPAC_MCROSS &&
		lineActivation != activationType &&
		!(line->flags & ML_MONSTERSCANACTIVATE))
	{
		return false;
	}
	return true;
}

//
// P_PlayerInSpecialSector
// Called every tic frame
//	that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t *player, sector_t * sector)
{
	// [BB] Check this!
	if (sector == NULL)
		sector = player->mo->Sector;
	int special = sector->special & ~SECRET_MASK;

	// [BC] Sector specials are server-side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		// Just do secret triggers, and get out.
		if ( sector->special & SECRET_MASK )
		{
			if (player->mo->CheckLocalView (consoleplayer))
			{
				player->secretcount++;
				level.found_secrets++;
				sector->special &= ~SECRET_MASK;
				C_MidPrint (secretmessage);
				S_Sound (CHAN_AUTO, "misc/secret", 1, ATTN_NORM);
			}
		}

		return;
	}

	// Falling, not all the way down yet?
	if (player->mo->z != sector->floorplane.ZatPoint (player->mo->x, player->mo->y)
		&& !player->mo->waterlevel)
	{
		return;
	}

	// Has hit ground.
	AInventory *ironfeet;

	// Allow subclasses. Better would be to implement it as armor and let that reduce
	// the damage as part of the normal damage procedure. Unfortunately, I don't have
	// different damage types yet, so that's not happening for now.
	for (ironfeet = player->mo->Inventory; ironfeet != NULL; ironfeet = ironfeet->Inventory)
	{
		if (ironfeet->IsKindOf (RUNTIME_CLASS(APowerIronFeet)))
			break;
	}

	// [RH] Normal DOOM special or BOOM specialized?
	if (special >= dLight_Flicker && special <= 255)
	{
		switch (special)
		{
		case Sector_Heal:
			// CoD's healing sector
			if (!(level.time & 0x1f))
				P_GiveBody (player->mo, 1);
			break;

		case Damage_InstantDeath:
			// Strife's instant death sector
			P_DamageMobj (player->mo, NULL, NULL, 999, NAME_InstantDeath);
			break;

		case dDamage_Hellslime:
			// HELLSLIME DAMAGE
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 10, NAME_Slime);
			break;

		case dDamage_Nukage:
			// NUKAGE DAMAGE
		case sLight_Strobe_Hurt:
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 5, NAME_Slime);
			break;

		case dDamage_SuperHellslime:
			// SUPER HELLSLIME DAMAGE
		case dLight_Strobe_Hurt:
			// STROBE HURT
			if (ironfeet == NULL || pr_playerinspecialsector() < 5)
			{
				if (!(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, 20, NAME_Slime);
			}
			break;

		case sDamage_Hellslime:
			if (ironfeet == NULL)
				player->hazardcount += 2;
			break;

		case sDamage_SuperHellslime:
			if (ironfeet == NULL)
				player->hazardcount += 4;
			break;

		case dDamage_End:
			// EXIT SUPER DAMAGE! (for E1M8 finale)
			player->cheats &= ~CF_GODMODE;

			if (!(level.time & 0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 20, NAME_None);

			// [BC] Don't do this in teamgame, either.
			if (player->health <= 10 && ((!deathmatch && !teamgame) || !(dmflags & DF_NO_EXIT)))
				G_ExitLevel(0, false);
			break;

		case dDamage_LavaWimpy:
		case dScroll_EastLavaDamage:
			if (!(level.time & 15))
			{
				P_DamageMobj(player->mo, NULL, NULL, 5, NAME_Fire);
				P_HitFloor(player->mo);
			}
			break;

		case dDamage_LavaHefty:
			if(!(level.time & 15))
			{
				P_DamageMobj(player->mo, NULL, NULL, 8, NAME_Fire);
				P_HitFloor(player->mo);
			}
			break;

		default:
			// [RH] Ignore unknown specials
			break;
		}
	}
	else
	{
		//jff 3/14/98 handle extended sector types for secrets and damage
		switch (special & DAMAGE_MASK)
		{
		case 0x000: // no damage
			break;
		case 0x100: // 2/5 damage per 31 ticks
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 5, NAME_Fire);
			break;
		case 0x200: // 5/10 damage per 31 ticks
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 10, NAME_Slime);
			break;
		case 0x300: // 10/20 damage per 31 ticks
			if (ironfeet == NULL
				|| pr_playerinspecialsector() < 5)	// take damage even with suit
			{
				if (!(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, 20, NAME_Slime);
			}
			break;
		}
	}

	// [RH] Apply any customizable damage
	if (sector->damage)
	{
		if (sector->damage < 20)
		{
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, sector->damage, MODtoDamageType (sector->mod));
		}
		else if (sector->damage < 50)
		{
			if ((ironfeet == NULL || (pr_playerinspecialsector()<5))
				 && !(level.time&0x1f))
			{
				P_DamageMobj (player->mo, NULL, NULL, sector->damage, MODtoDamageType (sector->mod));
			}
		}
		else
		{
			P_DamageMobj (player->mo, NULL, NULL, sector->damage, MODtoDamageType (sector->mod));
		}
	}

	if (sector->special & SECRET_MASK)
	{
		player->secretcount++;
		level.found_secrets++;
		sector->special &= ~SECRET_MASK;
		if (player->mo->CheckLocalView (consoleplayer))
		{
			C_MidPrint (secretmessage);
			S_Sound (CHAN_AUTO, "misc/secret", 1, ATTN_NORM);
		}
	}
}

//============================================================================
//
// P_PlayerOnSpecialFlat
//
//============================================================================

void P_PlayerOnSpecialFlat (player_t *player, int floorType)
{
	if (player->mo->z > player->mo->Sector->floorplane.ZatPoint (
		player->mo->x, player->mo->y) &&
		!player->mo->waterlevel)
	{ // Player is not touching the floor
		return;
	}
	if (Terrains[floorType].DamageAmount &&
		!(level.time & Terrains[floorType].DamageTimeMask))
	{
		P_DamageMobj (player->mo, NULL, NULL, Terrains[floorType].DamageAmount,
			Terrains[floorType].DamageMOD);
		if (Terrains[floorType].Splash != -1)
		{
			S_SoundID (player->mo, CHAN_AUTO,
				Splashes[Terrains[floorType].Splash].NormalSplashSound, 1,
				ATTN_IDLE);
		}
	}
}



//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
void P_UpdateSpecials ()
{
//	size_t j;
//	int i;
	
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		// LEVEL TIMER
		if (( deathmatch || teamgame ) && timelimit )
		{
			if (( level.time >= (int)( timelimit * TICRATE * 60 )) && ( GAME_GetEndLevelDelay( ) == 0 ))
			{
				// Special game modes handle this differently.
				if ( duel )
					DUEL_TimeExpired( );
				else if ( lastmanstanding || teamlms )
					LASTMANSTANDING_TimeExpired( );
				else if ( possession || teampossession )
					POSSESSION_TimeExpired( );
				else if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode() ) & GMF_PLAYERSONTEAMS )
					TEAM_TimeExpired( );

				// End the level after one second.
				else
				{
					ULONG				ulIdx;
					LONG				lWinner;
					LONG				lHighestFrags;
					bool				bTied;
					char				szString[64];
					DHUDMessageFadeOut	*pMsg;

					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVER_Printf( PRINT_HIGH, "%s\n", GStrings( "TXT_TIMELIMIT" ));
					else
						Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));

					GAME_SetEndLevelDelay( 1 * TICRATE );

					// Determine the winner.
					lWinner = -1;
					lHighestFrags = INT_MIN;
					bTied = false;
					for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
					{
						if ( playeringame[ulIdx] == false )
							continue;

						if ( players[ulIdx].fragcount > lHighestFrags )
						{
							lWinner = ulIdx;
							lHighestFrags = players[ulIdx].fragcount;
							bTied = false;
						}
						else if ( players[ulIdx].fragcount == lHighestFrags )
							bTied = true;
					}

					if ( bTied )
						sprintf( szString, "\\cdDRAW GAME!" );
					else
					{
						if (( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) && ( players[consoleplayer].mo->CheckLocalView( lWinner )))
							sprintf( szString, "YOU WIN!" );
						else
							sprintf( szString, "%s \\c-WINS!", players[lWinner].userinfo.netname );
					}
					V_ColorizeString( szString );

					if ( NETWORK_GetState( ) != NETSTATE_SERVER )
					{
						screen->SetFont( BigFont );

						// Display "%s WINS!" HUD message.
						pMsg = new DHUDMessageFadeOut( szString,
							160.4f,
							75.0f,
							320,
							200,
							CR_RED,
							3.0f,
							2.0f );

						StatusBar->AttachMessage( pMsg, 'CNTR' );
						screen->SetFont( SmallFont );
					}
					else
					{
						SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, 'CNTR' );
					}

					GAME_SetEndLevelDelay( 5 * TICRATE );
				}
			}
		}
	}
}



//
// SPECIAL SPAWNING
//

CUSTOM_CVAR (Bool, forcewater, false, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	if (gamestate == GS_LEVEL)
	{
		int i;

		for (i = 0; i < numsectors; i++)
		{
			if (sectors[i].heightsec &&
				!(sectors[i].heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
				!(sectors[i].heightsec->MoreFlags & SECF_UNDERWATER))
			{
				if (self)
				{
					sectors[i].heightsec->MoreFlags |= SECF_FORCEDUNDERWATER;
				}
				else
				{
					sectors[i].heightsec->MoreFlags &= ~SECF_FORCEDUNDERWATER;
				}
			}
		}
	}
}

class DLightTransfer : public DThinker
{
	DECLARE_ACTOR (DLightTransfer, DThinker)
public:
	DLightTransfer (sector_t *srcSec, int target, bool copyFloor);
	void Serialize (FArchive &arc);
	void Tick ();

protected:
	static void DoTransfer (BYTE level, int target, bool floor);

	BYTE LastLight;
	sector_t *Source;
	int TargetTag;
	bool CopyFloor;
};

IMPLEMENT_CLASS (DLightTransfer)

void DLightTransfer::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << LastLight << Source << TargetTag << CopyFloor;
}

DLightTransfer::DLightTransfer (sector_t *srcSec, int target, bool copyFloor)
{
	int secnum;

	Source = srcSec;
	TargetTag = target;
	CopyFloor = copyFloor;
	DoTransfer (LastLight = srcSec->lightlevel, target, copyFloor);

	if (copyFloor)
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].FloorFlags |= SECF_ABSLIGHTING;
	}
	else
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].CeilingFlags |= SECF_ABSLIGHTING;
	}
	ChangeStatNum (STAT_LIGHTTRANSFER);
}

void DLightTransfer::Tick ()
{
	BYTE light = Source->lightlevel;

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetTag, CopyFloor);
	}
}

void DLightTransfer::DoTransfer (BYTE level, int target, bool floor)
{
	int secnum;

	if (floor)
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].FloorLight = level;
	}
	else
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].CeilingLight = level;
	}
}


class DWallLightTransfer : public DThinker
{
	enum
	{
		WLF_SIDE1=1,
		WLF_SIDE2=2,
		WLF_NOFAKECONTRAST=4
	};

	DECLARE_ACTOR (DWallLightTransfer, DThinker)
public:
	DWallLightTransfer (sector_t *srcSec, int target, BYTE flags);
	void Serialize (FArchive &arc);
	void Tick ();

protected:
	static void DoTransfer (BYTE level, int target, BYTE flags);

	BYTE LastLight;
	BYTE Flags;
	sector_t *Source;
	int TargetID;
};

IMPLEMENT_CLASS (DWallLightTransfer)

void DWallLightTransfer::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << LastLight << Source << TargetID << Flags;
}

DWallLightTransfer::DWallLightTransfer (sector_t *srcSec, int target, BYTE flags)
{
	int linenum;
	int wallflags;

	Source = srcSec;
	TargetID = target;
	Flags = flags;
	DoTransfer (LastLight = srcSec->lightlevel, target, Flags);

	if (!(flags&WLF_NOFAKECONTRAST)) wallflags = WALLF_AUTOCONTRAST|WALLF_ABSLIGHTING;
	else wallflags = WALLF_ABSLIGHTING;

	for (linenum = -1; (linenum = P_FindLineFromID (target, linenum)) >= 0; )
	{
		if (flags & WLF_SIDE1 && lines[linenum].sidenum[0]!=NO_SIDE)
			sides[lines[linenum].sidenum[0]].Flags |= wallflags;

		if (flags & WLF_SIDE2 && lines[linenum].sidenum[1]!=NO_SIDE)
			sides[lines[linenum].sidenum[1]].Flags |= wallflags;
	}
	ChangeStatNum(STAT_LIGHTTRANSFER);
}

void DWallLightTransfer::Tick ()
{
	BYTE light = Source->lightlevel;

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetID, Flags);
	}
}

void DWallLightTransfer::DoTransfer (BYTE lightlevel, int target, BYTE flags)
{
	int linenum;

	for (linenum = -1; (linenum = P_FindLineFromID (target, linenum)) >= 0; )
	{
		line_t * line = &lines[linenum];

		if (flags & WLF_SIDE1 && line->sidenum[0]!=NO_SIDE)
		{
			sides[line->sidenum[0]].Light = (BYTE)lightlevel;
		}

		if (flags & WLF_SIDE2 && line->sidenum[1]!=NO_SIDE)
		{
			sides[line->sidenum[1]].Light = (BYTE)lightlevel;
		}
	}
}


//
// P_SpawnSpecials
//
// After the map has been loaded, scan for specials that spawn thinkers
//

void P_SpawnSpecials (void)
{
	sector_t *sector;
	int i;

	//	Init special SECTORs.
	sector = sectors;
	for (i = 0; i < numsectors; i++, sector++)
	{
		if (sector->special == 0)
			continue;

		// [RH] All secret sectors are marked with a BOOM-ish bitfield
		if (sector->special & SECRET_MASK)
			level.total_secrets++;

		switch (sector->special & 0xff)
		{
			// [RH] Normal DOOM/Hexen specials. We clear off the special for lights
			//	  here instead of inside the spawners.

		case dLight_Flicker:
			// FLICKERING LIGHTS
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DLightFlash (sector);
			}
			sector->special &= 0xff00;
			break;

		case dLight_StrobeFast:
			// STROBE FAST
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DStrobe (sector, STROBEBRIGHT, FASTDARK, false);
			}
			sector->special &= 0xff00;
			break;
			
		case dLight_StrobeSlow:
			// STROBE SLOW
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DStrobe (sector, STROBEBRIGHT, SLOWDARK, false);
			}
			sector->special &= 0xff00;
			break;

		case dLight_Strobe_Hurt:
		case sLight_Strobe_Hurt:
			// STROBE FAST/DEATH SLIME
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DStrobe (sector, STROBEBRIGHT, FASTDARK, false);
			}
			break;

		case dLight_Glow:
			// GLOWING LIGHT
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DGlow (sector);
			}
			sector->special &= 0xff00;
			break;
			
		case dSector_DoorCloseIn30:
			// DOOR CLOSE IN 30 SECONDS
			P_SpawnDoorCloseIn30 (sector);
			break;
			
		case dLight_StrobeSlowSync:
			// SYNC STROBE SLOW
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DStrobe (sector, STROBEBRIGHT, SLOWDARK, true);
			}
			sector->special &= 0xff00;
			break;

		case dLight_StrobeFastSync:
			// SYNC STROBE FAST
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
				new DStrobe (sector, STROBEBRIGHT, FASTDARK, true);
			sector->special &= 0xff00;
			break;

		case dSector_DoorRaiseIn5Mins:
			// DOOR RAISE IN 5 MINUTES
			P_SpawnDoorRaiseIn5Mins (sector);
			break;
			
		case dLight_FireFlicker:
			// fire flickering
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DFireFlicker (sector);
			}
			sector->special &= 0xff00;
			break;

		case dFriction_Low:
			// [BC] In client mode, let the server tell us about sectors' friction level.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				sector->friction = FRICTION_LOW;
				sector->movefactor = 0x269;
			}
			sector->special &= 0xff00;
			// [BC] In client mode, let the server tell us about sectors' friction level.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				sector->special |= FRICTION_MASK;
			}
			break;

		  // [RH] Hexen-like phased lighting
		case LightSequenceStart:

			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DPhased (sector);
			}
			break;

		case Light_Phased:

			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
				( CLIENTDEMO_IsPlaying( ) == false ))
			{
				new DPhased (sector, 48, 63 - (sector->lightlevel & 63));
			}
			break;

		case Sky2:
			sector->sky = PL_SKYFLAT;
			break;

		case dScroll_EastLavaDamage:

			// [BC] Damage is server-side.
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
				( CLIENTDEMO_IsPlaying( )))
			{
				break;
			}

			new DScroller (DScroller::sc_floor, (-FRACUNIT/2)<<3,
				0, -1, sector-sectors, 0);
			break;

		default:

			// [BC] Don't run any other specials in client mode.
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
				( CLIENTDEMO_IsPlaying( )))
			{
				break;
			}

			if ((sector->special & 0xff) >= Scroll_North_Slow &&
				(sector->special & 0xff) <= Scroll_SouthWest_Fast)
			{ // Hexen scroll special
				static const char hexenScrollies[24][2] =
				{
					{  0,  1 }, {  0,  2 }, {  0,  4 },
					{ -1,  0 }, { -2,  0 }, { -4,  0 },
					{  0, -1 }, {  0, -2 }, {  0, -4 },
					{  1,  0 }, {  2,  0 }, {  4,  0 },
					{  1,  1 }, {  2,  2 }, {  4,  4 },
					{ -1,  1 }, { -2,  2 }, { -4,  4 },
					{ -1, -1 }, { -2, -2 }, { -4, -4 },
					{  1, -1 }, {  2, -2 }, {  4, -4 }
				};

				int i = (sector->special & 0xff) - Scroll_North_Slow;
				fixed_t dx = hexenScrollies[i][0] * (FRACUNIT/2);
				fixed_t dy = hexenScrollies[i][1] * (FRACUNIT/2);
				new DScroller (DScroller::sc_floor, dx, dy, -1, sector-sectors, 0);
			}
			else if ((sector->special & 0xff) >= Carry_East5 &&
					 (sector->special & 0xff) <= Carry_East35)
			{ // Heretic scroll special
			  // Only east scrollers also scroll the texture
				new DScroller (DScroller::sc_floor,
					(-FRACUNIT/2)<<((sector->special & 0xff) - Carry_East5),
					0, -1, sector-sectors, 0);
			}
			break;
		}
	}
	
	// Init other misc stuff

	P_SpawnScrollers(); // killough 3/7/98: Add generalized scrollers
	P_SpawnFriction();	// phares 3/12/98: New friction model using linedefs
	P_SpawnPushers();	// phares 3/20/98: New pusher model using linedefs

	for (i=0; i<numlines; i++)
		switch (lines[i].special)
		{
			int s;
			sector_t *sec;

		// killough 3/7/98:
		// support for drawn heights coming from different sector
		case Transfer_Heights:
			sec = sides[*lines[i].sidenum].sector;
			if (lines[i].args[1] & 2)
			{
				sec->MoreFlags |= SECF_FAKEFLOORONLY;
			}
			if (lines[i].args[1] & 4)
			{
				sec->MoreFlags |= SECF_CLIPFAKEPLANES;
			}
			if (lines[i].args[1] & 8)
			{
				sec->MoreFlags |= SECF_UNDERWATER;
			}
			else if (forcewater)
			{
				sec->MoreFlags |= SECF_FORCEDUNDERWATER;
			}
			if (lines[i].args[1] & 16)
			{
				sec->MoreFlags |= SECF_IGNOREHEIGHTSEC;
			}
			if (lines[i].args[1] & 32)
			{
				sec->MoreFlags |= SECF_NOFAKELIGHT;
			}
			for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
			{
				sectors[s].heightsec = sec;
			}
			break;

		// killough 3/16/98: Add support for setting
		// floor lighting independently (e.g. lava)
		case Transfer_FloorLight:
			new DLightTransfer (sides[*lines[i].sidenum].sector, lines[i].args[0], true);
			break;

		// killough 4/11/98: Add support for setting
		// ceiling lighting independently
		case Transfer_CeilingLight:
			new DLightTransfer (sides[*lines[i].sidenum].sector, lines[i].args[0], false);
			break;

		// [Graf Zahl] Add support for setting lighting
		// per wall independently
		case Transfer_WallLight:
			new DWallLightTransfer (sides[*lines[i].sidenum].sector, lines[i].args[0], lines[i].args[1]);
			break;

		// [RH] ZDoom Static_Init settings
		case Static_Init:
			switch (lines[i].args[1])
			{
			case Init_Gravity:

				// [BC] The server will give us gravity updates.
				if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
					( CLIENTDEMO_IsPlaying( )))
				{
					break;
				}

				{
				float grav = ((float)P_AproxDistance (lines[i].dx, lines[i].dy)) / (FRACUNIT * 100.0f);
				for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					sectors[s].gravity = grav;
				}
				break;

			//case Init_Color:
			// handled in P_LoadSideDefs2()

			case Init_Damage:

				// [BC] Damage is server-side.
				if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
					( CLIENTDEMO_IsPlaying( )))
				{
					break;
				}

				{
					int damage = P_AproxDistance (lines[i].dx, lines[i].dy) >> FRACBITS;
					for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					{
						sectors[s].damage = damage;
						sectors[s].mod = 0;//MOD_UNKNOWN;
					}
				}
				break;

			// killough 10/98:
			//
			// Support for sky textures being transferred from sidedefs.
			// Allows scrolling and other effects (but if scrolling is
			// used, then the same sector tag needs to be used for the
			// sky sector, the sky-transfer linedef, and the scroll-effect
			// linedef). Still requires user to use F_SKY1 for the floor
			// or ceiling texture, to distinguish floor and ceiling sky.

			case Init_TransferSky:
				for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					sectors[s].sky = (i+1) | PL_SKYFLAT;
				break;
			}
			break;
		}


	// [BC] Save these values. If they change, and a client connects, send
	// him the new values.
	for ( i = 0; i < numsectors; i++ )
	{
		sectors[i].SavedLightLevel = sectors[i].lightlevel;
		sectors[i].SavedCeilingPic = sectors[i].ceilingpic;
		sectors[i].SavedFloorPic = sectors[i].floorpic;
		sectors[i].SavedCeilingPlane = sectors[i].ceilingplane;
		sectors[i].SavedFloorPlane = sectors[i].floorplane;
		sectors[i].SavedCeilingTexZ = sectors[i].ceilingtexz;
		sectors[i].SavedFloorTexZ = sectors[i].floortexz;
		sectors[i].SavedColorMap = sectors[i].ColorMap;
		sectors[i].SavedFloorXOffset = sectors[i].floor_xoffs;
		sectors[i].SavedFloorYOffset = sectors[i].floor_yoffs;
		sectors[i].SavedCeilingXOffset = sectors[i].ceiling_xoffs;
		sectors[i].SavedCeilingYOffset = sectors[i].ceiling_yoffs;
		sectors[i].SavedFloorXScale = sectors[i].floor_xscale;
		sectors[i].SavedFloorYScale = sectors[i].floor_yscale;
		sectors[i].SavedCeilingXScale = sectors[i].ceiling_xscale;
		sectors[i].SavedCeilingYScale = sectors[i].ceiling_yscale;
		sectors[i].SavedFloorAngle = sectors[i].floor_angle;
		sectors[i].SavedCeilingAngle = sectors[i].ceiling_angle;
		sectors[i].SavedBaseFloorAngle = sectors[i].base_floor_angle;
		sectors[i].SavedBaseFloorYOffset = sectors[i].base_floor_yoffs;
		sectors[i].SavedBaseCeilingAngle = sectors[i].base_ceiling_angle;
		sectors[i].SavedBaseCeilingYOffset = sectors[i].base_ceiling_yoffs;
		sectors[i].SavedFriction = sectors[i].friction;
		sectors[i].SavedMoveFactor = sectors[i].movefactor;
		sectors[i].SavedSpecial = sectors[i].special;
		sectors[i].SavedDamage = sectors[i].damage;
		sectors[i].SavedMOD = sectors[i].mod;
		sectors[i].SavedCeilingReflect = sectors[i].ceiling_reflect;
		sectors[i].SavedFloorReflect = sectors[i].floor_reflect;
	}

	// [RH] Start running any open scripts on this map
	// [BC] Clients don't run scripts.
	// [BB] Clients only run the open net scripts.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		FBehavior::StaticStartTypedScripts (SCRIPT_Open, NULL, false);
	}
	else
	{
		FBehavior::StaticStartTypedScripts (SCRIPT_Open, NULL, false, 0, false, true);
	}
}

// killough 2/28/98:
//
// This function, with the help of r_plane.c and r_bsp.c, supports generalized
// scrolling floors and walls, with optional mobj-carrying properties, e.g.
// conveyor belts, rivers, etc. A linedef with a special type affects all
// tagged sectors the same way, by creating scrolling and/or object-carrying
// properties. Multiple linedefs may be used on the same sector and are
// cumulative, although the special case of scrolling a floor and carrying
// things on it, requires only one linedef. The linedef's direction determines
// the scrolling direction, and the linedef's length determines the scrolling
// speed. This was designed so that an edge around the sector could be used to
// control the direction of the sector's scrolling, which is usually what is
// desired.
//
// Process the active scrollers.
//
// This is the main scrolling code
// killough 3/7/98

void DScroller::Tick ()
{
	fixed_t dx = m_dx, dy = m_dy;

	if (m_Control != -1)
	{	// compute scroll amounts based on a sector's height changes
		fixed_t height = sectors[m_Control].CenterFloor () +
						 sectors[m_Control].CenterCeiling ();
		fixed_t delta = height - m_LastHeight;
		m_LastHeight = height;
		dx = FixedMul(dx, delta);
		dy = FixedMul(dy, delta);
	}

	// killough 3/14/98: Add acceleration
	if (m_Accel)
	{
		m_vdx = dx += m_vdx;
		m_vdy = dy += m_vdy;
	}

	if (!(dx | dy))			// no-op if both (x,y) offsets are 0
		return;

	switch (m_Type)
	{
		case sc_side:					// killough 3/7/98: Scroll wall texture
			sides[m_Affectee].textureoffset += dx;
			sides[m_Affectee].rowoffset += dy;
			break;

		case sc_floor:						// killough 3/7/98: Scroll floor texture
			sectors[m_Affectee].floor_xoffs += dx;
			sectors[m_Affectee].floor_yoffs += dy;
			break;

		case sc_ceiling:					// killough 3/7/98: Scroll ceiling texture
			sectors[m_Affectee].ceiling_xoffs += dx;
			sectors[m_Affectee].ceiling_yoffs += dy;
			break;

		// [RH] Don't actually carry anything here. That happens later.
		case sc_carry:
			level.Scrolls[m_Affectee].ScrollX += dx;
			level.Scrolls[m_Affectee].ScrollY += dy;
			break;

		case sc_carry_ceiling:       // to be added later
			break;
	}
}

//*****************************************************************************
//
void DScroller::UpdateToClient( ULONG ulClient )
{
	switch ( m_Type )
	{
	case sc_side:
	case sc_floor:
	case sc_carry:
	case sc_ceiling:
	case sc_carry_ceiling:

		SERVERCOMMANDS_DoScroller( m_Type, m_dx, m_dy, m_Affectee, ulClient, SVCF_ONLYTHISCLIENT );
		break;
	}
}

//
// Add_Scroller()
//
// Add a generalized scroller to the thinker list.
//
// type: the enumerated type of scrolling: floor, ceiling, floor carrier,
//   wall, floor carrier & scroller
//
// (dx,dy): the direction and speed of the scrolling or its acceleration
//
// control: the sector whose heights control this scroller's effect
//   remotely, or -1 if no control sector
//
// affectee: the index of the affected object (sector or sidedef)
//
// accel: non-zero if this is an accelerative effect
//

DScroller::DScroller (EScrollType type, fixed_t dx, fixed_t dy,
					  int control, int affectee, int accel)
	: DThinker (STAT_SCROLLER)
{
	m_Type = type;
	m_dx = dx;
	m_dy = dy;
	m_Accel = accel;
	m_vdx = m_vdy = 0;
	if ((m_Control = control) != -1)
		m_LastHeight =
			sectors[control].CenterFloor () + sectors[control].CenterCeiling ();
	m_Affectee = affectee;
	switch (type)
	{
	case sc_carry:
		level.AddScroller (this, affectee);
		break;

	case sc_side:
		sides[affectee].Flags |= WALLF_NOAUTODECALS;
		setinterpolation (INTERP_WallPanning, &sides[affectee]);
		break;

	case sc_floor:
		setinterpolation (INTERP_FloorPanning, &sectors[affectee]);
		break;

	case sc_ceiling:
		setinterpolation (INTERP_CeilingPanning, &sectors[affectee]);
		break;

	default:
		break;
	}
}

DScroller::~DScroller ()
{
	switch (m_Type)
	{
	case sc_side:
		stopinterpolation (INTERP_WallPanning, &sides[m_Affectee]);
		break;

	case sc_floor:
		stopinterpolation (INTERP_FloorPanning, &sectors[m_Affectee]);
		break;

	case sc_ceiling:
		stopinterpolation (INTERP_CeilingPanning, &sectors[m_Affectee]);
		break;

	default:
		break;
	}
}

// Adds wall scroller. Scroll amount is rotated with respect to wall's
// linedef first, so that scrolling towards the wall in a perpendicular
// direction is translated into vertical motion, while scrolling along
// the wall in a parallel direction is translated into horizontal motion.
//
// killough 5/25/98: cleaned up arithmetic to avoid drift due to roundoff

DScroller::DScroller (fixed_t dx, fixed_t dy, const line_t *l,
					 int control, int accel)
	: DThinker (STAT_SCROLLER)
{
	fixed_t x = abs(l->dx), y = abs(l->dy), d;
	if (y > x)
		d = x, x = y, y = d;
	d = FixedDiv (x, finesine[(tantoangle[FixedDiv(y,x) >> DBITS] + ANG90)
						  >> ANGLETOFINESHIFT]);
	x = -FixedDiv (FixedMul(dy, l->dy) + FixedMul(dx, l->dx), d);
	y = -FixedDiv (FixedMul(dx, l->dy) - FixedMul(dy, l->dx), d);

	m_Type = sc_side;
	m_dx = x;
	m_dy = y;
	m_vdx = m_vdy = 0;
	m_Accel = accel;
	if ((m_Control = control) != -1)
		m_LastHeight = sectors[control].CenterFloor() + sectors[control].CenterCeiling();
	m_Affectee = *l->sidenum;
	sides[m_Affectee].Flags |= WALLF_NOAUTODECALS;

	setinterpolation (INTERP_WallPanning, &sides[m_Affectee]);
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
#define SCROLL_SHIFT 5

// Initialize the scrollers
static void P_SpawnScrollers(void)
{
	int i;
	line_t *l = lines;

	for (i = 0; i < numlines; i++, l++)
	{
		fixed_t dx;	// direction and speed of scrolling
		fixed_t dy;
		int control = -1, accel = 0;		// no control sector or acceleration
		int special = l->special;

		// killough 3/7/98: Types 245-249 are same as 250-254 except that the
		// first side's sector's heights cause scrolling when they change, and
		// this linedef controls the direction and speed of the scrolling. The
		// most complicated linedef since donuts, but powerful :)
		//
		// killough 3/15/98: Add acceleration. Types 214-218 are the same but
		// are accelerative.

		// [RH] Assume that it's a scroller and zero the line's special.
		l->special = 0;

		dx = dy = 0;	// Shut up, GCC

		if (special == Scroll_Ceiling ||
			special == Scroll_Floor ||
			special == Scroll_Texture_Model)
		{
			if (l->args[1] & 3)
			{
				// if 1, then displacement
				// if 2, then accelerative (also if 3)
				control = sides[*l->sidenum].sector - sectors;
				if (l->args[1] & 2)
					accel = 1;
			}
			if (special == Scroll_Texture_Model ||
				l->args[1] & 4)
			{
				// The line housing the special controls the
				// direction and speed of scrolling.
				dx = l->dx >> SCROLL_SHIFT;
				dy = l->dy >> SCROLL_SHIFT;
			}
			else
			{
				// The speed and direction are parameters to the special.
				dx = (l->args[3] - 128) * (FRACUNIT / 32);
				dy = (l->args[4] - 128) * (FRACUNIT / 32);
			}
		}

		switch (special)
		{
			register int s;

		case Scroll_Ceiling:

			// [BC] The server will update these for us.
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
				( CLIENTDEMO_IsPlaying( )))
			{
				break;
			}

			for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
				new DScroller (DScroller::sc_ceiling, -dx, dy, control, s, accel);
			break;

		case Scroll_Floor:

			// [BC] The server will update these for us.
			if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
				( CLIENTDEMO_IsPlaying( )))
			{
				break;
			}

			if (l->args[2] != 1)
			{ // scroll the floor texture
				for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
					new DScroller (DScroller::sc_floor, -dx, dy, control, s, accel);
			}

			if (l->args[2] > 0)
			{ // carry objects on the floor
				for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
					new DScroller (DScroller::sc_carry, dx, dy, control, s, accel);
			}
			break;

		// killough 3/1/98: scroll wall according to linedef
		// (same direction and speed as scrolling floors)
		case Scroll_Texture_Model:
			for (s=-1; (s = P_FindLineFromID (l->args[0],s)) >= 0;)
				if (s != i)
					new DScroller (dx, dy, lines+s, control, accel);
			break;

		case Scroll_Texture_Offsets:
			// killough 3/2/98: scroll according to sidedef offsets
			s = lines[i].sidenum[0];
			new DScroller (DScroller::sc_side, -sides[s].textureoffset,
						   sides[s].rowoffset, -1, s, accel);
			break;

		case Scroll_Texture_Left:
			new DScroller (DScroller::sc_side, l->args[0] * (FRACUNIT/64), 0,
						   -1, lines[i].sidenum[0], accel);
			break;

		case Scroll_Texture_Right:
			new DScroller (DScroller::sc_side, l->args[0] * (-FRACUNIT/64), 0,
						   -1, lines[i].sidenum[0], accel);
			break;

		case Scroll_Texture_Up:
			new DScroller (DScroller::sc_side, 0, l->args[0] * (FRACUNIT/64),
						   -1, lines[i].sidenum[0], accel);
			break;

		case Scroll_Texture_Down:
			new DScroller (DScroller::sc_side, 0, l->args[0] * (-FRACUNIT/64),
						   -1, lines[i].sidenum[0], accel);
			break;

		case Scroll_Texture_Both:
			if (l->args[0] == 0) {
				dx = (l->args[1] - l->args[2]) * (FRACUNIT/64);
				dy = (l->args[4] - l->args[3]) * (FRACUNIT/64);
				new DScroller (DScroller::sc_side, dx, dy, -1, lines[i].sidenum[0], accel);
			}
			break;

		default:
			// [RH] It wasn't a scroller after all, so restore the special.
			l->special = special;
			break;
		}
	}
}

// killough 3/7/98 -- end generalized scroll effects

////////////////////////////////////////////////////////////////////////////
//
// FRICTION EFFECTS
//
// phares 3/12/98: Start of friction effects

// As the player moves, friction is applied by decreasing the x and y
// momentum values on each tic. By varying the percentage of decrease,
// we can simulate muddy or icy conditions. In mud, the player slows
// down faster. In ice, the player slows down more slowly.
//
// The amount of friction change is controlled by the length of a linedef
// with type 223. A length < 100 gives you mud. A length > 100 gives you ice.
//
// Also, each sector where these effects are to take place is given a
// new special type _______. Changing the type value at runtime allows
// these effects to be turned on or off.
//
// Sector boundaries present problems. The player should experience these
// friction changes only when his feet are touching the sector floor. At
// sector boundaries where floor height changes, the player can find
// himself still 'in' one sector, but with his feet at the floor level
// of the next sector (steps up or down). To handle this, Thinkers are used
// in icy/muddy sectors. These thinkers examine each object that is touching
// their sectors, looking for players whose feet are at the same level as
// their floors. Players satisfying this condition are given new friction
// values that are applied by the player movement code later.

//
// killough 8/28/98:
//
// Completely redid code, which did not need thinkers, and which put a heavy
// drag on CPU. Friction is now a property of sectors, NOT objects inside
// them. All objects, not just players, are affected by it, if they touch
// the sector's floor. Code simpler and faster, only calling on friction
// calculations when an object needs friction considered, instead of doing
// friction calculations on every sector during every tic.
//
// Although this -might- ruin Boom demo sync involving friction, it's the only
// way, short of code explosion, to fix the original design bug. Fixing the
// design bug in Boom's original friction code, while maintaining demo sync
// under every conceivable circumstance, would double or triple code size, and
// would require maintenance of buggy legacy code which is only useful for old
// demos. Doom demos, which are more important IMO, are not affected by this
// change.
//
// [RH] On the other hand, since I've given up on trying to maintain demo
//		sync between versions, these considerations aren't a big deal to me.
//
/////////////////////////////
//
// Initialize the sectors where friction is increased or decreased

static void P_SpawnFriction(void)
{
	int i;
	line_t *l = lines;

	// [BC] Don't do this in client mode, because the friction for the sector could
	// have changed at some point on the server end.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	for (i = 0 ; i < numlines ; i++,l++)
	{
		if (l->special == Sector_SetFriction)
		{
			int length;

			if (l->args[1])
			{	// [RH] Allow setting friction amount from parameter
				length = l->args[1] <= 200 ? l->args[1] : 200;
			}
			else
			{
				length = P_AproxDistance(l->dx,l->dy)>>FRACBITS;
			}

			P_SetSectorFriction (l->args[0], length, false);
			l->special = 0;
		}
	}
}

void P_SetSectorFriction (int tag, int amount, bool alterFlag)
{
	int s;
	fixed_t friction, movefactor;

	// An amount of 100 should result in a friction of
	// ORIG_FRICTION (0xE800)
	friction = (0x1EB8*amount)/0x80 + 0xD001;

	// killough 8/28/98: prevent odd situations
	if (friction > FRACUNIT)
		friction = FRACUNIT;
	if (friction < 0)
		friction = 0;

	// The following check might seem odd. At the time of movement,
	// the move distance is multiplied by 'friction/0x10000', so a
	// higher friction value actually means 'less friction'.

	// [RH] Twiddled these values so that momentum on ice (with
	//		friction 0xf900) is the same as in Heretic/Hexen.
	if (friction >= ORIG_FRICTION)	// ice
//		movefactor = ((0x10092 - friction)*(0x70))/0x158;
		movefactor = ((0x10092 - friction) * 1024) / 4352 + 568;
	else
		movefactor = ((friction - 0xDB34)*(0xA))/0x80;

	// killough 8/28/98: prevent odd situations
	if (movefactor < 32)
		movefactor = 32;

	for (s = -1; (s = P_FindSectorFromTag (tag,s)) >= 0; )
	{
		// killough 8/28/98:
		//
		// Instead of spawning thinkers, which are slow and expensive,
		// modify the sector's own friction values. Friction should be
		// a property of sectors, not objects which reside inside them.
		// Original code scanned every object in every friction sector
		// on every tic, adjusting its friction, putting unnecessary
		// drag on CPU. New code adjusts friction of sector only once
		// at level startup, and then uses this friction value.

		sectors[s].friction = friction;
		sectors[s].movefactor = movefactor;
		if (alterFlag)
		{
			// When used inside a script, the sectors' friction flags
			// can be enabled and disabled at will.
			if (friction == ORIG_FRICTION)
			{
				sectors[s].special &= ~FRICTION_MASK;
			}
			else
			{
				sectors[s].special |= FRICTION_MASK;
			}

			// [BC] If we're the server, update clients about this friction change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFriction( s );
		}
	}
}

//
// phares 3/12/98: End of friction effects
//
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// PUSH/PULL EFFECT
//
// phares 3/20/98: Start of push/pull effects
//
// This is where push/pull effects are applied to objects in the sectors.
//
// There are four kinds of push effects
//
// 1) Pushing Away
//
//    Pushes you away from a point source defined by the location of an
//    MT_PUSH Thing. The force decreases linearly with distance from the
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PUSH. The force is felt only if the point
//    MT_PUSH can see the target object.
//
// 2) Pulling toward
//
//    Same as Pushing Away except you're pulled toward an MT_PULL point
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PULL. The force is felt only if the point
//    MT_PULL can see the target object.
//
// 3) Wind
//
//    Pushes you in a constant direction. Full force above ground, half
//    force on the ground, nothing if you're below it (water).
//
// 4) Current
//
//    Pushes you in a constant direction. No force above ground, full
//    force if on the ground or below it (water).
//
// The magnitude of the force is controlled by the length of a controlling
// linedef. The force vector for types 3 & 4 is determined by the angle
// of the linedef, and is constant.
//
// For each sector where these effects occur, the sector special type has
// to have the PUSH_MASK bit set. If this bit is turned off by a switch
// at run-time, the effect will not occur. The controlling sector for
// types 1 & 2 is the sector containing the MT_PUSH/MT_PULL Thing.


class APointPusher : public AActor
{
	DECLARE_STATELESS_ACTOR (APointPusher, AActor)
};

IMPLEMENT_STATELESS_ACTOR (APointPusher, Any, 5001, 0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS

class APointPuller : public AActor
{
	DECLARE_STATELESS_ACTOR (APointPuller, AActor)
};

IMPLEMENT_STATELESS_ACTOR (APointPuller, Any, 5002, 0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS

#define PUSH_FACTOR 7

/////////////////////////////
//
// Add a push thinker to the thinker list

DPusher::DPusher (DPusher::EPusher type, line_t *l, int magnitude, int angle,
				  AActor *source, int affectee)
{
	m_Source = source;
	m_Type = type;
	if (l)
	{
		m_Xmag = l->dx>>FRACBITS;
		m_Ymag = l->dy>>FRACBITS;
		m_Magnitude = P_AproxDistance (m_Xmag, m_Ymag);
	}
	else
	{ // [RH] Allow setting magnitude and angle with parameters
		ChangeValues (magnitude, angle);
	}
	if (source) // point source exist?
	{
		m_Radius = (m_Magnitude) << (FRACBITS+1); // where force goes to zero
		m_X = m_Source->x;
		m_Y = m_Source->y;
	}
	m_Affectee = affectee;
}

/////////////////////////////
//
// PIT_PushThing determines the angle and magnitude of the effect.
// The object's x and y momentum values are changed.
//
// tmpusher belongs to the point source (MT_PUSH/MT_PULL).
//

DPusher *tmpusher; // pusher structure for blockmap searches

bool PIT_PushThing (AActor *thing)
{
	if ((thing->flags2 & MF2_WINDTHRUST) && !(thing->flags & MF_NOCLIP))
	{
		int sx = tmpusher->m_X;
		int sy = tmpusher->m_Y;
		int dist = P_AproxDistance (thing->x - sx,thing->y - sy);
		int speed = (tmpusher->m_Magnitude -
					((dist>>FRACBITS)>>1))<<(FRACBITS-PUSH_FACTOR-1);

		// If speed <= 0, you're outside the effective radius. You also have
		// to be able to see the push/pull source point.

		if ((speed > 0) && (P_CheckSight (thing, tmpusher->m_Source, 1)))
		{
			angle_t pushangle = R_PointToAngle2 (thing->x, thing->y, sx, sy);
			if (tmpusher->m_Source->IsA (RUNTIME_CLASS(APointPusher)))
				pushangle += ANG180;    // away
			pushangle >>= ANGLETOFINESHIFT;
			thing->momx += FixedMul (speed, finecosine[pushangle]);
			thing->momy += FixedMul (speed, finesine[pushangle]);
		}
	}
	return true;
}

/////////////////////////////
//
// T_Pusher looks for all objects that are inside the radius of
// the effect.
//
extern fixed_t tmbbox[4];

void DPusher::Tick ()
{
	static TArray<AActor *> pushbt;
	sector_t *sec;
	AActor *thing;
	msecnode_t *node;
	int xspeed,yspeed;
	int xl,xh,yl,yh,bx,by;
	int radius;
	int ht;

	if (!var_pushers)
		return;

	sec = sectors + m_Affectee;

	// Be sure the special sector type is still turned on. If so, proceed.
	// Else, bail out; the sector type has been changed on us.

	if (!(sec->special & PUSH_MASK))
		return;

	// For constant pushers (wind/current) there are 3 situations:
	//
	// 1) Affected Thing is above the floor.
	//
	//    Apply the full force if wind, no force if current.
	//
	// 2) Affected Thing is on the ground.
	//
	//    Apply half force if wind, full force if current.
	//
	// 3) Affected Thing is below the ground (underwater effect).
	//
	//    Apply no force if wind, full force if current.
	//
	// Apply the effect to clipped players only for now.
	//
	// In Phase II, you can apply these effects to Things other than players.
	// [RH] No Phase II, but it works with anything having MF2_WINDTHRUST now.

	if (m_Type == p_push)
	{
		// Seek out all pushable things within the force radius of this
		// point pusher. Crosses sectors, so use blockmap.

		tmpusher = this; // MT_PUSH/MT_PULL point source
		radius = m_Radius; // where force goes to zero
		tmbbox[BOXTOP]    = m_Y + radius;
		tmbbox[BOXBOTTOM] = m_Y - radius;
		tmbbox[BOXRIGHT]  = m_X + radius;
		tmbbox[BOXLEFT]   = m_X - radius;

		pushbt.Clear();

		xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
		xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
		yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
		yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;
		for (bx=xl ; bx<=xh ; bx++)
			for (by=yl ; by<=yh ; by++)
				P_BlockThingsIterator (bx, by, PIT_PushThing, pushbt);
		return;
	}

	// constant pushers p_wind and p_current

	node = sec->touching_thinglist; // things touching this sector
	for ( ; node ; node = node->m_snext)
	{
		thing = node->m_thing;
		if (!(thing->flags2 & MF2_WINDTHRUST) || (thing->flags & MF_NOCLIP))
			continue;
		if (m_Type == p_wind)
		{
			if (sec->heightsec == NULL ||
				sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)
			{ // NOT special water sector
				if (thing->z > thing->floorz) // above ground
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
				else // on ground
				{
					xspeed = (m_Xmag)>>1; // half force
					yspeed = (m_Ymag)>>1;
				}
			}
			else // special water sector
			{
				ht = sec->heightsec->floorplane.ZatPoint (thing->x, thing->y);
				if (thing->z > ht) // above ground
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
				else if (thing->player->viewz < ht) // underwater
				{
					xspeed = yspeed = 0; // no force
				}
				else // wading in water
				{
					xspeed = (m_Xmag)>>1; // half force
					yspeed = (m_Ymag)>>1;
				}
			}
		}
		else // p_current
		{
			const secplane_t *floor;

			if (sec->heightsec == NULL ||
				(sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
			{ // NOT special water sector
				floor = &sec->floorplane;
			}
			else
			{ // special water sector
				floor = &sec->heightsec->floorplane;
			}
			if (thing->z > floor->ZatPoint (thing->x, thing->y))
			{ // above ground
				xspeed = yspeed = 0; // no force
			}
			else
			{ // on ground/underwater
				xspeed = m_Xmag; // full force
				yspeed = m_Ymag;
			}
		}
		thing->momx += xspeed<<(FRACBITS-PUSH_FACTOR);
		thing->momy += yspeed<<(FRACBITS-PUSH_FACTOR);
	}
}

/////////////////////////////
//
// P_GetPushThing() returns a pointer to an MT_PUSH or MT_PULL thing,
// NULL otherwise.

AActor *P_GetPushThing (int s)
{
	AActor* thing;
	sector_t* sec;

	sec = sectors + s;
	thing = sec->thinglist;

	while (thing &&
		   !thing->IsA (RUNTIME_CLASS(APointPusher)) &&
		   !thing->IsA (RUNTIME_CLASS(APointPuller)))
	{
		thing = thing->snext;
	}
	return thing;
}

/////////////////////////////
//
// Initialize the sectors where pushers are present
//

static void P_SpawnPushers ()
{
	int i;
	line_t *l = lines;
	register int s;

	for (i = 0; i < numlines; i++, l++)
	{
		switch (l->special)
		{
		case Sector_SetWind: // wind
			for (s = -1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0 ; )
				new DPusher (DPusher::p_wind, l->args[3] ? l : NULL, l->args[1], l->args[2], NULL, s);
			break;

		case Sector_SetCurrent: // current
			for (s = -1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0 ; )
				new DPusher (DPusher::p_current, l->args[3] ? l : NULL, l->args[1], l->args[2], NULL, s);
			break;

		case PointPush_SetForce: // push/pull
			if (l->args[0]) {	// [RH] Find thing by sector
				for (s = -1; (s = P_FindSectorFromTag (l->args[0], s)) >= 0 ; )
				{
					AActor *thing = P_GetPushThing (s);
					if (thing) {	// No MT_P* means no effect
						// [RH] Allow narrowing it down by tid
						if (!l->args[1] || l->args[1] == thing->tid)
							new DPusher (DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
										 0, thing, s);
					}
				}
			} else {	// [RH] Find thing by tid
				AActor *thing;
				FActorIterator iterator (l->args[1]);

				while ( (thing = iterator.Next ()) )
				{
					if (thing->IsA (RUNTIME_CLASS(APointPuller)) ||
						thing->IsA (RUNTIME_CLASS(APointPusher)))
						new DPusher (DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
									 0, thing, thing->Sector - sectors);
				}
			}
			break;
		}
	}
}

//
// phares 3/20/98: End of Pusher effects
//
////////////////////////////////////////////////////////////////////////////

void sector_t::AdjustFloorClip () const
{
	msecnode_t *node;

	for (node = touching_thinglist; node; node = node->m_snext)
	{
		if (node->m_thing->flags2 & MF2_FLOORCLIP)
		{
			node->m_thing->AdjustFloorClip();
		}
	}
}
