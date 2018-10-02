/*
 * mmc64.h - Cartridge handling, MMC64 cart.
 *
 * Written by
 *  Markus Stehr <bastetfurry@ircnet.de>
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

#ifndef CARTRIDGE_INCLUDE_PRIVATE_API
#ifndef CARTRIDGE_INCLUDE_PUBLIC_API
#error "do not include this header directly, use c64cart.h instead."
#endif
#endif

#ifndef VICE_MMC64_H
#define VICE_MMC64_H

#include "types.h"
#include "c64cart.h"

#define MMC64_REV_A  0
#define MMC64_REV_B  1

#define MMC64_TYPE_AUTO  0
#define MMC64_TYPE_MMC   1
#define MMC64_TYPE_SD    2
#define MMC64_TYPE_SDHC  3

/* FIXME: remove global clockport related variables */
extern int mmc64_clockport_enabled;
extern int mmc64_hw_clockport;

extern int mmc64_cart_enabled(void);
extern int mmc64_cart_active(void);
extern void mmc64_config_init(export_t *export);
extern int mmc64_roml_read(uint16_t addr, uint8_t *byte);
extern void mmc64_roml_store(uint16_t addr, uint8_t byte);
extern int mmc64_peek_mem(uint16_t addr, uint8_t *value);
extern void mmc64_passthrough_changed(export_t *export);

extern void mmc64_config_setup(uint8_t *rawcart);

extern int mmc64_crt_attach(FILE *fd, uint8_t *rawcart);
extern int mmc64_bin_attach(const char *filename, uint8_t *rawcart);
extern int mmc64_bin_save(const char *filename);
extern int mmc64_crt_save(const char *filename);
extern int mmc64_flush_image(void);

extern int mmc64_resources_init(void);
extern void mmc64_resources_shutdown(void);
extern int mmc64_cmdline_options_init(void);
extern void mmc64_init(void);
extern void mmc64_detach(void);
extern void mmc64_reset(void);
extern int mmc64_enable(void);
int mmc64_disable(void);
extern const char *mmc64_get_file_name(void);
extern int mmc64_mmu_translate(unsigned int addr, uint8_t **base, int *start, int *limit);

struct snapshot_s;
extern int mmc64_snapshot_read_module(struct snapshot_s *s);
extern int mmc64_snapshot_write_module(struct snapshot_s *s);

#endif
