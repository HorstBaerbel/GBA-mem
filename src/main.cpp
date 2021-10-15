#include <gba_base.h>
#include <gba_video.h>
#include <gba_input.h>
#include <gba_timers.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>

#include "dma.h"
#include "itoa.h"
#include "memory.h"
#include "tui.h"

struct alignas(4) TestConfig
{
	uint8_t *dest = nullptr;
	uint32_t destSize = 0;
	const uint8_t *src = nullptr;
	uint32_t srcSize = 0;
};

struct alignas(4) SpeedResult
{
	int32_t readTime = 0;
	int32_t writeTime = 0;
	int32_t setTime = 0;
	int32_t setDMATime = 0;
	int32_t copyToTime = 0;
	int32_t copyFromTime = 0;
	int32_t copyToDMATime = 0;
	int32_t copyFromDMATime = 0;
	uint32_t bytesPerTest = 0;
};

// define all functions here, so we can put them into IWRAM
void printSpeed(int32_t time, uint32_t bytes, uint16_t x, uint16_t y, TUI::Color backColor, TUI::Color textColor) IWRAM_CODE;
template <typename F>
void runBlockwise(F func, const TestConfig &config) IWRAM_CODE;
void readBlock(void *source, uint32_t value, uint32_t nrOfWords) IWRAM_CODE;
void writeBlock(void *destination, uint32_t value, uint32_t nrOfWords) IWRAM_CODE;
template <typename F>
int32_t runFunctionBlockwise(F func, const TestConfig &config, uint32_t nrOfCycles) IWRAM_CODE;
template <typename F>
int32_t runFunction(F func, const TestConfig &config, uint32_t nrOfCycles) IWRAM_CODE;
SpeedResult runSpeedTest(const TestConfig &config, uint32_t nrOfCycles) IWRAM_CODE;
uint32_t testPattern(uint32_t index) IWRAM_CODE;
uint32_t runErrorTest(const TestConfig &config, uint32_t nrOfCycles, uint32_t pattern) IWRAM_CODE;
int32_t getTime() IWRAM_CODE;
void updateTime() IWRAM_CODE;
void updateBlinkState() IWRAM_CODE;
void updateInput() IWRAM_CODE;
int main() IWRAM_CODE;

// ------------------------------------------------------------------------------------------------

static volatile uint32_t rwdummy;

template <typename F>
void runBlockwise(F func, const TestConfig &config)
{
	if (config.srcSize < config.destSize)
	{
		const uint32_t nrOfBlocks = config.destSize / config.srcSize;
		auto d = config.dest;
		for (uint32_t i = 0; i < nrOfBlocks; i++)
		{
			func(d, reinterpret_cast<const uint32_t *>(config.src), config.srcSize >> 2);
			d += config.srcSize;
		}
	}
	else if (config.srcSize > config.destSize)
	{
		const uint32_t nrOfBlocks = config.srcSize / config.destSize;
		auto s = config.src;
		for (uint32_t i = 0; i < nrOfBlocks; i++)
		{
			func(config.dest, reinterpret_cast<const uint32_t *>(s), config.destSize >> 2);
			s += config.destSize;
		}
	}
	else
	{
		func(config.dest, reinterpret_cast<const uint32_t *>(config.src), config.srcSize >> 2);
	}
}

void readBlock(void *source, uint32_t value, uint32_t nrOfWords)
{
	uint32_t v = value;
	auto src32 = reinterpret_cast<const uint32_t *>(source);
	for (uint32_t i = 0; i < nrOfWords; i++)
	{
		v += src32[i];
	}
	rwdummy = v;
}

void writeBlock(void *destination, uint32_t value, uint32_t nrOfWords)
{
	uint32_t v = value;
	auto dest32 = reinterpret_cast<uint32_t *>(destination);
	for (uint32_t i = 0; i < nrOfWords; i++)
	{
		dest32[i] = v++;
	}
	rwdummy = v;
}

// ------------------------------------------------------------------------------------------------

template <typename F>
int32_t runFunctionBlockwise(F func, const TestConfig &config, uint32_t nrOfCycles)
{
	auto startTime = getTime();
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		runBlockwise(func, config);
	}
	return getTime() - startTime;
}

template <typename F>
int32_t runFunction(F func, const TestConfig &config, uint32_t nrOfCycles)
{
	auto startTime = getTime();
	for (uint32_t cycle = 0; cycle < nrOfCycles; cycle++)
	{
		func(config.dest, 0x12345678, config.destSize >> 2);
	}
	return getTime() - startTime;
}

