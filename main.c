// 3DSage GBA template

//---#defines---
#include "gba.h"
//---Math functions---
#include <math.h> 
#include <stdlib.h>

//---Global variables---
#define  GBA_SW 160 // actual gba screen width
#define  SW     120 // game screen width
#define  SH      80 // game screen height
#define RGB(r,g,b) ((r)+((g)<<5)+((b)<<10)) // 15 bit, 0-31, 5bit=r, 5bit=g, 5bit=b

int lastFr = 0, FPS = 0; // for frames per second

int8_t terrainSeed[TERRAIN_SIZE];
uint8_t biomeSeed[BIOME_SIZE];
__attribute__((section (".sbss"))) uint8_t oreSeed[ORE_SIZE][33];
// the above creates an array of 0s and stores it in ewram instead of iwram

uint8_t currentChunk[30][40];
int32_t curChunkOffset = 0;
uint8_t curChanges = 0;
uint8_t nextChunk[30][40];
int32_t nextChunkOffset = -30;
uint8_t nextChanges = 0;

uint16_t blockGraphic[16];

uint32_t saveMemOffset = 0;
uint16_t GDS = 72+240; // General Data Size (in bytes(also edited in the init function))
uint8_t resetCounter = 0; // amount of frames until world is reset if buttons pressed
// player variables
int32_t viewX = 0; // i had these as unsigned for the longest tiiiiimmmmmeeee
int32_t viewY = -4; // it broke everythiiiinnnnggggggg... knew it was something dumb it always is

uint8_t keyRReleased = 1;
uint8_t keyLReleased = 1;
uint8_t keyUReleased = 1;
uint8_t keyDReleased = 1;
uint8_t keySTReleased = 1;
uint8_t keySLReleased = 1;

uint8_t aKey = 0;
uint8_t bKey = 0;
uint8_t rKey = 0;
uint8_t lKey = 0;
uint8_t uKey = 0;
uint8_t dKey = 0;
uint8_t slKey = 0;
uint8_t stKey = 0;
uint8_t lsKey = 0;
uint8_t rsKey = 0;

uint8_t keyLSPressed = 0;
uint8_t keyRSPressed = 0;

int8_t lastMovement = 0;
uint8_t gravityDelay = 0;
int16_t hForce = 0;
int16_t vForce = 0;

int8_t viewXChange = 0;
int8_t viewYChange = 0;

uint8_t playerMode = 0;
uint8_t inventory[15][2];
uint8_t invenSel = 0;
int8_t playerHP = 20;

int32_t telerocks[30][2];


struct Entity {
	uint8_t id;
	uint16_t color;
	int16_t hp;
	int32_t x;
	int32_t y;
	int8_t hF;
	int8_t vF;
};
struct Entity entities[6];
uint8_t numEntities = sizeof(entities)/sizeof(struct Entity);


void savePlayer()
{
	// DONT EXCEED GDS
	// seems to get angry if not done in 4 byte increments
	// save viewX
	saveMemory[5] = viewX;
	saveMemory[6] = viewX>>8;
	saveMemory[7] = viewX>>16;
	saveMemory[8] = viewX>>24;
	// save viewY
	saveMemory[9] = viewY;
	saveMemory[10] = viewY>>8;
	saveMemory[11] = viewY>>16;
	saveMemory[12] = viewY>>24;
}
void saveHealth() // save player health
{
	uint8_t invenSize = sizeof(inventory)/sizeof(inventory[0]);
	saveMemory[32+numEntities*13+invenSize*2] = playerHP;
	saveMemory[33+numEntities*13+invenSize*2] = playerHP>>8;
}

void saveEntities()
{
	// save entity data
	for(uint8_t e = 0; e < numEntities; e++)
	{
		saveMemory[17+e*13] = entities[e].id;
		saveMemory[18+e*13] = entities[e].color;
		saveMemory[19+e*13] = entities[e].color>>8;
		saveMemory[20+e*13] = entities[e].hp;
		saveMemory[21+e*13] = entities[e].hp>>8;
		saveMemory[22+e*13] = entities[e].x;
		saveMemory[23+e*13] = entities[e].x>>8;
		saveMemory[24+e*13] = entities[e].x>>16;
		saveMemory[25+e*13] = entities[e].x>>24;
		saveMemory[26+e*13] = entities[e].y;
		saveMemory[27+e*13] = entities[e].y>>8;
		saveMemory[28+e*13] = entities[e].y>>16;
		saveMemory[29+e*13] = entities[e].y>>24;
	}
}


void saveChunk(uint8_t chunk[30][40], int32_t chunkOffset)
{
	// check if chunk has already been saved previously
	uint16_t preSaveLocation = 0xFFFF;
	for(uint8_t c = 0; c < (31999-GDS)/CHUNK_SIZE; c++) // game pak sram is 64000B
	{
		// retrieve chunk location
		int32_t chOff = saveMemory[GDS+c*CHUNK_SIZE] + (saveMemory[GDS+c*CHUNK_SIZE+1]<<8) + (saveMemory[GDS+c*CHUNK_SIZE+2]<<16) + (saveMemory[GDS+c*CHUNK_SIZE+3]<<24);
		if(chOff == chunkOffset) // if this is the chunk we want to save
		{
			preSaveLocation = GDS+c*CHUNK_SIZE;
			break;
		}
	}
	// create new chunk save
	if(preSaveLocation == 0xFFFF)
	{
		// persistent memory works a byte at a time >:I
		saveMemory[GDS+saveMemOffset] = chunkOffset;
		saveMemory[GDS+saveMemOffset+1] = chunkOffset>>8;
		saveMemory[GDS+saveMemOffset+2] = chunkOffset>>16;
		saveMemory[GDS+saveMemOffset+3] = chunkOffset>>24;
		uint16_t index = 0;
		for(int x = 0; x < 30; x++)
		{
			for(int y = 0; y < 40; y++)
			{
				saveMemory[GDS+4+saveMemOffset+index] = chunk[x][y];
				index++;
			}
		}
		saveMemOffset += CHUNK_SIZE;
		// check if game pak sram memory exceeded
		if(saveMemOffset > 31999 - GDS - CHUNK_SIZE)
		{
			playNote(0xF4DF, 0x0C); // debug sound
			saveMemOffset = 0; // reset pointer
			// now the next new chunk saved will overwrite the first chunk saved
		}
		saveMemory[1] = saveMemOffset;
		saveMemory[2] = saveMemOffset>>8;
		saveMemory[3] = saveMemOffset>>16;
		saveMemory[4] = saveMemOffset>>24;
	}
	else // replace chunk data in previously created chunk save
	{
		uint16_t index = 0;
		for(int x = 0; x < 30; x++)
		{
			for(int y = 0; y < 40; y++)
			{
				saveMemory[4+preSaveLocation+index] = chunk[x][y];
				index++;
			}
		}
	}
}
// save player inventory
void saveInven()
{
	for(uint8_t i = 0; i < sizeof(inventory)/sizeof(inventory[0]); i++)
	{
		saveMemory[30+numEntities*13+i*2] = inventory[i][0];
		saveMemory[31+numEntities*13+i*2] = inventory[i][1];
	}
}
// save telerock locations
void saveTelerock()
{
	uint8_t invenSize = sizeof(inventory)/sizeof(inventory[0]);
	for(uint8_t i = 0; i < 30; i++)
	{
		saveMemory[34+numEntities*13+invenSize*2+i*8] = telerocks[i][0];
		saveMemory[35+numEntities*13+invenSize*2+i*8] = telerocks[i][0]>>8;
		saveMemory[36+numEntities*13+invenSize*2+i*8] = telerocks[i][0]>>16;
		saveMemory[37+numEntities*13+invenSize*2+i*8] = telerocks[i][0]>>24;
		saveMemory[38+numEntities*13+invenSize*2+i*8] = telerocks[i][1];
		saveMemory[39+numEntities*13+invenSize*2+i*8] = telerocks[i][1]>>8;
		saveMemory[40+numEntities*13+invenSize*2+i*8] = telerocks[i][1]>>16;
		saveMemory[41+numEntities*13+invenSize*2+i*8] = telerocks[i][1]>>24;
	}
}


inline int32_t arrayLoop(int32_t index, uint16_t size)
{
	while(index < 0)
	{
		index += size*3;
	}
	return index;
}

