#ifndef STATUS_H
#define STATUS_H

#include <stdbool.h>
#include <linux/wireless.h>
#include <curl/curl.h>
#include "config.h"

#define LENGTH(X)		(sizeof X / sizeof X[0])
#define CPU	"/proc/cpuinfo"
#define MEM	"/proc/meminfo"
#define STAT "/proc/stat"
#define DISKSTAT "/proc/diskstats"
#define NET_ADAPTERS "/proc/net/dev"
#define WIRELESS "/proc/net/wireless"
#define ACPI_ACSTATE "/proc/acpi/ac_adapter/AC/state"
#define ACPI_BAT "/proc/acpi/battery"
#define SYS_PS "/sys/class/power_supply"
#define SYS_ACSTATE "/sys/class/power_supply/AC/online"
#define SND_CMD "lsof /dev/snd/*"
#define DEVICES "/proc/bus/input/devices"
#define ENERGY_CMD "cat /sys/class/powercap/*/energy_uj"
#define kB			1024
#define mB			(kB * kB)
#define gB			(kB * mB)

typedef struct
{
  unsigned long user,
                nice,
                system,
                idle,
                iowait,
                irq,
                softirq;
  unsigned processor;
  float perc, mhz;
} cpu_t;

typedef struct
{
  unsigned long total,
                free,
                available,
                buffers,
                cached,
                swapcached,
                swaptotal,
                swapfree;
  float perc, swap;
} mem_t;

typedef struct
{
  char DEVNAME[16];
  unsigned long prev_rd_sec_or_wr_ios, prev_wr_sec, rd_sec_or_wr_ios, wr_sec;
  const char *blkdev;
} diskstats_t;

typedef struct
{
  char IFNAME[8];
  unsigned long prev_RXbytes, prev_TXbytes, RXbytes, TXbytes;
  const char *netif;
} net_t;

typedef struct
{
  char SSID[16];
  struct iwreq wreq;
  int sockfd;
} ssid_t;

typedef struct
{
  char IFNAME[8];
  float link, level, noise;
  ssid_t ssid;
  net_t *net;
} wireless_t;

typedef struct
{
  CURL *handle;
  char BUFFER[64], CURR[64], PREV[64];
} ip_t;

typedef struct
{
  unsigned capacity,
           remaining,
           rate, 
           perc;
  char BAT[8], STATEFILE[48], state;
} battery_t;

typedef struct
{
  unsigned NBAT, total_perc;
  battery_t BATTERY[MAX_BATTERIES];
} batteries_t;

typedef struct
{
  char *node;
  const char *type;
  bool found;
} device_t;

void read_file(void *, void (*)(), const char []);
void read_dir(void *, void (*)(), const char []);
char *format_units(float);
void input_event_node(char [], const char []);
void date(char [], size_t);
unsigned power(void *, unsigned);
void snd(char []);
void battery_cb(void *, const char []);
void battery_state_cb(void *, const char []);
void battery_info_cb(void *, const char []);
void ac_cb(void *, const char []);
void public_ip(ip_t *);
void ssid(ssid_t *);
void wireless_cb(void *, const char []);
unsigned up_kbps(net_t *, unsigned);
unsigned down_kbps(net_t *, unsigned);
void net_cb(void *, const char []);
unsigned wireless_link(wireless_t *);
unsigned du_perc(const char []);
unsigned read_kbps(diskstats_t *, unsigned);
unsigned write_kbps(diskstats_t *, unsigned);
void blkdev_cb(void *, const char []);
void mem_cb(void *, const char []);
void cpu_cb(void *, const char []);
void init_batteries(batteries_t *);
void init_net(net_t [], wireless_t []);
void init_diskstats(diskstats_t []);
void deinit_ip(ip_t *);
void init_ip(ip_t *);
#endif