SpeedResult runSpeedTest(const TestConfig &config, uint32_t nrOfCycles)
{
	SpeedResult result = {0, 0, 0, 0, 0, 0, 0, 0, config.destSize * nrOfCycles};
	auto to = config;
	result.readTime = runFunction(readBlock, to, nrOfCycles);
	result.writeTime = runFunction(writeBlock, to, nrOfCycles);
	result.setTime = runFunction(Memory::memset32, to, nrOfCycles);
	result.setDMATime = runFunction(DMA::dma_fill32, to, nrOfCycles);
	TestConfig from = {const_cast<uint8_t *>(config.src), config.srcSize, config.dest, config.destSize};
	result.copyFromTime = runFunctionBlockwise(Memory::memcpy32, from, nrOfCycles);
	result.copyToTime = runFunctionBlockwise(Memory::memcpy32, to, nrOfCycles);
	result.copyFromDMATime = runFunctionBlockwise(DMA::dma_copy32, from, nrOfCycles);
	result.copyToDMATime = runFunctionBlockwise(DMA::dma_copy32, to, nrOfCycles);
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
		for (int32_t i = 0; i < static_cast<int32_t>(count); i++)
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
		TUI::printChars('#', nrOfDash, 18, 1, TUI::Color::Blue, TUI::Color::White);
		TUI::printChars(' ', 13 - nrOfDash, 18 + nrOfDash, 1, TUI::Color::Blue, TUI::Color::White);
	}
	return nrOfErrors;
}

// ------------------------------------------------------------------------------------------------

void printSpeed(int32_t time, uint32_t bytes, uint16_t x, uint16_t y, TUI::Color backColor, TUI::Color textColor)
{
	int32_t MBperS = ((int64_t)bytes << 4) / ((int64_t)time);
	TUI::printString(" MB/s ", x + TUI::printFloat(MBperS, x, y, backColor, textColor), y, backColor, textColor);
}

// ------------------------------------------------------------------------------------------------

static bool blinkState{false};
static bool testSpeed{true};
static uint32_t waitStateIndex{0};
static const uint32_t WaitStates[]{Memory::WaitEwramNormal, Memory::WaitEwramFast, Memory::WaitEwramLudicrous};
static const char *WaitStateStrings[]{"3/3/6", "2/2/4", "1/1/2"};
static const char *WaitStateOptionStrings[]{"                (R) Timings --", "(L) Timings ++  (R) Timings --", "(L) Timings ++                "};
constexpr uint32_t CyclesSpeed{8};
constexpr uint32_t CyclesErrors{4};
static const TestConfig testConfig{Memory::EWRAM_BLOCK, Memory::EWRAM_ALLOC_SIZE, Memory::IWRAM_BLOCK, Memory::IWRAM_ALLOC_SIZE};

constexpr int32_t TimerIncrement{164}; // 65536/164=400 -> 1/400=2.5ms
static int32_t TimerTicks{0};		   // current wall clock in s in 16.16 format

int32_t getTime()
{
	return TimerTicks;
}

void updateTime()
{
	TimerTicks += TimerIncrement;
}

void updateBlinkState()
{
	// blink UI elements
	blinkState = !blinkState;
	TUI::printChar('+', 10, 0, TUI::Color::Green, blinkState ? TUI::Color::Red : TUI::Color::Black);
	TUI::printString("Error test", 4, 17, TUI::Color::LightGray, !testSpeed && blinkState ? TUI::Color::Red : TUI::Color::Blue);
	TUI::printString("Speed test", 20, 17, TUI::Color::LightGray, testSpeed && blinkState ? TUI::Color::Red : TUI::Color::Blue);
}

void updateInput()
{
	scanKeys();
	auto keys = keysDown();
	// check if need to change wait states
	auto waitStateIndexBefore = waitStateIndex;
	if (keys & KEY_L)
	{
		waitStateIndex = waitStateIndex > 0 ? waitStateIndex - 1 : 0;
	}
	else if (keys & KEY_R)
	{
		waitStateIndex = waitStateIndex < 2 ? waitStateIndex + 1 : 2;
	}
	if (waitStateIndex != waitStateIndexBefore)
	{
		Memory::RegWaitEwram = WaitStates[waitStateIndex];
		TUI::printString(WaitStateStrings[waitStateIndex], 15, 3, TUI::Color::Blue, TUI::Color::White);
		TUI::printString(WaitStateOptionStrings[waitStateIndex], 0, 18, TUI::Color::LightGray, TUI::Color::Blue);
	}
	// check if we need to change the test mode
	auto testSpeedBefore = testSpeed;
	if (keys & KEY_A)
	{
		testSpeed = true;
	}
	else if (keys & KEY_B)
	{
		testSpeed = false;
	}
	if (testSpeed != testSpeedBefore)
	{
		TUI::printString("Error test", 4, 17, TUI::Color::LightGray, !testSpeed && blinkState ? TUI::Color::Red : TUI::Color::Blue);
		TUI::printString("Speed test", 20, 17, TUI::Color::LightGray, testSpeed && blinkState ? TUI::Color::Red : TUI::Color::Blue);
	}
	// check if we need to reboot
	if (keys & KEY_START)
	{
		SystemCall(0x26);
	}
}

