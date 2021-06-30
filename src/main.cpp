#include <gba_base.h>
#include <gba_video.h>
#include <gba_input.h>

#include <debug/print.h>
#include <debug/itoa.h>
#include <memory/dma.h>
#include <memory/memory.h>
#include <sys/interrupts.h>
#include <sys/memctrl.h>
#include <sys/syscall.h>
#include <tiles.h>
#include <video.h>

#include "./data/font_8x8.h"

#define IWRAM_ALLOC_SIZE (16 * 1024)
#define EWRAM_ALLOC_SIZE (128 * 1024)

struct TestConfig
{
	uint8_t *dest;
	uint32_t destSize;
	const uint8_t *src;
	uint32_t srcSize;
};

struct TestResult
{
	Math::fp1616_t time;
	uint32_t errors;
};

struct SpeedResult
{
	Math::fp1616_t readTime;
	Math::fp1616_t writeTime;
	Math::fp1616_t setTime;
	Math::fp1616_t setDMATime;
	Math::fp1616_t copyToTime;
	Math::fp1616_t copyFromTime;
	Math::fp1616_t copyToDMATime;
	Math::fp1616_t copyFromDMATime;
	uint32_t bytesPerTest;
};

// define all functions here, so we can put them into IWRAM
int main() IWRAM_CODE;
void printChar(char c, uint16_t x, uint16_t y, uint16_t backColor = 0, uint16_t textColor = 15) IWRAM_CODE;
uint16_t printString(const char *s, uint16_t x, uint16_t y, uint16_t backColor = 0, uint16_t textColor = 15) IWRAM_CODE;
void printSpeed(Math::fp1616_t time, uint32_t bytes, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor) IWRAM_CODE;
void copyBlockwise(const TestConfig &config) IWRAM_CODE;
void copyBlockwiseDMA(const TestConfig &config) IWRAM_CODE;
void readBlockwise(const TestConfig &config) IWRAM_CODE;
void writeBlockwise(const TestConfig &config) IWRAM_CODE;
void setBlockwise(const TestConfig &config) IWRAM_CODE;
void setBlockwiseDMA(const TestConfig &config) IWRAM_CODE;
SpeedResult test_speed(const TestConfig &config, uint32_t nrOfCycles) IWRAM_CODE;

static char printBuffer[256] = {0};

// ------------------------------------------------------------------------------------------------

void printChar(char c, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor)
{
	uint16_t tileNo = static_cast<uint16_t>(c) - 32;
	uint32_t tileIndex = ((y & 31) * 32) + (x & 31);
	auto background = Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_1000);
	background[tileIndex] = 95 | ((backColor & 15) << 12);
	auto text = Tiles::SCREEN_BASE_TO_MEM(Tiles::ScreenBase::Base_2000);
	text[tileIndex] = tileNo | ((textColor & 15) << 12);
}

uint16_t printString(const char *s, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor)
{
	uint16_t nrOfCharsPrinted = 0;
	if (s != nullptr)
	{
		while (*s != '\0')
		{
			printChar(*s, x++, y, backColor, textColor);
			s++;
			nrOfCharsPrinted++;
		}
	}
	return nrOfCharsPrinted;
}

void printSpeed(Math::fp1616_t time, uint32_t bytes, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor)
{
	auto MBperS = ((bytes / 1024) / time) / 1024;
	fptoa(MBperS.raw(), printBuffer, decltype(MBperS)::BITSF, 2);
	auto valueLength = printString(printBuffer, x, y, backColor, textColor);
	printString(" MB/s", x + valueLength, y, backColor, textColor);
}

// ------------------------------------------------------------------------------------------------

