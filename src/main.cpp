#include <gba_base.h>
#include <gba_video.h>
#include <gba_input.h>
#include <gba_timers.h>

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
void printChar(char c, uint16_t x, uint16_t y, uint16_t backColor = 0, uint16_t textColor = 15) IWRAM_CODE;
uint16_t printChars(char c, uint16_t n, uint16_t x, uint16_t y, uint16_t backColor = 0, uint16_t textColor = 15) IWRAM_CODE;
uint16_t printString(const char *s, uint16_t x, uint16_t y, uint16_t backColor = 0, uint16_t textColor = 15) IWRAM_CODE;
void printValue(uint32_t value, uint32_t base, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor) IWRAM_CODE;
void printSpeed(Math::fp1616_t time, uint32_t bytes, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor) IWRAM_CODE;
void copyBlockwise(const TestConfig &config) IWRAM_CODE;
void copyBlockwiseDMA(const TestConfig &config) IWRAM_CODE;
void readBlockwise(const TestConfig &config) IWRAM_CODE;
void writeBlockwise(const TestConfig &config) IWRAM_CODE;
SpeedResult runSpeedTest(const TestConfig &config, uint32_t nrOfCycles) IWRAM_CODE;
uint32_t testPattern(uint32_t index) IWRAM_CODE;
uint32_t runErrorTest(const TestConfig &config, uint32_t nrOfCycles, uint32_t pattern) IWRAM_CODE;
void toggleState() IWRAM_CODE;
int main() IWRAM_CODE;

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

uint16_t printChars(char c, uint16_t n, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor)
{
	uint16_t nrOfCharsPrinted = 0;
	for (uint32_t i = 0; i < n; i++)
	{
		printChar(c, x++, y, backColor, textColor);
		nrOfCharsPrinted++;
	}
	return nrOfCharsPrinted;
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

void printValue(uint32_t value, uint32_t base, uint16_t x, uint16_t y, uint16_t backColor, uint16_t textColor)
{
	itoa(value, printBuffer, base);
	printString(printBuffer, x, y, backColor, textColor);
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

// ------------------------------------------------------------------------------------------------

SpeedResult runSpeedTest(const TestConfig &config, uint32_t nrOfCycles)
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
		Memory::memset32(config.dest, 0x12345678, config.destSize >> 2);
	}
	endTime = Time::getTime();
	result.setTime = endTime - startTime;
	startTime = endTime;
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		DMA::dma_fill32(config.dest, 0x12345678, config.destSize >> 2);
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

#define TEST_PATTERN_COUNT 128

uint32_t testPattern(uint32_t index)
{
	if (index < 32)
	{
		return (uint32_t(0b1) << index);
	}
	else if (index < 64)
	{
		return (uint32_t(0b11) << (index - 32));
	}
	else if (index < 96)
	{
		return (uint32_t(0b101) << (index - 64));
	}
	else if (index < 128)
	{
		return (uint32_t(0b111) << (index - 96));
	}
	return 0;
}

uint32_t runErrorTest(const TestConfig &config, uint32_t nrOfCycles, uint32_t pattern)
{
	uint32_t nrOfErrors = 0;
	auto complement = ~pattern;
	auto count = config.destSize >> 2;
	auto src32 = reinterpret_cast<uint32_t *>(config.dest);
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		// Moving inversions algorithm (sort of)
		DMA::dma_fill32(config.dest, pattern, count);
		for (int32_t i = 0; i < count; i++)
		{
			nrOfErrors += src32[i] != pattern ? 1 : 0;
			src32[i] = complement;
		}
		for (int32_t i = count - 1; i >= 0; i--)
		{
			nrOfErrors += src32[i] != complement ? 1 : 0;
			src32[i] = pattern;
		}
		auto nrOfDash = (cycle * 13) / nrOfCycles;
		printChars('#', nrOfDash, 18, 1, 1, 15);
		printChars(' ', 13 - nrOfDash, 18 + nrOfDash, 1, 1, 15);
	}
	return nrOfErrors;
}

// ------------------------------------------------------------------------------------------------

static bool state = false;
static bool testSpeed = true;

void toggleState()
{
	state = !state;
	printChar('+', 10, 0, 2, state ? 4 : 0);
	printString("Error test", 4, 17, 7, !testSpeed && state ? 4 : 1);
	printString("Speed test", 20, 17, 7, testSpeed && state ? 4 : 1);
}