void generateChunk(uint8_t chunk[30][40], int32_t chunkOffset)
{
	// generate chunk
	int32_t skyArea = 2;
	int32_t biome, index, terIndex;
	// select biome
	index = arrayLoop(chunkOffset/30, BIOME_SIZE);
	biome = biomeSeed[(index % BIOME_SIZE) / BIOME_CHUNKS];
	for(int x = 0; x < 30; x++)
	{
		// allow infinite negative array wrap around
		terIndex = arrayLoop(chunkOffset+x, TERRAIN_SIZE);
		terIndex %= TERRAIN_SIZE;
		// alter terrain based on biome
		switch(biome)
		{
			case 0:
				// default terrain
				skyArea = 7 + terrainSeed[terIndex] * 10 / 16;
				break;
			case 1:
				// flatter terrain
				skyArea = 7 + terrainSeed[terIndex] * 10 / 18;
				break;
			case 2:
				// more jagged terrain
				skyArea = 7 + terrainSeed[terIndex];
				break;
			case 3:
				// more jagged terrain
				skyArea = 7 + terrainSeed[terIndex];
				break;
		}
		for(int y = 0; y < 40; y++)
		{
			if(y < skyArea)
			{
				// sky
				chunk[x][y] = 4;
			}
			else if(y == skyArea)
			{
				// surface
				switch(biome)
				{
					case 0:
						chunk[x][y] = 2;
						break;
					case 1:
						chunk[x][y] = 7; // bouncy land
						break;
					case 2:
						chunk[x][y] = 2;
						break;
					case 3:
						chunk[x][y] = 13; // volcano land
						break;
				}
				if(y >= 11 && chunk[x][y] == 13)
				{
					chunk[x][y] = 14;
				}
			}
			else if(y == 39)
			{
				// bedrock
				chunk[x][y] = 3;
			}
			else if(y > skyArea + 3)
			{
				index = arrayLoop(chunkOffset+x, ORE_SIZE);
				// adjacent empty spaces increase the chance of empty spaces
				uint8_t caveChance = 0;
				if(chunk[x-1][y] == 10)
				{
					caveChance += 64;
				}
				if(chunk[x][y-1] == 10)
				{
					caveChance += 60;
				}
				if(oreSeed[index % ORE_SIZE][y-7] > 0 + caveChance)
				{
					// stone
					chunk[x][y] = 1;
					if(y > 10)
					{	// y-7 because oreSeed y range is 7 less then chunk y range
						uint8_t oreChance = 0;
						if(chunk[x-1][y] == 7)
						{
							oreChance += 18;
						}
						if(chunk[x][y-1] == 7)
						{
							oreChance += 18;
						}
						if(oreSeed[index % ORE_SIZE][y-7] + oreChance > 124)
						{
						// push
						chunk[x][y] = 7;
						}
					}
					if(y > 28)
					{
						uint8_t oreChance = 0;
						if(chunk[x-1][y] == 9)
						{
							oreChance += 16;
						}
						if(chunk[x][y-1] == 9)
						{
							oreChance += 16;
						}
						if(oreSeed[index % ORE_SIZE][y-7] + oreChance > 125)
						{
						// gold
						chunk[x][y] = 8;
						}
					}
					if(y > 35)
					{
						uint8_t oreChance = 0;
						if(chunk[x-1][y] == 9)
						{
							oreChance += 14;
						}
						if(chunk[x][y-1] == 9)
						{
							oreChance += 14;
						}
						if(oreSeed[index % ORE_SIZE][y-7] + oreChance > 126)
						{
						// telerock
						chunk[x][y] = 9;
						}
					}
				}
				else
				{
					// cave
					chunk[x][y] = 10;
				}
			}
			else
			{
				if(biome == 3)
				{
				// magma
				chunk[x][y] = 14;
				}
				else
				{
				// dirt
				chunk[x][y] = 0;
				}
			}
		}
	}
}

uint8_t loadChunk(uint8_t chunk[30][40], int32_t chunkOffset)
{
	for(uint8_t c = 0; c < (31999-GDS)/CHUNK_SIZE; c++)
	{
		// retrieve chunk location
		int32_t chOff = saveMemory[GDS+c*CHUNK_SIZE] + (saveMemory[GDS+c*CHUNK_SIZE+1]<<8) + (saveMemory[GDS+c*CHUNK_SIZE+2]<<16) + (saveMemory[GDS+c*CHUNK_SIZE+3]<<24);
		if(chOff == chunkOffset) // if this is the chunk we want to load
		{
			// load in chunk data
			uint16_t index = 0;
			for(int x = 0; x < 30; x++)
			{
				for(int y = 0; y < 40; y++)
				{
					chunk[x][y] = saveMemory[GDS+4+c*CHUNK_SIZE+index];
					index++;
				}
			}
			return 0;
		}
	}
	return 1;
}


void drawTerrain()
{
	int relCurOffset = viewX - curChunkOffset;
	int relNextOffset = viewX - nextChunkOffset;
	uint16_t g, blk;
	// variables to preload biome grass color
	// doing it in the loop was very slow
	uint16_t gPlusCur = 0;
	uint16_t gPlusNext = 0;
	// get biome grass color offset for both chunks
	uint8_t index = arrayLoop(curChunkOffset/30, BIOME_SIZE);
	switch(biomeSeed[(index % BIOME_SIZE) / BIOME_CHUNKS])
	{
		case 1:
			gPlusCur = RGB(7,0,0);
			break;
		case 2:
			gPlusCur = RGB(0,0,6);
			break;
	}
	index = arrayLoop(nextChunkOffset/30, BIOME_SIZE);
	switch(biomeSeed[(index % BIOME_SIZE) / BIOME_CHUNKS])
	{
		case 1:
			gPlusNext = RGB(7,0,0);
			break;
		case 2:
			gPlusNext = RGB(0,0,6);
			break;
	}
	// draw the blocks
	for(int x = 0; x < SW; x++)
	{
		// originally i wanted to make it so the blocks had texture but didnt
		// want to overwhelm myself and it might end up being too slow
		int relX = x/8;
		for(int y = 8; y < SH; y++) // start at 8 because HUD covers it anyway
		{
			if(relX == 7 && (y/8 == 5 || y/8 == 4))
			{
				VRAM[y*GBA_SW+x] = blockGraphic[6]; // draw player
			}
			else if(y/8+viewY >= 0 && y/8+viewY < 40)
			{
				if(relX+relCurOffset >= 0 && relX+relCurOffset < 30)
				{
					blk = currentChunk[relX+relCurOffset][y/8+viewY];
					g = blockGraphic[blk];
					if(blk == 2)
					{
						g += gPlusCur;
					}
					VRAM[y*GBA_SW+x] = g;
				}
				else if(relX+relNextOffset >= 0 && relX+relNextOffset < 30)
				{
					blk = nextChunk[relX+relNextOffset][y/8+viewY];
					g = blockGraphic[blk];
					if(blk == 2)
					{
						g += gPlusNext;
					}
					VRAM[y*GBA_SW+x] = g;
				}
				else
				{
					VRAM[y*GBA_SW+x] = blockGraphic[5]; // draw void
				}
			}
			else
			{
				VRAM[y*GBA_SW+x] = blockGraphic[5]; // draw void
			}
		}
	}
}
void drawHUD()
{
	// draw inventory
	uint8_t invenSize = sizeof(inventory)/sizeof(inventory[0]);
	for(uint8_t i = 0; i < invenSize*8; i++)
	{
		for(uint8_t y = 0; y < 8; y++)
		{
			if(y == 0 || y == 7 || i%8 == 0 || i == 0 || (i+1)%8 == 0)
			{
				if(i/8 == invenSel)
				{
					// highlight selected inventory slot
					VRAM[y*GBA_SW+i] = RGB(29,29,29);
				}
				else
				{
					// inventory slot borders
					VRAM[y*GBA_SW+i] = RGB(10,10,10);
				}
			}
			else if(inventory[i/8][0] != 255)
			{
				// contents of inventory slot
				VRAM[y*GBA_SW+i] = blockGraphic[inventory[i/8][0]];
			}
			else
			{
				// empty inventory slot color
				VRAM[y*GBA_SW+i] = RGB(22,22,22);
			}
		}
	}
	// draw health bar
	for(uint8_t i = 0; i < playerHP*SW/20; i++)
	{
		for(uint8_t y = 8; y < 11; y++)
		{
			VRAM[y*GBA_SW+i] = RGB(28,0,0);
		}
	}
}