void copyBlockwise(const TestConfig &config)
{
	if (config.srcSize < config.destSize)
	{
		const uint32_t nrOfBlocks = config.destSize / config.srcSize;
		auto d = config.dest;
		for (uint32_t i = 0; i < nrOfBlocks; i++)
		{
			Memory::memcpy32(d, config.src, config.srcSize >> 2);
			d += config.srcSize;
		}
	}
	else if (config.srcSize > config.destSize)
	{
		const uint32_t nrOfBlocks = config.srcSize / config.destSize;
		auto s = config.src;
		for (uint32_t i = 0; i < nrOfBlocks; i++)
		{
			Memory::memcpy32(config.dest, s, config.destSize >> 2);
			s += config.destSize;
		}
	}
	else
	{
		Memory::memcpy32(config.dest, config.src, config.srcSize >> 2);
	}
}

void copyBlockwiseDMA(const TestConfig &config)
{
	if (config.srcSize < config.destSize)
	{
		const uint32_t nrOfBlocks = config.destSize / config.srcSize;
		auto d = config.dest;
		for (uint32_t i = 0; i < nrOfBlocks; i++)
		{
			DMA::dma_copy32(d, reinterpret_cast<const uint32_t *>(config.src), config.srcSize >> 2);
			d += config.srcSize;
		}
	}
	else if (config.srcSize > config.destSize)
	{
		const uint32_t nrOfBlocks = config.srcSize / config.destSize;
		auto s = config.src;
		for (uint32_t i = 0; i < nrOfBlocks; i++)
		{
			DMA::dma_copy32(config.dest, reinterpret_cast<const uint32_t *>(s), config.destSize >> 2);
			s += config.destSize;
		}
	}
	else
	{
		DMA::dma_copy32(config.dest, reinterpret_cast<const uint32_t *>(config.src), config.srcSize >> 2);
	}
}

void readBlockwise(const TestConfig &config)
{
	volatile uint32_t v;
	auto count = config.srcSize >> 2;
	auto src32 = reinterpret_cast<const uint32_t *>(config.src);
	for (uint32_t i = 0; i < count; i++)
	{
		v = src32[i];
	}
}

void writeBlockwise(const TestConfig &config)
{
	volatile uint32_t v = 12345678;
	auto count = config.destSize >> 2;
	auto dest32 = reinterpret_cast<uint32_t *>(config.dest);
	for (uint32_t i = 0; i < count; i++)
	{
		dest32[i] = v;
	}
}

void setBlockwise(const TestConfig &config)
{
	Memory::memset32(config.dest, 0x12345678, config.destSize >> 2);
}

void setBlockwiseDMA(const TestConfig &config)
{
	DMA::dma_fill32(config.dest, 0x12345678, config.destSize >> 2);
}

// ------------------------------------------------------------------------------------------------

SpeedResult test_speed(const TestConfig &config, uint32_t nrOfCycles)
{
	SpeedResult result = {0, 0, 0, 0, 0, 0, 0, 0, config.destSize * nrOfCycles};
	TestConfig to = config;
	TestConfig from = {const_cast<uint8_t *>(config.src), config.srcSize, config.dest, config.destSize};
	auto startTime = Time::getTime();
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		readBlockwise(from);
	}
	auto endTime = Time::getTime();
	result.readTime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		writeBlockwise(to);
	}
	endTime = Time::getTime();
	result.writeTime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		setBlockwise(to);
	}
	endTime = Time::getTime();
	result.setTime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		setBlockwiseDMA(to);
	}
	endTime = Time::getTime();
	result.setDMATime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		copyBlockwise(from);
	}
	endTime = Time::getTime();
	result.copyFromTime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		copyBlockwise(to);
	}
	endTime = Time::getTime();
	result.copyToTime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		copyBlockwiseDMA(from);
	}
	endTime = Time::getTime();
	result.copyFromDMATime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		copyBlockwiseDMA(to);
	}
	endTime = Time::getTime();
	result.copyToDMATime = endTime - startTime;
	return result;
}

// ------------------------------------------------------------------------------------------------

