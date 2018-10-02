/*
 * vic20cart.c - VIC20 Cartridge emulation.
 *
 * Written by
 *  Daniel Kahlin <daniel@kahlin.net>
 *
 * Based on code by
 *  Andre Fachat <fachat@physik.tu-chemnitz.de>
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

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef AMIGA_AROS
#define __AROS_OFF_T_DECLARED
#define __AROS_PID_T_DECLARED
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "behrbonz.h"
#include "c64acia.h"
#include "cartridge.h"
#include "cmdline.h"
#include "debugcart.h"
#include "digimax.h"
#include "ds12c887rtc.h"
#include "export.h"
#include "finalexpansion.h"
#include "georam.h"
#include "ioramcart.h"
#include "lib.h"
#include "log.h"
#include "mem.h"
#include "ultimem.h"
#include "vic-fp.h"
#include "megacart.h"
#include "monitor.h"
#include "resources.h"
#include "sfx_soundexpander.h"
#include "sfx_soundsampler.h"
#include "sid-snapshot.h"
#include "sidcart.h"
#include "snapshot.h"
#ifdef HAVE_RAWNET
#define CARTRIDGE_INCLUDE_PRIVATE_API
#define CARTRIDGE_INCLUDE_PUBLIC_API
#include "ethernetcart.h"
#undef CARTRIDGE_INCLUDE_PRIVATE_API
#undef CARTRIDGE_INCLUDE_PUBLIC_API
#endif
#include "translate.h"
#include "util.h"
#include "vic20cart.h"
#include "vic20cartmem.h"
#include "vic20mem.h"
#include "vic20-generic.h"
#include "vic20-ieee488.h"
#include "vic20-midi.h"
#include "zfile.h"

/* #define DEBUGCART */

#ifdef DEBUGCART
#define DBG(x)  printf x
#else
#define DBG(x)
#endif

/* flag for indicating if cartridge is from snapshot, used for
   disabling "set as default" and write back */
int cartridge_is_from_snapshot = 0;

/* actual resources */
static char *cartridge_file = NULL;
static int cartridge_type;
static int vic20cartridge_reset;

/* local shadow of some resources (e.g not yet set as default) */
static int vic20cart_type = CARTRIDGE_NONE;
static char *cartfile = NULL;

static int cartres_flags = 0;

static int cartridge_attach_from_resource(int type, const char *filename);

void reset_try_flags(void)
{
    cartres_flags = 0;
}

int try_cartridge_attach(int c)
{
    cartres_flags ^= c;
    if (cartres_flags) {
        return 0;
    }

    return cartridge_attach_from_resource(vic20cart_type, cartfile);
}

static int set_cartridge_type(int val, void *param)
{
    switch (val) {
        case CARTRIDGE_NONE:
        case CARTRIDGE_VIC20_BEHRBONZ:
        case CARTRIDGE_VIC20_GENERIC:
        case CARTRIDGE_VIC20_MEGACART:
        case CARTRIDGE_VIC20_FINAL_EXPANSION:
        case CARTRIDGE_VIC20_UM:
        case CARTRIDGE_VIC20_FP:
        case CARTRIDGE_VIC20_IEEE488:
        case CARTRIDGE_VIC20_SIDCART:
        case CARTRIDGE_VIC20_DETECT:
        case CARTRIDGE_VIC20_4KB_2000:
        case CARTRIDGE_VIC20_8KB_2000:
        case CARTRIDGE_VIC20_4KB_6000:
        case CARTRIDGE_VIC20_8KB_6000:
        case CARTRIDGE_VIC20_4KB_A000:
        case CARTRIDGE_VIC20_8KB_A000:
        case CARTRIDGE_VIC20_4KB_B000:
        case CARTRIDGE_VIC20_8KB_4000:
        case CARTRIDGE_VIC20_4KB_4000:
        case CARTRIDGE_VIC20_16KB_2000:
        case CARTRIDGE_VIC20_16KB_4000:
        case CARTRIDGE_VIC20_16KB_6000:
            break;
        default:
            return -1;
    }

    cartridge_type = val;
    vic20cart_type = cartridge_type;

    return try_cartridge_attach(TRY_RESOURCE_CARTTYPE);
}

