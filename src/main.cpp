#include <gba_base.h>
#include <gba_video.h>
#include <gba_dma.h>
#include <gba_systemcalls.h>
#include <gba_interrupt.h>

#include <memory/memory.h>
#include <tiles.h>
#include <debug/print.h>

#include "./data/font_8x8.h"

int main() IWRAM_CODE;

void printChar(char c, uint32_t x, uint32_t y)
{
	uint16_t tileNo = static_cast<uint16_t>(c) - 32;
	uint32_t tileIndex = ((y & 255) * 256) + (x & 255);
	uint16_t *screen = Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_1000);
	screen[tileIndex] = tileNo;
}

void printString(const char *s, uint32_t x, uint32_t y)
{
	if (s != nullptr)
	{
		while (*s != '\0')
		{
			printChar(*s, x++, y);
			s++;
		}
	}
}

int main()
{
	// Set graphics to mode 0 and enable background 2
	REG_DISPCNT = MODE_0 | BG2_ON;
	// copy data to palette and tile map
	Memory::memcpy16(BG_PALETTE, FONT_8X8_PALETTE, FONT_8X8_PALETTE_SIZE);
	Memory::memcpy32(Tiles::TILE_BASE_TO_MEM(Tiles::TileBase::Base_0000), FONT_8X8_DATA, FONT_8X8_DATA_SIZE);
	// set up background 2 screen and tile map starts and sreen size to 256x256
	REG_BG2CNT = Tiles::backgroundConfig(Tiles::TileBase::Base_0000, Tiles::ScreenBase::Base_1000);
	do
	{
		printString("Hello world!", 0, 0);
	} while (true);
}
