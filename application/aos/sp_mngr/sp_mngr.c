//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
//============================================================================
//=== AO_SpMngr subsystem root: SST task + HSM
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sst.h"
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "app_sig.h"
#include "ui_evt.h"
#include "sp_thread/sp_thread.h"
#include <bits/sockaddr.h>
#include "hdlc_parser_priv.h"
#include "protocol_catalog_priv.h"
#include "protocol_decoder_priv.h"
#include "protocol_frame_formatter_priv.h"
#include "sp_mngr.h"
DBC_MODULE_NAME("sp_mngr")

//============================================================================
//=== AO instance

typedef struct {
    SST_Task super;
    SM_Hsm hsm;
    HdlcParser hdlcParser;
    ProtocolCatalog protocolCatalog;
    ProtocolDecoder protocolDecoder;
    ProtocolFrameFormatter protocolFrameFormatter;
} SpMngr;

static SpMngr SpMngr_inst_;
SST_Task * const AO_SpMngr = &SpMngr_inst_.super;

static void SpMngr_reportConfig_(char const *action,
                                 SpMngrConfigEvt const *e);
static bool SpMngr_makeSerialConfig_(
    SpMngrConfig const *source,
    SerialConfig *target);
static void SpMngr_onHdlcFrame_(void *ctx,
                                uint8_t const *frame,
                                size_t size);
static void SpMngr_refreshProtocols_(SpMngr *me);
static void SpMngr_loadProtocol_(SpMngr *me,
                                 SpMngrProtocolEvt const *request);
static bool SpMngr_postProtocolCatalog_(ProtocolCatalog const *catalog);

//============================================================================
//=== HSM states

// TOP-INIT
static SM_StatePtr SpMngr_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT;

// active
static SM_RetState SpMngr_active_(SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT;

static SM_HsmState SM_HSM_ROM SpMngr_active = {
    SM_HSM_TOP,                         // super
    (SM_InitHandler)0,                  // init_
    (SM_ActionHandler)0,                // entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&SpMngr_active_    // handler
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SpMngr_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&SpMngr_active);
}

