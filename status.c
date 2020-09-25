#include <stdbool.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
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
#define SND_CMD "fuser -v /dev/snd/* 2>&1 /dev/zero"
#define kB			1024
#define mB			(kB * kB)
#define gB			(kB * mB)
#define UP_ARROW                "\u2b06"
#define DOWN_ARROW		          "\u2b07"
#define UP_TRI                  "\u25b4"
#define DOWN_TRI                "\u25be"
#define RIGHT_ARROW		          "\u27a1"
#define FULL_SPACE		          "\u2000"
#define SHORT_SPACE		          "\u2005"
#define HEAVY_HORIZONTAL        "\u2501"
#define HEAVY_VERTICAL		      "\u2503"
#define DOUBLE_VERTICAL		      "\u2551"
#define LF_THREE_EIGHTHS_BLOCK	" \u258d"
#define LIGHT_SHADE		          "\u2591"
#define MEDIUM_SHADE		        " \u2592 "
#define DARK_SHADE		          " \u2593 "
#define DASH_VERT               "\u250a"
#define UP                      UP_TRI
#define DOWN                    DOWN_TRI
#define SNDSYM                  "♬"
#define SEPERATOR		            "|"

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
  unsigned long rd_sec_or_wr_ios, wr_sec;
  const char *blkdev;
  float readkbs, writekbs;
} diskstats_t;

typedef struct
{
  char IFNAME[8];
  unsigned long RXbytes, TXbytes;
  const char *netif;
  float downkbytes, upkbytes;
} net_t;

typedef struct
{
  char IFNAME[8], ESSID[32];
  unsigned int link;
  net_t *net;
} wireless_t;

typedef struct
{
  CURL *handle;
  char BUFFER[64], PREV[64];
} ip_t;

typedef struct
{
  unsigned capacity,
           remaining,
           rate;
  char BAT[8], STATE[16], STATEFILE[32];
  float perc;
} battery_t;

typedef struct
{
  unsigned NBAT;
  battery_t BATTERY[MAX_BATTERIES];
} batteries_t;

static unsigned char interval = UPDATE_INTV;
static bool quit = 0;

static void read_file(void *data, void (*cb)(), const char FILENAME[])
{
  FILE *fp = fopen(FILENAME, "r");
  if (!fp)
    return;

  static char LINE[STRLEN];
  while (fgets(LINE, sizeof LINE, fp))
    cb(data, LINE);

  fclose(fp);
}