static int set_cartridge_file(const char *name, void *param)
{
    util_string_set(&cartridge_file, name);
    util_string_set(&cartfile, name);

    return try_cartridge_attach(TRY_RESOURCE_CARTNAME);
}

static int set_cartridge_reset(int val, void *param)
{
    vic20cartridge_reset = val ? 1 : 0;

    return try_cartridge_attach(TRY_RESOURCE_CARTRESET);
}

static const resource_string_t resources_string[] = {
    { "CartridgeFile", "", RES_EVENT_NO, NULL,
      &cartridge_file, set_cartridge_file, NULL },
    RESOURCE_STRING_LIST_END
};

static const resource_int_t resources_int[] = {
    { "CartridgeType", CARTRIDGE_NONE,
      RES_EVENT_STRICT, (resource_value_t)CARTRIDGE_NONE,
      &cartridge_type, set_cartridge_type, NULL },
    { "CartridgeReset", 1, RES_EVENT_NO, NULL,
      &vic20cartridge_reset, set_cartridge_reset, NULL },
    RESOURCE_INT_LIST_END
};

int cartridge_resources_init(void)
{
    if (resources_register_int(resources_int) < 0
        || resources_register_string(resources_string) < 0
        || generic_resources_init() < 0
        || finalexpansion_resources_init() < 0
        || vic_fp_resources_init() < 0
        || vic_um_resources_init() < 0
        || megacart_resources_init() < 0
#ifdef HAVE_RAWNET
        || ethernetcart_resources_init() < 0
#endif
        || aciacart_resources_init() < 0
        || digimax_resources_init() < 0
        || ds12c887rtc_resources_init() < 0
        || sfx_soundexpander_resources_init() < 0
        || sfx_soundsampler_resources_init() < 0
        || ioramcart_resources_init() < 0
        || georam_resources_init() < 0
        || debugcart_resources_init() < 0) {
        return -1;
    }
    return 0;
}

void cartridge_resources_shutdown(void)
{
    megacart_resources_shutdown();
    finalexpansion_resources_shutdown();
    generic_resources_shutdown();
#ifdef HAVE_RAWNET
    ethernetcart_resources_shutdown();
#endif
    aciacart_resources_shutdown();
    digimax_resources_shutdown();
    ds12c887rtc_resources_shutdown();
    sfx_soundexpander_resources_shutdown();
    sfx_soundsampler_resources_shutdown();
    georam_resources_shutdown();
    debugcart_resources_shutdown();

    lib_free(cartridge_file);
    lib_free(cartfile);
}

static int detach_cartridge_cmdline(const char *param, void *extra_param)
{
    /*
     * this is called by '+cart' and relies on that command line options
     * are processed after the default cartridge gets attached via
     * resources/.ini.
     */
    cartridge_detach_image(-1);
    return 0;
}

static int attach_cartridge_cmdline(const char *param, void *extra_param)
{
    return cartridge_attach_image(vice_ptr_to_int(extra_param), param);
}

