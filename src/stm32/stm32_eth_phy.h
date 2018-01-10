/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_MOS_LIBS_ETHERNET_SRC_STM32_STM32_ETH_PHY_H_
#define CS_MOS_LIBS_ETHERNET_SRC_STM32_STM32_ETH_PHY_H_

#include <stdbool.h>

#include "mgos_eth.h"

#include "stm32f7xx_hal_conf.h"
#include "stm32f7xx_hal_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STM32_ETH_PHY_LAN8742A 1

struct stm32_eth_phy_autoneg_result {
  bool complete;
  enum mgos_eth_speed speed;
  enum mgos_eth_duplex duplex;
};

struct stm32_eth_phy_status {
  bool link_up;
  struct mgos_eth_phy_opts opts;
  struct stm32_eth_phy_autoneg_result ar;
};

const char *stm32_eth_phy_name(void);
bool stm32_eth_phy_init(ETH_HandleTypeDef *heth);
bool stm32_eth_phy_start_autoneg(ETH_HandleTypeDef *heth);
bool stm32_eth_phy_set_opts(ETH_HandleTypeDef *heth,
                            const struct mgos_eth_phy_opts *opts);
bool stm32_eth_phy_get_status(ETH_HandleTypeDef *heth,
                              struct stm32_eth_phy_status *status);

#ifdef __cplusplus
}
#endif
#endif /* CS_MOS_LIBS_ETHERNET_SRC_STM32_STM32_ETH_PHY_H_ */
