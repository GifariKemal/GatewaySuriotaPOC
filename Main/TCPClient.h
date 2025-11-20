#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <WiFi.h>
#include <Ethernet.h>
#include "NetworkManager.h"

// Generic TCP Client wrapper that auto-selects WiFiClient or EthernetClient
// based on active network mode
class TCPClient
{
private:
  WiFiClient *wifiClient;
  EthernetClient *ethClient;
  bool useEthernet;

public:
  TCPClient()
  {
    wifiClient = nullptr;
    ethClient = nullptr;

    // Detect active network mode
    NetworkMgr *netMgr = NetworkMgr::getInstance();
    if (netMgr)
    {
      String mode = netMgr->getCurrentMode();
      useEthernet = (mode == "ETH");
    }
    else
    {
      useEthernet = false; // Fallback to WiFi
    }

    // Create appropriate client
    if (useEthernet)
    {
      ethClient = new EthernetClient();
    }
    else
    {
      wifiClient = new WiFiClient();
    }
  }

  ~TCPClient()
  {
    if (wifiClient)
    {
      delete wifiClient;
      wifiClient = nullptr;
    }
    if (ethClient)
    {
      delete ethClient;
      ethClient = nullptr;
    }
  }

  void setTimeout(uint32_t milliseconds)
  {
    if (useEthernet && ethClient)
    {
      ethClient->setTimeout(milliseconds);
    }
    else if (wifiClient)
    {
      wifiClient->setTimeout(milliseconds);
    }
  }

  int connect(const char *host, uint16_t port)
  {
    if (useEthernet && ethClient)
    {
      return ethClient->connect(host, port);
    }
    else if (wifiClient)
    {
      return wifiClient->connect(host, port);
    }
    return 0;
  }

  int connect(IPAddress ip, uint16_t port)
  {
    if (useEthernet && ethClient)
    {
      return ethClient->connect(ip, port);
    }
    else if (wifiClient)
    {
      return wifiClient->connect(ip, port);
    }
    return 0;
  }

  size_t write(const uint8_t *buf, size_t size)
  {
    if (useEthernet && ethClient)
    {
      return ethClient->write(buf, size);
    }
    else if (wifiClient)
    {
      return wifiClient->write(buf, size);
    }
    return 0;
  }

  int available()
  {
    if (useEthernet && ethClient)
    {
      return ethClient->available();
    }
    else if (wifiClient)
    {
      return wifiClient->available();
    }
    return 0;
  }

  int read(uint8_t *buf, size_t size)
  {
    if (useEthernet && ethClient)
    {
      return ethClient->read(buf, size);
    }
    else if (wifiClient)
    {
      return wifiClient->read(buf, size);
    }
    return -1;
  }

  size_t readBytes(uint8_t *buffer, size_t length)
  {
    if (useEthernet && ethClient)
    {
      return ethClient->readBytes(buffer, length);
    }
    else if (wifiClient)
    {
      return wifiClient->readBytes(buffer, length);
    }
    return 0;
  }

  void stop()
  {
    if (useEthernet && ethClient)
    {
      ethClient->stop();
    }
    else if (wifiClient)
    {
      wifiClient->stop();
    }
  }

  uint8_t connected()
  {
    if (useEthernet && ethClient)
    {
      return ethClient->connected();
    }
    else if (wifiClient)
    {
      return wifiClient->connected();
    }
    return 0;
  }

  operator bool()
  {
    return connected();
  }

  // Utility method to check which client is being used
  bool isUsingEthernet() const
  {
    return useEthernet;
  }
};

#endif // TCP_CLIENT_H
