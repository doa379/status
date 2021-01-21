#ifndef STATUS_H
#define STATUS_H

#include <stdbool.h>
#include <linux/wireless.h>
#include <curl/curl.h>
#include <stdarg.h>
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
#define SOUND "/proc/asound"
#define DEVICES "/proc/bus/input/devices"
#define ENERGY "/sys/class/powercap"
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
  battery_t *battery;
  size_t size;
} batteries_t;

typedef struct
{
  char STATEFILE[64];
  unsigned long energy_uj;
} powercap_t;

typedef struct
{
  powercap_t *powercap;
  size_t size;
  unsigned long curr_energy_uj, prev_energy_uj;
} powercaps_t;

typedef struct
{
  char P_STATEFILE[64], C_STATEFILE[64];
  char P_SND[16], C_SND[16];
} asound_card_t;

typedef struct
{
  asound_card_t *card;
  size_t size;
} asound_cards_t;

typedef struct
{
  char *node;
  const char *type;
  bool found;
} device_t;

char *format_units(float);
void input_event_node(char [], const char []);
const char *date(void);
const char *asound_card_c(unsigned);
const char *asound_card_p(unsigned);
void refresh_asound_card(unsigned);
unsigned asound_cards_size(void);
unsigned power(unsigned);
bool ac(void);
char battery_state(unsigned);
unsigned battery_perc(unsigned);
const char *battery_string(unsigned);
void refresh_battery(unsigned);
unsigned batteries_size(void);
void refresh_ps(void);
const char *prev_ip();
const char *curr_ip();
void refresh_publicip(void);
unsigned wireless_link(unsigned);
const char *ssid_string(unsigned);
bool ssid(unsigned);
void refresh_netadapter(unsigned);
unsigned rx_total_kb(unsigned);
unsigned tx_total_kb(unsigned);
unsigned rx_kbps(unsigned, unsigned);
unsigned tx_kbps(unsigned, unsigned);
unsigned du_perc(const char []);
unsigned read_kbps(unsigned, unsigned);
unsigned write_kbps(unsigned, unsigned);
void refresh_diskstats(unsigned);
float mem_swap(void);
float mem_perc(void);
float cpu_mhz(void);
float cpu_perc(void);
void deinit_status(void);
void init_status(void);
void print(char [], const char *, ...);
#endif
