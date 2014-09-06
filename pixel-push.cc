//  PixelPusher protocol implementation for LED matrix
//
//  Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//    
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <algorithm>
#include <arpa/inet.h>
#include <assert.h>
#include <linux/netdevice.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "led-matrix.h"
#include "thread.h"
#include "universal-discovery-protocol.h"

static const char kNetworkInterface[] = "eth0";
static const uint16_t kPixelPusherDiscoveryPort = 7331;
static const uint16_t kPixelPusherListenPort = 9897;

// The maximum packet size we accept.
// Typicall, the PixelPusher network will attempt to send smaller,
// non-fragmenting packets of size 1460; however, we would accept up to
// the UDP packet size.
static const int kMaxUDPPacketSize = 65507;

// Say we want 60Hz update and 9 packets per frame (7 strips / packet), we
// don't really need more update rate than this.
static const uint32_t kMinUpdatePeriodUSec = 16666 / 9;

// Mapping of our 4x 32x32 display to a 64x64 display.
// [>] [>]
//         v
// [<] [<]
class LargeSquare64x64Canvas : public Canvas {
public:
  // This class takes over ownership of the delegatee.
  LargeSquare64x64Canvas(Canvas *delegatee) : delegatee_(delegatee) {
    // Our assumptions of the underlying geometry:
    assert(delegatee->height() == 32);
    assert(delegatee->width() == 128);
  }
  virtual ~LargeSquare64x64Canvas() { delete delegatee_; }

  virtual void ClearScreen() { delegatee_->ClearScreen(); }
  virtual void FillScreen(uint8_t red, uint8_t green, uint8_t blue) {
    delegatee_->FillScreen(red, green, blue);
  }
  virtual int width() const { return 64; }
  virtual int height() const { return 64; }
  virtual void SetPixel(int x, int y,
                        uint8_t red, uint8_t green, uint8_t blue) {
    // We have up to column 64 one direction, then folding around. Lets map
    if (y > 31) {
      x = 127 - x;
      y = 63 - y;
    }
    delegatee_->SetPixel(x, y, red, green, blue);
  }

private:
  Canvas *delegatee_;
};

int64_t CurrentTimeMicros() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  int64_t result = tv.tv_sec;
  return result * 1000000 + tv.tv_usec;
}

// Given the name of the interface, such as "eth0", fill the IP address and
// broadcast address into "header"
// Some socket and ioctl nastiness.
bool DetermineNetwork(const char *interface, DiscoveryPacketHeader *header) {
  int s;
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return false;
  }

  bool success = true;

  {
    // Get mac address for given interface name.
    struct ifreq mac_addr_query;
    strcpy(mac_addr_query.ifr_name, interface);
    if (ioctl(s, SIOCGIFHWADDR, &mac_addr_query) == 0) {
      memcpy(header->mac_address,  mac_addr_query.ifr_hwaddr.sa_data,
             sizeof(header->mac_address));
    } else {
      perror("getting hardware address");
      success = false;
    }
  }

  {
    struct ifreq ip_addr_query;
    strcpy(ip_addr_query.ifr_name, interface);
    if (ioctl(s, SIOCGIFADDR, &ip_addr_query) == 0) {
      struct sockaddr_in *s_in = (struct sockaddr_in *) &ip_addr_query.ifr_addr;
      memcpy(header->ip_address, &s_in->sin_addr, sizeof(header->ip_address));
    } else {
      perror("getting IP address");
      success = false;
    }
  }

  close(s);

  // Let's print what we're sending.
  char buf[256];
  inet_ntop(AF_INET, header->ip_address, buf, sizeof(buf));
  fprintf(stderr, "IP: %s; MAC: ", buf);
  for (int i = 0; i < 6; ++i) {
    fprintf(stderr, "%s%02x", (i == 0) ? "" : ":", header->mac_address[i]);
  }
  fprintf(stderr, "\n");

  return success;
}

// Threads deriving from this should exit Run() as soon as they see !running_
class ShutdownThread : public Thread {
public:
  ShutdownThread() : running_(true) {}
  virtual ~ShutdownThread() { running_ = false; }

protected:
  volatile bool running_;  // TODO: use mutex, but this is good enough for now.
};

// Broadcast every second the discovery protocol.
class Beacon : public ShutdownThread {
public:
  Beacon(const DiscoveryPacketHeader &header,
         const PixelPusher &pixel_pusher) : previous_sequence_(-1) {
    packet_.header = header;
    packet_.p.pixelpusher = pixel_pusher;
  }

  void UpdatePacketStats(uint32_t seen_sequence, uint32_t update_micros) {
    MutexLock l(&mutex_);
    packet_.p.pixelpusher.update_period = (update_micros < kMinUpdatePeriodUSec
                                           ? kMinUpdatePeriodUSec
                                           : update_micros);
    int32_t sequence_diff = seen_sequence - previous_sequence_ - 1;
    if (sequence_diff > 0)
      packet_.p.pixelpusher.delta_sequence += sequence_diff;
    previous_sequence_ = seen_sequence;
  }

  virtual void Run() {
    int s;
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("socket");
      exit(1);    // don't worry about graceful exit.
    }

    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
      perror("enable broadcast");
      exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addr.sin_port = htons(kPixelPusherDiscoveryPort);

