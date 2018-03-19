// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "led-matrix.h"
#include "transformer.h"
#include "pp-server.h"

using namespace rgb_matrix;

static const int kMaxUDPPacketSize = 65507;  // largest practical w/ IPv4 header
static const int kDefaultUDPPacketSize = 1460;

// Interface to our RGBMatrix
class RGBMatrixDevice : public pp::OutputDevice {
public:
  // Takes over ownerhip of matrix.
  RGBMatrixDevice(rgb_matrix::RGBMatrix *matrix)
    : matrix_(matrix),
      off_screen_(matrix_->CreateFrameCanvas()),
      on_screen_(matrix_->SwapOnVSync(NULL)),
      draw_canvas_(NULL) {
  }

  ~RGBMatrixDevice() { delete matrix_; }

  virtual int num_strips() const { return matrix_->height(); }
  virtual int num_pixel_per_strip() const { return matrix_->width(); }

  virtual void StartFrame(bool full_update) {
    // If we get a full update, we write the output to an off-screen and swap
    // on next VSync to minimize tearing.
    full_update_requested_ = full_update;
    draw_canvas_ = full_update_requested_ ? off_screen_ : on_screen_;
  }

  virtual void SetPixel(int strip, int pixel,
                        const ::pp::PixelColor &col) {
    draw_canvas_->SetPixel(pixel, strip, col.red, col.green, col.blue);
  }

  virtual void FlushFrame() {
    if (full_update_requested_) {
      on_screen_ = off_screen_;
      off_screen_ = matrix_->SwapOnVSync(off_screen_);
    }
  }

private:
  RGBMatrix *const matrix_;
  FrameCanvas *off_screen_;
  FrameCanvas *on_screen_;
  bool full_update_requested_;
  Canvas *draw_canvas_;
};

static int usage(const char *progname) {
  fprintf(stderr, "usage: %s <options>\n", progname);
  fprintf(stderr, "Options:\n"
          "\t-l            : Switch on logarithmic response (default: off)\n"
          "\t-i <iface>    : network interface, such as eth0, wlan0. "
          "Default eth0\n"
          "\t-G <group>    : PixelPusher group (default: 0)\n"
          "\t-C <controller> : PixelPusher controller (default: 0)\n"
          "\t-a <artnet-universe,artnet-channel>: if used with artnet bridge. Default 0,0\n"
          "\t-u <udp-size> : Max UDP data/packet (default %d)\n"
          "\t                Best use the maximum that works with your network (up to %d).\n"
          "\t-d            : Same as --led-daemon. Use this when starting in init scripts.\n",
          kDefaultUDPPacketSize, kMaxUDPPacketSize);

  rgb_matrix::PrintMatrixFlags(stderr);

  return 1;
}

int main(int argc, char *argv[]) {
  pp::PPOptions pp_options;
  pp_options.is_logarithmic = false;
  pp_options.artnet_universe = -1;
  pp_options.artnet_channel = -1;
  pp_options.network_interface = "eth0";

  bool ushape_display = false;  // 64x64
  const char* rotation = NULL;

  RGBMatrix::Options matrix_options;
  matrix_options.rows = 32;
  matrix_options.chain_length = 1;
  matrix_options.parallel = 1;
  rgb_matrix::RuntimeOptions runtime_opt;
  if (!ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }

  int opt;
  while ((opt = getopt(argc, argv, "dlLP:c:r:p:i:u:a:R:UG:C:")) != -1) {
    switch (opt) {
    case 'd':
      runtime_opt.daemon = 1;
      break;
    case 'l':
      pp_options.is_logarithmic = true;
      break;
    case 'L':   // Hidden option; used to be a specialized -U
      fprintf(stderr, "-L is deprecated. use\n\t--led-pixel-mapper=\"U-mapper;Rotate:180\" --led-chain=4\ninstead\n");
      matrix_options.rows = 32;
      matrix_options.chain_length = 4;
      rotation = "180";  // This is what the old transformer did.
      ushape_display = true;
      break;
    case 'U':  // Hidden option; use --led-pixel-mapper instead
      fprintf(stderr, "-U is deprecated. Use --led-pixel-mapper=\"U-mapper\" "
              "instead\n");
      ushape_display = true;
      break;
    case 'R':  // Hidden option: use --led-pixel-mapper instead
      fprintf(stderr, "-R is deprecated. Use --led-pixel-mapper=\"Rotate:%s\" "
              "instead\n", optarg);
      rotation = strdup(optarg);
      break;
    case 'P':
      matrix_options.parallel = atoi(optarg);
      break;
    case 'c':
      matrix_options.chain_length = atoi(optarg);
      break;
    case 'r':
      matrix_options.rows = atoi(optarg);
      break;
    case 'p':
      matrix_options.pwm_bits = atoi(optarg);
      break;
    case 'i':
      pp_options.network_interface = strdup(optarg);
      break;
    case 'u':
      pp_options.udp_packet_size = atoi(optarg);
      break;
    case 'G':
      pp_options.group = atoi(optarg);
      break;
    case 'C':
      pp_options.controller = atoi(optarg);
      break;
    case 'a':
      if (2 != sscanf(optarg, "%d,%d",
                      &pp_options.artnet_universe, &pp_options.artnet_channel)) {
        fprintf(stderr, "Artnet parameters must be <universe>,<channel>\n");
        return 1;
      }
      break;
    default:
      return usage(argv[0]);
    }
  }

  // Support for deprecated rotation options.
  std::string pixel_map_config;
  if (matrix_options.pixel_mapper_config)
    pixel_map_config.append(matrix_options.pixel_mapper_config);
  if (ushape_display) {
    pixel_map_config.insert(0, "U-mapper;");
    matrix_options.pixel_mapper_config = pixel_map_config.c_str();
  }
  if (rotation) {
    pixel_map_config.append(";").append("Rotate:").append(rotation);
    matrix_options.pixel_mapper_config = pixel_map_config.c_str();
  }

  // Some parameter checks.
  if (getuid() != 0) {
    fprintf(stderr, "Must run as root to be able to access /dev/mem\n"
            "Prepend 'sudo' to the command:\n\tsudo %s ...\n", argv[0]);
    return 1;
  }

  RGBMatrix *matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
  matrix->set_luminance_correct(pp_options.is_logarithmic);

  RGBMatrixDevice device(matrix);
  if (!pp::StartPixelPusherServer(pp_options, &device)) {
    return 1;
  }

  if (runtime_opt.daemon == 1) {
    for(;;) sleep(INT_MAX);
  } else {
    printf("Press <RETURN> to shut down (supply -d option to run as daemon)\n");
    getchar();  // for now, run until <RETURN>
    printf("shutting down\n");
  }

  pp::ShutdownPixelPusherServer();

  return 0;
}
