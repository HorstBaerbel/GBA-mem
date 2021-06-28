#include <gba_base.h>
#include <gba_video.h>

#include <debug/print.h>
#include <debug/itoa.h>
#include <memory/memory.h>
#include <sys/interrupts.h>
#include <sys/memctrl.h>
#include <tiles.h>
#include <video.h>

#include "./data/font_8x8.h"

#define EWRAM_START ((uint8_t *)(0x02000000))
#define EWRAM_SIZE ((uint32_t)0x40000)
#define NR_OF_CYCLES ((uint32_t)10)

// define all functions here, so we can put them into IWRAM
int main() IWRAM_CODE;
void printChar(char c, uint16_t x, uint16_t y, uint16_t backColor = 0, uint16_t textColor = 15) IWRAM_CODE;
void printString(const char *s, uint16_t x, uint16_t y, uint16_t backColor = 0, uint16_t textColor = 15) IWRAM_CODE;
// --------------------------------------------------------

void printChar(char c, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor)
{
	uint16_t tileNo = static_cast<uint16_t>(c) - 32;
	uint32_t tileIndex = ((y & 31) * 32) + (x & 31);
	uint16_t *background = Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_1000);
	background[tileIndex] = 95 | ((backColor & 15) << 12);
	uint16_t *text = Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_2000);
	text[tileIndex] = tileNo | ((textColor & 15) << 12);
}

void printString(const char *s, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor)
{
	if (s != nullptr)
	{
		while (*s != '\0')
		{
			printChar(*s, x++, y, backColor, textColor);
			s++;
		}
	}
}

int main()
{
	// lets set some cool waitstates
	RegWaitCnt = WaitCntNormal;
	// set undocumented register for EWRAM waitstates
	// sets EWRAM wait states to 2/2/4 vs. 3/3/6 (default 0x0D000020)
	RegWaitEwram = WaitEwramNormal;
	// Set graphics to mode 0 and enable background 2
	REG_DISPCNT = MODE_0 | BG0_ON | BG1_ON;
	// copy data to tile map
	Memory::memcpy32(Tiles::TILE_BASE_TO_MEM(Tiles::TileBase::Base_0000), FONT_8X8_DATA, FONT_8X8_DATA_SIZE);
	// set up background 0 (text background) and 1 (text foreground) screen and tile map starts and set screen size to 256x256
	REG_BG0CNT = Tiles::backgroundConfig(Tiles::TileBase::Base_0000, Tiles::ScreenBase::Base_1000, Tiles::ScreenSize::Size_0, 16, 1);
	REG_BG1CNT = Tiles::backgroundConfig(Tiles::TileBase::Base_0000, Tiles::ScreenBase::Base_2000, Tiles::ScreenSize::Size_0, 16, 0);
	// build CGA color palette, yesss!!!
	static const color16 COLORS[16] = {0, 0x5000, 0x280, 0x5280, 0x0014, 0x5014, 0x0154, 0x5294, 0x294a, 0x7d4a, 0x2bea, 0x7fea, 0x295f, 0x7d5f, 0x2bff, 0x7fff};
	for (uint32_t i = 0; i < 16; i++)
	{
		BG_PALETTE[i * 16] = 0;
		BG_PALETTE[i * 16 + 1] = COLORS[i];
	}
	// build start screen
	Memory::memset32(Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_1000), 0x105F105F, (32 * 32) >> 1);
	Memory::memset32(Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_1000) + 32 * 18, 0x705F705F, 64 >> 1);
	printString(" MemTestGBA ", 0, 0, 2, 0);
	printString("| Pass", 12, 0, 1, 15);
	printString("ARM7 16MHz  | Test", 0, 1, 1, 15);
	printString("L1 Cache 0K | Test", 0, 2, 1, 15);
	printString("L2 Cache 0K | Testing: 0-256K", 0, 3, 1, 15);
	printString("L3 Cache 0K | Pattern:", 0, 4, 1, 15);
	printString("EWRAM 256K  | Speed:", 0, 5, 1, 15);
	printString("EWRAM timing:", 0, 6, 1, 15);
	printString("        (START) Reboot        ", 0, 18, 7, 1);
	printString("(L) Timing --    (R) Timing ++", 0, 19, 7, 1);
	// start wall clock
	irqInit();
	Time::start();
	// start main loop
	uint32_t counter = 0;
	uint16_t color = 0;
	char buffer[256] = {0};
	const uint32_t EwRAMWaitStates[] = {WaitEwramNormal, WaitEwramFast, WaitEwramLudicrous};
	do
	{
		auto startTime = Time::getTime();
		for (uint32_t cycle = 0; cycle < NR_OF_CYCLES; cycle++)
		{
			Memory::memset32(EWRAM_START, 0x12345678, EWRAM_SIZE >> 2);
		}
		auto endTime = Time::getTime();
		auto time = endTime - startTime;
		auto MBperS = (((EWRAM_SIZE * NR_OF_CYCLES) / 1024) / time) / 1024;
		fptoa(MBperS.raw(), buffer, decltype(MBperS)::BITSF, 2);
		printString(buffer, 22, 5, 1, 15);
		printString("MB/s", 26, 5, 1, 15);
		// blink "GBA"
		if (counter & 3 == 3)
		{
			color = color == 0 ? 4 : 0;
		}
		printString("GBA", 8, 0, 2, color);
		//Video::waitForVblank(true);
		// reboot with SWI 26h
		counter++;
	} while (true);
}
