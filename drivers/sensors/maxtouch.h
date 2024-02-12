// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MXT336UD_ADDRESS (0x4A << 1)
#define MXT_I2C_TIMEOUT_MS 20
#define MXT_REG_INFORMATION_BLOCK (0)


// TODO: These are peacock specific, they are used for handling CPI calculations. Is there a better default?
#ifndef MXT_SENSOR_WIDTH_MM
#   define MXT_SENSOR_WIDTH_MM 156
#endif

#ifndef MXT_SENSOR_HEIGHT_MM
#   define MXT_SENSOR_HEIGHT_MM 91
#endif

// Object table entries cannot be read individually, we have to read starting at the
// beginning of an object. For now we will allocate a large object on the stack, but
// a general purpose I2C buffer might be a better approach if other interesting objects
// are large.

// TODO: The address bytes are reversed - can probably read them individuallys
#define MXT_MAX_OBJECTS 64

typedef struct PACKED {
    unsigned char type;
    unsigned char position_ls_byte;
    unsigned char position_ms_byte;
    unsigned char size_minus_one;
    unsigned char instances_minus_one;
    unsigned char report_ids_per_instance;
} mxt_object_table_element;

typedef struct PACKED {
    unsigned char family_id;
    unsigned char variant_id;
    unsigned char version;
    unsigned char build;
    unsigned char matrix_x_size;
    unsigned char matrix_y_size;
    unsigned char num_objects;
    //mxt_object_table_element objects[MXT_MAX_OBJECTS];
} mxt_information_block;

typedef struct PACKED {
    unsigned char report_id;
    unsigned char data[5];
} mxt_message;

typedef struct PACKED {
    unsigned char count;
} mxt_message_count;

typedef struct PACKED {
    unsigned short status;
    unsigned char payloadcrc[3];
    unsigned char enccustcrc[3];
    unsigned char error;
} mxt_gen_encryptionstatus_t2;

typedef struct PACKED {
    unsigned char reset;
    unsigned char backupnv;
    unsigned char calibrate;
    unsigned char reportall;
    unsigned char debugctrl;
    unsigned char diagnostic;
    unsigned char debugctrl2;
} mxt_gen_commandprocessor_t6;

typedef struct PACKED {
    unsigned char idleacqint;
    unsigned char actacqint;
    unsigned char actv2idelto;
    unsigned char cfg;
    unsigned char cfg2;
    unsigned char idleacqintfine;
    unsigned char actvaqintfine;
} mxt_gen_powerconfig_t7;

static const unsigned char T7_CFG_INITACTV = 0x80;
static const unsigned char T7_CFG_OVFRPTSUP = 0x40;
static const unsigned char T7_CFG_ACTV2IDLETOMSB_SHIFT = 2;
static const unsigned char T7_CFG_ACTV2IDLETOMSB_MASK = 0x3C;
static const unsigned char T7_CFG_ACTVPIPEEN = 0x2;
static const unsigned char T7_CFG_IDLEPIPEEN = 0x1;

typedef struct PACKED {
    unsigned char chrgtime;
    unsigned char reserved;
    unsigned char tchdrift;
    unsigned char driftst;
    unsigned char tchautocal;
    unsigned char sync;
    unsigned char acthcalst;
    unsigned char acthcalsthr;
    unsigned char atchfrccalthr;
    unsigned char atchfrccalratio;
    unsigned char measallow;
    unsigned char reserved2[3];
    unsigned char cfg;
} mxt_gen_acquisitionconfig_t8;

typedef struct PACKED {
    unsigned char reserved[2];
    unsigned char idlesyncsperx;
    unsigned char activesyncsperx;
    unsigned char adcspersync;
    unsigned char piusesperadc;
    unsigned char xslew;
    unsigned short syncdelay;
    unsigned char xvoltage;
    unsigned char reserved2;
    unsigned char inrushcfg;
    unsigned char reserved3[6];
    unsigned char cfg;
} mxt_spt_cteconfig_t46;

typedef struct PACKED {
    unsigned char ctrl;
    unsigned char cfg1;
    unsigned char scraux;
    unsigned char tchaux;
    unsigned char tcheventcfg;
    unsigned char akscfg;
    unsigned char numtch;
    unsigned char xycfg;
    unsigned char xorigin;
    unsigned char xsize;
    unsigned char xpitch;
    unsigned char xlocip;
    unsigned char xhiclip;
    unsigned short xrange;
    unsigned char xedgecfg;
    unsigned char xedgedist;
    unsigned char dxxedgecfg;
    unsigned char dxxedgedist;
    unsigned char yorigin;
    unsigned char ysize;
    unsigned char ypitch;
    unsigned char ylocip;
    unsigned char yhiclip;
    unsigned short yrange;
    unsigned char yedgecfg;
    unsigned char yedgedist;
    unsigned char gain;
    unsigned char dxgain;
    unsigned char tchthr;
    unsigned char tchhyst;
    unsigned char intthr;
    unsigned char noisesf;
    unsigned char cutoffthr;
    unsigned char mrgthr;
    unsigned char mrgthradjstr;
    unsigned char mrghyst;
    unsigned char dxthrsf;
    unsigned char tchdidown;
    unsigned char tchdiup;
    unsigned char nexttchdi;
    unsigned char calcfg;
    unsigned char jumplimit;
    unsigned char movfilter;
    unsigned char movsmooth;
    unsigned char movpred;
    unsigned short movhysti;
    unsigned short movhystn;
    unsigned char amplhyst;
    unsigned char scrareahyst;
    unsigned char intthryst;
    unsigned char xedgecfghi;
    unsigned char xedgedisthi;
    unsigned char dxxedgecfghi;
    unsigned char dxxedgedisthi;
    unsigned char yedgecfghi;
    unsigned char yedgedisthi;
    unsigned char cfg2;
    unsigned char movhystcfg;
    unsigned char amplcoeff;
    unsigned char amploffset;
    unsigned char jumplimitmov;
    unsigned short jlmmovthr;
    unsigned char jlmmovintthr;
} mxt_touch_multiscreen_t100;

static const unsigned char T100_CTRL_SCANEN = 0x80;
static const unsigned char T100_CTRL_DISSCRMSG0 = 0x4;
static const unsigned char T100_CTRL_RPTEN = 0x2;
static const unsigned char T100_CTRL_ENABLE = 0x1;
 
static const unsigned char T100_CFG_INVERTX= 0x80;
static const unsigned char T100_CFG_INVERTY = 0x40;
static const unsigned char T100_CFG_SWITCHXY = 0x20;
static const unsigned char T100_CFG_DISLOCK = 0x10;
static const unsigned char T100_CFG_ATCHTHRSEL = 0x8;
static const unsigned char T100_CFG_RPTEACHCYCLE = 0x1;


// Touch events reported in the t100 messages
enum {
    NO_EVENT,
    MOVE,
    UNSUP,
    SUP,
    DOWN,
    UP,
    UNSUPSUP,
    UNSUPUP,
    DOWNSUP,
    DOWNUP
};
