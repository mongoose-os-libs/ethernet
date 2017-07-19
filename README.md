# Ethernet support

## ESP32

ESP32 includes an Ethernet MAC and requires an external PHY, connected over RMII interface.

Two PHY models are currently supported: Microchip LAN87x0 ([LAN8710](http://ww1.microchip.com/downloads/en/DeviceDoc/00002164B.pdf) supports both MII and RMII; [LAN8720](http://ww1.microchip.com/downloads/en/DeviceDoc/00002165B.pdf) is RMII only) and [TI TLK110](http://www.ti.com/lit/ds/symlink/tlk110.pdf). PHY model selection is a compile-time option and is set [here](https://github.com/mongoose-os-libs/ethernet/blob/master/mos_esp32.yml#L5).

There is a detailed article on how to connect a PHY yourself [here](https://sautter.com/blog/ethernet-on-esp32-using-lan8720/), but it is much easier to buy a dev board which already has one, for example, [Olimex ESP32-EVB](https://www.olimex.com/Products/IoT/ESP32-EVB/open-source-hardware).

Once wired, there are anumber of settings you will need to enable Ethernet support in mOS by setting `eth.enable=true` config option.

There are also a number of settings you may want to tweak:
 * `eth.phy_addr` - PHY has SMI address selection pins, set this accordingly. `0` is the default.
 * `eth.mdc_gpio` and `eth.mdio_gpio` - most of the RMII pins are fixed, but the SMI interface pins can be adjusted if necessary. These can be any bi-directional GPIO pins. The defaults, `23` and `18` respecively.

Note: the defaults match the EVB-ESP32 board, so if you use that, you won't need to adjust anything except setting `eth.enable` to `true`.