static SM_RetState SpMngr_active_(SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT {
    (void)me;

    switch (e->sig) {
        case SPMNGR_CONFIG_UPDATE_SIG: {
            SpMngrConfigEvt const * const command =
                SST_EVT_DOWNCAST(SpMngrConfigEvt, e);
            SerialConfig config;
            SpMngr_reportConfig_("Configuration updated", command);
            if (SpMngr_makeSerialConfig_(&command->config, &config)) {
                SpThread_postApplyConfig(&config);
            }
            return _SM_HANDLED();
        }

        case SPMNGR_CONFIG_APPLIED_SIG: {
            UI_postText(
                "[SYS_INFO]>>Serial configuration applied.\n");
            return _SM_HANDLED();
        }

        case SPMNGR_CONFIG_APPLY_FAILED_SIG: {
            UI_postText(
                "[SYS_INFO]>>Serial configuration failed; port closed.\n");
            UI_postConnectionStatus(UI_CONNECTION_DISCONNECTED);
            return _SM_HANDLED();
        }

        case SPMNGR_LOAD_PROTOCOL_SIG: {
            SpMngr * const manager = containerof(me, SpMngr, hsm);
            SpMngr_loadProtocol_(
                manager,
                SST_EVT_DOWNCAST(SpMngrProtocolEvt, e));
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_CONNECT_SIG: {
            SpMngrConfigEvt const * const command =
                SST_EVT_DOWNCAST(SpMngrConfigEvt, e);
            SerialConfig config;
            if (SpMngr_makeSerialConfig_(&command->config, &config)) {
                SpMngr_reportConfig_("Connect requested", command);
                SpThread_postOpenPort(&config);
            } else {
                UI_postText(
                    "[SYS_INFO]>>Invalid serial configuration.\n");
            }
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_OPENED_SIG: {
            UI_postText("[SYS_INFO]>>Serial port opened.\n");
            UI_postConnectionStatus(UI_CONNECTION_CONNECTED);
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_OPEN_FAILED_SIG: {
            UI_postText("[SYS_INFO]>>Serial port open failed.\n");
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_DISCONNECT_SIG: {
            UI_postText("[SYS_INFO]>>Disconnect requested.\n");
            SpThread_postClosePort();
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_CLOSED_SIG: {
            UI_postText("[SYS_INFO]>>Serial port closed.\n");
            UI_postConnectionStatus(UI_CONNECTION_DISCONNECTED);
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_CLOSE_FAILED_SIG: {
            UI_postText("[SYS_INFO]>>Serial port close failed.\n");
            UI_postConnectionStatus(UI_CONNECTION_DISCONNECTED);
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_CONNECTION_LOST_SIG: {
            UI_postText("[SYS_INFO]>>Serial port connection lost.\n");
            UI_postConnectionStatus(UI_CONNECTION_DISCONNECTED);
            return _SM_HANDLED();
        }

        case SPMNGR_REFRESH_PROTOCOLS_SIG: {
            SpMngr * const manager = containerof(me, SpMngr, hsm);
            SpMngr_refreshProtocols_(manager);
            return _SM_HANDLED();
        }

        case SPMNGR_REFRESH_PORTS_SIG: {
            SpThread_postRefreshPorts();
            return _SM_HANDLED();
        }

        case SPMNGR_REFRESHED_PORTS_SIG: {
            SpMngrPortsEvt const * const result =
                SST_EVT_DOWNCAST(SpMngrPortsEvt, e);
            if (result->portNames != (char *)0) {
                UI_postPortList(result->portNames,
                                result->portNamesSize);
                free(result->portNames);
            } else {
                UI_postText(
                    "[SYS_INFO]>>Serial port refresh failed.\n");
            }
            return _SM_HANDLED();
        }

        case SPMNGR_RX_PACKET_SIG: {
            SpMngr * const manager = containerof(me, SpMngr, hsm);
            SpMngrRxPacketEvt const * const packet =
                SST_EVT_DOWNCAST(SpMngrRxPacketEvt, e);
            DBC_ASSERT(600, packet->data != (uint8_t *)0);
            DBC_ASSERT(601, packet->size > 0U);

            for (size_t i = 0U; i < packet->size; ++i) {
                HdlcParser_input(&manager->hdlcParser,
                                 packet->data[i]);
            }
            free(packet->data);
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static void SpMngr_onHdlcFrame_(void * const ctx,
                                uint8_t const * const frame,
                                size_t const size)
{
    SpMngr * const me = (SpMngr *)ctx;
    DBC_REQUIRE(500, me != (SpMngr *)0);
    DBC_REQUIRE(501, frame != (uint8_t const *)0);
    DBC_REQUIRE(502, (size > 0U)
                     && (size <= HDLC_PARSER_FRAME_MAX_SIZE));

    (void)ProtocolFrameFormatter_format(
        &me->protocolFrameFormatter, &me->protocolDecoder,
        frame, size);
    UI_postText(ProtocolFrameFormatter_text(
        &me->protocolFrameFormatter));
}

static void SpMngr_reportConfig_(
    char const * const action,
    SpMngrConfigEvt const * const e)
{
    DBC_REQUIRE(300, action != (char const *)0);
    DBC_REQUIRE(301, e != (SpMngrConfigEvt const *)0);

    char text[2048];
    int const len = snprintf(
        text, sizeof(text),
        "[SYS_INFO]>>%s: port=%s baud=%s data=%s stop=%s "
        "parity=%s flow=%s\n",
        action, e->config.port, e->config.baudrate,
        e->config.dataBits, e->config.stopBits,
        e->config.parity, e->config.flowControl);
    DBC_ASSERT(302, (len > 0) && ((size_t)len < sizeof(text)));
    (void)len;
    UI_postText(text);
}

static bool SpMngr_parseInt_(char const * const text,
                             int const min,
                             int const max,
                             int * const value)
{
    if (memchr(text, '\0', SPMNGR_VALUE_LEN) == (void *)0) {
        return false;
    }

    errno = 0;
    char *end;
    long const parsed = strtol(text, &end, 10);
    if ((errno == ERANGE) || (end == text) || (*end != '\0')
        || (parsed < min) || (parsed > max))
    {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static bool SpMngr_parseStopBits_(char const * const text,
                                  SerialStopBits * const value)
{
    if (strcmp(text, "1") == 0) {
        *value = SERIAL_STOP_BITS_1;
        return true;
    } else if (strcmp(text, "2") == 0) {
        *value = SERIAL_STOP_BITS_2;
        return true;
    } else {
        return false;
    }
}

static bool SpMngr_parseParity_(char const * const text,
                                SerialParity * const value)
{
    static char const * const names[] = {
        "none", "odd", "even", "mark", "space",
    };
    for (unsigned i = 0U; i < (sizeof(names) / sizeof(names[0])); ++i) {
        if (strcmp(text, names[i]) == 0) {
            *value = (SerialParity)i;
            return true;
        }
    }
    return false;
}

static bool SpMngr_parseFlowControl_(
    char const * const text,
    SerialFlowControl * const value)
{
    static char const * const names[] = {
        "none", "xon/xoff", "rts/cts", "dtr/dsr",
    };
    for (unsigned i = 0U; i < (sizeof(names) / sizeof(names[0])); ++i) {
        if (strcmp(text, names[i]) == 0) {
            *value = (SerialFlowControl)i;
            return true;
        }
    }
    return false;
}

static bool SpMngr_makeSerialConfig_(
    SpMngrConfig const * const source,
    SerialConfig * const target)
{
    DBC_REQUIRE(400, source != (SpMngrConfig const *)0);
    DBC_REQUIRE(401, target != (SerialConfig *)0);

    char const * const terminator = (char const *)memchr(
        source->port, '\0', sizeof(source->port));
    if (terminator == (char const *)0) {
        return false;
    }
    size_t const portNameLen = (size_t)(terminator - source->port);
    if ((portNameLen == 0U) || (portNameLen >= sizeof(target->portName))
        || (strcmp(source->port, "none") == 0))
    {
        return false;
    }
    if ((memchr(source->stopBits, '\0', sizeof(source->stopBits))
         == (void *)0)
        || (memchr(source->parity, '\0', sizeof(source->parity))
            == (void *)0)
        || (memchr(source->flowControl, '\0',
                   sizeof(source->flowControl)) == (void *)0))
    {
        return false;
    }

    int dataBits;
    if (!SpMngr_parseInt_(source->baudrate, 1, INT_MAX,
                          &target->baudRate)
        || !SpMngr_parseInt_(source->dataBits, 5, 8, &dataBits)
        || !SpMngr_parseStopBits_(source->stopBits,
                                  &target->stopBits)
        || !SpMngr_parseParity_(source->parity, &target->parity)
        || !SpMngr_parseFlowControl_(source->flowControl,
                                     &target->flowControl))
    {
        return false;
    }

    memcpy(target->portName, source->port, portNameLen + 1U);
    target->dataBits = (uint8_t)dataBits;
    return true;
}

static void SpMngr_refreshProtocols_(SpMngr * const me) {
    DBC_REQUIRE(700, me != (SpMngr *)0);

    ProtocolCatalogResult const result =
        ProtocolCatalog_scan(&me->protocolCatalog);
    if (result == PROTOCOL_CATALOG_OK) {
        if (!SpMngr_postProtocolCatalog_(&me->protocolCatalog)) {
            UI_postText(
                "[SYS_INFO]>>Protocol catalog allocation failed.\n");
        }
    } else {
        char text[256];
        int const length = snprintf(
            text, sizeof(text),
            "[SYS_INFO]>>Protocol catalog failed: %s\n",
            ProtocolCatalog_error(&me->protocolCatalog)->detail);
        DBC_ASSERT(702, (length > 0)
                        && ((size_t)length < sizeof(text)));
        (void)length;
        UI_postText(text);
    }
}

static bool SpMngr_postProtocolCatalog_(
    ProtocolCatalog const * const catalog)
{
    size_t const count = ProtocolCatalog_count(catalog);
    size_t packedSize = 1U;
    for (size_t i = 0U; i < count; ++i) {
        packedSize += strlen(
            ProtocolCatalog_relativePath(catalog, i)) + 1U;
    }
    if (count == 0U) {
        ++packedSize;
    }

    char * const packed = (char *)malloc(packedSize);
    if (packed == (char *)0) {
        return false;
    }

    size_t offset = 0U;
    for (size_t i = 0U; i < count; ++i) {
        char const * const path =
            ProtocolCatalog_relativePath(catalog, i);
        size_t const pathSize = strlen(path) + 1U;
        memcpy(&packed[offset], path, pathSize);
        offset += pathSize;
    }
    packed[offset++] = '\0';
    if (count == 0U) {
        packed[offset++] = '\0';
    }
    DBC_ASSERT(703, offset == packedSize);

    UI_postProtocolList(packed, packedSize);
    free(packed);
    return true;
}

static void SpMngr_loadProtocol_(
    SpMngr * const me,
    SpMngrProtocolEvt const * const request)
{
    DBC_REQUIRE(710, me != (SpMngr *)0);
    DBC_REQUIRE(711, request != (SpMngrProtocolEvt const *)0);

    char text[512];
    if (memchr(request->relativePath, '\0',
               sizeof(request->relativePath)) == (void *)0)
    {
        UI_postText(
            "[SYS_INFO]>>Protocol load failed: invalid path.\n");
        return;
    }

    char filePath[PROTOCOL_CATALOG_FILE_PATH_CAPACITY];
    ProtocolCatalogResult const resolveResult =
        ProtocolCatalog_resolvePath(
            &me->protocolCatalog, request->relativePath,
            filePath, sizeof(filePath));
    if (resolveResult != PROTOCOL_CATALOG_OK) {
        int const length = snprintf(
            text, sizeof(text),
            "[SYS_INFO]>>Protocol load failed: %s\n",
            ProtocolCatalog_error(&me->protocolCatalog)->detail);
        DBC_ASSERT(712, (length > 0)
                        && ((size_t)length < sizeof(text)));
        (void)length;
        UI_postText(text);
        return;
    }

    ProtocolLoadResult const loadResult =
        ProtocolDecoder_loadFile(&me->protocolDecoder, filePath);
    if (loadResult != PROTOCOL_LOAD_OK) {
        ProtocolLoadError const * const error =
            ProtocolDecoder_error(&me->protocolDecoder);
        int const length = snprintf(
            text, sizeof(text),
            "[SYS_INFO]>>Protocol load failed: %s\n",
            error->detail);
        DBC_ASSERT(713, (length > 0)
                        && ((size_t)length < sizeof(text)));
        (void)length;
        UI_postText(text);
        return;
    }

    UI_postProtocolLoaded(request->relativePath);
    int const length = snprintf(
        text, sizeof(text),
        "[SYS_INFO]>>Protocol loaded: %s\n",
        request->relativePath);
    DBC_ASSERT(714, (length > 0)
                    && ((size_t)length < sizeof(text)));
    (void)length;
    UI_postText(text);
}

//============================================================================
//=== SST virtuals

static void SpMngr_init_(SpMngr * const me,
                         SST_Evt const * const e)
{
    static SST_Evt const initialProtocolRefreshEvt = {
        .sig = SPMNGR_REFRESH_PROTOCOLS_SIG,
    };
    static SST_Evt const initialPortRefreshEvt = {
        .sig = SPMNGR_REFRESH_PORTS_SIG,
    };

    DBC_REQUIRE(100, me != (SpMngr *)0);
    (void)e;

    HdlcParser_init(&me->hdlcParser);
    SM_Hsm_init_(&me->hsm, (SM_InitHandler)SpMngr_TOP_initial_);

    // Queue initial business intents only after the HSM reaches its stable
    // leaf state. Future UI refresh commands reuse these same event paths.
    UI_postText(
        "[SYS_INFO]>>Requesting initial protocol catalog refresh.\n");
    SST_Task_post(&me->super, &initialProtocolRefreshEvt);
    UI_postText(
        "[SYS_INFO]>>Requesting initial serial port refresh.\n");
    SST_Task_post(&me->super, &initialPortRefreshEvt);
}

static void SpMngr_dispatch_(SpMngr * const me,
                             SST_Evt const * const e)
{
    DBC_REQUIRE(200, me != (SpMngr *)0);
    DBC_REQUIRE(201, e != (SST_Evt const *)0);

    SM_Hsm_dispatch_(&me->hsm, e);
}

//============================================================================
//=== Constructor

void SpMngr_ctor(void) {
    SpMngr * const me = &SpMngr_inst_;

    HdlcParser_ctor(&me->hdlcParser,
                    &SpMngr_onHdlcFrame_, me);
    ProtocolCatalog_ctor(&me->protocolCatalog);
    ProtocolDecoder_ctor(&me->protocolDecoder);
    ProtocolFrameFormatter_ctor(&me->protocolFrameFormatter);
    SST_Task_ctor(&me->super,
                  (SST_Handler)&SpMngr_init_,
                  (SST_Handler)&SpMngr_dispatch_);
}
