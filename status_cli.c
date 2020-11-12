#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include "status.h"

static const char *BLKDEV[] = { BLKDEVS };
static const char *DIRECTORY[] = { DIRECTORIES };
static const char *NETIF[] = { NETIFS };

static bool quit;
static cpu_t cpu;
static mem_t mem;
static diskstats_t DISKSTATS[LENGTH(BLKDEV)];
static net_t NET[LENGTH(NETIF)];
static wireless_t WLAN[LENGTH(NETIF)];
static ip_t ip;
static bool ac_state;
static batteries_t batteries;
static asound_cards_t asound_cards;
static char TIME[32];
static powercaps_t powercaps;
static unsigned char interval = UPDATE_INTV;

void set_quit_flag(const int signo)
{
  (void) signo;
  quit = 1;
}

static void deinit_status(void)
{
  deinit_power(&powercaps);
  deinit_snd(&asound_cards);
  deinit_batteries(&batteries);
  deinit_ip(&ip);
}

static void init_status(void)
{
  setlocale(LC_ALL, "");
  setbuf(stdout, NULL);
  init_diskstats(DISKSTATS);
  init_net(NET, WLAN);
  init_ip(&ip);
  init_batteries(&batteries);
  init_snd(&asound_cards);
  init_power(&powercaps);
}

int main(int argc, char *argv[])
{
  (void) argc;
  (void) **argv;
  struct sigaction SA;
  memset(&SA, 0, sizeof SA);
  SA.sa_handler = set_quit_flag;
  sigaction(SIGINT,  &SA, NULL);
  sigaction(SIGTERM, &SA, NULL);
  init_status();

  while (!quit)
  {
    fprintf(stdout, "%s%dW", PWRSYM, power(&powercaps, interval));
    read_file(&cpu, cpu_cb, CPU);
    read_file(&cpu, cpu_cb, STAT);
    read_file(&mem, mem_cb, MEM);
    fprintf(stdout, "%s%.0lf%% %.0fMHz", DELIM, cpu.perc, cpu.mhz);
    fprintf(stdout, "%s%.0f%% (%s)", DELIM, mem.perc, format_units(mem.swap));
    
    for (unsigned i = 0; i < LENGTH(BLKDEV); i++)
    {
      read_file(&DISKSTATS[i], blkdev_cb, DISKSTAT);
      fprintf(stdout, "%s%s ", DELIM, BLKDEV[i]);
      fprintf(stdout, "%s%s", UP, format_units(read_kbps(&DISKSTATS[i], interval)));
      fprintf(stdout, "%s%s", DOWN, format_units(write_kbps(&DISKSTATS[i], interval)));
    }
    
    fprintf(stdout, "%s", DELIM);
    for (unsigned i = 0; i < LENGTH(DIRECTORY); i++)
      fprintf(stdout, "%s %u%% ", DIRECTORY[i], du_perc(DIRECTORY[i]));

    for (unsigned i = 0; i < LENGTH(NETIF); i++)
    {
      read_file(&NET[i], net_cb, NET_ADAPTERS);
      read_file(&WLAN[i], wireless_cb, WIRELESS);
      fprintf(stdout, "%s%s ", DELIM, NET[i].netif);
      ssid(&WLAN[i].ssid);
      if (strlen(WLAN[i].ssid.SSID))
        fprintf(stdout, "%s %d%% ", WLAN[i].ssid.SSID, wireless_link(&WLAN[i]));

      fprintf(stdout, "%s%s", UP, format_units(up_kbps(&NET[i], interval)));
      fprintf(stdout, "(%s)", format_units(NET[i].TXbytes / kB));
      fprintf(stdout, "%s%s", DOWN, format_units(down_kbps(&NET[i], interval)));
      fprintf(stdout, "(%s)", format_units(NET[i].RXbytes / kB));
    }

    public_ip(&ip);
    if (!strcmp(ip.PREV, ip.CURR) || !strlen(ip.PREV))
      fprintf(stdout, "%s%s", DELIM, ip.CURR);
    else
      fprintf(stdout, "%s%s%s%s", DELIM, ip.PREV, RIGHT_ARROW, ip.CURR);
#ifdef PROC_ACPI
    read_file(&ac_state, ac_cb, ACPI_ACSTATE);
#else
    read_file(&ac_state, ac_cb, SYS_ACSTATE);
#endif
    batteries.total_perc = 0;
    for (unsigned i = 0; i < batteries.size; i++)
    {
      read_file(&batteries.battery[i], battery_state_cb, batteries.battery[i].STATEFILE);
      fprintf(stdout, "%s%s %d%% %c", DELIM, 
          batteries.battery[i].BAT, 
          batteries.battery[i].perc, 
          batteries.battery[i].state);
      batteries.total_perc += batteries.battery[i].perc;
    }
    
    for (unsigned i = 0; i < asound_cards.size; i++)
    {
      asound_cards.card[i].P_SND[0] = '\0';
      asound_cards.card[i].C_SND[0] = '\0';
      read_file(asound_cards.card[i].P_SND, snd_cb, asound_cards.card[i].P_STATEFILE);
      read_file(asound_cards.card[i].C_SND, snd_cb, asound_cards.card[i].C_STATEFILE);
      fprintf(stdout, "%s%s%s%s%s", 
        DELIM, SNDSYM, asound_cards.card[i].P_SND, MICSYM, asound_cards.card[i].C_SND);
    }
    date(TIME, sizeof TIME);
    fprintf(stdout, "%s%s\n", DELIM, TIME);
    /* Wait */
    interval = ac_state ? UPDATE_INTV_ON_BATTERY : UPDATE_INTV;
    sleep(interval);
  }

  deinit_status();
  return 0;
}
