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

void printChar(char c, uint16_t x, uint16_t y, uint16_t color = 15)
{
	uint16_t tileNo = static_cast<uint16_t>(c) - 32;
	uint32_t tileIndex = ((y & 255) * 256) + (x & 255);
	uint16_t *screen = Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_1000);
	screen[tileIndex] = tileNo | ((color & 15) << 12);
}

void printString(const char *s, uint16_t x, uint16_t y, uint16_t color = 15)
{
	if (s != nullptr)
	{
		while (*s != '\0')
		{
			printChar(*s, x++, y, color);
			s++;
		}
	}
}

int main()
{
	// Set graphics to mode 0 and enable background 2
	REG_DISPCNT = MODE_0 | BG2_ON;
	// copy data to tile map
	Memory::memcpy32(Tiles::TILE_BASE_TO_MEM(Tiles::TileBase::Base_0000), FONT_8X8_DATA, FONT_8X8_DATA_SIZE);
	// set up background 2 screen and tile map starts and sreen size to 256x256
	REG_BG2CNT = Tiles::backgroundConfig(Tiles::TileBase::Base_0000, Tiles::ScreenBase::Base_1000);
	// build palettes - CGA colors, yesss!!!
	static const color16 COLORS[16] = {0, 0x5000, 0x280, 0x5280, 0x0014, 0x5014, 0x0154, 0x5294, 0x294a, 0x7d4a, 0x2bea, 0x7fea, 0x295f, 0x7d5f, 0x2bff, 0x7fff};
	for (uint32_t i = 0; i < 16; i++)
	{
		BG_PALETTE[i * 16] = 0;
		BG_PALETTE[i * 16 + 1] = COLORS[i];
	}
	uint16_t color = 0;
	do
	{
		printString("Hello world!", 0, 0, color++);
	} while (true);
}