void teleport(int32_t x, int32_t y)
{
	viewX = x;
	viewY = y;
	savePlayer();
	// generate spawn chunks
	curChunkOffset = x/30*30; // round down to nearest 30
	if(loadChunk(currentChunk, curChunkOffset))
	{
		generateChunk(currentChunk, curChunkOffset);
		curChanges = 0;
	}
	else
	{
		curChanges = CHANGE_CAP;
	}
	if((x+7)%30 < 15) // check which side of the current chunk the player is
	{
		x -= 30;
	}
	else
	{
		x += 30;
	}
	nextChunkOffset = x/30*30;
	if(loadChunk(nextChunk, nextChunkOffset))
	{
		generateChunk(nextChunk, nextChunkOffset);
		nextChanges = 0;
	}
	else
	{
		nextChanges = CHANGE_CAP;
	}
}

// player die
void playerDie()
{
	// DIE
	playerHP = 20;
	hForce = 0;
	vForce = 0;
	// clear inventory
	for(uint8_t i = 0; i < sizeof(inventory)/sizeof(inventory[0]); i++)
	{
		inventory[i][0] = 255;
		inventory[i][1] = 0;
	}
	saveInven();
	// reset player position
	teleport(0, -1);
}

// handle entities
void updateEntities()
{
	for(uint8_t e = 0; e < numEntities; e++)
	{
		// skip deactivated entites
		if(entities[e].id == 0)
		{
			continue;
		}
		// despawn entities
		// if the entity is not in a rendered chunk it is deactivated
		if(nextChunkOffset < curChunkOffset)
		{
			if(entities[e].x < nextChunkOffset || entities[e].x > curChunkOffset+30)
			{
				entities[e].id = 0;
				continue;
			}
		}
		else
		{
			if(entities[e].x < curChunkOffset || entities[e].x > nextChunkOffset+30)
			{
				entities[e].id = 0;
				continue;
			}
		}
		int32_t chunkOffset = 0;
		uint8_t(* chunk)[40];
		if(entities[e].x >= curChunkOffset && entities[e].x < curChunkOffset+30)
		{
			chunk = currentChunk;
			chunkOffset = curChunkOffset;
		}
		else
		{
			chunk = nextChunk;
			chunkOffset = nextChunkOffset;
		}
		// get enemy location relative to chunk
		int32_t enOffsetX = (entities[e].x - chunkOffset);
		int32_t enOffsetY = entities[e].y;
		// update entities
		if(entities[e].id == 1) // mine creatures... sleepers?
		{
			if(entities[e].hF == 0 && entities[e].vF == 0 && abs(viewX + 7 - entities[e].x) < 2) // check distance to player
			{
				// check what chunk the enemy is in
				int32_t chunkOffset = 0;
				uint8_t(* chunk)[40];
				if(entities[e].x >= curChunkOffset && entities[e].x < curChunkOffset+30)
				{
					chunk = currentChunk;
					chunkOffset = curChunkOffset;
				}
				else
				{
					chunk = nextChunk;
					chunkOffset = nextChunkOffset;
				}
				// get enemy location relative to chunk
				int32_t enOffsetX = (entities[e].x - chunkOffset);
				int32_t enOffsetY = entities[e].y;
				if(abs(viewY + 5 - entities[e].y) < 3) // explode
				{
					// deactivate enemy
					entities[e].id = 0;
					saveEntities();
					// set surrounding blocks to sky/wall
					uint8_t radius = 2;
					for(uint8_t bx = 0; bx < radius*2+1; bx++)
					{	// make sure only the chunk memory is altered
						if(enOffsetX - radius + bx < 0 || enOffsetX - radius + bx > 29) { continue; }
						for(uint8_t by = 0; by < radius*2+1; by++)
						{
							if(enOffsetY - radius + by < 0 || enOffsetY - radius + by > 39) { continue; }
							// properly remove active telerock
							if(chunk[enOffsetX - radius + bx][enOffsetY - radius + by] == 12)
							{
								int32_t realX = enOffsetX - radius + bx + chunkOffset;
								for(uint8_t t = 0; t < sizeof(telerocks)/sizeof(telerocks[0]); t++)
								{
									if(realX == telerocks[t][0] && enOffsetY - radius + by == telerocks[t][1])
									{
										telerocks[t][1] = -1;
										playNote(0xF6, 0x5);
										break;
									}
								}
								saveTelerock();
							}
							if(enOffsetY - radius + by > UNDERGROUND)
							{
								chunk[enOffsetX - radius + bx][enOffsetY - radius + by] = 10;
							}
							else
							{
								chunk[enOffsetX - radius + bx][enOffsetY - radius + by] = 4;
							}
						}
					}
					playNote(0xF00F, 0x20);
					playerHP -= (3 - abs(viewX + 7 - entities[e].x)) * 7;
					if(playerHP <= 0)
					{
						playerDie();
					}
					saveHealth();
				}
				else if(entities[e].hF == 0 && entities[e].vF == 0 && viewY + 5 - entities[e].y < 0 && viewY + 5 - entities[e].y > -13)
				{
					// bounce
					if(chunk[enOffsetX][enOffsetY+1] == 7)
					{
						entities[e].vF -= 8;
					}
					if(chunk[enOffsetX][enOffsetY-1] == 7)
					{
						entities[e].vF += 8;
					}
					if(chunk[enOffsetX+1][enOffsetY] == 7)
					{
						entities[e].hF -= 8;
					}
					if(chunk[enOffsetX-1][enOffsetY] == 7)
					{
						entities[e].hF += 8;
					}
					entities[e].y -= 1;
				}
			}
		}
		else if(entities[e].id == 2)
		{
			// gravity
			if((entities[e].hF == 0 && entities[e].vF == 0)
				&& (chunk[enOffsetX][enOffsetY+1] == 4 || chunk[enOffsetX][enOffsetY+1] == 10))
			{
				entities[e].y++;
				saveEntities();
			}
			if(rand() % 12 == 0) // 10 percent chance to do something
			{
				if(entities[e].hF == 0 && entities[e].vF == 0)
				{
					if(viewX+7 - entities[e].x < -1 || viewX+7 - entities[e].x > 1) // enemy move
					{
						// get direction of player in the x axis
						int8_t dirMod = (viewX+7 -  entities[e].x)/abs(viewX+7 -  entities[e].x);
						// check what chunk the enemy would move into
						int32_t chO = 0;
						uint8_t(* ch)[40];
						if(entities[e].x+dirMod >= curChunkOffset && entities[e].x+dirMod < curChunkOffset+30)
						{
							ch = currentChunk;
							chO = curChunkOffset;
						}
						else
						{
							ch = nextChunk;
							chO = nextChunkOffset;
						}
						// get enemy location relative to chunk
						int32_t enX = (entities[e].x - chO);
						int32_t enY = entities[e].y;
						if(ch[enX+dirMod][enY] == 4 || ch[enX+dirMod][enY] == 10)
						{
							entities[e].x+=dirMod;
							saveEntities();
						}
						else if((ch[enX+dirMod][enY-1] == 4 || ch[enX+dirMod][enY-1] == 10)
							&& (chunk[enOffsetX][enOffsetY-1] == 4 || chunk[enOffsetX][enOffsetY-1] == 10)
							&& (chunk[enOffsetX][enOffsetY+1] != 4 && chunk[enOffsetX][enOffsetY+1] != 10))
						{
							entities[e].y--;
							entities[e].x+=dirMod;
							saveEntities();
						}
					}
					else if(entities[e].y == viewY+5 || entities[e].y == viewY+4) // enemy in attack range of player
					{
						playerHP --;
						if(playerHP <= 0)
						{
							playerDie();
						}
						saveHealth();
						playNote(0xF10F, 0x0F);
					}
					else if((chunk[enOffsetX][enOffsetY-1] == 4 || chunk[enOffsetX][enOffsetY-1] == 10)
						&& (chunk[enOffsetX][enOffsetY+1] != 4 && chunk[enOffsetX][enOffsetY+1] != 10))
					{
						entities[e].y--;
						saveEntities();
						if(entities[e].y == viewY+5 || entities[e].y == viewY+4) // enemy in attack range of player
						{
							playerHP --;
							if(playerHP <= 0)
							{
								playerDie();
							}
							saveHealth();
							playNote(0xF10F, 0x0F);
						}
					}
				}
				// burn
				if(chunk[enOffsetX][enOffsetY+1] == 14)
				{
					entities[e].hp--;
					if(entities[e].hp <= 0)
					{
						entities[e].id = 0;
					}
					saveEntities();
				}
				// bounce
				if(chunk[enOffsetX][enOffsetY+1] == 7)
				{
					entities[e].vF -= 8;
					playNote(0xF3FF, 0x0D);
				}
				if(chunk[enOffsetX][enOffsetY-1] == 7)
				{
					entities[e].vF += 8;
					playNote(0xF3FF, 0x0D);
				}
				if(chunk[enOffsetX+1][enOffsetY] == 7)
				{
					entities[e].hF -= 8;
					playNote(0xF3FF, 0x0D);
				}
				if(chunk[enOffsetX-1][enOffsetY] == 7)
				{
					entities[e].hF += 8;
					playNote(0xF3FF, 0x0D);
				}
			}
		}
		if(entities[e].vF != 0)
		{
			// push vertically
			int8_t vPush = 1*entities[e].vF/abs(entities[e].vF);
			// makes sure you don't go through blocks or outside the world
			if(entities[e].vF > 0)
			{
				if(chunk[enOffsetX][enOffsetY+1] == 4 || chunk[enOffsetX][enOffsetY+1] == 10)
				{
					goto VerticalPushE;
				}
			}
			else
			{
				if(chunk[enOffsetX][enOffsetY-1] == 4 || chunk[enOffsetX][enOffsetY-1] == 10)
				{
					goto VerticalPushE;
				}
			}
			// take damage if movement blocked
			entities[e].hp -= abs(entities[e].vF/7);
			if(entities[e].hp <= 0)
			{
				entities[e].id = 0;
			}
			saveEntities();
			goto SkipVPushE;
		VerticalPushE:
			entities[e].y += vPush;
		SkipVPushE:
			entities[e].vF -= vPush;
		}
		// push horizontally
		if(entities[e].hF != 0)
		{
			int8_t hPush = 1*entities[e].hF/abs(entities[e].hF);
			entities[e].hF -= hPush;
			
			// check what chunk the enemy would move into
			int32_t chO = 0;
			uint8_t(* ch)[40];
			if(entities[e].x+hPush >= curChunkOffset && entities[e].x+hPush < curChunkOffset+30)
			{
				ch = currentChunk;
				chO = curChunkOffset;
			}
			else
			{
				ch = nextChunk;
				chO = nextChunkOffset;
			}
			// get enemy location relative to chunk
			int32_t enX = (entities[e].x - chO);
			int32_t enY = entities[e].y;
			
			if(ch[enX+hPush][enY] == 4 || ch[enX+hPush][enY] == 10)
			{
				entities[e].x += hPush;
			}
			else
			{
				entities[e].hp -= abs(entities[e].hF/7);
				if(entities[e].hp <= 0)
				{
					entities[e].id = 0;
				}
				saveEntities();
			}
		}
		// draw entities
		if(entities[e].id != 0 && entities[e].x >= viewX && entities[e].x < viewX+15
			&& entities[e].y >= viewY && entities[e].y < viewY+10)
		{
			for(uint8_t x = 0; x < 8; x++)
			{
				for(uint8_t y = 0; y < 8; y++)
				{
					int32_t enOffsetX = (entities[e].x - viewX) * 8 + x;
					int32_t enOffsetY = (entities[e].y - viewY) * 8 + y;
					VRAM[enOffsetY*GBA_SW+enOffsetX] = entities[e].color;
				}
			}
		}
	}
}

