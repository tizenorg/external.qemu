/*
 * Emulator
 *
 * Copyright (C) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * DoHyung Hong <don.hong@samsung.com>
 * SeokYeon Hwang <syeon.hwang@samsung.com>
 * Hyunjun Son <hj79.son@samsung.com>
 * SangJin Kim <sangjin3.kim@samsung.com>
 * MunKyu Im <munkyu.im@samsung.com>
 * KiTae Kim <kt920.kim@samsung.com>
 * JinHyung Jo <jinhyung.jo@samsung.com>
 * SungMin Ha <sungmin82.ha@samsung.com>
 * JiHye Kim <jihye1128.kim@samsung.com>
 * GiWoong Kim <giwoong.kim@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 * DongKyun Yun <dk77.yun@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include <unistd.h>
#include <assert.h>
#include "dbi_parser.h"

#include "debug_ch.h"

//DEFAULT_DEBUG_CHANNEL(tizen);
MULTI_DEBUG_CHANNEL(tizen, dbi_parser);


static int  rotate_keycode[4];


/**
  * @brief      show DBI info
  * @return     void
  */
static void print_parse_info(void)
{
    int i,j;
    TRACE(" PHONE.mode_cnt =%d \n",PHONE.mode_cnt );

    for( i=0;i< PHONE.mode_cnt ; i ++) {
        for(j=0;j < PHONE.mode[i].lcd_list_cnt ; j ++ ) {
            TRACE(" PHONE.mode[%d].lcd_list[%d].id =%d \n",i,j,PHONE.mode[i].lcd_list[j].id);
            TRACE(" PHONE.mode[%d].lcd_list[%d].bitsperpixel =%d \n",i,j,PHONE.mode[i].lcd_list[j].bitsperpixel);
            TRACE(" PHONE.mode[%d].lcd_list[%d].nonstd =%d \n",i,j,PHONE.mode[i].lcd_list[j].nonstd);
            TRACE(" PHONE.mode[%d].lcd_list[%d].lcd_region.x =%d \n",i,j,PHONE.mode[i].lcd_list[j].lcd_region.x);
            TRACE(" PHONE.mode[%d].lcd_list[%d].lcd_region.y =%d \n",i,j,PHONE.mode[i].lcd_list[j].lcd_region.y);
            TRACE(" PHONE.mode[%d].lcd_list[%d].lcd_region.w =%d \n",i,j,PHONE.mode[i].lcd_list[j].lcd_region.w);
            TRACE(" PHONE.mode[%d].lcd_list[%d].lcd_region.h =%d \n",i,j,PHONE.mode[i].lcd_list[j].lcd_region.h);
        }
    }

}

static int dbi_atoi(xmlNode *node, const char *field)
{
    xmlChar *xml_str = xmlGetProp(node, (const xmlChar*)field);
    int val = 0;
    if (xml_str) {
        val = atoi((const char*) xml_str);
        xmlFree(xml_str);
    }
    return val;
}

