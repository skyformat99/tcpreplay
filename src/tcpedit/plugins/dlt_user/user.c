/* $Id:$ */

/*
 * Copyright (c) 2006-2007 Aaron Turner.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright owners nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dlt_plugins-int.h"
#include "dlt_utils.h"
#include "user.h"
#include "tcpedit.h"
#include "common.h"
#include "tcpr.h"

static char dlt_name[] = "user";
static char __attribute__((unused)) dlt_prefix[] = "user";
static u_int16_t dlt_value = DLT_USER0;

/*
 * Function to register ourselves.  This function is always called, regardless
 * of what DLT types are being used, so it shouldn't be allocating extra buffers
 * or anything like that (use the dlt_user_init() function below for that).
 * Tasks:
 * - Create a new plugin struct
 * - Fill out the provides/requires bit masks.  Note:  Only specify which fields are
 *   actually in the header.
 * - Add the plugin to the context's plugin chain
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_user_register(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    assert(ctx);

    /* create  a new plugin structure */
    plugin = tcpedit_dlt_newplugin();

    /* FIXME: set what we provide & require */
    plugin->provides += PLUGIN_MASK_PROTO + PLUGIN_MASK_SRCADDR + PLUGIN_MASK_DSTADDR;
    plugin->requires += 0; // PLUGIN_MASK_PROTO + PLUGIN_MASK_SRCADDR + PLUGIN_MASK_DSTADDR;

     /* what is our DLT value? */
    plugin->dlt = dlt_value;

    /* set the prefix name of our plugin.  This is also used as the prefix for our options */
    plugin->name = safe_strdup(dlt_name);

    /* 
     * Point to our functions, note, you need a function for EVERY method.  
     * Even if it is only an empty stub returning success.
     */
    plugin->plugin_init = dlt_user_init;
    plugin->plugin_cleanup = dlt_user_cleanup;
    plugin->plugin_parse_opts = dlt_user_parse_opts;
    plugin->plugin_decode = dlt_user_decode;
    plugin->plugin_encode = dlt_user_encode;
    plugin->plugin_proto = dlt_user_proto;
    plugin->plugin_l2addr_type = dlt_user_l2addr_type;
    plugin->plugin_l2len = dlt_user_l2len;
    plugin->plugin_get_layer3 = dlt_user_get_layer3;
    plugin->plugin_merge_layer3 = dlt_user_merge_layer3;

    /* add it to the available plugin list */
    return tcpedit_dlt_addplugin(ctx, plugin);
}

 
/*
 * Initializer function.  This function is called only once, if and only iif
 * this plugin will be utilized.  Remember, if you need to keep track of any state, 
 * store it in your plugin->config, not a global!
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_user_init(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    user_config_t *config;
    assert(ctx);
    
    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to initalize unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }
    
    /* allocate memory for our deocde extra data */
    if (sizeof(user_extra_t) > 0)
        ctx->decoded_extra = safe_malloc(sizeof(user_extra_t));

    /* allocate memory for our config data */
    if (sizeof(user_config_t) > 0)
        plugin->config = safe_malloc(sizeof(user_config_t));
    
    config = (user_config_t *)plugin->config;
    /* do nothing */
    return TCPEDIT_OK; /* success */
}

/*
 * Since this is used in a library, we should manually clean up after ourselves
 * Unless you allocated some memory in dlt_user_init(), this is just an stub.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_user_cleanup(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    assert(ctx);

    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to cleanup unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }

    /* FIXME: make this function do something if necessary */
    if (ctx->decoded_extra != NULL) {
        free(ctx->decoded_extra);
        ctx->decoded_extra = NULL;
    }
        
    if (plugin->config != NULL) {
        free(plugin->config);
        plugin->config = NULL;
    }

    return TCPEDIT_OK; /* success */
}

/*
 * This is where you should define all your AutoGen AutoOpts option parsing.
 * Any user specified option should have it's bit turned on in the 'provides'
 * bit mask.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_user_parse_opts(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    user_config_t *config;
    assert(ctx);

    plugin = tcpedit_dlt_getplugin(ctx, dlt_value);
    config = plugin->config;

    /* --user-dlt will override the output DLT type, otherwise we'll user DLT_USER0 */
    if (HAVE_OPT(USER_DLT)) {
        ctx->dlt = OPT_VALUE_USER_DLT;
    } else {
        tcpedit_seterr(ctx->tcpedit, "%s", "Must set --user-dlt for --dlt=user");
        return TCPEDIT_ERROR;
    }

    /* --user-dlink */
    if (HAVE_OPT(USER_DLINK)) {
        int  ct = STACKCT_OPT(USER_DLINK);
        char **list = STACKLST_OPT(USER_DLINK);
        int first = 1;
        
        do  {
            char *p = *list++;
            if (first) {
                config->length = read_hexstring(p, config->l2server, USER_L2MAXLEN);
                memcpy(config->l2client, config->l2server, config->length);
            } else {
                if (config->length != read_hexstring(p, config->l2client, USER_L2MAXLEN)) {
                    tcpedit_seterr(ctx->tcpedit, "%s",
                            "both --dlink's must contain the same number of bytes");
                    return TCPEDIT_ERROR;
                }
            }

            first = 0;
        } while (--ct > 0);
    }
    
    return TCPEDIT_OK; /* success */
}

