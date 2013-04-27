/*
 * PixelPusher protocol implementation for LED matrix
 */
#include <arpa/inet.h>
#include <linux/netdevice.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "led-matrix.h"
#include "thread.h"
#include "universal-discovery-protocol.h"

#define PUSHER_NETWORK_INTERFACE "eth0"
#define PIXELPUSHER_DISCOVERYPORT 7331
#define PIXELPUSHER_LISTENPORT 9897

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
    packet_.p.pixelpusher.update_period = update_micros;
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
    addr.sin_port = htons(PIXELPUSHER_DISCOVERYPORT);

    fprintf(stderr, "Starting discovery beacon; to port %d\n",
            PIXELPUSHER_DISCOVERYPORT);
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

// Push framebuffer updates.
class DisplayUpdater : public ShutdownThread {
public:
  DisplayUpdater(RGBMatrix *m) : matrix_(m) {}

  void Run() {
    while (running_) {
      matrix_->UpdateScreen();
    }
  }

private:
  RGBMatrix *const matrix_;
};

class PacketReceiver : public ShutdownThread {
public:
  PacketReceiver(RGBMatrix *m, Beacon *beacon) : matrix_(m), beacon_(beacon) {
  }

  virtual void Run() {
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
    addr.sin_port = htons(PIXELPUSHER_LISTENPORT);
    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
      perror("bind");
      exit(1);
    }
    char buf[1500]; // max 1460
    fprintf(stderr, "Listening for pixels on port %d\n", PIXELPUSHER_LISTENPORT);
    while (running_) {
      ssize_t buffer_bytes = recvfrom(s, buf, sizeof(buf), 0, NULL, 0);
      const int64_t start_time = CurrentTimeMicros();
      if (buffer_bytes < 0) {
        perror("receive problem");
        continue;
      }
      if (buffer_bytes <= 4) {
        fprintf(stderr, "weird, no sequence number ? Got %d bytes\n",
                buffer_bytes);
      }

      const char *buf_pos = buf;

      uint32_t sequence;
      memcpy(&sequence, buf_pos, sizeof(sequence));
      buffer_bytes -= 4;
      buf_pos += 4;

      if (buffer_bytes % strip_data_len != 0) {
        fprintf(stderr, "Expecting multiple of {1 + (rgb)*%d} = %d, "
                "but got %d bytes (leftover: %d)\n", matrix_->width(),
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
  }
  
private:
  RGBMatrix *const matrix_;
  Beacon *const beacon_;
};

int main(int argc, char *argv[]) {
  // Init RGB matrix
  GPIO io;
  if (!io.Init())
    return 1;
  RGBMatrix matrix(&io);

  // Init PixelPusher protocol
  DiscoveryPacketHeader header;
  memset(&header, 0, sizeof(header));

  if (!DetermineNetwork(PUSHER_NETWORK_INTERFACE, &header)) {
    return 1;
  }
  header.device_type = PIXELPUSHER;
  header.protocol_version = 1;  // ?
  header.vendor_id = 3;  // h.zeller@acm.org
  header.product_id = 0;
  header.link_speed = 10000000;  // 10MBit

  PixelPusher pixel_pusher;
  memset(&pixel_pusher, 0, sizeof(pixel_pusher));
  pixel_pusher.strips_attached = matrix.height();
  pixel_pusher.pixels_per_strip = matrix.width();
  pixel_pusher.max_strips_per_packet
    = (1460 - 4 /* sequence */) / (1 + 3 * pixel_pusher.pixels_per_strip);
  pixel_pusher.power_total = 1;         // ?
  pixel_pusher.update_period = 1000;    // this is a lie.
  pixel_pusher.controller_ordinal = 0;  // make configurable.
  pixel_pusher.group_ordinal = 0;       // make configurable.

  // Create our threads.
  Beacon *discovery_beacon = new Beacon(header, pixel_pusher);
  PacketReceiver *receiver = new PacketReceiver(&matrix, discovery_beacon);
  DisplayUpdater *updater = new DisplayUpdater(&matrix);

  receiver->Start(1);         // fairly low priority
  discovery_beacon->Start(5); // This should accurately send updates.
  updater->Start(10);         // High prio: PWM timing.

  printf("Press <RETURN> to shut down.\n");
  getchar();  // for now, run until <RETURN>
  printf("shutting down\n");

  // Stopping threads and wait for them to join.
  delete updater;

  // Final thing before exit: clear screen and update once, so that
  // we don't have random pixels burn without refresh.
  matrix.ClearScreen();
  matrix.UpdateScreen();

  delete discovery_beacon;
  //delete receiver;    // recvfrom() blocking; don't care for now.

  return 0;
}