static int dbi_load_mode_list(xmlNode *child_node, mode_list *ml, const gchar *dbi_path)
{
    xmlNode *region_child_node;
    xmlNode *region_node;
    xmlNode *event_node;
    xmlNode *event_value;
    xmlNode *image_node;
    int led_chk_cnt = 0;
    int lcd_chk_cnt = 0;
    int key_map_chk_cnt = 0;
    int event_value_chk_cnt = 0;
    int event_info_chk_cnt = 0;
    char *pSrc_Buf;
    char *pDes_Buf = NULL;
    int event_info_index = -1;

    // mode
    ml->id = dbi_atoi(child_node, "id");
    pSrc_Buf = (char *)xmlGetProp(child_node, (const xmlChar *)"name");
    ml->name = strdup(pSrc_Buf);
    xmlFree(pSrc_Buf);

    // mode children : image_list, region, lcd_list,
    // led_list, key_map_list
    for (region_node = child_node->children; region_node != NULL; region_node = region_node->next) {
        if (region_node->type != XML_ELEMENT_NODE)
            continue;

        // image_list
        if (!xmlStrcmp(region_node->name, (const xmlChar *)"image_list")) {
            for (image_node = region_node->children; image_node != NULL; image_node = image_node->next) {
                if (image_node->type != XML_ELEMENT_NODE)
                    continue;
                if (!xmlStrcmp(image_node->name, (const xmlChar *) "main_image")) {
                    pSrc_Buf = (char *) xmlNodeGetContent(image_node);

                    pDes_Buf = g_strdup_printf("%s/%s", dbi_path, pSrc_Buf);
                    TRACE(" main_image = %s \n", pDes_Buf);
                    xmlFree(pSrc_Buf);
                    ml->image_list.main_image = pDes_Buf;
                }

                else if (!xmlStrcmp(image_node->name, (const xmlChar *) "keypressed_image")) {
                    pSrc_Buf = (char *) xmlNodeGetContent(image_node);

                    pDes_Buf = g_strdup_printf("%s/%s", dbi_path, pSrc_Buf);
                    TRACE( " keypressed_image = %s \n", pDes_Buf);
                    xmlFree(pSrc_Buf);
                    ml->image_list.keypressed_image = pDes_Buf;
                }

                else if (!xmlStrcmp(image_node->name, (const xmlChar *) "led_main_image")) {
                    pSrc_Buf = (char *) xmlNodeGetContent(image_node);

                    pDes_Buf = g_strdup_printf("%s/%s", dbi_path, pSrc_Buf);
                    TRACE( " led_main_image = %s \n", pDes_Buf);
                    xmlFree(pSrc_Buf);
                    ml->image_list.led_main_image = pDes_Buf;
                }

                else if (!xmlStrcmp(image_node->name, (const xmlChar *) "led_keypressed_image")) {
                    pSrc_Buf = (char *) xmlNodeGetContent(image_node);

                    pDes_Buf = g_strdup_printf("%s/%s", dbi_path, pSrc_Buf);
                    TRACE( " led_keypressed_image = %s \n", pDes_Buf);
                    xmlFree(pSrc_Buf);
                    ml->image_list.led_keypressed_image = pDes_Buf;
                }
                /* To identify dual display split area image(middle bar) */
                else if (!xmlStrcmp(image_node->name, (const xmlChar *) "splitted_screen_image"))
                {
                    pSrc_Buf = (char *) xmlNodeGetContent(image_node);

                    pDes_Buf = g_strdup_printf("%s/%s", dbi_path, pSrc_Buf);
                    TRACE( " ^^^^^^^^^^^^^^^^^^^^^^^^ = %s \n", pDes_Buf);
                    xmlFree(pSrc_Buf);
                    ml->image_list.splitted_area_image = pDes_Buf;
                }
            }
        }
        // region check
        else if (!xmlStrcmp(region_node->name, (const xmlChar *)"region")) {
            ml->REGION.x = dbi_atoi(region_node, "left");
            ml->REGION.y = dbi_atoi(region_node, "top");
            ml->REGION.w = dbi_atoi(region_node, "width");
            ml->REGION.h = dbi_atoi(region_node, "height");
        }
        // lcd_list
        else if (!xmlStrcmp(region_node->name, (const xmlChar *)"lcd_list")) {
            for (image_node = region_node->children; image_node != NULL; image_node = image_node->next) {
                if (image_node->type != XML_ELEMENT_NODE)
                    continue;
                if (!xmlStrcmp(image_node->name, (const xmlChar *)"lcd")) {
                    lcd_list_data *ll = &ml->lcd_list[lcd_chk_cnt];
                    ll->id = dbi_atoi(image_node, "id");
                    ll->bitsperpixel = dbi_atoi(image_node, "bitsperpixel");
                    // hwjang add for overlay
                    ll->nonstd = dbi_atoi(image_node, "nonstd");
                    // node 5
                    for (region_child_node = image_node->children; region_child_node != NULL; region_child_node = region_child_node->next) {
                        if (region_node->type != XML_ELEMENT_NODE)
                            continue;
                        if (!xmlStrcmp(region_child_node->name, (const xmlChar *) "region")) {
                            xmlChar *scale = NULL;
                            ll->lcd_region.x = dbi_atoi(region_child_node, "left");
                            ll->lcd_region.y = dbi_atoi(region_child_node, "top");
                            ll->lcd_region.w = dbi_atoi(region_child_node, "width");
                            ll->lcd_region.h = dbi_atoi(region_child_node, "height");

                            scale = xmlGetProp(region_child_node, (const xmlChar *) "scale");
                            if (scale != NULL)
                                ll->lcd_region.s = atof((char *)scale);
                            else
                                ll->lcd_region.s = 1.0;

                            ll->lcd_region.split = dbi_atoi(region_child_node,  "split");
                        }
                    }
                    lcd_chk_cnt++;  // lcd list count
                    // value
                    ml->lcd_list_cnt = lcd_chk_cnt;
                }
            }
        }
        // led_list
        else if (!xmlStrcmp(region_node->name, (const xmlChar *)"led_list")) {
            for (image_node = region_node->children; image_node != NULL; image_node = image_node->next) {
                if (region_node->type != XML_ELEMENT_NODE)
                    continue;
                if (!xmlStrcmp(image_node->name, (const xmlChar *)"led")) {
                    ml->led_list[led_chk_cnt].led_region.x = dbi_atoi(image_node, "left");
                    ml->led_list[led_chk_cnt].led_region.y = dbi_atoi(image_node, "top");
                    ml->led_list[led_chk_cnt].led_region.w = dbi_atoi(image_node, "width");
                    ml->led_list[led_chk_cnt].led_region.h = dbi_atoi(image_node, "height");
                    // node 5
                    for (region_child_node = image_node->children; region_child_node != NULL; region_child_node = region_child_node->next) {
                        if (region_child_node->type == XML_ELEMENT_NODE && !xmlStrcmp(region_child_node->name, (const xmlChar *)
                                                                                      "led_color")) {
                            pSrc_Buf = (char *) xmlGetProp(region_child_node, (const xmlChar *)"id");
                            ml->led_list[led_chk_cnt].id = strdup(pSrc_Buf);
                            xmlFree(pSrc_Buf);
                            pSrc_Buf = (char *) xmlGetProp(region_child_node, (const xmlChar *)"name");
                            ml->led_list[led_chk_cnt].name = strdup(pSrc_Buf);
                            xmlFree(pSrc_Buf);
                            pSrc_Buf = (char *) xmlGetProp(region_child_node, (const xmlChar *) "imagepath");
                            ml->led_list[led_chk_cnt].imagepath = strdup(pSrc_Buf);
                            xmlFree(pSrc_Buf);
                        }
                    }
                    led_chk_cnt++;  // led list count
                    ml->led_list_cnt = led_chk_cnt;
                }
            }
        }
        // key_map_list
        else if (!xmlStrcmp(region_node->name, (const xmlChar *) "key_map_list")) {
            for (image_node = region_node->children; image_node != NULL; image_node = image_node->next) {
                if (image_node->type != XML_ELEMENT_NODE)
                    continue;
                if (!xmlStrcmp(image_node->name, (const xmlChar *) "key_map")) {
                    // node5
                    for (region_child_node = image_node->children; region_child_node != NULL; region_child_node = region_child_node->next) {
                        if (region_node->type != XML_ELEMENT_NODE)
                            continue;
                        if (!xmlStrcmp(region_child_node->name, (const xmlChar *) "region")) {
                            region *reg = &ml->key_map_list[key_map_chk_cnt].key_map_region;
                            reg->x = dbi_atoi(region_child_node, "left");
                            reg->y = dbi_atoi(region_child_node, "top");
                            reg->w = dbi_atoi(region_child_node, "width");
                            reg->h = dbi_atoi(region_child_node, "height");
                        }

                        else if (!xmlStrcmp(region_child_node->name, (const xmlChar *) "event_info")) {
                            // array[0] VKS_KEY_PRESSED
                            // array[1] VKS_KEY_RELEASED
                            char *temp = (char *) xmlGetProp(region_child_node, (const xmlChar *) "status");
                            if (strcmp(temp, KEY_PRESS) == 0) {
                                event_info_index = 0;
                            } else if (strcmp(temp, KEY_RELEASE) == 0) {
                                event_info_index = 1;
                            } else
                                return -1;
                            xmlFree(temp);

                            for (event_node = region_child_node->children; event_node != NULL; event_node = event_node->next) {
                                event_info_data *ei = &ml->key_map_list[key_map_chk_cnt].event_info[event_info_index];
                                if (region_node->type != XML_ELEMENT_NODE)
                                    continue;

                                // event_id
                                if (!xmlStrcmp(event_node->name, (const xmlChar *) "event_id")) {
                                    pSrc_Buf = (char *) xmlNodeGetContent(event_node);
                                    ei->event_id = strdup(pSrc_Buf);
                                    xmlFree(pSrc_Buf);
                                }
                                // event_value
                                else if (!xmlStrcmp(event_node->name, (const xmlChar *) "event_value")) {
                                    // node 7
                                    for (event_value = event_node->children; event_value != NULL; event_value = event_value->next) {
                                        event_value_data *ev = &ei->event_value[event_value_chk_cnt];
                                        if (region_node->type != XML_ELEMENT_NODE)
                                            continue;
                                        if (!xmlStrcmp(event_value->name, (const xmlChar *) "key_code")) {
                                            ev->key_code = atoi((char *) xmlNodeGetContent(event_value));
                                        }

                                        else if (!xmlStrcmp(event_value->name, (const xmlChar *) "key_name")) {
                                            pSrc_Buf = (char *) xmlNodeGetContent(event_value);
                                            ev->key_name = strdup(pSrc_Buf);
                                            xmlFree(pSrc_Buf);
                                            event_value_chk_cnt++;
                                            ei->event_value_cnt = event_value_chk_cnt;
                                        }
                                    }
                                }
                                // keyboard
                                else if (!xmlStrcmp(event_node->name, (const xmlChar *) "keyboard")) {
                                    pSrc_Buf = (char *) xmlNodeGetContent(event_node);
                                    ei->keyboard = strdup(pSrc_Buf);
                                    xmlFree(pSrc_Buf);
                                }
                            }
                            event_value_chk_cnt = 0;
                            event_info_chk_cnt++;
                            ml->key_map_list[key_map_chk_cnt].event_info_cnt = event_info_chk_cnt;
                        }

                        else if (!xmlStrcmp(region_child_node->name, (const xmlChar *) "tooltip")) {
                            pSrc_Buf = (char *) xmlNodeGetContent(region_child_node->children);
                            if (pSrc_Buf == NULL) {
                                ml->key_map_list[key_map_chk_cnt].tooltip = NULL;
                            } else {
                                ml->key_map_list[key_map_chk_cnt].tooltip = strdup(pSrc_Buf);
                                xmlFree(pSrc_Buf);
                            }
                        }

                    }
                    key_map_chk_cnt++;  // key_map list
                    // count
                    ml->key_map_list_cnt = key_map_chk_cnt;
                }
            }
        }
    }
    return 0;
}