static const cmdline_option_t cmdline_options[] =
{
    { "-cartreset", SET_RESOURCE, 0,
      NULL, NULL, "CartridgeReset", (void *)1,
      USE_PARAM_STRING, USE_DESCRIPTION_ID,
      IDCLS_UNUSED, IDCLS_CART_ATTACH_DETACH_RESET,
      NULL, NULL },
    { "+cartreset", SET_RESOURCE, 0,
      NULL, NULL, "CartridgeReset", (void *)0,
      USE_PARAM_STRING, USE_DESCRIPTION_ID,
      IDCLS_UNUSED, IDCLS_CART_ATTACH_DETACH_NO_RESET,
      NULL, NULL },
    { "-cart2", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_16KB_2000, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_EXT_ROM_2000_NAME,
      NULL, NULL },
    { "-cart4", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_16KB_4000, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_EXT_ROM_4000_NAME,
      NULL, NULL },
    { "-cart6", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_16KB_6000, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_EXT_ROM_6000_NAME,
      NULL, NULL },
    { "-cartA", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_8KB_A000, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_EXT_ROM_A000_NAME,
      NULL, NULL },
    { "-cartB", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_4KB_B000, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_EXT_ROM_B000_NAME,
      NULL, NULL },
    { "-cartbb", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_BEHRBONZ, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_BEHRBONZ_ROM_NAME,
      NULL, NULL },
    { "-cartgeneric", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_GENERIC, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_GENERIC_ROM_NAME,
      NULL, NULL },
    { "-cartmega", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_MEGACART, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_MEGA_CART_ROM_NAME,
      NULL, NULL },
    { "-cartfe", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_FINAL_EXPANSION, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_FINAL_EXPANSION_ROM_NAME,
      NULL, NULL },
    { "-ultimem", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_UM, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_VIC_UM_ROM_NAME,
      NULL, NULL },
    { "-cartfp", CALL_FUNCTION, 1,
      attach_cartridge_cmdline, (void *)CARTRIDGE_VIC20_FP, NULL, NULL,
      USE_PARAM_ID, USE_DESCRIPTION_ID,
      IDCLS_P_NAME, IDCLS_SPECIFY_VIC_FP_ROM_NAME,
      NULL, NULL },
    { "+cart", CALL_FUNCTION, 0,
      detach_cartridge_cmdline, NULL, NULL, NULL,
      USE_PARAM_STRING, USE_DESCRIPTION_ID,
      IDCLS_UNUSED, IDCLS_DISABLE_CART,
      NULL, NULL },
    CMDLINE_LIST_END
};

