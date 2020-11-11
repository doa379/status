/* See LICENSE file for license details.
 * Copyright 2017-2020 by doa379
 */

#include <locale.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "status.h"

static const char *BLKDEV[] = { BLKDEVS };
static const char *NETIF[] = { NETIFS };
static const char *IPURL[] = { IPURLS };
static char LINE[STRLEN];

void read_file(void *data, void (*cb)(), const char FILENAME[])
{
  FILE *fp = fopen(FILENAME, "r");
  if (!fp)
    return;

  while (fgets(LINE, sizeof LINE, fp))
    cb(data, LINE);

  fclose(fp);
}

void read_dir(void *data, void (*cb)(), const char DIRNAME[])
{
  DIR *dp = opendir(DIRNAME);
  if (!dp)
    return;

  struct dirent *d;
  while ((d = readdir(dp)))
    if (strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
      cb(data, d->d_name);

  closedir(dp);
}

char *format_units(float val)
{ /* Function expects numeric to be in kB units */
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

static void parse_dev_cb(void *data, char LINE[])
{
  device_t *device = data;
  if (!strncmp("N:", LINE, 2) && strstr(LINE, device->type))
    device->found = 1;

  else if (!strncmp("H:", LINE, 2) && device->found)
  {
    char *p;
    if ((p = strstr(LINE, "event")))
    {
      char EV[8];
      sscanf(p, "%s", EV);
      sprintf(device->node, "/dev/input/%s", EV);
    }

    device->found = 0;
  }
}

void input_event_node(char NODE[], const char TYPE[])
{
  device_t device = { NODE, TYPE, 0 };
  read_file(&device, parse_dev_cb, DEVICES);
}

void date(char TIME[], size_t size)
{
  time_t t = time(NULL);
  strftime(TIME , size, "%R %a %d %b", localtime(&t));
}

void energyuj_cb(void *data, const char LINE[])
{
  unsigned long energy, *total_energy = data;
  sscanf(LINE, "%lu", &energy);
  *total_energy += energy;
}

unsigned power(void *data, unsigned interval)
{
  powercaps_t *powercaps = data;
  powercaps->prev_energy_uj = powercaps->curr_energy_uj;
  powercaps->curr_energy_uj = 0;
  for (unsigned i = 0; i < powercaps->size; i++)
    read_file(&powercaps->curr_energy_uj, energyuj_cb, powercaps->powercap[i].STATEFILE);

  return (powercaps->curr_energy_uj - powercaps->prev_energy_uj) / 1e6 / interval;
}

void powercap_cb(void *data, const char STRING[])
{
  powercaps_t *powercaps = data;
  powercap_t powercap;
  char FILENAME[sizeof powercap.STATEFILE];
  sprintf(FILENAME, "%s/%s/energy_uj", ENERGY, STRING);
  FILE *fp = fopen(FILENAME, "r");
  if (fp)
  {
    powercaps->powercap = realloc(powercaps->powercap, 
      (powercaps->size + 1) * sizeof powercap);
    strcpy(powercaps->powercap[powercaps->size].STATEFILE, FILENAME);
    powercaps->size++;
    fclose(fp);
  }
}

void deinit_power(powercaps_t *powercaps)
{
  free(powercaps->powercap);
}

void init_power(powercaps_t *powercaps)
{
  powercaps->powercap = malloc(1);
  powercaps->size = 0;
  read_dir(powercaps, powercap_cb, ENERGY);
}

void snd_cb(void *data, const char LINE[])
{
  char *snd = data;
  unsigned pid;
  if (sscanf(LINE, "  Client application %d : running", &pid))
    sprintf(snd, "%d", pid);
}

void snd(char SND[])
{
  SND[0] = '\0';
  read_file(SND, snd_cb, SOUND);
}

void battery_cb(void *data, const char STRING[])
{
  batteries_t *batteries = data;
  if (!strncmp("BAT", STRING, 3))
  {
    batteries->battery = realloc(batteries->battery, 
      (batteries->size + 1) * sizeof(battery_t));
    strcpy(batteries->battery[batteries->size].BAT, STRING);
    batteries->size++;
  }
}

void deinit_batteries(batteries_t *batteries)
{
  free(batteries->battery);
}

#ifdef PROC_ACPI
void battery_state_cb(void *data, const char STRING[])
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

void battery_info_cb(void *data, const char STRING[])
{
  battery_t *battery = data, tmp;
  if (sscanf(STRING, "design capacity: %d", &tmp.capacity))
    battery->capacity = tmp.capacity;
}

static void init_batteries(batteries_t *batteries)
{
  batteries->battery = malloc(1);
  batteries->size = 0;
  read_dir(batteries, battery_cb, ACPI_BAT);
  for (unsigned i = 0; i < batteries->size; i++)
  {
    char INFOFILE[32], *bat = batteries->battery[i].BAT;
    sprintf(INFOFILE, "%s/%s/info", ACPI_BAT, bat);
    read_file(&batteries->battery[i], battery_info_cb, INFOFILE);
    sprintf(batteries->battery[i].STATEFILE, "%s/%s/state", ACPI_BAT, bat);
  }
}

void ac_cb(void *data, const char STRING[])
{
  bool *ac_state = data;
  if (!strcmp("state: on-line", STRING))
    *ac_state = 1;
  else
    *ac_online = 0;
}
#else
void battery_state_cb(void *data, const char STRING[])
{
  battery_t *battery = data, tmp;
  if (sscanf(STRING, "POWER_SUPPLY_STATUS=%c", &tmp.state))
    battery->state = tmp.state > 96 ? tmp.state - 32 : tmp.state;
  else if (sscanf(STRING, "POWER_SUPPLY_CURRENT_NOW=%ud", &tmp.rate))
    battery->rate = tmp.rate;
  else if (sscanf(STRING, "POWER_SUPPLY_CAPACITY=%ud", &tmp.perc))
    battery->perc = tmp.perc;
}

void init_batteries(batteries_t *batteries)
{
  batteries->battery = malloc(1);
  batteries->size = 0;
  read_dir(batteries, battery_cb, SYS_PS); 
  for (unsigned i = 0; i < batteries->size; i++)
  {
    char *bat = batteries->battery[i].BAT;
    sprintf(batteries->battery[i].STATEFILE, "%s/%s/uevent", SYS_PS, bat);
  }
}

void ac_cb(void *data, const char STRING[])
{
  bool *ac_state = data;
  unsigned val;
  sscanf(STRING, "%ud", &val);
  *ac_state = !val;
}
#endif
void public_ip(ip_t *ip)
{
  if (curl_easy_perform(ip->handle) != CURLE_OK)
    strcpy(ip->CURR, "No IP");
  else if (strcmp(ip->BUFFER, ip->CURR)
    && strcmp("No IP", ip->CURR))
  {
    FILE *fp = fopen(IPLIST, "a+");
    if (!fp)
      return;

    fprintf(fp, "%s\n", ip->BUFFER);
    fclose(fp);
    strcpy(ip->PREV, ip->CURR);
    strcpy(ip->CURR, ip->BUFFER);
  }
}

static bool init_ssid(ssid_t *ssid, const char NETIF[])
{
  memset(&ssid->wreq, 0, sizeof ssid->wreq);
  strcpy(ssid->wreq.ifr_name, NETIF);
  if ((ssid->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) > -1)
    return 1;

  return 0;
}

void ssid(ssid_t *ssid)
{
  memset(ssid->SSID, 0, sizeof ssid->SSID);
  ssid->wreq.u.essid.pointer = ssid->SSID;
  ssid->wreq.u.essid.length = sizeof ssid->SSID;
  ioctl(ssid->sockfd, SIOCGIWESSID, &ssid->wreq);
}

void wireless_cb(void *data, const char LINE[])
{
  wireless_t *wireless = data, tmp;
  sscanf(LINE, "%s %*[^ ] %f %f %f", tmp.IFNAME, &tmp.link, &tmp.level, &tmp.noise);
  if (strncmp(tmp.IFNAME, wireless->net->netif, strlen(wireless->net->netif)) == 0)
  {
    wireless->link = tmp.link;
    wireless->level = tmp.level;
    wireless->noise = tmp.noise;
  }
}

unsigned up_kbps(net_t *net, unsigned interval)
{
  return (net->TXbytes - net->prev_TXbytes) / kB / interval;
}

unsigned down_kbps(net_t *net, unsigned interval)
{
  return (net->RXbytes - net->prev_RXbytes) / kB / interval;
}

void net_cb(void *data, const char LINE[])
{
  net_t *net = data, tmp;
  sscanf(LINE, "%s %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ]",
      tmp.IFNAME,
      &tmp.RXbytes,
      &tmp.TXbytes);

  if (strncmp(tmp.IFNAME, net->netif, strlen(net->netif)) == 0)
  {
    net->prev_RXbytes = net->RXbytes;
    net->prev_TXbytes = net->TXbytes;
    net->RXbytes = tmp.RXbytes;
    net->TXbytes = tmp.TXbytes;
  }
}

unsigned wireless_link(wireless_t *wireless)
{
  return wireless->link / 70. * 100;
}

void init_net(net_t NET[], wireless_t WLAN[])
{
  for (unsigned i = 0; i < LENGTH(NETIF); i++)
  {
    NET[i].netif = NETIF[i];
    WLAN[i].net = &NET[i];
    while(!init_ssid(&WLAN[i].ssid, NETIF[i]));
    read_file(&NET[i], net_cb, NET_ADAPTERS);
    read_file(&WLAN[i], wireless_cb, WIRELESS);
  }
}

unsigned du_perc(const char DIRECTORY[])
{
  struct statvfs fs;
  if (statvfs(DIRECTORY, &fs) < 0)
    return 0;

  return (1 - ((float) fs.f_bfree / (float) fs.f_blocks)) * 100;
}

unsigned read_kbps(diskstats_t *diskstats, unsigned interval)
{
  return (diskstats->rd_sec_or_wr_ios - diskstats->prev_rd_sec_or_wr_ios) / 2. / interval;
}

unsigned write_kbps(diskstats_t *diskstats, unsigned interval)
{
  return (diskstats->wr_sec - diskstats->prev_wr_sec) / 2. / interval;
}

void blkdev_cb(void *data, const char LINE[])
{
  diskstats_t *diskstats = data, tmp;
  sscanf(LINE, " %*[^ ] %*[^ ] %s %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ]",
      tmp.DEVNAME,
      &tmp.rd_sec_or_wr_ios,
      &tmp.wr_sec);

  if (strcmp(tmp.DEVNAME, diskstats->blkdev) == 0)
  {
    diskstats->prev_rd_sec_or_wr_ios = diskstats->rd_sec_or_wr_ios;
    diskstats->prev_wr_sec = diskstats->wr_sec;
    diskstats->rd_sec_or_wr_ios = tmp.rd_sec_or_wr_ios;
    diskstats->wr_sec = tmp.wr_sec;
  }
}

void init_diskstats(diskstats_t DISKSTATS[])
{
  for (unsigned i = 0; i < LENGTH(BLKDEV); i++)
  {
    DISKSTATS[i].blkdev = BLKDEV[i];
    read_file(&DISKSTATS[i], blkdev_cb, DISKSTAT);
  }
}

void mem_cb(void *data, const char LINE[])
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

void cpu_cb(void *data, const char LINE[])
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

  else if (sscanf(LINE, "processor : %ud", &tmp.processor) == 1)
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
  curl_easy_setopt(ip->handle, CURLOPT_TIMEOUT_MS, 250L);
}

void deinit_ip(ip_t *ip)
{
  deinit_curl(ip);
}

void init_ip(ip_t *ip)
{
  init_curl(ip);
  tail(ip->CURR, sizeof ip->CURR, IPLIST, 1);
  tail(ip->PREV, sizeof ip->PREV, IPLIST, 2);
}
