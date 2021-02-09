#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "status.h"

static const char *BLKDEV[] = { BLKDEVS };
static const char *DIRECTORY[] = { DIRECTORIES };
static const char *NETIF[] = { NETIFS };

static bool quit;
static unsigned char interval = UPDATE_INTV;

void set_quit_flag(const int signo)
{
  (void) signo;
  quit = 1;
  fprintf(stdout, "Exit..\n");
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
    fprintf(stdout, "%s%dW", PWRSYM, power(interval));
    fprintf(stdout, "%s%.0lf%% %.0fMHz", DELIM, cpu_perc(), cpu_mhz());
    fprintf(stdout, "%s%.0f%% (%s)", DELIM, mem_perc(), format_units(mem_swap()));
    
    for (unsigned i = 0; i < LENGTH(BLKDEV); i++)
    {
      fprintf(stdout, "%s%s ", DELIM, BLKDEV[i]);
      refresh_diskstats(i);
      fprintf(stdout, "%s%s", UP, format_units(read_kbps(i, interval)));
      fprintf(stdout, "%s%s", DOWN, format_units(write_kbps(i, interval)));
    }
    
    fprintf(stdout, "%s", DELIM);
    for (unsigned i = 0; i < LENGTH(DIRECTORY); i++)
      fprintf(stdout, "%s %u%% ", DIRECTORY[i], du_perc(DIRECTORY[i]));

    for (unsigned i = 0; i < LENGTH(NETIF); i++)
    {
      fprintf(stdout, "%s%s ", DELIM, NETIF[i]);
      if (ssid(i))
        fprintf(stdout, "%s %d%% ", ssid_string(i), wireless_link(i));

      refresh_netadapter(i);
      fprintf(stdout, "%s%s", UP, format_units(tx_kbps(i, interval)));
      fprintf(stdout, "(%s)", format_units(tx_total_kb(i)));
      fprintf(stdout, "%s%s", DOWN, format_units(rx_kbps(i, interval)));
      fprintf(stdout, "(%s)", format_units(rx_total_kb(i)));
    }

    refresh_publicip();
    if (!strcmp(prev_ip(), curr_ip()))
      fprintf(stdout, "%s%s", DELIM, curr_ip());
    else
      fprintf(stdout, "%s%s%s%s", DELIM, prev_ip(), RIGHT_ARROW, curr_ip());
    
    refresh_ps();
    /*
    for (unsigned i = 0; i < batteries_size(); i++)
    {
      refresh_battery(i);
      fprintf(stdout, "%s%s %d%% %c", DELIM, 
          battery_string(i), 
          battery_perc(i), 
          battery_state(i));
    }
    */
    refresh_batteries();
    fprintf(stdout, "%s%s %d%% %c", DELIM, "BAT", batteries_perc(), batteries_state());
    
    for (unsigned i = 0; i < asound_cards_size(); i++)
    {
      refresh_asound_card(i);
      fprintf(stdout, "%s%s%s%s%s", 
        DELIM, SNDSYM, asound_card_p(i), MICSYM, asound_card_c(i));
    }
    fprintf(stdout, "%s%s\n", DELIM, date());
    /* Wait */
    interval = ac() ? UPDATE_INTV_ON_BATTERY : UPDATE_INTV;
    sleep(interval);
  }

  deinit_status();
  return 0;
}
