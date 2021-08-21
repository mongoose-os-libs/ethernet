/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>

#include "esp_eth.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "lwip/dhcp.h"
#include "lwip/netif.h"

#include "common/cs_dbg.h"

#include "mgos_eth.h"
#include "mgos_gpio.h"
#include "mgos_lwip.h"
#include "mgos_net.h"
#include "mgos_net_hal.h"
#include "mgos_sys_config.h"
#include "mgos_system.h"

#if MGOS_ETH_PHY_IP101
#define PHY_MODEL "IP101"
#define PHY_CREATE_FUNC esp_eth_phy_new_ip101
#elif MGOS_ETH_PHY_RTL8201
#define PHY_MODEL "RTL8201"
#define PHY_CREATE_FUNC esp_eth_phy_new_rtl8201
#elif MGOS_ETH_PHY_LAN87x0
#define PHY_MODEL "LAN87x0"
#define PHY_CREATE_FUNC esp_eth_phy_new_lan8720
#elif MGOS_ETH_PHY_DP83848
#define PHY_MODEL "DP83848"
#define PHY_CREATE_FUNC esp_eth_phy_new_dp83848
#else
#error Unknown/unspecified PHY model
#endif

static void esp32_eth_event_handler(void *ctx, esp_event_base_t ev_base,
                                    int32_t ev_id, void *ev_data) {
  esp_netif_t *eth_if = (esp_netif_t *) ctx;
  if (ev_base == ETH_EVENT) {
    switch (ev_id) {
      case ETHERNET_EVENT_START: {
        esp_netif_action_start(eth_if, ev_base, ev_id, ev_data);
        break;
      }
      case ETHERNET_EVENT_STOP: {
        esp_netif_action_stop(eth_if, ev_base, ev_id, ev_data);
        break;
      }
      case ETHERNET_EVENT_CONNECTED: {
        esp_netif_action_connected(eth_if, ev_base, ev_id, ev_data);
        mgos_net_dev_event_cb(MGOS_NET_IF_TYPE_ETHERNET, 0,
                              MGOS_NET_EV_CONNECTED);
        break;
      }
      case ETHERNET_EVENT_DISCONNECTED: {
        mgos_net_dev_event_cb(MGOS_NET_IF_TYPE_ETHERNET, 0,
                              MGOS_NET_EV_DISCONNECTED);
        esp_netif_action_disconnected(eth_if, ev_base, ev_id, ev_data);
        break;
      }
    }
  } else {
    mgos_net_dev_event_cb(MGOS_NET_IF_TYPE_ETHERNET, 0,
                          MGOS_NET_EV_IP_ACQUIRED);
  }
  (void) ctx;
  (void) ev_base;
  (void) ev_data;
}

bool mgos_ethernet_init(void) {
  bool res = false;

  if (!mgos_sys_config_get_eth_enable()) {
    res = true;
    goto out;
  }

  /* Create MAC */
  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  mac_config.smi_mdc_gpio_num = mgos_sys_config_get_eth_mdc_gpio();
  mac_config.smi_mdio_gpio_num = mgos_sys_config_get_eth_mdio_gpio();
  esp_eth_mac_t *mac = esp_eth_mac_new_esp32_clock_mode(
      &mac_config, (emac_clock_mode_t) mgos_sys_config_get_eth_clk_mode());

  /* Create PHY */
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  const char *phy_model = PHY_MODEL;
  phy_config.phy_addr = mgos_sys_config_get_eth_phy_addr();
  if (mgos_sys_config_get_eth_phy_pwr_gpio() != -1) {
    phy_config.reset_gpio_num = mgos_sys_config_get_eth_phy_pwr_gpio();
  }
  esp_eth_phy_t *phy = PHY_CREATE_FUNC(&phy_config);

  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

  esp_eth_handle_t eth_handle = NULL;
  esp_err_t ret = esp_eth_driver_install(&eth_config, &eth_handle);
  if (ret != ESP_OK) {
    LOG(LL_ERROR, ("Ethernet %s failed: %d", "driver init", ret));
    return false;
  }

  esp_netif_config_t eth_if_cfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t *eth_if = esp_netif_new(&eth_if_cfg);

  ret = esp_netif_attach(eth_if, esp_eth_new_netif_glue(eth_handle));
  if (ret != ESP_OK) {
    LOG(LL_ERROR, ("Ethernet %s failed: %d", "if attach", ret));
    return false;
  }

  if (mgos_sys_config_get_eth_dhcp_hostname() != NULL &&
      esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("ETH_DEF"),
                             mgos_sys_config_get_eth_dhcp_hostname()) !=
          ESP_OK) {
    LOG(LL_ERROR, ("ETH: Failed to set host name"));
    goto out;
  }

  esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                             esp32_eth_event_handler, eth_if);
  esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                             esp32_eth_event_handler, eth_if);

  uint8_t mac_addr[6];
  mac->get_addr(mac, mac_addr);

  struct sockaddr_in ip = {0}, netmask = {0}, gw = {0};
  if (!mgos_eth_get_static_ip_config(&ip, &netmask, &gw)) {
    goto out;
  }
  esp_netif_ip_info_t static_ip = {
      .ip.addr = ip.sin_addr.s_addr,
      .netmask.addr = netmask.sin_addr.s_addr,
      .gw.addr = gw.sin_addr.s_addr,
  };

  bool is_dhcp =
      (static_ip.ip.addr == IPADDR_ANY || static_ip.netmask.addr == IPADDR_ANY);

  LOG(LL_INFO,
      ("ETH: MAC %02x:%02x:%02x:%02x:%02x:%02x; PHY: %s @ %d%s", mac_addr[0],
       mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
       phy_model, phy_config.phy_addr, (is_dhcp ? "; IP: DHCP" : "")));
  if (!is_dhcp) {
    char ips[16], nms[16], gws[16];
    ip4addr_ntoa_r((ip4_addr_t *) &static_ip.ip, ips, sizeof(ips));
    ip4addr_ntoa_r((ip4_addr_t *) &static_ip.netmask, nms, sizeof(nms));
    ip4addr_ntoa_r((ip4_addr_t *) &static_ip.gw, gws, sizeof(gws));
    LOG(LL_INFO, ("ETH: IP %s/%s, GW %s", ips, nms, gws));
    esp_netif_dhcpc_stop(eth_if);
    if ((ret = esp_netif_set_ip_info(eth_if, &static_ip)) != ESP_OK) {
      LOG(LL_ERROR, ("ETH: Failed to set ip info: %d", ret));
      goto out;
    }
  }

  ret = esp_eth_start(eth_handle);
  if (ret != ESP_OK) {
    LOG(LL_ERROR, ("Ethernet %s failed: %d", "start", ret));
    return false;
  }

  res = true;

out:
  return res;
}

bool mgos_eth_dev_get_ip_info(int if_instance,
                              struct mgos_net_ip_info *ip_info) {
  esp_netif_t *esp_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
  if (esp_netif == NULL) return false;
  struct netif *nif = esp_netif_get_lwip_netif(esp_netif);
  return mgos_lwip_if_get_ip_info(nif, mgos_sys_config_get_eth_nameserver(),
                                  ip_info);
}