void createEntity(uint8_t id, uint16_t color, int16_t hp, int32_t x, int32_t y)
{
	for(uint8_t e = 0; e < numEntities; e++)
	{
		if(entities[e].id != 0) { continue; }
		entities[e].id = id;
		entities[e].color = color;
		entities[e].hp = hp;
		entities[e].x = x;
		entities[e].y = y;
		saveEntities();
		// get enemy location relative to chunk
		int32_t chunkOffset = 0;
		uint8_t(* chunk)[40];
		if(entities[e].x >= curChunkOffset && entities[e].x < curChunkOffset+30)
		{
			chunk = currentChunk;
			chunkOffset = curChunkOffset;
		}
		else
		{
			chunk = nextChunk;
			chunkOffset = nextChunkOffset;
		}
		if(id == 1)
		{
			int32_t enOffsetX = (entities[e].x - chunkOffset);
			int32_t enOffsetY = entities[e].y;
			// create hole
			for(int8_t y = 0; y < enOffsetY; y++)
			{
				if(enOffsetY-y > UNDERGROUND)
				{
					chunk[enOffsetX][enOffsetY-y] = 10;
				}
				else
				{
					chunk[enOffsetX][enOffsetY-y] = 4;
				}
			}
		}
		else
		{
			while(chunk[entities[e].x - chunkOffset][entities[e].y] != 4 && chunk[entities[e].x - chunkOffset][entities[e].y] != 10
				 && chunk[entities[e].x - chunkOffset][entities[e].y] != 5)
			{
				entities[e].y--;
			}
			while(chunk[entities[e].x - chunkOffset][entities[e].y] == 5)
			{
				int8_t dist = viewX - entities[e].x;
				int8_t dirMod = (dist)/abs(dist);
				entities[e].x += dirMod;
			}
		}
		break;
	}
}
void spawnSleeper(int32_t x)
{
	uint8_t r = rand() % 4;
	if(r == 0)
	{
		createEntity(1, RGB(0,27,0), 6, x, 18);
	}
}
void spawnSlime(int32_t x, int32_t y)
{
	uint8_t r = rand() % 270;
	if(r == 0 || r == 269)
	{
		uint16_t col;
		if(r > 134)
		{
			col = RGB(28,7,7);
		}
		else
		{
			col = RGB(4,12,28);
		}
		createEntity(2, col, 10, x, y);
	}
}


uint8_t checkBreak(uint8_t block, uint8_t relX, uint8_t relY, uint8_t target, int32_t chunkOff)
{
	// resource collection
	if(block == 4) // if sky
	{
		uint8_t invenSize = sizeof(inventory)/sizeof(inventory[0]);
		uint8_t firstEmpty = 255;
		uint8_t match = 255;
		// handle telerock
		if(target == 12)
		{
			target = 9;
			int32_t realX = chunkOff + relX;
			for(uint8_t t = 0; t < sizeof(telerocks)/sizeof(telerocks[0]); t++)
			{
				if(realX == telerocks[t][0] && relY == telerocks[t][1])
				{
					telerocks[t][1] = -1;
					playNote(0xF6, 0x5);
					break;
				}
			}
			saveTelerock();
		}
		// loop through inventory slots
		for(uint8_t i = 0; i < invenSize; i++)
		{
			if(inventory[i][0] == target && inventory[i][1] < 65)
			{
				match = i;
				break;
			}
			if(firstEmpty == 255 && inventory[i][0] == 255) // if slot empty
			{
				firstEmpty = i;
			}
		}
		if(match < 255)
		{
			inventory[match][1] += 1;
		}
		else if(firstEmpty < 255)
		{
			inventory[firstEmpty][0] = target;
			inventory[firstEmpty][1] = 1;
		}
		if(relY > UNDERGROUND)
		{
			block = 10; // cave wall
		}
	}
	else
	{
		// resources get used up when placed
		inventory[invenSel][1]--;
		if(inventory[invenSel][1] <= 0)
		{
			inventory[invenSel][0] = 255;
			inventory[invenSel][1] = 0;
		}
	}
	return block;
}
// place/remove blocks
void changeBlock(int8_t xOff, int8_t yOff, uint8_t block)
{
	int32_t chunkOffset = 0;
	uint8_t(* chunk)[40];
	uint8_t* changes;
	if(viewX + 7+xOff >= curChunkOffset && viewX + 7+xOff < curChunkOffset + 30)
	{
		chunkOffset = curChunkOffset;
		chunk = currentChunk;
		changes = &curChanges;
	}
	else
	{
		chunkOffset = nextChunkOffset;
		chunk = nextChunk;
		changes = &nextChanges;
	}
	// block location in chunk
	uint8_t relX = viewX + 7+xOff - chunkOffset;
	uint8_t relY = viewY + 5+yOff;
	if(block == 9)
	{
		block = 12;
		// find inactive array position
		for(uint8_t t = 0; t < sizeof(telerocks)/sizeof(telerocks[0]); t++)
		{
			if(telerocks[t][1] == -1)
			{
				telerocks[t][1] = relY;
				telerocks[t][0] = chunkOffset + relX;
				playNote(0xF6, 0x5);
				break;
			}
		}
		saveTelerock();
	}
	block = checkBreak(block, relX, relY, chunk[relX][relY], chunkOffset);
	chunk[relX][relY] = block;
	if(block == 12 || block == 7)
	{
		*changes = CHANGE_CAP;
	}
	else
	{
		*changes += 1;
	}
	if(*changes >= CHANGE_CAP)
	{
		saveChunk(chunk, chunkOffset);
		*changes = CHANGE_CAP;
	}
	// save player inventory
	saveInven();
}