static void read_dir(void *data, void (*cb)(), const char DIRNAME[])
{
  DIR *dp = opendir(DIRNAME);
  if (!dp)
    return;

  static struct dirent *d;
  while ((d = readdir(dp)))
    if (strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
      cb(data, d->d_name);

  closedir(dp);
}

static char *format_units(float val)
{
  /* Function expects numeric to be in kB units */
  static char STRING[8];

  if (val * kB < kB)
    sprintf(STRING, "%.0fB", val * kB);

  else if (val * kB > kB - 1 && val * kB < mB)
    sprintf(STRING, "%.0fK", val);

  else if (val * kB > mB - 1 && val * kB < gB)
    sprintf(STRING, "%.1fM", val / kB);

  else if (val * kB > gB - 1)
    sprintf(STRING, "%.1fG", val / mB);

  return STRING;
}

static void date(char TIME[], size_t size)
{
  time_t t = time(NULL);
  strftime(TIME , size, "%l:%M %a %d %b", localtime(&t));
}

static void battery_state_cb(void *data, const char STRING[])
{
  battery_t *battery = data, tmp;
  if (sscanf(STRING, "charging state: %s", tmp.STATE))
    strcpy(battery->STATE, tmp.STATE);
  else if (sscanf(STRING, "present rate: %d", &tmp.rate))
    battery->rate = tmp.rate;
  else if (sscanf(STRING, "remaining capacity: %d", &tmp.remaining))
  {
    battery->remaining = tmp.remaining;
    battery->perc = (float) battery->remaining / battery->capacity * 100;
  }
}

static void snd(char SND[])
{
  FILE *fp = popen(SND_CMD, "r");
  if (!fp)
    return;

  char STRING[128];
  if (fgets(STRING, 128, fp) && fgets(STRING, 128, fp))
    sscanf(STRING, "%*[^ ] %*[^ ] %*[^ ] %*[^ ] %s ", SND);
  else SND[0] = '\0';
  pclose(fp);
}

static void battery_info_cb(void *data, const char STRING[])
{
  battery_t *battery = data, tmp;
  if (sscanf(STRING, "design capacity: %d", &tmp.capacity))
    battery->capacity = tmp.capacity;
}

static void battery_cb(void *data, const char STRING[])
{
  batteries_t *batteries = data;
  unsigned *NBAT = &batteries->NBAT;
  if (strlen(STRING))
  {
    strcpy(batteries->BATTERY[*NBAT].BAT, STRING);
    (*NBAT)++;
  }
}

static void init_batteries(batteries_t *batteries)
{
  batteries->NBAT = 0;
  read_dir(batteries, battery_cb, ACPI_BAT); 
  for (unsigned i = 0; i < batteries->NBAT; i++)
  {
    char INFOFILE[32], *bat = batteries->BATTERY[i].BAT;
    sprintf(INFOFILE, "%s/%s/info", ACPI_BAT, bat);
    read_file(&batteries->BATTERY[i], battery_info_cb, INFOFILE);
    sprintf(batteries->BATTERY[i].STATEFILE, "%s/%s/state", ACPI_BAT, bat);
  }
}

static void ac_cb(void *data, const char STRING[])
{
  bool *ac_state = data;
  char STATE[16];
  sscanf(STRING, "state: %s", STATE);

  if (strcmp(STATE, "on-line") == 0)
    *ac_state = 1;
  else
    *ac_state = 0;
}

static void tail(char LINE[], size_t size, const char FILENAME[], unsigned char n)
{
  FILE *fp = fopen(FILENAME, "r");
  if (!fp)
    return;

  char err;
  fseek(fp, -sizeof err, SEEK_END);
  unsigned i = 0;
  while (i < n && !(err = fseek(fp, -2 * sizeof err, SEEK_CUR)))
    if (fgetc(fp) == '\n')
      i++;

  if (err)
    fseek(fp, 0L, SEEK_SET);

  if (fgets(LINE, size, fp))
    LINE[strlen(LINE) - 1] = '\0';
  fclose(fp);
}

static void public_ip(ip_t *ip)
{
  char SWAP[64];
  strcpy(SWAP, ip->BUFFER);
  if (curl_easy_perform(ip->handle) != CURLE_OK)
  {
    strcpy(ip->BUFFER, "No IP");
    return;
  }
  else if (strcmp(ip->BUFFER, ip->PREV))
  {
    FILE *fp = fopen(IPLIST, "a+");
    if (!fp)
      return;

    fprintf(fp, "%s\n", ip->BUFFER);
    fclose(fp);
    strcpy(ip->PREV, SWAP);
  }
}

static void ssid(char SSID[], size_t size, const char NETIF[])
{
  struct iwreq wreq;
  memset(&wreq, 0, sizeof wreq);
  sprintf(wreq.ifr_name, NETIF);
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd > 0)
  {
    wreq.u.essid.pointer = SSID;
    wreq.u.essid.length = size - 1;
    ioctl(sockfd, SIOCGIWESSID, &wreq);
  }
}

static void wireless_cb(void *data, const char LINE[])
{
  wireless_t *wireless = data, tmp;
  sscanf(LINE, "%s %*[^ ] %d", tmp.IFNAME, &tmp.link);

  if (strncmp(tmp.IFNAME, wireless->net->netif, strlen(wireless->net->netif)) == 0)
  {
    strncpy(wireless->IFNAME, tmp.IFNAME, strlen(wireless->net->netif));
    wireless->link = tmp.link;
  }
  else wireless->link = 0;
}

static void net_cb(void *data, const char LINE[])
{
  net_t *net = data, tmp;

  sscanf(LINE, "%s %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ]",
      tmp.IFNAME,
      &tmp.RXbytes,
      &tmp.TXbytes);

  if (strncmp(tmp.IFNAME, net->netif, strlen(net->netif)) == 0)
  {
    tmp.RXbytes /= kB;
    tmp.TXbytes /= kB;
    net->downkbytes = (tmp.RXbytes - net->RXbytes) / interval;
    net->upkbytes = (tmp.TXbytes - net->TXbytes) / interval;
    net->RXbytes = tmp.RXbytes;
    net->TXbytes = tmp.TXbytes;
  }
}

static float wireless_link(wireless_t *wireless)
{
  return wireless->link / 70. * 100;
}

static void init_net(net_t NET[], wireless_t WLAN[])
{
  for (unsigned i = 0; i < LENGTH(NETIF); i++)
  {
    NET[i].netif = NETIF[i];
    WLAN[i].net = &NET[i];
    read_file(&NET[i], net_cb, NET_ADAPTERS);
    read_file(&WLAN[i], wireless_cb, WIRELESS);
  }
}

static unsigned du_perc(const char DIRECTORY[])
{
  struct statvfs fs;
  if (statvfs(DIRECTORY, &fs) < 0)
  {
    printf("Unable to get fs info %s\n", DIRECTORY);
    return 0;
  }

  return (1 - ((float) fs.f_bfree / (float) fs.f_blocks)) * 100;
}

