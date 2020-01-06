# OpenThread MQTT-SN Examples

This repository contains C/C++ examples of using MQTT-SN client on Thread network. Feature is implemented in fork of OpenThread SDK released by Google.

Before using MQTT-SN client in Thread network you must install OpenThread Border Router and MQTT-SN Gateway. For more information about building and running CLI application with MQTT-SN client see [kyberpunk/openthread](https://github.com/kyberpunk/openthread/blob/master/README.md#Trying-MQTT-SN-client-with-CLI-application-example).

## Border Router and MQTT-SN gateway setup

It is recommended to use all examples including SEARCHGW and ADVERTISE messages with IPv6 MQTT-SN gateway. Gateway is attached directly to Border Router wpan0 interface and can receive and send broadcast messages directly to Thread network. Border Router and MQTT-SN gateway may be run as Docker images. See [full guide](https://github.com/kyberpunk/openthread/blob/master/README.md) for more information about required services.

Run OpenThread border router container. No custom network is needed in this case:
```
sudo docker run -d --name otbr --sysctl "net.ipv6.conf.all.disable_ipv6=0 \
        net.ipv4.conf.all.forwarding=1 net.ipv6.conf.all.forwarding=1" -p 80:8080 \
        --dns=127.0.0.1 -v /dev/ttyACM0:/dev/ttyACM0 --privileged \
        openthread/otbr --ncp-path /dev/ttyACM0
```

If needed replace `/dev/ttyACM0` in `-v` and `--ncp-path` parameter with name under which appear NCP device in your system (`/dev/ttyS0`, `/dev/ttyUSB0` etc.).

Then run IPv6 MQTT-SN gateway:

```
sudo docker run -d --name paho --net "service:otbr" kyberpunk/paho6 \
        --broker-name <mqtt-broker> --broker-port 1883
```

Replace `<mqtt-broker>` with MQTT broker IPv4 address or hostname. Gateway container uses the same network stack as Border Router container and is able to listen to wpan0 interface.

## C API Examples

## C++ Examples
