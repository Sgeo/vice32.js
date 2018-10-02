/*
 * spcu64-snapshot.c - SCPU64 snapshot handling.
 *
 * Written by
 *  Kajtar Zsolt <soci@c64.rulez.org>
 *  Andreas Boose <viceteam@t-online.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
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

#include "scpu64-snapshot.h"
#include "scpu64.h"
#include "scpu64gluelogic.h"
#include "scpu64memsnapshot.h"
#include "cia.h"
#include "drive-snapshot.h"
#include "drive.h"
#include "ioutil.h"
#include "joyport.h"
#include "joystick.h"
#include "keyboard.h"
#include "log.h"
#include "machine.h"
#include "main65816cpu.h"
#include "sid-snapshot.h"
#include "snapshot.h"
#include "sound.h"
#include "tape-snapshot.h"
#include "types.h"
#include "userport.h"
#include "vice-event.h"
#include "vicii.h"

#define SNAP_MAJOR 1
#define SNAP_MINOR 1

int scpu64_snapshot_write(const char *name, int save_roms, int save_disks, int event_mode)
{
    snapshot_t *s;

    s = snapshot_create(name, ((uint8_t)(SNAP_MAJOR)), ((uint8_t)(SNAP_MINOR)), machine_get_name());
    if (s == NULL) {
        return -1;
    }

    sound_snapshot_prepare();

    /* Execute drive CPUs to get in sync with the main CPU.  */
    drive_cpu_execute_all(maincpu_clk);

    if (maincpu_snapshot_write_module(s) < 0
        || scpu64_snapshot_write_module(s, save_roms) < 0
        || ciacore_snapshot_write_module(machine_context.cia1, s) < 0
        || ciacore_snapshot_write_module(machine_context.cia2, s) < 0
        || sid_snapshot_write_module(s) < 0
        || drive_snapshot_write_module(s, save_disks, save_roms) < 0
        || vicii_snapshot_write_module(s) < 0
        || scpu64_glue_snapshot_write_module(s) < 0
        || event_snapshot_write_module(s, event_mode) < 0
        || keyboard_snapshot_write_module(s) < 0
        || joyport_snapshot_write_module(s, JOYPORT_1) < 0
        || joyport_snapshot_write_module(s, JOYPORT_2) < 0
        || userport_snapshot_write_module(s) < 0) {
        snapshot_close(s);
        ioutil_remove(name);
        return -1;
    }

    snapshot_close(s);
    return 0;
}

int scpu64_snapshot_read(const char *name, int event_mode)
{
    snapshot_t *s;
    uint8_t minor, major;

    s = snapshot_open(name, &major, &minor, machine_get_name());
    if (s == NULL) {
        return -1;
    }

    if (major != SNAP_MAJOR || minor != SNAP_MINOR) {
        log_error(LOG_DEFAULT, "Snapshot version (%d.%d) not valid: expecting %d.%d.", major, minor, SNAP_MAJOR, SNAP_MINOR);
        snapshot_set_error(SNAPSHOT_MODULE_INCOMPATIBLE);
        goto fail;
    }

    vicii_snapshot_prepare();

    joyport_clear_devices();

    if (maincpu_snapshot_read_module(s) < 0
        || scpu64_snapshot_read_module(s) < 0
        || ciacore_snapshot_read_module(machine_context.cia1, s) < 0
        || ciacore_snapshot_read_module(machine_context.cia2, s) < 0
        || sid_snapshot_read_module(s) < 0
        || drive_snapshot_read_module(s) < 0
        || vicii_snapshot_read_module(s) < 0
        || scpu64_glue_snapshot_read_module(s) < 0
        || event_snapshot_read_module(s, event_mode) < 0
        || keyboard_snapshot_read_module(s) < 0
        || joyport_snapshot_read_module(s, JOYPORT_1) < 0
        || joyport_snapshot_read_module(s, JOYPORT_2) < 0
        || userport_snapshot_read_module(s) < 0) {
        goto fail;
    }

    snapshot_close(s);

    sound_snapshot_finish();

    return 0;

fail:
    if (s != NULL) {
        snapshot_close(s);
    }

    machine_trigger_reset(MACHINE_RESET_MODE_SOFT);

    return -1;
}
