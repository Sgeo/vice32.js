/*
 * westermann.c - Cartridge handling, Westermann cart.
 *
 * Written by
 *  groepaz <groepaz@gmx.net>
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

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64cartsystem.h"
#undef CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64mem.h"
#include "cartio.h"
#include "cartridge.h"
#include "export.h"
#include "monitor.h"
#include "snapshot.h"
#include "types.h"
#include "util.h"
#include "westermann.h"
#include "crt.h"

/*
    Westermann Utility Cartridge

    16kb ROM

    - starts in 16k game config
    - any read access to io2 switches to 8k game config
*/

/* some prototypes are needed */
static uint8_t westermann_io2_read(uint16_t addr);
static uint8_t westermann_io2_peek(uint16_t addr);
static int westermann_dump(void);

static io_source_t westermann_device = {
    CARTRIDGE_NAME_WESTERMANN,
    IO_DETACH_CART,
    NULL,
    0xdf00, 0xdfff, 0xff,
    0, /* read is never valid */
    NULL,
    westermann_io2_read,
    westermann_io2_peek,
    westermann_dump,
    CARTRIDGE_WESTERMANN,
    0,
    0
};

static io_source_list_t *westermann_list_item = NULL;

static const export_resource_t export_res_westermann = {
    CARTRIDGE_NAME_WESTERMANN, 1, 1, NULL, &westermann_device, CARTRIDGE_WESTERMANN
};

/* ---------------------------------------------------------------------*/

static int westermann_a000 = 0;

static uint8_t westermann_io2_read(uint16_t addr)
{
    cart_config_changed_slotmain(0, 0, CMODE_READ);
    westermann_a000 = 0;
    return 0;
}

static uint8_t westermann_io2_peek(uint16_t addr)
{
    return 0;
}

static int westermann_dump(void)
{
    mon_out("$A000-$BFFF ROM: %s\n", (westermann_a000) ? "enabled" : "disabled");

    return 0;
}

/* ---------------------------------------------------------------------*/

void westermann_config_init(void)
{
    cart_config_changed_slotmain(1, 1, CMODE_READ);
    westermann_a000 = 1;
}

void westermann_config_setup(uint8_t *rawcart)
{
    memcpy(roml_banks, rawcart, 0x2000);
    memcpy(romh_banks, &rawcart[0x2000], 0x2000);
    cart_config_changed_slotmain(1, 1, CMODE_READ);
    westermann_a000 = 1;
}

static int westermann_common_attach(void)
{
    if (export_add(&export_res_westermann) < 0) {
        return -1;
    }
    westermann_list_item = io_source_register(&westermann_device);

    return 0;
}

int westermann_bin_attach(const char *filename, uint8_t *rawcart)
{
    if (util_file_load(filename, rawcart, 0x4000, UTIL_FILE_LOAD_SKIP_ADDRESS) < 0) {
        return -1;
    }
    return westermann_common_attach();
}

int westermann_crt_attach(FILE *fd, uint8_t *rawcart)
{
    crt_chip_header_t chip;

    if (crt_read_chip_header(&chip, fd)) {
        return -1;
    }

    if (chip.start != 0x8000 || chip.size != 0x4000) {
        return -1;
    }

    if (crt_read_chip(rawcart, 0, &chip, fd)) {
        return -1;
    }

    return westermann_common_attach();
}

void westermann_detach(void)
{
    export_remove(&export_res_westermann);
    io_source_unregister(westermann_list_item);
    westermann_list_item = NULL;
}

/* ---------------------------------------------------------------------*/

/* CARTDELAEP256 snapshot module format:

   type  | name     | version | description
   ----------------------------------------
   BYTE  | ROM a000 |   0.1   | ROM at $A000 flag
   ARRAY | ROML     |   0.0+  | 8192 BYTES of ROML data
   ARRAY | ROMH     |   0.0+  | 8192 BYTES of ROMH data
 */

static char snap_module_name[] = "CARTWEST";
#define SNAP_MAJOR   0
#define SNAP_MINOR   1

int westermann_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, snap_module_name, SNAP_MAJOR, SNAP_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (0
        || SMW_B(m, (uint8_t)westermann_a000) < 0
        || SMW_BA(m, roml_banks, 0x2000) < 0
        || SMW_BA(m, romh_banks, 0x2000) < 0) {
        snapshot_module_close(m);
        return -1;
    }

    return snapshot_module_close(m);
}

int westermann_snapshot_read_module(snapshot_t *s)
{
    uint8_t vmajor, vminor;
    snapshot_module_t *m;

    m = snapshot_module_open(s, snap_module_name, &vmajor, &vminor);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than currrent */
    if ((vmajor != SNAP_MAJOR) || (vminor != SNAP_MINOR)) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    /* new in 0.1 */
    if (SNAPVAL(vmajor, vminor, 0, 1)) {
        if (SMR_B_INT(m, &westermann_a000) < 0) {
            goto fail;
        }
    } else {
        westermann_a000 = 0;
    }

    if (0
        || SMR_BA(m, roml_banks, 0x2000) < 0
        || SMR_BA(m, romh_banks, 0x2000) < 0) {
        goto fail;
    }

    snapshot_module_close(m);

    return westermann_common_attach();

fail:
    snapshot_module_close(m);
    return -1;
}