static int dbi_parse_mode_selection(xmlNode *cur_node, const gchar * filename, PHONEMODELINFO * pDeviceData)
{
    xmlNode *child_node;
    int mode_chk_cnt = 0;

    gchar* dbi_path = g_path_get_dirname(filename);

    // mode_section
    for (child_node = cur_node->children; child_node != NULL; child_node = child_node->next) {
        if (child_node->type == XML_ELEMENT_NODE && !xmlStrcmp(child_node->name, (const xmlChar *)"mode")) {
            assert (mode_chk_cnt < MODE_MAX);
            dbi_load_mode_list(child_node, &pDeviceData->mode[mode_chk_cnt], dbi_path);
            mode_chk_cnt++; // mode count check
            pDeviceData->mode_cnt = mode_chk_cnt;
        }

        // Cover mode paser !!!!!
        if (child_node->type == XML_ELEMENT_NODE && !xmlStrcmp(child_node->name, (const xmlChar *)"cover_mode")) {
            assert(pDeviceData->cover_mode_cnt == 0);
            pDeviceData->cover_mode_cnt++;
            dbi_load_mode_list(child_node, &pDeviceData->cover_mode, dbi_path);
        }
    }

    g_free(dbi_path);
    return 0;
}

/**
  * @brief      parse DBI file which describes specific phone model outlook
  * @param  filename:   dbi file name
  * @param  pDeviceData: pointer to structure saving the device information
  * @return     success : 0 failure : -1
  */