static void blkdev_cb(void *data, const char LINE[])
{
  diskstats_t *diskstats = data, tmp;
  sscanf(LINE, " %*[^ ] %*[^ ] %s %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ]",
      tmp.DEVNAME,
      &tmp.rd_sec_or_wr_ios,
      &tmp.wr_sec);

  if (strcmp(tmp.DEVNAME, diskstats->blkdev) == 0)
  {
    diskstats->readkbs = (tmp.rd_sec_or_wr_ios - diskstats->rd_sec_or_wr_ios) / 2. / interval;
    diskstats->writekbs = (tmp.wr_sec - diskstats->wr_sec) / 2. / interval;
    diskstats->rd_sec_or_wr_ios =  tmp.rd_sec_or_wr_ios;
    diskstats->wr_sec = tmp.wr_sec;
  }
}

static void init_diskstats(diskstats_t DISKSTATS[])
{
  for (unsigned i = 0; i < LENGTH(BLKDEV); i++)
  {
    DISKSTATS[i].blkdev = BLKDEV[i];
    read_file(&DISKSTATS[i], blkdev_cb, DISKSTAT);
  }
}

static void mem_cb(void *data, const char LINE[])
{
  mem_t *mem = data;
  sscanf(LINE, "MemTotal: %lu", &mem->total);
  sscanf(LINE, "MemFree: %lu", &mem->free);
  sscanf(LINE, "MemAvailable: %lu", &mem->available);
  sscanf(LINE, "Buffers: %lu", &mem->buffers);
  sscanf(LINE, "Cached: %lu", &mem->cached);
  sscanf(LINE, "SwapCached: %lu", &mem->swapcached);
  sscanf(LINE, "SwapTotal: %lu", &mem->swaptotal);
  sscanf(LINE, "SwapFree: %lu", &mem->swapfree);
  mem->perc = (float) (mem->total - mem->free - mem->buffers - mem->cached) / mem->total * 100;
  mem->swap = mem->swaptotal - mem->swapfree - mem->swapcached;
}

static void cpu_cb(void *data, const char LINE[])
{
  cpu_t *cpu = data, tmp;

  if (sscanf(LINE, "cpu%*[^0-9] %lu %lu %lu %lu %lu %lu %lu",
        &tmp.user,
        &tmp.nice,
        &tmp.system,
        &tmp.idle,
        &tmp.iowait,
        &tmp.irq,
        &tmp.softirq) == 7)
  {
    cpu->perc = (double) (tmp.user + tmp.nice + tmp.system + tmp.irq + tmp.softirq
        - cpu->user - cpu->nice - cpu->system - cpu->irq - cpu->softirq) /
      (double) (tmp.user + tmp.nice + tmp.system + tmp.idle + tmp.iowait + tmp.irq + tmp.softirq
          - cpu->user - cpu->nice - cpu->system - cpu->idle - cpu->iowait - cpu->irq - cpu->softirq) * 100.;

    cpu->user = tmp.user;
    cpu->nice = tmp.nice;
    cpu->system = tmp.system;
    cpu->idle = tmp.idle;
    cpu->iowait = tmp.iowait;
    cpu->irq = tmp.irq;
    cpu->softirq = tmp.softirq;
  }

  else if (sscanf(LINE, "processor : %d", &tmp.processor) == 1)
    cpu->processor = tmp.processor;
  else if (sscanf(LINE, "cpu MHz : %f", &tmp.mhz) == 1)
    /* Calculate the rolling average wrt number of cores */
    cpu->mhz = (cpu->mhz * (cpu->processor) + tmp.mhz) / (cpu->processor + 1);
}

static size_t writefunc(void *ptr, size_t size, size_t nmemb, void *data)
{
  ip_t *ip = data;
  size_t S = size * nmemb;
  if (S < sizeof ip->BUFFER)
  {
    memset(ip->BUFFER, 0, S);
    memcpy(ip->BUFFER, ptr, S);
    ip->BUFFER[S] = '\0';
  }
  else ip->BUFFER[0] = '\0';

  return S;
}

static void deinit_curl(ip_t *ip)
{
  curl_easy_cleanup(ip->handle);
}

