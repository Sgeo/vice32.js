/*
 * serial-iec-bus.h - Common IEC bus emulation.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
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

#ifndef VICE_SERIAL_IEC_BUS_H
#define VICE_SERIAL_IEC_BUS_H

#include "types.h"

extern void serial_iec_bus_init(void);
extern void serial_iec_bus_reset(void);
extern void serial_iec_bus_open(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t));
extern void serial_iec_bus_close(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t));
extern void serial_iec_bus_listen(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t));
extern void serial_iec_bus_talk(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t));
extern void serial_iec_bus_unlisten(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t));
extern void serial_iec_bus_untalk(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t));
extern void serial_iec_bus_write(unsigned int device, uint8_t secondary, uint8_t data, void (*st_func)(uint8_t));
extern uint8_t serial_iec_bus_read(unsigned int device, uint8_t secondary, void (*st_func)(uint8_t));

#endif