uint8_t checkEmpty(uint8_t x, uint8_t y)
{
	if(VRAM[y*8*GBA_SW+x*8] == blockGraphic[4] || VRAM[y*8*GBA_SW+x*8] == blockGraphic[10] || VRAM[y*8*GBA_SW+x*8] == blockGraphic[11])
	{
		return 1;
	}
	return 0;
}
uint8_t checkBreakable(uint8_t x, uint8_t y)
{
	if(VRAM[y*8*GBA_SW+x*8] != blockGraphic[4] && VRAM[y*8*GBA_SW+x*8] != blockGraphic[10] && VRAM[y*8*GBA_SW+x*8] != blockGraphic[11] && VRAM[y*8*GBA_SW+x*8] != blockGraphic[3] && VRAM[y*8*GBA_SW+x*8] != blockGraphic[5])
	{
		return 1;
	}
	return 0;
}
uint8_t checkSolid(uint8_t x, uint8_t y)
{
	if(VRAM[y*8*GBA_SW+x*8] != blockGraphic[4] && VRAM[y*8*GBA_SW+x*8] != blockGraphic[10] && VRAM[y*8*GBA_SW+x*8] != blockGraphic[11] && VRAM[y*8*GBA_SW+x*8] != blockGraphic[5])
	{
		return 1;
	}
	return 0;
}

void randomizeNextSeed()
{
	uint8_t invenSize = sizeof(inventory);
	uint16_t tSizeB = sizeof(telerocks);
	
	uint32_t nextSeed = rand();
	saveMemory[42+numEntities*13+invenSize+tSizeB] = nextSeed;
	saveMemory[43+numEntities*13+invenSize+tSizeB] = nextSeed>>8;
	saveMemory[44+numEntities*13+invenSize+tSizeB] = nextSeed>>16;
	saveMemory[45+numEntities*13+invenSize+tSizeB] = nextSeed>>24;
}

