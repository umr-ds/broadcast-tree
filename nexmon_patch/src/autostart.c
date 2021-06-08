/***************************************************************************
 *                                                                         *
 *          ###########   ###########   ##########    ##########           *
 *         ############  ############  ############  ############          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ###########   ####  ######  ##   ##   ##  ##    ######          *
 *          ###########  ####  #       ##   ##   ##  ##    #    #          *
 *                   ##  ##    ######  ##   ##   ##  ##    #    #          *
 *                   ##  ##    #       ##   ##   ##  ##    #    #          *
 *         ############  ##### ######  ##   ##   ##  ##### ######          *
 *         ###########    ###########  ##   ##   ##   ##########           *
 *                                                                         *
 *            S E C U R E   M O B I L E   N E T W O R K I N G              *
 *                                                                         *
 * This file is part of NexMon.                                            *
 *                                                                         *
 * Copyright (c) 2016 NexMon Team                                          *
 *                                                                         *
 * NexMon is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * NexMon is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with NexMon. If not, see <http://www.gnu.org/licenses/>.          *
 *                                                                         *
 **************************************************************************/

#pragma NEXMON targetregion "patch"

#include <firmware_version.h>   // definition of firmware version macros
#include <wrapper.h>            // wrapper definitions for functions that already exist in the firmware
#include <structs.h>            // structures that are used by the code in the firmware
#include <helper.h>             // useful helper functions
#include <patcher.h>            // macros used to craete patches such as BLPatch, BPatch, ...

#include <btp.h>
#include <store.h>
#include <hashmap.h>

Store seq_nums = {0};
map_t neighbors = 0;
MAC *selected_parent = 0;
int child_end_of_game = 1;
int iterations_without_improving = 0;

// Occasionally, we have to compare mac addresses with our own.
// So, we store it globally here.
MAC my_mac = {.addr = {0}};
MAC broadcast_mac = {.addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

void
autostart(void)
{
	// The autostart function is called as soon as the device comes up.
	// We init our sequence number store here, to be sure that it is initialized
	// when needed.
	init_store(&seq_nums, 10);

	neighbors = hashmap_new();

	printf("autostart\n");
}

__attribute__((at(0x2a94, "", CHIP_VER_BCM43430a1, FW_VER_7_45_41_46)))
HookPatch4(hndrte_idle, autostart, "push {r4, lr}\nmov r4, r0");