int main()
{
	// set default waitstates for GamePak ROM and EWRAM
	Memory::RegWaitCnt = Memory::WaitCntFast;
	Memory::RegWaitEwram = Memory::WaitEwramNormal;
	// set up UI
	TUI::setup();
	// build start screen
	TUI::fillBackground(TUI::Color::Blue);
	TUI::fillBackgroundRect(0, 18, TUI::Width, 2, TUI::Color::LightGray);
	TUI::printString("MemTestGBA+", 0, 0, TUI::Color::Green, TUI::Color::Black);
	TUI::printString("| Pass", 11, 0, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("ARM7 16MHz | Test", 0, 1, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("EWRAM 256K | Pattern:", 0, 2, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("EWRAM Timings: 3/3/6", 0, 3, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("Read EWRAM   :", 0, 5, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("Write EWRAM  :", 0, 6, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("Set EWRAM    :", 0, 7, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("Set EWRAM DMA:", 0, 8, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("IWRAM<-EWRAM :", 0, 9, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("IWRAM->EWRAM :", 0, 10, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("IWRAM<-EWRAM DMA:", 0, 11, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("IWRAM->EWRAM DMA:", 0, 12, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("Errors: 0", 0, 14, TUI::Color::Blue, TUI::Color::White);
	TUI::printString("(B) Error test  (A) Speed test", 0, 17, TUI::Color::LightGray, TUI::Color::Blue);
	TUI::printString(WaitStateOptionStrings[0], 0, 18, TUI::Color::LightGray, TUI::Color::Blue);
	TUI::printString("        (START) Reboot        ", 0, 19, TUI::Color::LightGray, TUI::Color::Blue);
	// start wall clock
	irqInit();
	// set up time to increase time every ~5ms
	irqSet(irqMASKS::IRQ_TIMER3, updateTime);
	irqEnable(irqMASKS::IRQ_TIMER3);
	REG_TM3CNT_L = 65536 - TimerIncrement;
	REG_TM3CNT_H = TIMER_START | TIMER_IRQ | 2;
	// set up timer to call function every 1s
	irqSet(irqMASKS::IRQ_TIMER2, updateBlinkState);
	irqEnable(irqMASKS::IRQ_TIMER2);
	REG_TM2CNT_L = 65536 - 16384;
	REG_TM2CNT_H = TIMER_START | TIMER_IRQ | 3;
	// set up timer to call function every 150ms
	irqSet(irqMASKS::IRQ_TIMER1, updateInput);
	irqEnable(irqMASKS::IRQ_TIMER1);
	REG_TM1CNT_L = 65536 - 2458;
	REG_TM1CNT_H = TIMER_START | TIMER_IRQ | 3;
	// set up some variables
	uint32_t errorCount = 0;
	uint32_t patternIndex = 0;
	// start main loop
	do
	{
		if (testSpeed)
		{
			auto result = runSpeedTest(testConfig, CyclesSpeed);
			printSpeed(result.readTime, result.bytesPerTest, 15, 5, TUI::Color::Blue, TUI::Color::White);
			printSpeed(result.writeTime, result.bytesPerTest, 15, 6, TUI::Color::Blue, TUI::Color::White);
			printSpeed(result.setTime, result.bytesPerTest, 15, 7, TUI::Color::Blue, TUI::Color::White);
			printSpeed(result.setDMATime, result.bytesPerTest, 15, 8, TUI::Color::Blue, TUI::Color::White);
			printSpeed(result.copyFromTime, result.bytesPerTest, 15, 9, TUI::Color::Blue, TUI::Color::White);
			printSpeed(result.copyToTime, result.bytesPerTest, 15, 10, TUI::Color::Blue, TUI::Color::White);
			printSpeed(result.copyFromDMATime, result.bytesPerTest, 18, 11, TUI::Color::Blue, TUI::Color::White);
			printSpeed(result.copyToDMATime, result.bytesPerTest, 18, 12, TUI::Color::Blue, TUI::Color::White);
		}
		else
		{
			auto pattern = testPattern(patternIndex);
			TUI::printInt(pattern, 16, 22, 2, TUI::Color::Blue, TUI::Color::White);
			errorCount += runErrorTest(testConfig, CyclesErrors, pattern);
			TUI::printInt(errorCount, 10, 8, 14, TUI::Color::Blue, errorCount > 0 ? TUI::Color::Red : TUI::Color::White);
			patternIndex = patternIndex >= TEST_PATTERN_COUNT ? 0 : patternIndex + 1;
			auto nrOfDash = (patternIndex * 12) / TEST_PATTERN_COUNT;
			TUI::printChars('#', nrOfDash, 18, 0, TUI::Color::Blue, TUI::Color::White);
			TUI::printChars(' ', 12 - nrOfDash, 18 + nrOfDash, 0, TUI::Color::Blue, TUI::Color::White);
		}
	} while (true);
	return 0;
}
