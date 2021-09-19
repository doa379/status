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
  fprintf(stdout, "\nExit..\n");
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
    interval = ac() ? UPDATE_INTV_ON_BATTERY : UPDATE_INTV;
    fprintf(stdout, "%s%dW", PWRSYM, power(interval));
    fprintf(stdout, "%s%.0lf%% %.0fMHz", DELIM, cpu_perc(), cpu_mhz());
    fprintf(stdout, "%s%.0f%% (%s)", DELIM, mem_perc(), fmt_units(mem_swap()));
    fprintf(stdout, "%s", DELIM);
    for (unsigned i = 0; i < LENGTH(BLKDEV); i++)
    {
      fprintf(stdout, "%s", BLKDEV[i]);
      /**/
      read_diskstats(i);
      fprintf(stdout, "%s%s", UP, fmt_units(read_kbps(i, interval)));
      fprintf(stdout, "%s%s ", DOWN, fmt_units(write_kbps(i, interval)));
    }
    
    fprintf(stdout, "%c%s", '\b', DELIM);
    for (unsigned i = 0; i < LENGTH(DIRECTORY); i++)
      fprintf(stdout, "%s %u%% ", DIRECTORY[i], du_perc(DIRECTORY[i]));
    fprintf(stdout, "%c", '\b');

    for (unsigned i = 0; i < LENGTH(NETIF); i++)
    {
      fprintf(stdout, "%s", DELIM);
      if (ssid(i))
        fprintf(stdout, "%.5s..%d%%", ssid_string(i), wireless_link(i));
      else
        fprintf(stdout, "%s ", NETIF[i]);

      /**/
      read_netadapter(i);
      fprintf(stdout, "%s%s", UP, fmt_units(tx_kbps(i, interval)));
      fprintf(stdout, "(%s)", fmt_units(tx_total_kb(i)));
      fprintf(stdout, "%s%s", DOWN, fmt_units(rx_kbps(i, interval)));
      fprintf(stdout, "(%s)", fmt_units(rx_total_kb(i)));
    }

    fprintf(stdout, "%s%s", DELIM, public_ip());
    /**/
    read_batteries();
    fprintf(stdout, "%s%s %d%% %c", 
      DELIM, BATSYM, batteries_perc(), batteries_state() ? batteries_state() : '\b');
    if (batteries_perc() < BAT_THRESHOLD_VAL && fork() == 0)
    {
      char *args[] = { BAT_THRESHOLD_SPAWN, BAT_THRESHOLD_SPAWN_ARG, NULL };
      execvp(BAT_THRESHOLD_SPAWN, args);
      sleep(2);
    }
    for (unsigned i = 0; i < asound_cards_size(); i++)
      if (asound_card_p(i)[0] != '\0' || asound_card_c(i)[0] != '\0')
        fprintf(stdout, "%s%s%s", 
          DELIM, asound_card_p(i)[0] != '\0' ? SNDSYM : "", asound_card_c(i)[0] != '\0' ? MICSYM : "");

    fprintf(stdout, "%s%s\n", DELIM, date());
    sleep(interval);
  }

  deinit_status();
  return 0;
}
