#include "g_local.h"



/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission (edict_t *ent)
{
	if (deathmatch->value || coop->value)
		ent->client->showscores = true;
	VectorCopy (level.intermission_origin, ent->s.origin);
	ent->client->ps.pmove.origin[0] = level.intermission_origin[0]*8;
	ent->client->ps.pmove.origin[1] = level.intermission_origin[1]*8;
	ent->client->ps.pmove.origin[2] = level.intermission_origin[2]*8;
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->ps.gunindex = 0;
	ent->client->ps.blend[3] = 0;
	ent->client->ps.rdflags &= ~RDF_UNDERWATER;

	// clean up powerup info
	ent->client->quad_framenum = 0;
	ent->client->invincible_framenum = 0;
	ent->client->breather_framenum = 0;
	ent->client->enviro_framenum = 0;
	ent->client->grenade_blew_up = false;
	ent->client->grenade_time = 0;

	ent->viewheight = 0;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;

	// add the layout

	if (deathmatch->value || coop->value)
	{
		DeathmatchScoreboardMessage (ent, NULL);
		gi.unicast (ent, true);
	}

}

void BeginIntermission (edict_t *targ)
{
	int		i, n;
	edict_t	*ent, *client;

	if (level.intermissiontime)
		return;		// already activated

	game.autosaved = false;

	// respawn any dead clients
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		if (client->health <= 0)
			respawn(client);
	}

	level.intermissiontime = level.time;
	level.changemap = targ->map;

	if (strstr(level.changemap, "*"))
	{
		if (coop->value)
		{
			for (i=0 ; i<maxclients->value ; i++)
			{
				client = g_edicts + 1 + i;
				if (!client->inuse)
					continue;
				// strip players of all keys between units
				for (n = 0; n < MAX_ITEMS; n++)
				{
					if (itemlist[n].flags & IT_KEY)
						client->client->pers.inventory[n] = 0;
				}
			}
		}
	}
	else
	{
		if (!deathmatch->value)
		{
			level.exitintermission = 1;		// go immediately to the next level
			return;
		}
	}

	level.exitintermission = 0;

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if (!ent)
	{	// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start");
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch");
	}
	else
	{	// chose one of four spots
		i = rand() & 3;
		while (i--)
		{
			ent = G_Find (ent, FOFS(classname), "info_player_intermission");
			if (!ent)	// wrap around the list
				ent = G_Find (ent, FOFS(classname), "info_player_intermission");
		}
	}

	VectorCopy (ent->s.origin, level.intermission_origin);
	VectorCopy (ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		MoveClientToIntermission (client);
	}
}