static void init_curl(ip_t *ip)
{
  curl_global_init(CURL_GLOBAL_ALL);
  ip->handle = curl_easy_init();
  curl_easy_setopt(ip->handle, CURLOPT_URL, IPURL[0]);
  curl_easy_setopt(ip->handle, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(ip->handle, CURLOPT_WRITEDATA, ip);
  curl_easy_setopt(ip->handle, CURLOPT_TIMEOUT, 1L);
}

static void deinit_ip(ip_t *ip)
{
  deinit_curl(ip);
}

static void init_ip(ip_t *ip)
{
  init_curl(ip);
  ip->BUFFER[0] = '\0';
  tail(ip->PREV, sizeof ip->PREV, IPLIST, 1);
}

void set_quit_flag(const int signo)
{
  (void) signo;
  quit = 1;
}

int main(int argc, char **argv)
{
  (void) argc;
  (void) **argv;
  struct timeval tv;
  struct sigaction SA;
  memset(&SA, 0, sizeof SA);
  SA.sa_handler = set_quit_flag;
  sigaction(SIGINT,  &SA, NULL);
  sigaction(SIGTERM, &SA, NULL);
  setlocale(LC_ALL, "");
  setbuf(stdout, NULL);
  cpu_t cpu = { };
  mem_t mem = { };
  diskstats_t DISKSTATS[LENGTH(BLKDEV)] = { };
  init_diskstats(DISKSTATS);
  net_t NET[LENGTH(NETIF)] = { };
  wireless_t WLAN[LENGTH(NETIF)] = { };
  init_net(NET, WLAN);
  ip_t ip;
  init_ip(&ip);
  bool ac_state;
  batteries_t batteries;
  init_batteries(&batteries);
  char SSID[16] = { }, SND[16], TIME[32];

  while (!quit)
  {
    read_file(&cpu, cpu_cb, CPU);
    read_file(&cpu, cpu_cb, STAT);
    read_file(&mem, mem_cb, MEM);
    fprintf(stdout, "%.0lf%% %.0fMHz", cpu.perc, cpu.mhz);
    fprintf(stdout, "%s%.0f%% [%s]", SEPERATOR, mem.perc, format_units(mem.swap));

    for (unsigned i = 0; i < LENGTH(BLKDEV); i++)
    {
      read_file(&DISKSTATS[i], blkdev_cb, DISKSTAT);
      fprintf(stdout, "%s%s ", SEPERATOR, BLKDEV[i]);
      fprintf(stdout, "%s%s ", UP, format_units(DISKSTATS[i].readkbs));
      fprintf(stdout, "%s%s", DOWN, format_units(DISKSTATS[i].writekbs));
    }

    fprintf(stdout, "%s", SEPERATOR);
    for (unsigned i = 0; i < LENGTH(DIRECTORY); i++)
      fprintf(stdout, "%s %u%% ", DIRECTORY[i], du_perc(DIRECTORY[i]));

    for (unsigned i = 0; i < LENGTH(NETIF); i++)
    {
      read_file(&NET[i], net_cb, NET_ADAPTERS);
      read_file(&WLAN[i], wireless_cb, WIRELESS);
      float wl = wireless_link(&WLAN[i]);
      if (!wl)
        fprintf(stdout, "%s%s ", SEPERATOR, NET[i].netif);
      else
      {
        ssid(SSID, sizeof SSID, NET[i].netif);
        fprintf(stdout, "%s%s %s %.0f%% ", SEPERATOR, NET[i].netif, SSID, wl);
      }

      fprintf(stdout, "%s%s", UP, format_units(NET[i].upkbytes));
      fprintf(stdout, "[%s] ", format_units(NET[i].TXbytes));
      fprintf(stdout, "%s%s", DOWN, format_units(NET[i].downkbytes));
      fprintf(stdout, "[%s]", format_units(NET[i].RXbytes));
    }

    public_ip(&ip);
    if (!strcmp(ip.PREV, ip.BUFFER) || !strlen(ip.PREV))
      fprintf(stdout, "%s%s", SEPERATOR, ip.BUFFER);
    else
      fprintf(stdout, "%s%s%s%s", SEPERATOR, ip.PREV, RIGHT_ARROW, ip.BUFFER);

    read_file(&ac_state, ac_cb, ACPI_ACSTATE);
    if (ac_state)
      interval = UPDATE_INTV;
    else
      interval = UPDATE_INTV_ON_BATTERY;

    for (unsigned i = 0; i < batteries.NBAT; i++)
    {
      read_file(&batteries.BATTERY[i], battery_state_cb, batteries.BATTERY[i].STATEFILE);
      fprintf(stdout, "%s%s %.0f%% %c", SEPERATOR, 
          batteries.BATTERY[i].BAT, 
          batteries.BATTERY[i].perc, 
          batteries.BATTERY[i].STATE[0] - 32);
    }

    snd(SND);
    fprintf(stdout, "%s%s%s", SEPERATOR, SNDSYM, SND);
    date(TIME, sizeof TIME);
    fprintf(stdout, "%s%s\n", SEPERATOR, TIME);
    // Wait
    tv.tv_sec = interval;
    tv.tv_usec = 0;
    select(0, NULL, NULL, NULL, &tv);
  }

  deinit_ip(&ip);
  return 0;
}
