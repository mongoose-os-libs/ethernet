/*
 * Copyright 2019 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ifaddrs.h>
#include <netdb.h>

#include "ubuntu_eth.h"

#include "mgos_hal.h"
#include "mgos_mongoose.h"
#include "mgos_net_hal.h"
#include "ubuntu.h"
#include "ubuntu_ipc.h"

bool ubuntu_get_default_gateway(char *dev, size_t devlen,
                                struct sockaddr_in *gw) {
  int fd;
  FILE *f = NULL;
  char line[100], *p, *c, *g, *saveptr;
  bool ret = false;

  if (!dev || !gw) {
    return false;
  }

  fd = ubuntu_ipc_open("/proc/net/route", O_RDONLY);
  if (fd < 0) {
    LOG(LL_ERROR, ("Could not open /proc/net/route"));
    return false;
  }
  f = fdopen(fd, "r");
  if (!f) {
    return false;
  }

  while (fgets(line, 100, f)) {
    p = strtok_r(line, " \t", &saveptr);
    c = strtok_r(NULL, " \t", &saveptr);
    g = strtok_r(NULL, " \t", &saveptr);

    if (p != NULL && c != NULL) {
      if (strcmp(c, "00000000") == 0) {
        strncpy(dev, p, devlen - 1);
        dev[devlen - 1] = 0;
        if (g) {
          char *pEnd;
          int ng = strtol(g, &pEnd, 16);
          gw->sin_addr.s_addr = ng;
          gw->sin_port = 0;
          gw->sin_family = AF_INET;
          ret = true;
        }
        break;
      }
    }
  }

  fclose(f);
  close(fd);
  return ret;
}

// Will always return the IP address which has a default gateway attached,
// regardless of 'if_instance'.
bool mgos_eth_dev_get_ip_info(int if_instance,
                              struct mgos_net_ip_info *ip_info) {
  struct ifaddrs *ifaddr, *ifa;
  char gw_dev[64];
  struct sockaddr_in gw;

  if (ip_info == NULL) return false;

  memset(ip_info, 0, sizeof(*ip_info));

  if (!ubuntu_get_default_gateway(gw_dev, sizeof(gw_dev), &gw)) {
    memcpy(gw_dev, "lo", 3);
  }

  LOG(LL_INFO, ("External interface: %s", gw_dev));

  if (getifaddrs(&ifaddr) == -1) {
    LOG(LL_ERROR, ("Cannot get interfaces"));
    return false;
  }

  bool found = false;
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    char host[NI_MAXHOST], netmask[NI_MAXHOST], gateway[NI_MAXHOST];
    if (ifa->ifa_addr == NULL || strcmp(gw_dev, ifa->ifa_name) != 0) {
      continue;
    }
    if (0 != getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
                         NI_MAXHOST, NULL, 0, NI_NUMERICHOST)) {
      continue;
    }
    if (0 != getnameinfo(ifa->ifa_netmask, sizeof(struct sockaddr_in), netmask,
                         NI_MAXHOST, NULL, 0, NI_NUMERICHOST)) {
      continue;
    }
    memcpy((void *) &ip_info->ip, (void *) ifa->ifa_addr,
           sizeof(struct sockaddr_in));
    memcpy((void *) &ip_info->netmask, (void *) ifa->ifa_netmask,
           sizeof(struct sockaddr_in));
    if (getnameinfo((const struct sockaddr *) &gw, sizeof(gw), gateway,
                    NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
      memcpy((void *) &ip_info->gw, (void *) &gw, sizeof(gw));
    }
    found = true;
    break;
  }

  freeifaddrs(ifaddr);

  if (!found) {
    LOG(LL_ERROR, ("Failed to get interface configuration"));
  }

  return found;

  (void) if_instance;
}

bool mgos_ethernet_init(void) {
  return true;
}