void buttons() // buttons to press
{
	if(aKey) // build
	{
		playerMode = 1;
	}
	else if(bKey) // break
	{
		playerMode = 2;
	}
	else // move
	{
		playerMode = 0;
	}
	if(lsKey){ keyLSPressed = 1; } else { keyLSPressed = 0; } 
	if(rsKey){ keyRSPressed = 1; } else { keyRSPressed = 0; } 
	// reset world
	if(stKey && slKey && aKey)
	{
		if(resetCounter > 44)
		{
			saveMemory[13] = 0x00;
			resetCounter = 0;
			playNote(0xF4BF, 0x0F);
		}
		else
		{
			resetCounter++;
		}
	}
	else // select from inventory
	{
		if(stKey)
		{
			if(keySTReleased)
			{
				keySTReleased = 0;
				if(invenSel > sizeof(inventory)/sizeof(inventory[0])-2)
				{
					invenSel = 0;
				}
				else
				{
					invenSel++;
				}
				randomizeNextSeed();
			}
		}
		else
		{
			keySTReleased = 1;
		}
		if(slKey)
		{
			if(keySLReleased)
			{
				keySLReleased = 0;
				if(invenSel < 1)
				{
					invenSel = sizeof(inventory)/sizeof(inventory[0])-1;
				}
				else
				{
					invenSel--;
				}
				randomizeNextSeed();
			}
		}
		else
		{
			keySLReleased = 1;
		}
	}
	if(rKey) // move right
	{
		lastMovement = 1;
		if(keyRReleased)
		{
			if(playerMode == 0)
			{
				if((VRAM[5*8*GBA_SW+8*8] == blockGraphic[4] || VRAM[5*8*GBA_SW+8*8] == blockGraphic[10])
						&& (VRAM[4*8*GBA_SW+8*8] == blockGraphic[4] || VRAM[4*8*GBA_SW+8*8] == blockGraphic[10]))
				{
					keyRReleased = 0;
					viewX += 1;
					viewXChange += 1;
					if(nextChunkOffset > curChunkOffset)
					{
						if(viewX > nextChunkOffset+13)
						{
							// copy the next chunk into the current chunk
							for(int x = 0; x < 30; x++)
							{
								for(int y = 0; y < 40; y++)
								{
									currentChunk[x][y] = nextChunk[x][y];
								}
							}
							// shift current chunk forward
							curChunkOffset += 30;
							curChanges = nextChanges;
							// generate new chunk
							nextChunkOffset += 30;
							if(loadChunk(nextChunk, nextChunkOffset))
							{
								generateChunk(nextChunk, nextChunkOffset);
								nextChanges = 0;
							}
							else
							{
								nextChanges = CHANGE_CAP;
							}
							spawnSleeper(nextChunkOffset+27);
						}
					}
					else
					{
						if(viewX > curChunkOffset+13)
						{
							// generate new chunk
							nextChunkOffset += 60;
							if(loadChunk(nextChunk, nextChunkOffset))
							{
								generateChunk(nextChunk, nextChunkOffset);
								nextChanges = 0;
							}
							else
							{
								nextChanges = CHANGE_CAP;
							}
							spawnSleeper(nextChunkOffset+27);
						}
					}
				}
			}
			else if(playerMode == 1)
			{
				lastMovement = 0;
				if(inventory[invenSel][0] != 255 && inventory[invenSel][1] > 0)
				{
					if(keyRSPressed)
					{
						if(checkEmpty(8, 6) && checkEmpty(8, 5))
						{
							keyRReleased = 0;
							changeBlock(1, 1, inventory[invenSel][0]);
						}
					}
					else if(keyLSPressed)
					{
						if(checkEmpty(8, 4))
						{
							keyRReleased = 0;
							changeBlock(1, -1, inventory[invenSel][0]);
						}
					}
					else
					{
						if(checkEmpty(8, 5))
						{
							keyRReleased = 0;
							changeBlock(1, 0, inventory[invenSel][0]);
						}
					}
				}
			}
			else if(playerMode == 2)
			{
				lastMovement = 0;
				if(keyRSPressed)
				{
					if(checkBreakable(8, 6) && checkEmpty(8, 5))
					{
						keyRReleased = 0;
						changeBlock(1, 1, 4);
					}
				}
				else if(keyLSPressed)
				{
					if(checkBreakable(8, 4))
					{
						keyRReleased = 0;
						changeBlock(1, -1, 4);
					}
				}
				else
				{
					if(checkBreakable(8, 5))
					{
						keyRReleased = 0;
						changeBlock(1, 0, 4);
					}
				}
			}
		}
	}
	else
	{
		keyRReleased = 1;
	}
	if(lKey) // move left
	{
		if(keyRReleased)
		{
			lastMovement = -1;
			if(keyLReleased)
			{
				if(playerMode == 0)
				{
					if((VRAM[5*8*GBA_SW+6*8] == blockGraphic[4] || VRAM[5*8*GBA_SW+6*8] == blockGraphic[10])
							&& (VRAM[4*8*GBA_SW+6*8] == blockGraphic[4] || VRAM[4*8*GBA_SW+6*8] == blockGraphic[10]))
					{
						keyLReleased = 0;
						viewX -= 1;
						viewXChange -= 1;
						if(nextChunkOffset < curChunkOffset)
						{
							if(viewX < nextChunkOffset+2)
							{
								// copy the next chunk into the current chunk
								for(int x = 0; x < 30; x++)
								{
									for(int y = 0; y < 40; y++)
									{
										currentChunk[x][y] = nextChunk[x][y];
									}
								}
								// shift current chunk forward
								curChunkOffset -= 30;
								curChanges = nextChanges;
								// generate new chunk
								nextChunkOffset -= 30;
								if(loadChunk(nextChunk, nextChunkOffset))
								{
									generateChunk(nextChunk, nextChunkOffset);
									nextChanges = 0;
								}
								else
								{
									nextChanges = CHANGE_CAP;
								}
								spawnSleeper(nextChunkOffset+2);
							}
						}
						else
						{
							if(viewX < curChunkOffset+2)
							{
								// generate new chunk
								nextChunkOffset -= 60;
								if(loadChunk(nextChunk, nextChunkOffset))
								{
									generateChunk(nextChunk, nextChunkOffset);
									nextChanges = 0;
								}
								else
								{
									nextChanges = CHANGE_CAP;
								}
								spawnSleeper(nextChunkOffset+2);
							}
						}
					}
				}
				else if(playerMode == 1)
				{
					lastMovement = 0;
					if(inventory[invenSel][0] != 255)
					{
						if(keyRSPressed)
						{
							if(checkEmpty(6, 6) && checkEmpty(6, 5))
							{
								keyLReleased = 0;
								changeBlock(-1, 1, inventory[invenSel][0]);
							}
						}
						else if(keyLSPressed)
						{
							if(checkEmpty(6, 4))
							{
								keyLReleased = 0;
								changeBlock(-1, -1, inventory[invenSel][0]);
							}
						}
						else
						{
							if(checkEmpty(6, 5))
							{
								keyLReleased = 0;
								changeBlock(-1, 0, inventory[invenSel][0]);
							}
						}
					}
				}
				else if(playerMode == 2)
				{
					lastMovement = 0;
					if(keyRSPressed)
					{
						if(checkBreakable(6, 6) && checkEmpty(6, 5))
						{
							keyLReleased = 0;
							changeBlock(-1, 1, 4);
						}
					}
					else if(keyLSPressed)
					{
						if(checkBreakable(6, 4))
						{
							keyLReleased = 0;
							changeBlock(-1, -1, 4);
						}
					}
					else
					{
						if(checkBreakable(6, 5))
						{
							keyLReleased = 0;
							changeBlock(-1, 0, 4);
						}
					}
				}
			}
		}
	}
	else
	{
		keyLReleased = 1;
	}
	if(uKey) // move up
	{
		if(keyRReleased && keyLReleased)
		{
			if(keyUReleased)
			{
				if(playerMode == 0)
				{
					/* if(lastMovement != 0) // diagonal step movement
					{
						if((VRAM[6*8*GBA_SW+7*8] != blockGraphic[4] && VRAM[6*8*GBA_SW+7*8] != blockGraphic[10])
								&& (VRAM[4*8*GBA_SW+(7+lastMovement)*8] == blockGraphic[4] || VRAM[4*8*GBA_SW+(7+lastMovement)*8] == blockGraphic[10])
								&& (VRAM[3*8*GBA_SW+(7+lastMovement)*8] == blockGraphic[4] || VRAM[3*8*GBA_SW+(7+lastMovement)*8] == blockGraphic[10]))
						{
							//playNote(0xFF0F, 0x09); // debug sound
							viewY -= 1;
							viewX += lastMovement;
							viewYChange -= 1;
							viewXChange += lastMovement;
							gravityDelay = 4;
							lastMovement = 0;
							keyUReleased = 0;
						}
						else
						{
							goto Jump;
						}
					}
					else
					{
						Jump: */
						if((VRAM[6*8*GBA_SW+7*8] != blockGraphic[4] && VRAM[6*8*GBA_SW+7*8] != blockGraphic[10]) && (VRAM[3*8*GBA_SW+7*8] == blockGraphic[4] || VRAM[3*8*GBA_SW+7*8] == blockGraphic[10]))
						{
							viewY -= 1;
							viewYChange -= 1;
							gravityDelay = 6;
							keyUReleased = 0;
						}
					//}
				}
				else if(playerMode == 1)
				{
					if(inventory[invenSel][0] != 255)
					{
						if(VRAM[3*8*GBA_SW+7*8] == blockGraphic[4] || VRAM[3*8*GBA_SW+7*8] == blockGraphic[10])
						{
							keyUReleased = 0;
							changeBlock(0, -2, inventory[invenSel][0]);
						}
					}
				}
				else if(playerMode == 2)
				{
					if((VRAM[3*8*GBA_SW+7*8] != blockGraphic[4] && VRAM[3*8*GBA_SW+7*8] != blockGraphic[10]) && VRAM[3*8*GBA_SW+7*8] != blockGraphic[3] && VRAM[3*8*GBA_SW+7*8] != blockGraphic[5])
					{
						keyUReleased = 0;
						changeBlock(0, -2, 4);
					}
				}
			}
		}
	}
	else
	{
		keyUReleased = 1;
	}
	if(dKey)
	{
		if(keyRReleased && keyLReleased && keyUReleased)
		{
			if(keyDReleased)
			{
				if(playerMode == 1)
				{
					if(inventory[invenSel][0] != 255)
					{
						// nerd pole
						if((VRAM[6*8*GBA_SW+7*8] != blockGraphic[4] && VRAM[6*8*GBA_SW+7*8] != blockGraphic[10]) && (VRAM[3*8*GBA_SW+7*8] == blockGraphic[4] || VRAM[3*8*GBA_SW+7*8] == blockGraphic[10]))
						{
							keyDReleased = 0;
							gravityDelay = 1; // delay gravity so player move up properly
							viewY -= 1;
							viewYChange -= 1;
							changeBlock(0, 1, inventory[invenSel][0]);
						}
					}
				}
				else if(playerMode == 2)
				{
					if((VRAM[6*8*GBA_SW+7*8] != blockGraphic[4] && VRAM[6*8*GBA_SW+7*8] != blockGraphic[10]) && VRAM[6*8*GBA_SW+7*8] != blockGraphic[3] && VRAM[6*8*GBA_SW+7*8] != blockGraphic[5])
					{
						keyDReleased = 0;
						changeBlock(0, 1, 4);
					}
				}
			}
		}
	}
	else
	{
		keyDReleased = 1;
	}
}