/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage (edict_t *ent, edict_t *killer)
{
	char	entry[1024];
	char	string[1400];
	int		stringlength;
	int		i, j, k;
	int		sorted[MAX_CLIENTS];
	int		sortedscores[MAX_CLIENTS];
	int		score, total;
	int		picnum;
	int		x, y;
	gclient_t	*cl;
	edict_t		*cl_ent;
	char	*tag;

	// sort the clients by score
	total = 0;
	for (i=0 ; i<game.maxclients ; i++)
	{
		cl_ent = g_edicts + 1 + i;
		if (!cl_ent->inuse || game.clients[i].resp.spectator)
			continue;
		score = game.clients[i].resp.score;
		for (j=0 ; j<total ; j++)
		{
			if (score > sortedscores[j])
				break;
		}
		for (k=total ; k>j ; k--)
		{
			sorted[k] = sorted[k-1];
			sortedscores[k] = sortedscores[k-1];
		}
		sorted[j] = i;
		sortedscores[j] = score;
		total++;
	}

	// print level name and exit rules
	string[0] = 0;

	stringlength = strlen(string);

	// add the clients in sorted order
	if (total > 12)
		total = 12;

	for (i=0 ; i<total ; i++)
	{
		cl = &game.clients[sorted[i]];
		cl_ent = g_edicts + 1 + sorted[i];

		picnum = gi.imageindex ("i_fixme");
		x = (i>=6) ? 160 : 0;
		y = 32 + 32 * (i%6);

		// add a dogtag
		if (cl_ent == ent)
			tag = "tag1";
		else if (cl_ent == killer)
			tag = "tag2";
		else
			tag = NULL;
		if (tag)
		{
			Com_sprintf (entry, sizeof(entry),
				"xv %i yv %i picn %s ",x+32, y, tag);
			j = strlen(entry);
			if (stringlength + j > 1024)
				break;
			strcpy (string + stringlength, entry);
			stringlength += j;
		}

		// send the layout
		Com_sprintf (entry, sizeof(entry),
			"client %i %i %i %i %i %i ",
			x, y, sorted[i], cl->resp.score, cl->ping, (level.framenum - cl->resp.enterframe)/600);
		j = strlen(entry);
		if (stringlength + j > 1024)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
}


/*
==================
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
void DeathmatchScoreboard (edict_t *ent)
{
	DeathmatchScoreboardMessage (ent, ent->enemy);
	gi.unicast (ent, true);
}


/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f (edict_t *ent)
{
	ent->client->showinventory = false;
	ent->client->showhelp = false;

	if (!deathmatch->value && !coop->value)
		return;

	if (ent->client->showscores)
	{
		ent->client->showscores = false;
		return;
	}

	ent->client->showscores = true;
	DeathmatchScoreboard (ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
void HelpComputer (edict_t *ent)
{
	char	string[1024];
	char	*sk;

	if (skill->value == 0)
		sk = "easy";
	else if (skill->value == 1)
		sk = "medium";
	else if (skill->value == 2)
		sk = "hard";
	else
		sk = "hard+";

	// send the layout
	Com_sprintf (string, sizeof(string),
		"xv 32 yv 8 picn help "			// background
		"xv 202 yv 12 string2 \"%s\" "		// skill
		"xv 0 yv 24 cstring2 \"%s\" "		// level name
		"xv 0 yv 54 cstring2 \"%s\" "		// help 1
		"xv 0 yv 110 cstring2 \"%s\" "		// help 2
		"xv 50 yv 164 string2 \" kills     goals    secrets\" "
		"xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ", 
		sk,
		level.level_name,
		game.helpmessage1,
		game.helpmessage2,
		level.killed_monsters, level.total_monsters, 
		level.found_goals, level.total_goals,
		level.found_secrets, level.total_secrets);

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
}

/*
==================
Get_Card_Name

Get the name of the specified cards based on their card ID
==================
*/

char * Get_Card_Name (card_t card)
{
	static char cardName[128];

	if (card == CARD_BITE)
	{
		Com_sprintf(cardName, sizeof(cardName),
			"Vicious Bite");
	}
	else if (card == CARD_BLOCK)
	{
		Com_sprintf(cardName, sizeof(cardName),
			"Block Attack");
	}
	else if (card == CARD_HEAL)
	{
		Com_sprintf(cardName, sizeof(cardName),
			"Restore Health");
	}
	else if (card == CARD_PUNCH)
	{
		Com_sprintf(cardName, sizeof(cardName),
			"Brute Punch");
	}
	else if (card == CARD_SHOTGUN)
	{
		Com_sprintf(cardName, sizeof(cardName),
			"Shotgun Attack");
	}
	else if (card == CARD_HIJACK)
	{
		Com_sprintf(cardName, sizeof(cardName),
			"Hijack Weapon");
	}
	else
	{
		Com_sprintf(cardName, sizeof(cardName),
			"Error: Unknown card.");
	}

	return cardName;
}

/*
==================
Get_Card_Desc

Get the description of the specified cards based on their card ID
==================
*/

char * Get_Card_Desc (card_t card)
{
	static char cardDesc[128];

	if (card == CARD_BITE)
	{
		Com_sprintf(cardDesc, sizeof(cardDesc),
			"Deal 10 damage to the enemy,\n and shuffle this\n into your opponent's deck.");
	}
	else if (card == CARD_BLOCK)
	{
		Com_sprintf(cardDesc, sizeof(cardDesc),
			"During this turn, \n all damage is reduced to 1.");
	}
	else if (card == CARD_HEAL)
	{
		Com_sprintf(cardDesc, sizeof(cardDesc),
			"Heal yourself for 15 health.");
	}
	else if (card == CARD_PUNCH)
	{
		Com_sprintf(cardDesc, sizeof(cardDesc),
			"Deal 10 damage to the enemy.");
	}
	else if (card == CARD_SHOTGUN)
	{
		Com_sprintf(cardDesc, sizeof(cardDesc),
			"Deal 3 damage to the enemy, \n 5 times.");
	}
	else if (card == CARD_HIJACK)
	{
		Com_sprintf(cardDesc, sizeof(cardDesc),
			"Draw a card from your\n opponent's deck, and shuffle\n it into yours.");
	}
	else
	{
		Com_sprintf(cardDesc, sizeof(cardDesc),
			"Error: Unknown card.");
	}

	return cardDesc;
}

/*
==================
Initialize_Deckbuilding_UI

Draw the interface for deckbuilding and collection management
==================
*/

//This is a modified version of the battle screen UI. Expect to make a fair number of changes in the general logic and implementation.
void Initialize_Deckbuilding_UI (edict_t *ent)
{
	char	string[2048 + 2048 + 2048];
	int cardPositionX;
	int cardPositionY;
	int cardCount;
	char	cardName[CARD_MAX][128];
	char	cardDesc[CARD_MAX][256];
	char	cardHUD[1024 + 1024];
	char	deckCount[CARD_MAX][128];
	char	numInDeck[3];
	int numInvisible;
	// Debug

	cardCount = CARD_MAX;
	
	cardPositionX = 32 - cardCount * 50;
	cardPositionY = 200;
	Com_sprintf(string, sizeof(string),
		"");

	while (cardCount > 0)
	{
		Com_sprintf (cardName[cardCount - 1], sizeof(cardName[cardCount - 1]), Get_Card_Name(cardCount - 1));

		cardCount -= 1;
	}

	cardCount = CARD_MAX;

	while (cardCount > 0)
	{		
		Com_sprintf (cardDesc[cardCount - 1], sizeof(cardDesc[cardCount - 1]), Get_Card_Desc(cardCount - 1));

		cardCount -= 1;
	}
	
	// Debug
	// Generate the cards for reference
	// Only show the selected card and its details.
	Com_sprintf(cardHUD, sizeof(cardHUD),
		"xv %i yv %i picn help "								// background
		"xv %i yv %i cstring2 \"%s\" "							// card name
		"xv %i yv %i cstring2 \"%s\" "
		"xv %i yv %i cstring \"%s\" "
		"xv %i yv %i cstring \"%s%i\" ",							// card desc

		32,				// X background
		-275 + 8 + cardPositionY,	// Y background
		0,				// X name
		-275 + 24 + cardPositionY,		// Y name
		cardName[ent->client->pers.DB_selectedCard],											// Name
		0,				// X desc
		-275 + 54 + cardPositionY,		// Y desc
		cardDesc[ent->client->pers.DB_selectedCard],
		
		0,
		-190 + 24 + cardPositionY,
		ent->client->pers.DB_selectedCard == CARD_BITE ? "Cannot be manually added \nor removed from deck." : "",

		0,				// X name
		-150 + 24 + cardPositionY,
		"Stored: ",
		ent->client->pers.collection[ent->client->pers.DB_selectedCard]);
		
	strcat(string, cardHUD);

	// Not displaying properly
	// Going to try developing a new system for it in the UI
	// Generate the current deck details
	cardCount = 0;
	numInvisible = 0;
	while (cardCount < CARD_MAX)
	{
		//Iterate through the deck
		//Print out any cards that are in it, and the number of that card
		if (ent->client->pers.deck[cardCount] != 0)
		{
			strcat(cardName[cardCount], ":");
			Com_sprintf(deckCount[cardCount], sizeof(deckCount[cardCount]),
				"xv %i yv %i cstring \"%s\" "
				"xv %i yv %i cstring %i ",
				
				0,													
				-200 + 20 * (cardCount - numInvisible),									
				cardName[cardCount],
				
				100,
				-200 + 20 * (cardCount - numInvisible),
				ent->client->pers.deck[cardCount]);
		
			strcat(string, deckCount[cardCount]);

		}
		else
		{
			numInvisible += 1;
		}

		cardCount += 1;
	}
	// send the layout
	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
}




/*
==================
Initialize_Battle_UI

Draw the card selection screen at the start of a battle
==================
*/
void Initialize_Battle_UI (edict_t *ent)
{
	char	string[1024];
	char	*sk;
	char	battleLog[1024];
	int cardPositionX;
	int cardPositionY;
	int cardCount;
	int battleLogCount;
	char	cardName[4][1024];
	char	cardDesc[5][1024];

	cardCount = ent->client->pers.numCards;
	
	cardPositionX = 32 - cardCount * 75;
	cardPositionY = 200;

	while (cardCount > 0)
	{
		Com_sprintf (cardName[cardCount - 1], sizeof(cardName[cardCount - 1]), Get_Card_Name(ent->client->pers.currentHand[cardCount - 1]));

		cardCount -= 1;
	}

	cardCount = ent->client->pers.numCards;
	Com_sprintf (cardName[cardCount - 1], sizeof(cardName[cardCount - 1]), Get_Card_Name(ent->client->pers.currentOpponent->chosenCard));
	Com_sprintf (cardDesc[cardCount - 1], sizeof(cardDesc[cardCount - 1]), Get_Card_Desc(ent->client->pers.currentOpponent->chosenCard));
	cardCount -= 1;
	while (cardCount > 0)
	{		
		Com_sprintf (cardDesc[cardCount - 1], sizeof(cardDesc[cardCount - 1]), Get_Card_Desc(ent->client->pers.currentHand[cardCount - 1]));

		cardCount -= 1;
	}

	// send the layout
	Com_sprintf (string, sizeof(string),
		"xv %i yv %i picn help "			// background
		"xv %i yv %i cstring2 \"%s\" "		// card1 name
		"xv %i yv %i cstring2 \"%s\" "		// card1 text
		
		"xv %i yv %i picn help "			// background
		"xv %i yv %i cstring2 \"%s\" "		// card2 name
		"xv %i yv %i cstring2 \"%s\" "		// card2 text
		
		"xv %i yv %i picn help "			// background
		"xv %i yv %i cstring2 \"%s\" "		// card3 name
		"xv %i yv %i cstring2 \"%s\" "		// card3 text
		
		"xv %i yv %i picn help "			// background
		"xv %i yv %i cstring2 \"%s\" "		// enemy card name
		"xv %i yv %i cstring2 \"%s\" "		// enemy card text
		
		"xv %i yv %i cstring2 \"%s\" "		// battle log??????
		"xv %i yv %i cstring2 \"%s\" "		// battle log??????
		"xv %i yv %i cstring2 \"%s\" "
		
		"xv %i yv %i cstring2 \"%s%i\" ",	// enemy health

		// FIXME
		// Change UI logic to base positions on the number of cards in a player's hand

		32 + cardPositionX,
		8 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 0),
		0 + cardPositionX,
		24 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 0),
		cardName[0], 
		0 + cardPositionX,
		54 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 0),
		cardDesc[0],
		
		32,
		8 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 1),
		0,
		24 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 1),
		cardName[1], 
		0,
		54 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 1),
		cardDesc[1],
		
		32 - cardPositionX,
		8 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 2),
		0 - cardPositionX,
		24 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 2),
		cardName[2], 
		0 - cardPositionX,
		54 + cardPositionY - 10 * (ent->client->pers.DB_selectedCard == 2),
		cardDesc[2],
		
		32,
		-350 + 8 + cardPositionY,
		0,
		-350 + 24 + cardPositionY,
		cardName[3], 
		0,
		-350 + 54 + cardPositionY,
		cardDesc[3],
		
		0 + cardPositionX,
		-250 + 24 + cardPositionY + 10,
		ent->client->pers.battleLog[0],
		0 + cardPositionX,
		-250 + 24 + cardPositionY + 20,
		ent->client->pers.battleLog[1],
		0 + cardPositionX,
		-250 + 24 + cardPositionY + 30,
		ent->client->pers.battleLog[2],
		
		0 + cardPositionX,
		-350 + 24 + cardPositionY,
		"ENEMY HEALTH: ",
		ent->client->pers.currentOpponent->health);

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
	
}