int main()
{
	// set up some variables
	uint32_t errorCount = 0;
	uint32_t patternIndex = 0;
	uint32_t waitStateIndex = 0;
	static const uint32_t CyclesSpeed[] = {8, 10, 12};
	static const uint32_t CyclesErrors[] = {4, 6, 8};
	static const uint32_t WaitStates[] = {WaitEwramNormal, WaitEwramFast, WaitEwramLudicrous};
	static const char *WaitStateStrings[] = {"3/3/6", "2/2/4", "1/1/2"};
	static const char *WaitStateOptionStrings[] = {"                (R) Timings --", "(L) Timings ++  (R) Timings --", "(L) Timings ++                "};
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
	printString("MemTestGBA+", 0, 0, 2, 0);
	printString("| Pass", 11, 0, 1, 15);
	printString("ARM7 16MHz | Test", 0, 1, 1, 15);
	printString("EWRAM 256K | Pattern:", 0, 2, 1, 15);
	printString("EWRAM Timings: 3/3/6", 0, 3, 1, 15);
	printString("Read EWRAM   :", 0, 5, 1, 15);
	printString("Write EWRAM  :", 0, 6, 1, 15);
	printString("Set EWRAM    :", 0, 7, 1, 15);
	printString("Set EWRAM DMA:", 0, 8, 1, 15);
	printString("IWRAM<-EWRAM :", 0, 9, 1, 15);
	printString("IWRAM->EWRAM :", 0, 10, 1, 15);
	printString("IWRAM<-EWRAM DMA:", 0, 11, 1, 15);
	printString("IWRAM->EWRAM DMA:", 0, 12, 1, 15);
	printString("Errors: 0", 0, 14, 1, 15);
	printString("(B) Error test  (A) Speed test", 0, 17, 7, 1);
	printString(WaitStateOptionStrings[0], 0, 18, 7, 1);
	printString("        (START) Reboot        ", 0, 19, 7, 1);
	// start wall clock
	irqInit();
	Time::start();
	// set up time to call function every second
	irqSet(IRQMask::IRQ_TIMER2, toggleState);
	irqEnable(IRQMask::IRQ_TIMER2);
	REG_TM2CNT_L = 65536 - 16384;
	REG_TM2CNT_H = TIMER_START | TIMER_IRQ | 3;
	// allocate memory
	Memory::init();
	const TestConfig config = {Memory::malloc_EWRAM<uint8_t>(EWRAM_ALLOC_SIZE), EWRAM_ALLOC_SIZE, Memory::malloc_IWRAM<uint8_t>(IWRAM_ALLOC_SIZE), IWRAM_ALLOC_SIZE};
	// start main loop
	do
	{
		if (testSpeed)
		{
			auto result = runSpeedTest(config, CyclesSpeed[waitStateIndex]);
			printSpeed(result.readTime, result.bytesPerTest, 15, 5, 1, 15);
			printSpeed(result.writeTime, result.bytesPerTest, 15, 6, 1, 15);
			printSpeed(result.setTime, result.bytesPerTest, 15, 7, 1, 15);
			printSpeed(result.setDMATime, result.bytesPerTest, 15, 8, 1, 15);
			printSpeed(result.copyFromTime, result.bytesPerTest, 15, 9, 1, 15);
			printSpeed(result.copyToTime, result.bytesPerTest, 15, 10, 1, 15);
			printSpeed(result.copyFromDMATime, result.bytesPerTest, 18, 11, 1, 15);
			printSpeed(result.copyToDMATime, result.bytesPerTest, 18, 12, 1, 15);
		}
		else
		{
			auto pattern = testPattern(patternIndex);
			printValue(pattern, 16, 22, 2, 1, 15);
			errorCount += runErrorTest(config, CyclesErrors[waitStateIndex], pattern);
			printValue(errorCount, 10, 8, 14, 1, errorCount > 0 ? 4 : 15);
			patternIndex = patternIndex >= TEST_PATTERN_COUNT ? 0 : patternIndex + 1;
			auto nrOfDash = (patternIndex * 12) / TEST_PATTERN_COUNT;
			printChars('#', nrOfDash, 18, 0, 1, 15);
			printChars(' ', 12 - nrOfDash, 18 + nrOfDash, 0, 1, 15);
		}
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
		printString(WaitStateStrings[waitStateIndex], 15, 3, 1, 15);
		printString(WaitStateOptionStrings[waitStateIndex], 0, 18, 7, 1);
		if (keys & KEY_A)
		{
			testSpeed = true;
		}
		else if (keys & KEY_B)
		{
			testSpeed = false;
			patternIndex = 0;
		}
		if (keys & KEY_START)
		{
			SYSCALL(0x26);
		}
		//Video::waitForVblank(true);
	} while (true);
	return 0;
}
