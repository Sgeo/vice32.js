/*
 * hs-unix-pci.c - Unix specific PCI hardsid driver.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

/* Tested and confirmed working on:

 - Linux 2.6 (/dev/port based PCI I/O, PCI HardSID)
 - Linux 2.6 (/dev/port based PCI I/O, PCI HardSID Quattro)
 - Linux 2.6 (permission based PCI I/O, PCI HardSID)
 - Linux 2.6 (permission based PCI I/O, PCI HardSID Quattro)
 */

#include "vice.h"

#ifdef UNIX_COMPILE

#if defined(HAVE_HARDSID) && defined(HAVE_HARDSID_IO)

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include "hardsid.h"
#include "io-access.h"
#include "log.h"
#include "pci-unix-drv.h"
#include "types.h"

#include "hs-unix.h"


#define MAXSID 4

static int base1 = 0;
static int base2 = 0;
static int sids_found = -1;
static int hssids[MAXSID] = {-1, -1, -1, -1};

int hs_pci_read(uint16_t addr, int chipno)
{
    uint8_t ret = 0;

    if (chipno < MAXSID && hssids[chipno] != -1 && addr < 0x20) {
        io_access_store(base1 + 4, (uint8_t)((chipno << 6) | (addr & 0x1f) | 0x20));
        usleep(2);
        io_access_store(base2 + 2, 0x20);
        ret = io_access_read(base1);
        io_access_store(base2 + 2, 0x80);
    }
    return ret;
}

void hs_pci_store(uint16_t addr, uint8_t outval, int chipno)
{
    if (chipno < MAXSID && hssids[chipno] != -1 && addr < 0x20) {
        io_access_store(base1 + 3, outval);
        io_access_store(base1 + 4, (uint8_t)((chipno << 6) | (addr & 0x1f)));
        usleep(2);
    }
}

static int detect_sid_uno(void)
{
    int i;
    int j;

    for (j = 0; j < 4; ++j) {
        for (i = 0x18; i >= 0; --i) {
            hs_pci_store((uint16_t)i, 0, j);
        }
    }

    hs_pci_store(0x12, 0xff, 0);

    for (i = 0; i < 100; ++i) {
        if (hs_pci_read(0x1b, 3)) {
            return 0;
        }
    }

    hs_pci_store(0x0e, 0xff, 0);
    hs_pci_store(0x0f, 0xff, 0);
    hs_pci_store(0x12, 0x20, 0);

    for (i = 0; i < 100; ++i) {
        if (hs_pci_read(0x1b, 3)) {
            return 1;
        }
    }
    return 0;
}

static int detect_sid(int chipno)
{
    int i;

    for (i = 0x18; i >= 0; --i) {
        hs_pci_store((uint16_t)i, 0, chipno);
    }

    hs_pci_store(0x12, 0xff, chipno);

    for (i = 0; i < 100; ++i) {
        if (hs_pci_read(0x1b, chipno)) {
            return 0;
        }
    }

    hs_pci_store(0x0e, 0xff, chipno);
    hs_pci_store(0x0f, 0xff, chipno);
    hs_pci_store(0x12, 0x20, chipno);

    for (i = 0; i < 100; ++i) {
        if (hs_pci_read(0x1b, chipno)) {
            return 1;
        }
    }
    return 0;
}

static char *HStype = "PCI HardSID Quattro";

int hs_pci_open(void)
{
    int j;
    uint32_t b1 = 0;
    uint32_t b2 = 0;

    if (!sids_found) {
        return -1;
    }

    if (sids_found > 0) {
        return 0;
    }

    sids_found = 0;

    log_message(LOG_DEFAULT, "Detecting PCI HardSID boards.");

    j = pci_get_base(0x6581, 0x8580, &b1, &b2);

    if (j < 0) {
        log_message(LOG_DEFAULT, "No PCI HardSID boards found.");
        return -1;
    }

    base1 = b1 & 0xfffc;
    base2 = b2 & 0xfffc;

    if (io_access_map(base1, 8) < 0) {
        log_message(LOG_DEFAULT, "Cannot get permission to access $%X.", base1);
        return -1;
    }

    if (io_access_map(base2, 4) < 0) {
        log_message(LOG_DEFAULT, "Cannot get permission to access $%X.", base2);
        io_access_unmap(base1, 8);
        return -1;
    }

    log_message(LOG_DEFAULT, "PCI HardSID board found at $%04X and $%04X.", base1, base2);

    for (j = 0; j < MAXSID; ++j) {
        hssids[sids_found] = j;
        if (detect_sid(j)) {
            sids_found++;
        }
    }

    if (!sids_found) {
        log_message(LOG_DEFAULT, "No PCI HardSID boards found.");
        io_access_unmap(base1, 8);
        io_access_unmap(base2, 4);
        return -1;
    }

    /* Check for classic HardSID if 4 SIDs were found. */
    if (sids_found == 4) {
        if (detect_sid_uno()) {
            HStype = "PCI HardSID";
            sids_found = 1;
        }
    }

    log_message(LOG_DEFAULT, "%s: opened, found %d SIDs.", HStype, sids_found);

    return 0;
}

int hs_pci_close(void)
{
    int i;

    io_access_unmap(base1, 8);
    io_access_unmap(base2, 4);

    for (i = 0; i < MAXSID; ++i) {
        hssids[i] = -1;
    }

    sids_found = -1;

    log_message(LOG_DEFAULT, "%s: closed.", HStype);

    return 0;
}

int hs_pci_available(void)
{
    return sids_found;
}

/* ---------------------------------------------------------------------*/

void hs_pci_state_read(int chipno, struct sid_hs_snapshot_state_s *sid_state)
{
    sid_state->hsid_main_clk = 0;
    sid_state->hsid_alarm_clk = 0;
    sid_state->lastaccess_clk = 0;
    sid_state->lastaccess_ms = 0;
    sid_state->lastaccess_chipno = 0;
    sid_state->chipused = 0;
    sid_state->device_map[0] = 0;
    sid_state->device_map[1] = 0;
    sid_state->device_map[2] = 0;
    sid_state->device_map[3] = 0;
}

void hs_pci_state_write(int chipno, struct sid_hs_snapshot_state_s *sid_state)
{
}
#endif
#endif