/*
==================
Draw_Card

Handle card draw after a round
==================
*/

void Draw_Card (edict_t *ent, int *deck, int *currentHand)
{
	int counter;
	int deckSize;
	int newCard;
	int cardThreshold;
	int emptySlot;

	// FIXME
	// Various situations where "Hand is full" occurs, for reasons not yet understood.
	// Find out what is causing this and whether it is a bug or the draw executing properly.

	counter = 0;
	deckSize = 0;
	while (counter < CARD_MAX)
	{
		deckSize += deck[counter];
		counter += 1;
	}
	if (deckSize <= 0)
	{
		// When your deck is empty, draw a hijack
		if (ent->client)
			gi.bprintf(PRINT_CHAT, "You're ");
		else
			gi.bprintf(PRINT_CHAT, "The opponent is ");
		gi.bprintf(PRINT_CHAT, "out of cards!\n");

		newCard = CARD_HIJACK;
	}
	else
	{
		// Otherwise, find a card from your deck
		newCard = rand() % deckSize + 1;

		counter = 0;
		cardThreshold = 0;
		while (counter < CARD_MAX)
		{
			cardThreshold += deck[counter];
			if (newCard <= cardThreshold)
			{
				newCard = counter;
				if (deck[counter] <= 0)
					gi.bprintf(PRINT_CHAT, "We're drawing a card that doesn't exist.\n");
				break;
			}
			counter += 1;
		}
		if (counter == CARD_MAX)
		{
			// This should never happen. If it does, print this out
			gi.bprintf(PRINT_CHAT, "ERROR: no card found in deck. newCard = %i. deckSize = %i", newCard, deckSize);
		}
	}
	

	counter = 0;
	while (counter < 3)
	{
		if (currentHand[counter] == -1)
		{
			//Add it to the leftmost empty hand position, temporarily remove it from deck
			currentHand[counter] = newCard;
			if (deckSize > 0)
				deck[newCard] -= 1;
			/*
			if (ent->client)
				gi.bprintf(PRINT_CHAT, "Player ");
			gi.bprintf(PRINT_CHAT, "Card drawn: %i\n", newCard);
			if (ent->client)
				gi.bprintf(PRINT_CHAT, "Player ");
			gi.bprintf(PRINT_CHAT, "Hand: %i %i %i \n", currentHand[0], currentHand[1], currentHand[2]);
			*/
			return;
		}
		counter += 1;
	}
	if (counter == 3)
	{
		gi.bprintf(PRINT_CHAT, "ERROR: Hand is full. \n");
	}
}