/* you should never decode packets with this plugin! */
int 
dlt_user_decode(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    assert(ctx);
    assert(packet);
    assert(pktlen);

    tcpedit_seterr(ctx->tcpedit, "%s", "DLT_USER0 plugin does not support packet decoding");
    return TCPEDIT_ERROR;
}

/*
 * Function to encode the layer 2 header back into the packet.
 * Returns: total packet len or TCPEDIT_ERROR
 */
int 
dlt_user_encode(tcpeditdlt_t *ctx, u_char **packet_ex, int pktlen, tcpr_dir_t dir)
{
    u_char *packet;
    int l2len;
    user_config_t *config;
    tcpeditdlt_plugin_t *plugin;
    u_char tmpbuff[MAXPACKET];

    assert(ctx);
    assert(packet_ex);
    assert(pktlen > 0);
    
    packet = *packet_ex;
    assert(packet);
    
    plugin = tcpedit_dlt_getplugin(ctx, dlt_value);
    config = plugin->config;

    /* Make room for our new l2 header if l2len != config->length */
    if (l2len > config->length) {
        memmove(packet + config->length, packet + ctx->l2len, pktlen - ctx->l2len);
    } else if (l2len < config->length) {
        memcpy(tmpbuff, packet, pktlen);
        memcpy(packet + config->length, (tmpbuff + ctx->l2len), pktlen - ctx->l2len);
    }

    /* update the total packet length */
    pktlen += config->length - ctx->l2len;
    
    if (dir == TCPR_DIR_C2S) {
        memcpy(packet, config->l2client, config->length);
    } else if (dir == TCPR_DIR_S2C) {
        memcpy(packet, config->l2server, config->length);
    } else {
        tcpedit_seterr(ctx->tcpedit, "%s", "Encoders only support C2S or C2S!");
        return TCPEDIT_ERROR;
    }
 
    
    return pktlen; /* success */
}

/*
 * Function returns the Layer 3 protocol type of the given packet, or TCPEDIT_ERROR on error
 */
int 
dlt_user_proto(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    assert(ctx);
    assert(packet);
    assert(pktlen);

    /* calling this for DLT_USER0 is broken */
    tcpedit_seterr(ctx->tcpedit, "%s", "Nonsensical calling of dlt_user_proto()");
    return TCPEDIT_ERROR; 
}

/*
 * Function returns a pointer to the layer 3 protocol header or NULL on error
 */
u_char *
dlt_user_get_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen)
{
    int l2len;
    assert(ctx);
    assert(packet);

    /* FIXME: Is there anything else we need to do?? */
    l2len = dlt_user_l2len(ctx, packet, pktlen);

    assert(pktlen >= l2len);

    return tcpedit_dlt_l3data_copy(ctx, packet, pktlen, l2len);
}

/*
 * function merges the packet (containing L2 and old L3) with the l3data buffer
 * containing the new l3 data.  Note, if L2 % 4 == 0, then they're pointing to the
 * same buffer, otherwise there was a memcpy involved on strictly aligned architectures
 * like SPARC
 */
u_char *
dlt_user_merge_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen, u_char *l3data)
{
    int l2len;
    assert(ctx);
    assert(packet);
    assert(l3data);
    
    /* FIXME: Is there anything else we need to do?? */
    l2len = dlt_user_l2len(ctx, packet, pktlen);
    
    assert(pktlen >= l2len);
    
    return tcpedit_dlt_l3data_merge(ctx, packet, pktlen, l3data, l2len);
}

/* 
 * return the length of the L2 header of the current packet
 */
int
dlt_user_l2len(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    tcpeditdlt_plugin_t *plugin;
    user_config_t *config;
    assert(ctx);
    assert(packet);
    assert(pktlen);

    plugin = tcpedit_dlt_getplugin(ctx, dlt_value);
    config = plugin->config;

    return config->length;
}


tcpeditdlt_l2addr_type_t 
dlt_user_l2addr_type(void)
{
    return NONE;
}
