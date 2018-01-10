/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_MOS_LIBS_ETHERNET_SRC_STM32_STM32_ETH_NETIF_H_
#define CS_MOS_LIBS_ETHERNET_SRC_STM32_STM32_ETH_NETIF_H_

#include "lwip/err.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t stm32_eth_netif_init(struct netif *netif);

#ifdef __cplusplus
}
#endif
#endif /* CS_MOS_LIBS_ETHERNET_SRC_STM32_STM32_ETH_NETIF_H_ */