/*
==================
Move_Cards

Handle card draw after a round
==================
*/

void Move_Cards (edict_t *ent, int *currentHand)
{
	int emptySlot;
	int counter;
	qboolean foundEmpty;

	counter = 0;
	foundEmpty = false;

	while (counter < 3)
	{
		//If we find a card to the right of an empty position, move it
		if (foundEmpty)
		{
			if (currentHand[counter] != -1)
			{
				break;
			}
		}
		else if (currentHand[counter] == -1)
		{
			foundEmpty = true;
		}

		counter += 1;
	}
	//If nothing needs to be moved
	if (counter == 3)
	{
		return;
	}
	currentHand[counter - 1] = currentHand[counter];
	currentHand[counter] = -1;
	Move_Cards(ent, currentHand);
}


/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f (edict_t *ent)
{
	// this is for backwards compatability
	if (deathmatch->value)
	{
		Cmd_Score_f (ent);
		return;
	}

	ent->client->showinventory = false;
	ent->client->showscores = false;

	if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged))
	{
		ent->client->showhelp = false;
		return;
	}

	ent->client->showhelp = true;
	ent->client->pers.helpchanged = 0;
	HelpComputer (ent);
}

void Cmd_NextCard_f (edict_t *ent)
{
	// Prevent the commands from overflowing
	if (level.time - (ent->client->pers.lastDeckbuildingCmd) < 0.05)
	{
		return;
	}


	if (ent->client->pers.inDeckbuilding)
	{
		if (ent->client->pers.DB_selectedCard >= CARD_MAX - 1)
		{
			return;
		}
		ent->client->pers.DB_selectedCard += 1;
		Initialize_Deckbuilding_UI (ent);
		ent->client->pers.lastDeckbuildingCmd = level.time;
	}
	else if (ent->client->pers.inBattle)
	{
		if (ent->client->pers.DB_selectedCard >= 2)
		{
			return;
		}
		ent->client->pers.DB_selectedCard += 1;
		Initialize_Battle_UI (ent);
		ent->client->pers.lastDeckbuildingCmd = level.time;
	}
}