int cartridge_cmdline_options_init(void)
{
    mon_cart_cmd.cartridge_attach_image = cartridge_attach_image;
    mon_cart_cmd.cartridge_detach_image = cartridge_detach_image;

    if (cmdline_register_options(cmdline_options) < 0
        || finalexpansion_cmdline_options_init() < 0
        || vic_fp_cmdline_options_init() < 0
        || vic_um_cmdline_options_init() < 0
        || megacart_cmdline_options_init() < 0
#ifdef HAVE_RAWNET
        || ethernetcart_cmdline_options_init() < 0
#endif
        || aciacart_cmdline_options_init() < 0
        || digimax_cmdline_options_init() < 0
        || ds12c887rtc_cmdline_options_init() < 0
        || sfx_soundexpander_cmdline_options_init() < 0
        || sfx_soundsampler_cmdline_options_init() < 0
        || ioramcart_cmdline_options_init() < 0
        || georam_cmdline_options_init() < 0
        || debugcart_cmdline_options_init() < 0) {
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
static int cartridge_attach_from_resource(int type, const char *filename)
{
    if (vic20cart_type == CARTRIDGE_VIC20_GENERIC) {
        /* special case handling for the multiple file generic type */
        return generic_attach_from_resource(vic20cart_type, cartfile);
    }
    return cartridge_attach_image(vic20cart_type, cartfile);
}

int cartridge_attach_image(int type, const char *filename)
{
    int type_orig;
    int generic_multifile = 0;
    int ret = 0;

    /* Attaching no cartridge always works.  */
    if (type == CARTRIDGE_NONE || filename == NULL || *filename == '\0') {
        return 0;
    }

    log_message(LOG_DEFAULT, "Attached cartridge type %d, file=`%s'.", type, filename);

    type_orig = type;
    switch (type_orig) {
        case CARTRIDGE_VIC20_DETECT:
        case CARTRIDGE_VIC20_4KB_2000:
        case CARTRIDGE_VIC20_8KB_2000:
        case CARTRIDGE_VIC20_4KB_6000:
        case CARTRIDGE_VIC20_8KB_6000:
        case CARTRIDGE_VIC20_4KB_A000:
        case CARTRIDGE_VIC20_8KB_A000:
        case CARTRIDGE_VIC20_4KB_B000:
        case CARTRIDGE_VIC20_8KB_4000:
        case CARTRIDGE_VIC20_4KB_4000:
        case CARTRIDGE_VIC20_16KB_2000:
        case CARTRIDGE_VIC20_16KB_4000:
        case CARTRIDGE_VIC20_16KB_6000:
            /*
             * For specific layouts only detach if we were something else than
             * CARTRIDGE_VIC20_GENERIC before.
             * This allows us to add images to a generic type.
             */
            if (vic20cart_type != CARTRIDGE_VIC20_GENERIC) {
                cartridge_detach_image(-1);
            }
            generic_multifile = 1;
            type = CARTRIDGE_VIC20_GENERIC;
            break;
        case CARTRIDGE_VIC20_GENERIC:
            /*
             * this is because the only generic cart that is attachable
             * will be attached as a auto detected multi file cart for now
             * Remove when this changes.
             */
            generic_multifile = 1;
            break;
        default:
            cartridge_detach_image(-1);
    }

    switch (type) {
        case CARTRIDGE_VIC20_BEHRBONZ:
            ret = behrbonz_bin_attach(filename);
            break;
        case CARTRIDGE_VIC20_GENERIC:
            ret = generic_bin_attach(type_orig, filename);
            break;
        case CARTRIDGE_VIC20_UM:
            ret = vic_um_bin_attach(filename);
            break;
        case CARTRIDGE_VIC20_FP:
            ret = vic_fp_bin_attach(filename);
            break;
        case CARTRIDGE_VIC20_MEGACART:
            ret = megacart_bin_attach(filename);
            break;
        case CARTRIDGE_VIC20_FINAL_EXPANSION:
            ret = finalexpansion_bin_attach(filename);
            break;
    }

    vic20cart_type = type;
    if (generic_multifile) {
        util_string_set(&cartfile, NULL);
    } else {
        util_string_set(&cartfile, filename);
    }
    if (ret == 0) {
        cartridge_attach(type, NULL);
    }
    return ret;
}

void cartridge_detach_image(int type)
{
    cartridge_detach(vic20cart_type);
    vic20cart_type = CARTRIDGE_NONE;
    cartridge_is_from_snapshot = 0;
}

void cartridge_set_default(void)
{
    if (cartridge_is_from_snapshot) {
        /* TODO replace with ui_error */
        log_warning(LOG_DEFAULT, "Set as default disabled");
        return;
    }

    set_cartridge_type(vic20cart_type, NULL);
    set_cartridge_file((vic20cart_type == CARTRIDGE_NONE) ? "" : cartfile, NULL);
    /* special case handling for the multiple file generic type */
    generic_set_default();

    /* reset the try flags (we've only called the set function once each) */
    reset_try_flags();
}

const char *cartridge_get_file_name(int addr)
{
    if (vic20cart_type == CARTRIDGE_VIC20_GENERIC) {
        /* special case handling for the multiple file generic type */
        return generic_get_file_name((uint16_t)addr);
    }

    return cartfile;
}

/*
    save cartridge to binary file

    *atleast* all carts whose image might be modified at runtime should be hooked up here.

    TODO: add bin save for all ROM carts also
*/
int cartridge_bin_save(int type, const char *filename)
{
    switch (type) {
        case CARTRIDGE_VIC20_GEORAM:
            return georam_bin_save(filename);
    }
    return -1;
}

/* FIXME: this can be used once we implement a crt like format for vic20 */
#if 0
/*
    save cartridge to crt file

    *atleast* all carts whose image might be modified at runtime AND
    which have a valid crt id should be hooked up here.

    TODO: add crt save for all ROM carts also
*/
int cartridge_crt_save(int type, const char *filename)
{
    switch (type) {
    }
    return -1;
}
#endif

/*
    flush cart image

    all carts whose image might be modified at runtime should be hooked up here.
*/
int cartridge_flush_image(int type)
{
    switch (type) {
        case CARTRIDGE_VIC20_GEORAM:
            return georam_flush_image();
    }
    return -1;
}

int cartridge_save_image(int type, const char *filename)
{
/* FIXME: this can be used once we implement a crt like format for vic20 */
#if 0
    char *ext = util_get_extension((char *)filename);
    if (ext != NULL && !strcmp(ext, "crt")) {
        return cartridge_crt_save(type, filename);
    }
#endif
    return cartridge_bin_save(type, filename);
}

/* ------------------------------------------------------------------------- */

#define VIC20CART_DUMP_MAX_CARTS  16

#define VIC20CART_DUMP_VER_MAJOR   2
#define VIC20CART_DUMP_VER_MINOR   1
#define SNAP_MODULE_NAME  "VIC20CART"

int vic20cart_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;
    uint8_t i;
    uint8_t number_of_carts = 0;
    int cart_ids[VIC20CART_DUMP_MAX_CARTS];
    int last_cart = 0;
    export_list_t *e = export_query_list(NULL);

    memset(cart_ids, 0, sizeof(cart_ids));

    while (e != NULL) {
        if (number_of_carts == VIC20CART_DUMP_MAX_CARTS) {
            DBG(("CART snapshot save: active carts > max (%i)\n", number_of_carts));
            return -1;
        }
        if (last_cart != (int)e->device->cartid) {
            last_cart = e->device->cartid;
            cart_ids[number_of_carts++] = last_cart;
        }
        e = e->next;
    }

    m = snapshot_module_create(s, SNAP_MODULE_NAME, VIC20CART_DUMP_VER_MAJOR, VIC20CART_DUMP_VER_MINOR);
    if (m == NULL) {
        return -1;
    }

    if (SMW_DW(m, (uint32_t)vic20cart_type) < 0) {
        goto fail;
    }

    if (SMW_B(m, number_of_carts) < 0) {
        goto fail;
    }

    /* Not much to do if no carts present */
    if (number_of_carts == 0) {
        return snapshot_module_close(m);
    }

    /* Save cart IDs */
    for (i = 0; i < number_of_carts; i++) {
        if (SMW_DW(m, (uint32_t)cart_ids[i]) < 0) {
            goto fail;
        }
    }

    snapshot_module_close(m);

    /* Save individual cart data */
    for (i = 0; i < number_of_carts; i++) {
        switch (cart_ids[i]) {
            case CARTRIDGE_VIC20_BEHRBONZ:
                if (behrbonz_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_FINAL_EXPANSION:
                if (finalexpansion_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_IO2_RAM:
                if (ioramcart_io2_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_IO3_RAM:
                if (ioramcart_io3_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_MEGACART:
                if (megacart_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_UM:
                if (vic_um_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_IEEE488:
                if (vic20_ieee488_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
#ifdef HAVE_MIDI
            case CARTRIDGE_MIDI_MAPLIN:
                if (vic20_midi_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
#endif
            case CARTRIDGE_VIC20_SIDCART:
                if (sidcart_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_FP:
                if (vic_fp_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_ACIA:
                if (aciacart_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_DIGIMAX:
                if (digimax_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_DS12C887RTC:
                if (ds12c887rtc_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_GEORAM:
                if (georam_write_snapshot_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_SFX_SOUND_EXPANDER:
                if (sfx_soundexpander_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_SFX_SOUND_SAMPLER:
                if (sfx_soundsampler_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
#ifdef HAVE_RAWNET
            case CARTRIDGE_TFE:
                if (ethernetcart_snapshot_write_module(s) < 0) {
                    return -1;
                }
                break;
#endif
        }
    }

    if (vic20cart_type == CARTRIDGE_VIC20_GENERIC) {
        if (generic_snapshot_write_module(s) < 0) {
            return -1;
        }
    }
    return 0;

fail:
    snapshot_module_close(m);
    return -1;
}

int vic20cart_snapshot_read_module(snapshot_t *s)
{
    uint8_t vmajor, vminor;
    snapshot_module_t *m;
    int new_cart_type, cartridge_reset;
    uint8_t i;
    uint8_t number_of_carts = 0;
    int cart_ids[VIC20CART_DUMP_MAX_CARTS];

    m = snapshot_module_open(s, SNAP_MODULE_NAME, &vmajor, &vminor);
    if (m == NULL) {
        return -1;
    }

    if (vmajor != VIC20CART_DUMP_VER_MAJOR) {
        goto fail;
    }

    if (SMR_DW_INT(m, &new_cart_type) < 0) {
        goto fail;
    }

    if (vminor < 1) {
        /* FIXME: test if loading older snapshots with cartridge actually works */
        if (new_cart_type != CARTRIDGE_NONE) {
            number_of_carts = 1;
            cart_ids[0] = new_cart_type;
        }
    } else {
        if (SMR_B(m, &number_of_carts) < 0) {
            goto fail;
        }

        /* Not much to do if no carts in snapshot */
        if (number_of_carts == 0) {
            return snapshot_module_close(m);
        }

        if (number_of_carts > VIC20CART_DUMP_MAX_CARTS) {
            DBG(("CART snapshot read: carts %i > max %i\n", number_of_carts, VIC20CART_DUMP_MAX_CARTS));
            goto fail;
        }

        /* Read cart IDs */
        for (i = 0; i < number_of_carts; i++) {
            if (SMR_DW_INT(m, &cart_ids[i]) < 0) {
                goto fail;
            }
        }
    }

    snapshot_module_close(m);

    /* disable cartridge reset while detaching old cart */
    resources_get_int("CartridgeReset", &cartridge_reset);
    resources_set_int("CartridgeReset", 0);
    cartridge_detach_image(-1);
    resources_set_int("CartridgeReset", cartridge_reset);

    /* disallow "set as default" and write back */
    cartridge_is_from_snapshot = 1;

    vic20cart_type = new_cart_type;
    mem_cartridge_type = new_cart_type;

    /* Read individual cart data */
    for (i = 0; i < number_of_carts; i++) {
        switch (cart_ids[i]) {
            case CARTRIDGE_VIC20_BEHRBONZ:
                if (behrbonz_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_FINAL_EXPANSION:
                if (finalexpansion_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_IO2_RAM:
                if (ioramcart_io2_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_IO3_RAM:
                if (ioramcart_io3_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_MEGACART:
                if (megacart_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_UM:
                if (vic_um_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_IEEE488:
                if (vic20_ieee488_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
#ifdef HAVE_MIDI
            case CARTRIDGE_MIDI_MAPLIN:
                if (vic20_midi_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
#endif
            case CARTRIDGE_VIC20_SIDCART:
                if (sidcart_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_VIC20_FP:
                if (vic_fp_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_ACIA:
                if (aciacart_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_DIGIMAX:
                if (digimax_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_DS12C887RTC:
                if (ds12c887rtc_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_GEORAM:
                if (georam_read_snapshot_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_SFX_SOUND_EXPANDER:
                if (sfx_soundexpander_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
            case CARTRIDGE_SFX_SOUND_SAMPLER:
                if (sfx_soundsampler_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
#ifdef HAVE_RAWNET
            case CARTRIDGE_TFE:
                if (ethernetcart_snapshot_read_module(s) < 0) {
                    return -1;
                }
                break;
#endif
        }
    }

    if (vic20cart_type == CARTRIDGE_VIC20_GENERIC) {
        if (generic_snapshot_read_module(s) < 0) {
            return -1;
        }
    }

    return 0;
fail:
    snapshot_module_close(m);
    return -1;
}