int main()
{
	// set default waitstates for GamePak ROM and EWRAM
	RegWaitCnt = WaitCntFast;
	RegWaitEwram = WaitEwramNormal;
	// set graphics to mode 0 and enable background 2
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
	printString("Cache 0K    | Test", 0, 2, 1, 15);
	printString("EWRAM 256K  | Pattern:", 0, 3, 1, 15);
	printString("EWRAM Timings: 3/3/6", 0, 4, 1, 15);
	printString("Read EWRAM   :", 0, 6, 1, 15);
	printString("Write EWRAM  :", 0, 7, 1, 15);
	printString("Set EWRAM    :", 0, 8, 1, 15);
	printString("Set EWRAM DMA:", 0, 9, 1, 15);
	printString("IWRAM<-EWRAM :", 0, 10, 1, 15);
	printString("IWRAM->EWRAM :", 0, 11, 1, 15);
	printString("IWRAM<-EWRAM DMA:", 0, 12, 1, 15);
	printString("IWRAM->EWRAM DMA:", 0, 13, 1, 15);
	printString("(B) Error test  (A) Speed test", 0, 17, 7, 1);
	printString("(L) Timings ++  (R) Timings --", 0, 18, 7, 1);
	printString("        (START) Reboot        ", 0, 19, 7, 1);
	// start wall clock
	irqInit();
	Time::start();
	// allocate memory
	Memory::init();
	const TestConfig config = {Memory::malloc_EWRAM<uint8_t>(EWRAM_ALLOC_SIZE), EWRAM_ALLOC_SIZE, Memory::malloc_IWRAM<uint8_t>(IWRAM_ALLOC_SIZE), IWRAM_ALLOC_SIZE};
	// set up some state variables
	bool testSpeed = true;
	uint32_t counter = 0;
	uint16_t color = 0;
	uint32_t waitStateIndex = 0;
	static const uint32_t CyclesSpeed[] = {8, 10, 12};
	static const uint32_t CyclesErrors[] = {10, 12, 14};
	static const uint32_t WaitStates[] = {WaitEwramNormal, WaitEwramFast, WaitEwramLudicrous};
	static const char *WaitStateStrings[] = {"3/3/6", "2/2/4", "1/1/2"};
	// start main loop
	do
	{
		if (testSpeed)
		{
			auto result = test_speed(config, CyclesSpeed[waitStateIndex]);
			printSpeed(result.readTime, result.bytesPerTest, 15, 6, 1, 15);
			printSpeed(result.writeTime, result.bytesPerTest, 15, 7, 1, 15);
			printSpeed(result.setTime, result.bytesPerTest, 15, 8, 1, 15);
			printSpeed(result.setDMATime, result.bytesPerTest, 15, 9, 1, 15);
			printSpeed(result.copyFromTime, result.bytesPerTest, 15, 10, 1, 15);
			printSpeed(result.copyToTime, result.bytesPerTest, 15, 11, 1, 15);
			printSpeed(result.copyFromDMATime, result.bytesPerTest, 18, 12, 1, 15);
			printSpeed(result.copyToDMATime, result.bytesPerTest, 18, 13, 1, 15);
		}
		else
		{
			//TODO: implement
		}
		// blink "GBA"
		if (counter & 3 == 3)
		{
			color = color == 0 ? 4 : 0;
		}
		printString("GBA", 8, 0, 2, color);
		// check keys
		scanKeys();
		auto keys = keysDown();
		if (keys & KEY_L)
		{
			waitStateIndex = waitStateIndex > 0 ? waitStateIndex - 1 : 0;
		}
		else if (keys & KEY_R)
		{
			waitStateIndex = waitStateIndex < 2 ? waitStateIndex + 1 : 2;
		}
		RegWaitEwram = WaitStates[waitStateIndex];
		printString(WaitStateStrings[waitStateIndex], 15, 4, 1, 15);
		if (keys & KEY_A)
		{
			testSpeed = true;
		}
		else if (keys & KEY_B)
		{
			testSpeed = false;
		}
		if (keys & KEY_START)
		{
			SYSCALL(0x26);
		}
		//Video::waitForVblank(true);
		counter++;
	} while (true);
}