void Cmd_PrevCard_f (edict_t *ent)
{
	// Prevent the commands from overflowing
	if (level.time - (ent->client->pers.lastDeckbuildingCmd) < 0.05)
	{
		return;
	}

	if (ent->client->pers.inDeckbuilding)
	{
		if (ent->client->pers.DB_selectedCard <= 0)
		{
			return;
		}
		ent->client->pers.DB_selectedCard -= 1;
		Initialize_Deckbuilding_UI (ent);
		ent->client->pers.lastDeckbuildingCmd = level.time;
	}
	else if (ent->client->pers.inBattle)
	{
		if (ent->client->pers.DB_selectedCard <= 0)
		{
			return;
		}
		ent->client->pers.DB_selectedCard -= 1;
		Initialize_Battle_UI (ent);
		ent->client->pers.lastDeckbuildingCmd = level.time;
	}
}

void Cmd_AddCard_f (edict_t *ent)
{
	if (ent->client->pers.DB_selectedCard == CARD_BITE)
	{
		return;
	}
	if (ent->client->pers.deck[ent->client->pers.DB_selectedCard] >= 5)
	{
		return;
	}
	if (ent->client->pers.collection[ent->client->pers.DB_selectedCard] > 0)
	{
		ent->client->pers.collection[ent->client->pers.DB_selectedCard] -= 1;
		ent->client->pers.deck[ent->client->pers.DB_selectedCard] += 1;
		Initialize_Deckbuilding_UI (ent);
	}

}


