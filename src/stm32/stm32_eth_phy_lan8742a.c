/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

#include "stm32_eth_phy.h"

#include <string.h>

#if STM32_ETH_PHY == STM32_ETH_PHY_LAN8742A

#include "stm32f7xx_hal.h"

#define BIT(n) (1UL << n)

#define LAN8742A_BCR 0x0
#define LAN8742A_BCR_SPEED_100M BIT(13)
#define LAN8742A_BCR_AUTONEG_ON BIT(12)
#define LAN8742A_BCR_AUTONEG_RESTART BIT(9)
#define LAN8742A_BCR_DUPLEX_FULL BIT(8)

#define LAN8742A_BSR 0x1
#define LAN8742A_BSR_AUTONEG_CMPL BIT(5)
#define LAN8742A_BSR_LINK_UP BIT(2)

#define LAN8742A_SCR 0x1f
#define LAN8742A_SCR_AUTONEG_CMPL BIT(12)
#define LAN8742A_SCR_SI_DUPLEX_FULL BIT(4)
#define LAN8742A_SCR_SI_SPEED_100M BIT(3)

#define LAN8742A_CONFIG_DELAY_MS 500

const char *stm32_eth_phy_name(void) {
  return "LAN8742a";
}

bool stm32_eth_phy_init(ETH_HandleTypeDef *heth) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /** RMII GPIO Configuration
  PC1     ------> ETH_MDC
  PA1     ------> ETH_REF_CLK
  PA2     ------> ETH_MDIO
  PA7     ------> ETH_CRS_DV
  PC4     ------> ETH_RXD0
  PC5     ------> ETH_RXD1
  PB13     ------> ETH_TXD1
  PG11     ------> ETH_TX_EN
  PG13     ------> ETH_TXD0
  */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

#define RMII_MDC_Pin GPIO_PIN_1
#define RMII_MDC_GPIO_Port GPIOC
#define RMII_MDIO_Pin GPIO_PIN_2
#define RMII_MDIO_GPIO_Port GPIOA
#define RMII_RXD0_Pin GPIO_PIN_4
#define RMII_RXD0_GPIO_Port GPIOC
#define RMII_RXD1_Pin GPIO_PIN_5
#define RMII_RXD1_GPIO_Port GPIOC
#define RMII_TXD1_Pin GPIO_PIN_13
#define RMII_TXD1_GPIO_Port GPIOB
#define RMII_TX_EN_Pin GPIO_PIN_11
#define RMII_TX_EN_GPIO_Port GPIOG
#define RMII_TXD0_Pin GPIO_PIN_13
#define RMII_TXD0_GPIO_Port GPIOG

  GPIO_InitStruct.Pin = RMII_MDC_Pin | RMII_RXD0_Pin | RMII_RXD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1 | RMII_MDIO_Pin | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RMII_TXD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(RMII_TXD1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RMII_TX_EN_Pin | RMII_TXD0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  heth->Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;

  return true;
}

bool stm32_eth_phy_set_opts(ETH_HandleTypeDef *heth,
                            const struct mgos_eth_phy_opts *opts) {
  uint32_t bcr = 0;
  if (HAL_ETH_ReadPHYRegister(heth, LAN8742A_BCR, &bcr) != HAL_OK) return false;
  uint32_t old_bcr = bcr;

  switch (opts->speed) {
    case MGOS_ETH_SPEED_10M:
      bcr &= ~LAN8742A_BCR_SPEED_100M;
      break;
    case MGOS_ETH_SPEED_100M:
      bcr |= LAN8742A_BCR_SPEED_100M;
      break;
    default:
      return false;
  }

  if (opts->duplex == MGOS_ETH_DUPLEX_FULL) {
    bcr |= LAN8742A_BCR_DUPLEX_FULL;
  } else {
    bcr &= ~LAN8742A_BCR_DUPLEX_FULL;
  }

  if (opts->autoneg_on) {
    bcr |= LAN8742A_BCR_AUTONEG_ON;
  } else {
    bcr &= ~LAN8742A_BCR_AUTONEG_ON;
  }

  if (bcr == old_bcr) return true;

  if (HAL_ETH_WritePHYRegister(heth, LAN8742A_BCR, bcr) != HAL_OK) {
    return false;
  }

  HAL_Delay(LAN8742A_CONFIG_DELAY_MS);

  return true;
}

bool stm32_eth_phy_start_autoneg(ETH_HandleTypeDef *heth) {
  uint32_t bcr = 0;
  if (HAL_ETH_ReadPHYRegister(heth, LAN8742A_BCR, &bcr) != HAL_OK) return false;
  bcr |= LAN8742A_BCR_AUTONEG_RESTART;
  return (HAL_ETH_WritePHYRegister(heth, LAN8742A_BCR, bcr) == HAL_OK);
}

bool stm32_eth_phy_get_status(ETH_HandleTypeDef *heth,
                              struct stm32_eth_phy_status *status) {
  uint32_t bsr = 0, bcr = 0, scr = 0;
  if (HAL_ETH_ReadPHYRegister(heth, LAN8742A_BSR, &bsr) != HAL_OK ||
      HAL_ETH_ReadPHYRegister(heth, LAN8742A_BCR, &bcr) != HAL_OK ||
      HAL_ETH_ReadPHYRegister(heth, LAN8742A_SCR, &scr) != HAL_OK) {
    return false;
  }
  memset(status, 0, sizeof(*status));
  status->link_up = (bsr & LAN8742A_BSR_LINK_UP) != 0;
  struct mgos_eth_phy_opts *opts = &status->opts;
  opts->autoneg_on = (bcr & LAN8742A_BCR_AUTONEG_ON) != 0;
  opts->speed = (bcr & LAN8742A_BCR_SPEED_100M ? MGOS_ETH_SPEED_100M
                                               : MGOS_ETH_SPEED_10M);
  opts->duplex = (bcr & LAN8742A_BCR_DUPLEX_FULL ? MGOS_ETH_DUPLEX_FULL
                                                 : MGOS_ETH_DUPLEX_HALF);
  struct stm32_eth_phy_autoneg_result *ar = &status->ar;
  ar->complete = (bsr & LAN8742A_SCR_AUTONEG_CMPL) != 0;
  ar->speed = (scr & LAN8742A_SCR_SI_SPEED_100M ? MGOS_ETH_SPEED_100M
                                                : MGOS_ETH_SPEED_10M);
  ar->duplex = (scr & LAN8742A_SCR_SI_DUPLEX_FULL ? MGOS_ETH_DUPLEX_FULL
                                                  : MGOS_ETH_DUPLEX_HALF);
  return true;
}

#endif /* STM32_ETH_PHY == STM32_ETH_PHY_LAN8742A */