void init()
{
	// allocate memory for entities
	GDS += 13*numEntities;
	// generate/load seed
	uint32_t seed = 0;
	uint8_t invenSize = sizeof(inventory)/sizeof(inventory[0]);
	uint16_t tSizeB = sizeof(telerocks);
	if(saveMemory[13] + (saveMemory[14]<<8) + (saveMemory[15]<<16) + (saveMemory[16]<<24) != 0xF0F0F0F0)
	{
		seed = saveMemory[42+numEntities*13+invenSize*2+tSizeB] + (saveMemory[43+numEntities*13+invenSize*2+tSizeB]<<8) + (saveMemory[44+numEntities*13+invenSize*2+tSizeB]<<16) + (saveMemory[45+numEntities*13+invenSize*2+tSizeB]<<24) + 1;
		
		saveMemory[38+numEntities*13+invenSize*2+tSizeB] = seed;
		saveMemory[39+numEntities*13+invenSize*2+tSizeB] = seed>>8;
		saveMemory[40+numEntities*13+invenSize*2+tSizeB] = seed>>16;
		saveMemory[41+numEntities*13+invenSize*2+tSizeB] = seed>>24;
	}
	else
	{
		seed = saveMemory[38+numEntities*13+invenSize*2+tSizeB] + (saveMemory[39+numEntities*13+invenSize*2+tSizeB]<<8) + (saveMemory[40+numEntities*13+invenSize*2+tSizeB]<<16) + (saveMemory[41+numEntities*13+invenSize*2+tSizeB]<<24);
	}
	// initialize randomization
	srand(seed);
	// create terrain seed
	int top = 0;
	for(int i = 0; i < TERRAIN_SIZE; i++)
	{
		// make terrain loop more seamless
		if(i > TERRAIN_SIZE - 2)
		{
			if(top > 1)
			{
				top -= 1;
			}
			else if(top < -1)
			{
				top += 1;
			}
		}
		else if(i == 0)
		{
			top = 0;
		}
		else
		{
			top += rand() % 5 - 2;
			// limit terrain height
			if(top < -5)
			{
				top = -5;
			}
			else if(top > 5)
			{
				top = 5;
			}
		}
		terrainSeed[i] = top;
	}
	// create biome seed
	for(int i = 0; i < BIOME_SIZE; i++)
	{
		biomeSeed[i] = rand() % 4;
		if(i == 0 && biomeSeed[i] == 1) { biomeSeed[i] = 0; }
	}
	// create ore/cave/underground seed
	for(int i = 0; i < ORE_SIZE; i++)
	{
		for(int z = 0; z < 33; z++)
		{
			oreSeed[i][z] = rand() % 128;
		}
	}
	// initialize variables if garbaj
	if(saveMemory[13] + (saveMemory[14]<<8) + (saveMemory[15]<<16) + (saveMemory[16]<<24) != 0xF0F0F0F0)
	{
		saveMemory[13] = 0xF0;
		saveMemory[14] = 0xF0;
		saveMemory[15] = 0xF0;
		saveMemory[16] = 0xF0;
		// initialize all entities to deactivated
		for(uint8_t e = 0; e < numEntities; e++)
		{
			entities[e].id = 0;
		}
		saveEntities();
		// clear inventory
		for(uint8_t i = 0; i < invenSize; i++)
		{
			inventory[i][0] = 255;
			inventory[i][1] = 0;
		}
		// debug inventory stuff
		//inventory[0][0] = 0;
		//inventory[0][1] = 255;
		//inventory[1][0] = 7;
		//inventory[1][1] = 255;
		// save player health
		saveHealth();
		// initialize telerocks
		for(uint8_t t = 0; t < sizeof(telerocks)/sizeof(telerocks[0]); t++)
		{
			telerocks[t][1] = -1;
		}
	}
	else
	{
		//load starting variables
		saveMemOffset = saveMemory[1] + (saveMemory[2]<<8) + (saveMemory[3]<<16) + (saveMemory[4]<<24);
		viewX = saveMemory[5] + (saveMemory[6]<<8) + (saveMemory[7]<<16) + (saveMemory[8]<<24);
		viewY = saveMemory[9] + (saveMemory[10]<<8) + (saveMemory[11]<<16) + (saveMemory[12]<<24);
		// get starting chunks
		curChunkOffset = viewX/30*30;
		if(viewX - curChunkOffset > 15)
		{
			nextChunkOffset = curChunkOffset+30;
		}
		else
		{
			nextChunkOffset = curChunkOffset-30;
		}
		// load enemy data
		for(uint8_t e = 0; e < numEntities; e++)
		{
			entities[e].id = saveMemory[17+e*13];
			entities[e].color = saveMemory[18+e*13] + (saveMemory[19+e*13]<<8);
			entities[e].hp = saveMemory[20+e*13] + (saveMemory[21+e*13]<<8);
			entities[e].x = saveMemory[22+e*13] + (saveMemory[23+e*13]<<8) + (saveMemory[24+e*13]<<16) + (saveMemory[25+e*13]<<24);
			entities[e].y = saveMemory[26+e*13] + (saveMemory[27+e*13]<<8) + (saveMemory[28+e*13]<<16) + (saveMemory[29+e*13]<<24);
		}
		// load inventory
		for(uint8_t i = 0; i < invenSize; i++)
		{
			inventory[i][0] = saveMemory[30+numEntities*13+i*2];
			inventory[i][1] = saveMemory[31+numEntities*13+i*2];
		}
		// load player health
		playerHP = saveMemory[32+numEntities*13+invenSize*2] + (saveMemory[33+numEntities*13+invenSize*2]<<8);
		// load telerocks
		uint8_t invenSize = sizeof(inventory)/sizeof(inventory[0]);
		for(uint8_t i = 0; i < 30; i++)
		{
			telerocks[i][0] = saveMemory[34+numEntities*13+invenSize*2+i*8] + (saveMemory[35+numEntities*13+invenSize*2+i*8]<<8) + (saveMemory[36+numEntities*13+invenSize*2+i*8]<<16) + (saveMemory[37+numEntities*13+invenSize*2+i*8]<<24);
			telerocks[i][1] = saveMemory[38+numEntities*13+invenSize*2+i*8] + (saveMemory[39+numEntities*13+invenSize*2+i*8]<<8) + (saveMemory[40+numEntities*13+invenSize*2+i*8]<<16) + (saveMemory[41+numEntities*13+invenSize*2+i*8]<<24);
		}
	}
	// check to see if spawn was already generated
	uint8_t curSaved = 0;
	uint8_t nextSaved = 0;
	for(uint8_t c = 0; c < 26; c++)
	{
		// retrieve chunk location
		int32_t chOff = saveMemory[GDS+c*CHUNK_SIZE] + (saveMemory[GDS+c*CHUNK_SIZE+1]<<8) + (saveMemory[GDS+c*CHUNK_SIZE+2]<<16) + (saveMemory[GDS+c*CHUNK_SIZE+3]<<24);
		if(chOff == curChunkOffset) // if this is the chunk we want to load
		{
			curSaved = 1;
		}
		if(chOff == nextChunkOffset)
		{
			nextSaved = 1;
		}
	}
	// initialize block graphics
	blockGraphic[0] = RGB(22,14,8); // dirt
	blockGraphic[1] = RGB(16,16,16); // stone
	blockGraphic[2] = RGB(8,22,8); // grass
	blockGraphic[3] = RGB(4,4,4); // bedrock
	blockGraphic[4] = RGB(5,20,25); // sky
	blockGraphic[5] = RGB(12,20,23); // void
	blockGraphic[6] = RGB(0,0,26); // player
	blockGraphic[7] = RGB(0,15,22); // push
	blockGraphic[8] = RGB(25,24,0); // heal
	blockGraphic[9] = RGB(8,0,13); // telerock
	blockGraphic[10] = RGB(12,12,12); // cave wall
	blockGraphic[11] = RGB(0,23,23); // water
	blockGraphic[12] = RGB(17,0,25); // active telerock
	blockGraphic[13] = RGB(5,5,6); // basalt
	blockGraphic[14] = RGB(22,5,5); // magma
	// generate spawn
	if(curSaved)
	{
		loadChunk(currentChunk, curChunkOffset);
		curChanges = CHANGE_CAP;
	}
	else
	{
		generateChunk(currentChunk, curChunkOffset);
	}
	if(nextSaved)
	{
		loadChunk(nextChunk, nextChunkOffset);
		nextChanges = CHANGE_CAP;
	}
	else
	{
		generateChunk(nextChunk, nextChunkOffset);
	}
	for(uint8_t e = 0; e < numEntities; e++)
	{
		if(entities[e].id == 1)
		{
			int32_t chunkOffset = 0;
			uint8_t(* chunk)[40];
			if(entities[e].x >= curChunkOffset && entities[e].x < curChunkOffset+30)
			{
				chunk = currentChunk;
				chunkOffset = curChunkOffset;
			}
			else
			{
				chunk = nextChunk;
				chunkOffset = nextChunkOffset;
			}
			// get enemy location relative to chunk
			int32_t enOffsetX = (entities[e].x - chunkOffset);
			int32_t enOffsetY = entities[e].y;
			// create hole
			for(int8_t y = 0; y < enOffsetY; y++)
			{
				if(enOffsetY-y > UNDERGROUND)
				{
					chunk[enOffsetX][enOffsetY-y] = 10;
				}
				else
				{
					chunk[enOffsetX][enOffsetY-y] = 4;
				}
			}
		}
	}
}