    fprintf(stderr, "Starting PixelPusher discovery beacon "
            "broadcasting to port %d\n", kPixelPusherDiscoveryPort);
    struct timespec sleep_time = { 1, 0 };  // todo: tweak.
    while (running_) {
      DiscoveryPacket sending_header;
      {
        MutexLock l(&mutex_);
        sending_header = packet_;
        packet_.p.pixelpusher.delta_sequence = 0;
      }
      if (sendto(s, &sending_header, sizeof(sending_header), 0,
                 (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("Broadcasting problem");
      }
      nanosleep(&sleep_time, NULL);
    }
  }

private:
  Mutex mutex_;
  uint32_t previous_sequence_;
  DiscoveryPacket packet_;
};

class PacketReceiver : public ShutdownThread {
public:
  PacketReceiver(Canvas *c, Beacon *beacon) : matrix_(c), beacon_(beacon) {
  }

  virtual void Run() {
    char *packet_buffer = new char[kMaxUDPPacketSize];
    const int strip_data_len = 1 /* strip number */ + 3 * matrix_->width();
    struct Pixel {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
    };
    struct StripData {
      uint8_t strip_index;
      Pixel pixel[0];
    };
    
    int s;
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
      perror("creating listen socket");
      exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPixelPusherListenPort);
    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
      perror("bind");
      exit(1);
    }
    fprintf(stderr, "Listening for pixels pushed to port %d\n",
            kPixelPusherListenPort);
    while (running_) {
      ssize_t buffer_bytes = recvfrom(s, packet_buffer, kMaxUDPPacketSize,
                                      0, NULL, 0);
      const int64_t start_time = CurrentTimeMicros();
      if (buffer_bytes < 0) {
        perror("receive problem");
        continue;
      }
      if (buffer_bytes <= 4) {
        fprintf(stderr, "weird, no sequence number ? Got %zd bytes\n",
                buffer_bytes);
      }

      const char *buf_pos = packet_buffer;

      uint32_t sequence;
      memcpy(&sequence, buf_pos, sizeof(sequence));
      buffer_bytes -= 4;
      buf_pos += 4;

      if (buffer_bytes % strip_data_len != 0) {
        fprintf(stderr, "Expecting multiple of {1 + (rgb)*%d} = %d, "
                "but got %zd bytes (leftover: %zd)\n", matrix_->width(),
                strip_data_len, buffer_bytes, buffer_bytes % strip_data_len);
        continue;
      }

      const int received_strips = buffer_bytes / strip_data_len;
      for (int i = 0; i < received_strips; ++i) {
        StripData *data = (StripData *) buf_pos;
        // Copy into frame buffer.
        for (int x = 0; x < matrix_->width(); ++x) {
          matrix_->SetPixel(x, data->strip_index,
                            data->pixel[x].red,
                            data->pixel[x].green,
                            data->pixel[x].blue);
        }
        buf_pos += strip_data_len;
      }

      const int64_t end_time = CurrentTimeMicros();
      beacon_->UpdatePacketStats(sequence, end_time - start_time);
    }
    delete [] packet_buffer;
  }
  
private:
  Canvas *const matrix_;
  Beacon *const beacon_;
};

int main(int argc, char *argv[]) {
  // Init RGB matrix
  GPIO io;
  if (!io.Init())
    return 1;
  Canvas *canvas = new LargeSquare64x64Canvas(new RGBMatrix(&io, 32, 4));

  // Init PixelPusher protocol
  DiscoveryPacketHeader header;
  memset(&header, 0, sizeof(header));

  if (!DetermineNetwork(kNetworkInterface, &header)) {
    return 1;
  }
  header.device_type = PIXELPUSHER;
  header.protocol_version = 1;  // ?
  header.vendor_id = 3;  // h.zeller@acm.org
  header.product_id = 0;
  header.link_speed = 10000000;  // 10MBit

  PixelPusher pixel_pusher;
  memset(&pixel_pusher, 0, sizeof(pixel_pusher));
  pixel_pusher.strips_attached = canvas->height();
  pixel_pusher.pixels_per_strip = canvas->width();
  static const int kUsablePacketSize = kMaxUDPPacketSize - 4; // 4 bytes seq#
  // Whatever fits in one packet, but not more than one 'frame'.
  pixel_pusher.max_strips_per_packet
    = std::min(kUsablePacketSize / (1 + 3 * pixel_pusher.pixels_per_strip),
               (int) pixel_pusher.strips_attached);
  fprintf(stderr, "Accepting max %d strips per packet.\n",
          pixel_pusher.max_strips_per_packet);
  pixel_pusher.power_total = 1;         // ?
  pixel_pusher.update_period = 1000;    // Some initial assumption.
  pixel_pusher.controller_ordinal = 0;  // make configurable.
  pixel_pusher.group_ordinal = 0;       // make configurable.

  // Create our threads.
  Beacon *discovery_beacon = new Beacon(header, pixel_pusher);
  PacketReceiver *receiver = new PacketReceiver(canvas, discovery_beacon);

  receiver->Start(0);         // fairly low priority
  discovery_beacon->Start(5); // This should accurately send updates.

  printf("Press <RETURN> to shut down.\n");
  getchar();  // for now, run until <RETURN>
  printf("shutting down\n");

  delete discovery_beacon;
  //delete receiver;    // recvfrom() blocking; don't care for now.

  return 0;
}