int parse_dbi_file(const gchar * filename, PHONEMODELINFO * pDeviceData)
{
    // event menu
    int event_menu_cnt = 0;
    int event_list_cnt = 0;

    // malloc buffer
    char *pSrc_Buf;

    if (filename == NULL || strlen(filename) == 0 || pDeviceData == NULL) {
        WARN( "Please input skin path (%s)\n", filename);
        return -1;
    }

    // initialize
    xmlNode *cur_node, *child_node; // node1, node2
    xmlNode *region_node;       // node3

    // open XML Document
    xmlDocPtr doc;
    doc = xmlParseFile(filename);

    if (doc == NULL) {
        WARN( "can't parse file %s\n", filename);
        return -1;
    }
    // XML root
    WARN( "start to parse file %s\n", filename);

    xmlNode *root = NULL;
    root = xmlDocGetRootElement(doc);

    int run_step = 0;
    // Must have root element, a name and the name must be "PHONEMODELINFO"
    if (!root || !root->name || xmlStrcmp(root->name, (const xmlChar *)"device_info")) {
        xmlFreeDoc(doc);
        return FALSE;
    }
    // PHONEMODELINFO children
    for (cur_node = root->children; cur_node != NULL; cur_node = cur_node->next) {

        /* 1. key mode parsing (old(XOcean), new(mirage))) in 20090330 */

        if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *)"key_mode")) {

        /* 1. 1 child_mode */

            for (child_node = cur_node->children; child_node != NULL; child_node = child_node->next) {
                /* 1. 2 key */

                if (child_node->type == XML_ELEMENT_NODE && !xmlStrcmp(child_node->name, (const xmlChar *)"key")) {

                    /* 1. 3 key type parsing */

                    rotate_keycode[run_step] = dbi_atoi(child_node, "code");
                    run_step++;
                }
            }
            run_step = 0;
        }
        /* New node for enabling dual_display */
        if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *)"device_information"))
        {
            for (child_node = cur_node->children; child_node != NULL; child_node = child_node->next)
            {
                if (child_node->type == XML_ELEMENT_NODE && !xmlStrcmp(child_node->name, (const xmlChar *)"device_name"))
                {
                    strcpy(pDeviceData->model_name,(char *)xmlGetProp(child_node, (const xmlChar *)"name"));
                    if (strcmp(pDeviceData->model_name,"dual_display")==0)
                    {
                        pDeviceData->dual_display = 1;
                    }
                    else
                    {
                        pDeviceData->dual_display = 0;
                    }
                    run_step++;
                }
            }

            run_step = 0;
        }

        if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *)"mode_section")) {
            int r;
            r = dbi_parse_mode_selection(cur_node, filename, pDeviceData);
            if (r < 0)
                return r;   /* FIXME: leak memory here */
        }

        // event menu start
        if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *)"event_menu")) {
            for (child_node = cur_node->children; child_node != NULL; child_node = child_node->next) {
                if (child_node->type == XML_ELEMENT_NODE && !xmlStrcmp(child_node->name, (const xmlChar *)"menu")) {
                    event_menu_list *em = &pDeviceData->event_menu[event_menu_cnt];
                    pSrc_Buf = (char *)xmlGetProp(child_node, (const xmlChar *)"name");
                    g_strlcpy(em->name, pSrc_Buf, sizeof em->name);
                    xmlFree(pSrc_Buf);

                    // event id
                    for (region_node = child_node->children; region_node != NULL; region_node = region_node->next) {
                        if (region_node->type == XML_ELEMENT_NODE && !xmlStrcmp(region_node->name, (const xmlChar *)"event")) {
                            event_prop *ep = &em->event_list[event_list_cnt];
                            pSrc_Buf = (char *)xmlGetProp(region_node, (const xmlChar *) "id");
                            g_strlcpy(ep->event_eid, pSrc_Buf, sizeof ep->event_eid);
                            xmlFree(pSrc_Buf);

                            pSrc_Buf = (char *)xmlGetProp(region_node, (const xmlChar *) "value");
                            g_strlcpy(ep->event_evalue, pSrc_Buf, sizeof ep->event_evalue);
                            xmlFree(pSrc_Buf);

                            event_list_cnt++;
                            em->event_list_cnt = event_list_cnt;
                        }// endif
                    }// end for
                    event_menu_cnt++;
                    pDeviceData->event_menu_cnt = event_menu_cnt;
                    event_list_cnt = 0;
                }
            }// end for
        }
    }


    print_parse_info();
    // free document
    xmlFreeDoc(doc);

    // free the global variables that way have been allocated by the
    // parser.
    xmlCleanupParser();
    return 1;
}

static void free_modelist(mode_list *ml)
{
    int i;

    if (ml->name) {
        free(ml->name);
    }

    g_free(ml->image_list.main_image);
    g_free(ml->image_list.keypressed_image);
    g_free(ml->image_list.led_main_image);
    g_free(ml->image_list.led_keypressed_image);
    g_free(ml->image_list.splitted_area_image);

    for (i = 0; i < ml->led_list_cnt; i++) {
        if (ml->led_list[i].name) {
            free(ml->led_list[i].name);
        }
        if (ml->led_list[i].imagepath) {
            free(ml->led_list[i].imagepath);
        }
    }

    for (i = 0; i < ml->key_map_list_cnt; i++) {
        if (ml->key_map_list[i].tooltip) {
            free(ml->key_map_list[i].tooltip);
        }
        //todo:
    }
}

int free_dbi_file(PHONEMODELINFO *pDeviceData)
{
    int i;

    for (i = 0; i < pDeviceData->mode_cnt; i++) {
        mode_list *ml = &pDeviceData->mode[i];
        free_modelist(ml);
    }

    return 0;
}

/**
 * vim:set tabstop=4 shiftwidth=4 foldmethod=marker wrap:
 *
 */

