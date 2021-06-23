// - You should be able to build using F7 and build + run using F5.

#include <gba_video.h>
#include <gba_dma.h>
#include <gba_systemcalls.h>
#include <gba_interrupt.h>

#include "memory.h"
#include "./data/dkp_logo.h"

int main()
{
	// Set graphics to mode 4 and enable background 2
	REG_DISPCNT = MODE_4 | BG2_ON;
	// copy data to palette and backbuffer
	Memory::memcpy32(BG_PALETTE, dkp_logo_palette, DKP_LOGO_PALETTE_SIZE >> 2);
	Memory::memcpy32(MODE5_BB, dkp_logo, DKP_LOGO_SIZE >> 2);
	// swap backbuffer
	REG_DISPCNT = REG_DISPCNT | BACKBUFFER;
	do
	{
	} while (true);
}
