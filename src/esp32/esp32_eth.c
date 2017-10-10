/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

#include <stdbool.h>

#include "esp_eth.h"
#include "eth_phy/phy_lan8720.h"
#include "eth_phy/phy_tlk110.h"
#include "tcpip_adapter.h"

#include "mgos_net.h"
#include "mgos_net_hal.h"
#include "mgos_sys_config.h"

static void eth_config_pins(void) {
  phy_rmii_configure_data_interface_pins();
  phy_rmii_smi_configure_pins(mgos_sys_config_get_eth_mdc_gpio(),
                              mgos_sys_config_get_eth_mdio_gpio());
}

bool mgos_ethernet_init(void) {
  if (!mgos_sys_config_get_eth_enable()) return true;

  eth_config_t config;
  const char *phy_model;
#if defined(MGOS_ETH_PHY_LAN87x0)
  phy_model = "LAN87x0";
  config = phy_lan8720_default_ethernet_config;
#elif defined(MGOS_ETH_PHY_TLK110)
  phy_model = "TLK110";
  config = phy_tlk110_default_ethernet_config;
#else
#error Unknown/unspecified PHY model
#endif

  /* Set the PHY address in the example configuration */
  config.phy_addr = mgos_sys_config_get_eth_phy_addr();
  config.gpio_config = eth_config_pins;
  config.tcpip_input = tcpip_adapter_eth_input;

  LOG(LL_INFO,
      ("Eth init: %s PHY @ %d", phy_model, mgos_sys_config_get_eth_phy_addr()));
  esp_err_t ret = esp_eth_init(&config);
  if (ret == ESP_OK) {
    esp_eth_enable();
  } else {
    LOG(LL_ERROR, ("Ethernet init failed: %d", ret));
    return false;
  }

  return true;
}

bool mgos_eth_dev_get_ip_info(int if_instance,
                              struct mgos_net_ip_info *ip_info) {
  tcpip_adapter_ip_info_t info;
  if (if_instance != 0 ||
      tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &info) != ESP_OK ||
      info.ip.addr == 0) {
    return false;
  }
  ip_info->ip.sin_addr.s_addr = info.ip.addr;
  ip_info->netmask.sin_addr.s_addr = info.netmask.addr;
  ip_info->gw.sin_addr.s_addr = info.gw.addr;
  return true;
}