int main()
{
	// Init mode 5------------------------------------------------------------------
	*(u16*)0x4000000 = 0x405; // mode 5 background 2
	*(u16*)0x400010A = 0x82; // enable timer for fps
	*(u16*)0x400010E = 0x84; // cnt timer overflow

	//scale small mode 5 screen to full screen-------------------------------------
	REG_BG2PA=256/2; // 256=normal 128=scale 
	REG_BG2PD=256/2; // 256=normal 128=scale

	init(); // init game variables

	while(1) 
	{
		// should make button inputs more responsive
		if(KEY_A){ aKey = 1; } else { aKey = 0; }
		if(KEY_B){ bKey = 1; } else { bKey = 0; }
		if(KEY_R){ rKey = 1; } else { rKey = 0; }
		if(KEY_L){ lKey = 1; } else { lKey = 0; }
		if(KEY_U){ uKey = 1; } else { uKey = 0; }
		if(KEY_D){ dKey = 1; } else { dKey = 0; }
		if(KEY_SL){ slKey = 1; } else { slKey = 0; }
		if(KEY_ST){ stKey = 1; } else { stKey = 0; }
		if(KEY_LS){ lsKey = 1; } else { lsKey = 0; }
		if(KEY_RS){ rsKey = 1; } else { rsKey = 0; }
		
		if(REG_TM2D>>12!=lastFr) // draw 15 frames a second
		{
			drawTerrain(); // draw terrain-----------------------------------------
			updateEntities();
			drawHUD();
			buttons(); // button input
			// player gravity
			if(vForce == 0)
			{
				if(checkEmpty(7+viewXChange, 6+viewYChange))
				{
					if(gravityDelay == 0) // delay gravity after jumping
					{
						viewY += 1;
						viewYChange += 1;
					}
					else
					{
						gravityDelay -= 1;
					}
				}
			}
			else
			{
				// push vertically
				int8_t vPush = 1*vForce/abs(vForce);
				// makes sure you don't go through blocks or outside the world
				if(vForce > 0)
				{
					if(checkEmpty(7+viewXChange, 6+viewYChange))
					{
						goto VerticalPush;
					}
				}
				else
				{
					if(checkEmpty(7+viewXChange, 3+viewYChange))
					{
						goto VerticalPush;
					}
				}
				// take damage if movement blocked
				int8_t force = abs(vForce/7);
				playerHP -= force;
				if(playerHP <= 0)
				{
					playerDie();
				}
				saveHealth();
				if(force != 0) { playNote(0xF10F, 0x0F); }
				goto SkipVPush;
			VerticalPush:
				viewY += vPush;
				viewYChange += vPush;
			SkipVPush:
				vForce -= vPush;
			}
			// push horizontally
			if(hForce != 0)
			{
				int8_t hPush = 1*hForce/abs(hForce);
				hForce -= hPush;
				if(checkEmpty(7+hPush+viewXChange, 5+viewYChange)
					&& checkEmpty(7+hPush+viewXChange, 4+viewYChange))
				{
					viewX += hPush;
					viewXChange += hPush;
				}
				else
				{
					int8_t force = abs(hForce/7);
					playerHP -= force;
					if(playerHP <= 0)
					{
						playerDie();
					}
					saveHealth();
					if(force != 0) { playNote(0xF10F, 0x0F); }
				}
			}
			// save player stats if moved
			if(viewXChange != 0 || viewYChange != 0)
			{
				// teleport by jumping while standing on active telerock
				if(VRAM[((7+viewYChange)*8)*GBA_SW+((7+viewXChange)*8)] == blockGraphic[12])
				{
					int32_t teleX = viewX + 7;
					int32_t teleY = viewY + 7;
					int8_t dest = -1;
					for(uint8_t t = 0; t < sizeof(telerocks)/sizeof(telerocks[0]); t++)
					{
						// find telerock being used
						if(telerocks[t][1] != - 1 && telerocks[t][0] == teleX && telerocks[t][1] == teleY)
						{
							// find next telerock in array
							for(uint8_t r = t+1; r < sizeof(telerocks)/sizeof(telerocks[0]); r++)
							{
								if(telerocks[r][1] != - 1)
								{
									playNote(0xF20F, 0x0C);
									dest = r;
									break;
								}
							}
							if(dest == -1)
							{
								// find first telerock in array
								for(uint8_t t2 = 0; t2 < sizeof(telerocks)/sizeof(telerocks[0]); t2++)
								{
									if(telerocks[t2][1] != - 1)
									{
										playNote(0xF20F, 0x0C);
										dest = t2;
										break;
									}
								}
							}
						}
					}
					// make sure you dont get teleported outside the world
					if(dest != -1 && telerocks[dest][1] > 0 && telerocks[dest][1] < 40)
					{
						teleport(telerocks[dest][0] - 7, telerocks[dest][1] - 6);
					}
				}
				// get pushed son
				if(VRAM[((6+viewYChange)*8)*GBA_SW+((7+viewXChange)*8)] == blockGraphic[7])
				{
					vForce -= 8;
					playNote(0xF3FF, 0x0D);
				}
				if(VRAM[((3+viewYChange)*8)*GBA_SW+((7+viewXChange)*8)] == blockGraphic[7])
				{
					vForce += 8;
					playNote(0xF3FF, 0x0D);
				}
				if(VRAM[((5+viewYChange)*8)*GBA_SW+((8+viewXChange)*8)] == blockGraphic[7]
					|| VRAM[((4+viewYChange)*8)*GBA_SW+((8+viewXChange)*8)] == blockGraphic[7])
				{
					hForce -= 8;
					playNote(0xF3FF, 0x0D);
				}
				if(VRAM[((5+viewYChange)*8)*GBA_SW+((6+viewXChange)*8)] == blockGraphic[7]
					|| VRAM[((4+viewYChange)*8)*GBA_SW+((6+viewXChange)*8)] == blockGraphic[7])
				{
					hForce += 8;
					playNote(0xF3FF, 0x0D);
				}
				if(playerHP < 20)
				{
					// get healed son
					if(VRAM[((6+viewYChange)*8)*GBA_SW+((7+viewXChange)*8)] == blockGraphic[8])
					{
						playerHP = 20;
						playNote(0xF30F, 0x0F);
						changeBlock(0, 1, 4);
					}
					/*if(VRAM[((3+viewYChange)*8)*GBA_SW+((7+viewXChange)*8)] == blockGraphic[8])
					{
						playerHP = 20;			can be mined from below
						playNote(0xF3, 0x7);
						changeBlock(0, -2, 4);
					}*/
					if(VRAM[((5+viewYChange)*8)*GBA_SW+((8+viewXChange)*8)] == blockGraphic[8])
					{
						playerHP = 20;
						saveHealth();
						playNote(0xF30F, 0x0F);
						changeBlock(1, 0, 4);
					}
					if(VRAM[((4+viewYChange)*8)*GBA_SW+((8+viewXChange)*8)] == blockGraphic[8])
					{
						playerHP = 20;
						saveHealth();
						playNote(0xF30F, 0x0F);
						changeBlock(1, -1, 4);
						
					}
					if(VRAM[((5+viewYChange)*8)*GBA_SW+((6+viewXChange)*8)] == blockGraphic[8])
					{
						playerHP = 20;
						saveHealth();
						playNote(0xF30F, 0x0F);
						changeBlock(-1, 0, 4);
					}
					if(VRAM[((4+viewYChange)*8)*GBA_SW+((6+viewXChange)*8)] == blockGraphic[8])
					{
						playerHP = 20;
						saveHealth();
						playNote(0xF30F, 0x0F);
						changeBlock(-1, -1, 4);
					}
				}
				// get burned sonny
				if(VRAM[((6+viewYChange)*8)*GBA_SW+((7+viewXChange)*8)] == blockGraphic[14])
				{
					playerHP--;
					if(playerHP <= 0)
					{
						playerDie();
						saveHealth();
					}
					playNote(0xF10F, 0x0F);
				}
				savePlayer();
			}
			// reset view changes
			viewXChange = 0;
			viewYChange = 0;

			//frames per second---------------------------------------------------------- 
			FPS+=1; // increase frame
			if(lastFr>REG_TM2D>>12)
			{
				// runs once per second
				FPS=0;
				
				if(rand() % 2 == 0)
				{
					spawnSlime(viewX + 20, viewY + 4);
				}
				else
				{
					spawnSlime(viewX - 6, viewY + 4);
				}
				
				randomizeNextSeed();
			}
			lastFr=REG_TM2D>>12; // reset counter

			//swap buffers---------------------------------------------------------------
			while(*Scanline<160){} // wait all scanlines 
			if  ( DISPCNT&BACKB){ DISPCNT &= ~BACKB; VRAM=(u16*)VRAM_B;} // back  buffer
			else{                 DISPCNT |=  BACKB; VRAM=(u16*)VRAM_F;} // front buffer  
		}
	}
}