void Cmd_RemoveCard_f (edict_t *ent)
{
	if (ent->client->pers.DB_selectedCard == CARD_BITE)
	{
		return;
	}
	if (ent->client->pers.deck[ent->client->pers.DB_selectedCard] <= 0)
	{
		return;
	}

	ent->client->pers.deck[ent->client->pers.DB_selectedCard] -= 1;
	ent->client->pers.collection[ent->client->pers.DB_selectedCard] += 1;
	Initialize_Deckbuilding_UI (ent);
}

void Cmd_Deckbuilding_f (edict_t *ent)
{
	//This is not the proper logic
	//Fix me later for battle start
	if (!ent->client)
		return;
	// this is for backwards compatability
	if (deathmatch->value)
	{
		Cmd_Score_f (ent);
		return;
	}

	ent->client->showinventory = false;
	ent->client->showscores = false;

	if (ent->client->pers.inDeckbuilding)
	{
		ent->client->pers.inDeckbuilding = false;
		ent->client->showhelp = false;
		return;
	}

	ent->client->pers.inDeckbuilding = true;
	ent->client->pers.DB_selectedCard = 0;
	ent->client->showhelp = true;
	ent->client->pers.helpchanged = 0;
	Initialize_Deckbuilding_UI (ent);
}

void Cmd_Battle_f (edict_t *ent)
{
	//This is not the proper logic
	//Fix me later for battle start

	// this is for backwards compatability
	if (deathmatch->value)
	{
		Cmd_Score_f (ent);
		return;
	}

	ent->client->showinventory = false;
	ent->client->showscores = false;

	if (ent->client->pers.inBattle)
	{
		//If player is busy, do nothing
		return;
	}

	ent->client->pers.inDeckbuilding = false;
	ent->client->pers.inBattle = true;
	ent->client->showhelp = true;
	ent->client->pers.helpchanged = 0;
	start_battle (ent);
	gi.bprintf (PRINT_CHAT, "No errors so far.\n");
	Initialize_Battle_UI (ent);
}


//=======================================================================

/*
===============
G_SetStats
===============
*/
void G_SetStats (edict_t *ent)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;

	//
	// health
	//
	ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
	ent->client->ps.stats[STAT_HEALTH] = ent->health;

	//
	// ammo
	//
	if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */)
	{
		ent->client->ps.stats[STAT_AMMO_ICON] = 0;
		ent->client->ps.stats[STAT_AMMO] = 0;
	}
	else
	{
		item = &itemlist[ent->client->ammo_index];
		ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex (item->icon);
		ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
	}
	
	//
	// armor
	//
	power_armor_type = PowerArmorType (ent);
	if (power_armor_type)
	{
		cells = ent->client->pers.inventory[ITEM_INDEX(FindItem ("cells"))];
		if (cells == 0)
		{	// ran out of cells for power armor
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
			power_armor_type = 0;;
		}
	}

	index = ArmorIndex (ent);
	if (power_armor_type && (!index || (level.framenum & 8) ) )
	{	// flash between power armor and other armor icon
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex ("i_powershield");
		ent->client->ps.stats[STAT_ARMOR] = cells;
	}
	else if (index)
	{
		item = GetItemByIndex (index);
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex (item->icon);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
	}
	else
	{
		ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
		ent->client->ps.stats[STAT_ARMOR] = 0;
	}

	//
	// pickup message
	//
	if (level.time > ent->client->pickup_msg_time)
	{
		ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
		ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	//
	// timers
	//
	if (ent->client->quad_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_quad");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_framenum - level.framenum)/10;
	}
	else if (ent->client->invincible_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_invulnerability");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_framenum - level.framenum)/10;
	}
	else if (ent->client->enviro_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_envirosuit");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_framenum - level.framenum)/10;
	}
	else if (ent->client->breather_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_rebreather");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_framenum - level.framenum)/10;
	}
	else
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = 0;
		ent->client->ps.stats[STAT_TIMER] = 0;
	}

	//
	// selected item
	//
	if (ent->client->pers.selected_item == -1)
		ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
	else
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex (itemlist[ent->client->pers.selected_item].icon);

	ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

	//
	// layouts
	//
	ent->client->ps.stats[STAT_LAYOUTS] = 0;

	if (deathmatch->value)
	{
		if (ent->client->pers.health <= 0 || level.intermissiontime
			|| ent->client->showscores)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}
	else
	{
		if (ent->client->showscores || ent->client->showhelp)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}

	//
	// frags
	//
	ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

	//
	// help icon / current weapon if not shown
	//
	if (ent->client->pers.helpchanged && (level.framenum&8) )
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex ("i_help");
	else if ( (ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91)
		&& ent->client->pers.weapon)
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex (ent->client->pers.weapon->icon);
	else
		ent->client->ps.stats[STAT_HELPICON] = 0;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;
}

/*
===============
G_CheckChaseStats
===============
*/
void G_CheckChaseStats (edict_t *ent)
{
	int i;
	gclient_t *cl;

	for (i = 1; i <= maxclients->value; i++) {
		cl = g_edicts[i].client;
		if (!g_edicts[i].inuse || cl->chase_target != ent)
			continue;
		memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
		G_SetSpectatorStats(g_edicts + i);
	}
}

/*
===============
G_SetSpectatorStats
===============
*/
void G_SetSpectatorStats (edict_t *ent)
{
	gclient_t *cl = ent->client;

	if (!cl->chase_target)
		G_SetStats (ent);

	cl->ps.stats[STAT_SPECTATOR] = 1;

	// layouts are independant in spectator
	cl->ps.stats[STAT_LAYOUTS] = 0;
	if (cl->pers.health <= 0 || level.intermissiontime || cl->showscores)
		cl->ps.stats[STAT_LAYOUTS] |= 1;
	if (cl->showinventory && cl->pers.health > 0)
		cl->ps.stats[STAT_LAYOUTS] |= 2;

	if (cl->chase_target && cl->chase_target->inuse)
		cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS + 
			(cl->chase_target - g_edicts) - 1;
	else
		cl->ps.stats[STAT_CHASE] = 0;
}

