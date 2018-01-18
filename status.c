#include <stdbool.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <X11/Xlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <curl/curl.h>
#include "config.h"

#define LENGTH(X)								(sizeof X / sizeof X[0])
#define SIZE										64

#define POWER_SUPPLIES					"/sys/class/power_supply/"
#define CPU											"/proc/cpuinfo"
#define RAM											"/proc/meminfo"
#define STAT										"/proc/stat"
#define DISKSTAT								"/proc/diskstats"
#define NET_ADAPTERS						"/proc/net/dev"
#define WIRELESS								"/proc/net/wireless"

#define kB											1024
#define mB											(kB * kB)
#define gB											(kB * mB)

#define UP_ARROW								"\u2b06"
#define DOWN_ARROW							"\u2b07"
#define RIGHT_ARROW							"\u27a1"
#define FULL_SPACE							"\u2000"
#define SHORT_SPACE							"\u2005"
#define HEAVY_HORIZONTAL				"\u2501"
#define HEAVY_VERTICAL					"\u2503"
#define DOUBLE_VERTICAL					"\u2551"
#define LF_THREE_EIGHTHS_BLOCK	" \u258d"
#define LIGHT_SHADE							"\u2591"
#define MEDIUM_SHADE						" \u2592 "
#define DARK_SHADE							" \u2593 "
#define SEPERATOR								LF_THREE_EIGHTHS_BLOCK

typedef struct
{
	unsigned long user;
	unsigned long nice;
	unsigned long system;
	unsigned long idle;
	unsigned long iowait;
	unsigned long irq;
	unsigned long softirq;
	double perc;
	unsigned int processor;
	float mhz;
} Cpu;

typedef struct
{
	unsigned long MemTotal;
	unsigned long MemFree;
	unsigned long MemAvailable;
	unsigned long Buffers;
	unsigned long Cached;
	unsigned long SwapCached;
	unsigned long SwapTotal;
	unsigned long SwapFree;
} Mem;

typedef struct
{
	char dev_name[SIZE];
	unsigned long rd_sec_or_wr_ios;
	unsigned long wr_sec;
	const char *disk;
	double read_kBs;
	double write_kBs;
} Diskstats;

typedef struct
{
	char if_name[SIZE];
	unsigned long RX_bytes;
	unsigned long TX_bytes;
	const char *net_if;
	float down_kbytes;
	float up_kbytes;
} Net;

typedef struct
{
	char if_name[SIZE];
	unsigned int link;
	Net *ns;
	char essid[SIZE];
} Wireless;

typedef struct
{
	CURL *handle;
	char buffer[SIZE];
} IP;

typedef struct
{
	unsigned char capacity;
	char capacity_level;
	unsigned long charge_full;
	unsigned long charge_now;
	char status;
} BAT;

typedef struct
{
	bool AC_online;
	BAT b;
} PS;

char status[STRLEN];
char tmp[STRLEN];
unsigned short interval;
bool quit;

inline static void difftimespec(struct timespec *res, struct timespec *b, struct timespec *a)
{
	res->tv_sec = b->tv_sec - a->tv_sec - (b->tv_nsec < a->tv_nsec);
	res->tv_nsec = b->tv_nsec - a->tv_nsec + (b->tv_nsec < a->tv_nsec);
}

inline static void read_file(const char *file, void *callback(), void *data)
{
	FILE *fp;
	char line[STRLEN];

	if (!(fp = fopen(file, "r")))
	{
		fprintf(stderr, "Unable to open file\n");
		return;
	}

	while (fgets(line, sizeof line, fp))
		callback(line, data);

	fclose(fp);
}

inline static void read_dir(const char *dir, void *callback(), void *data)
{
	DIR *dp;
	struct dirent *d;

	if (!(dp = opendir(dir)))
	{
		fprintf(stderr, "Unable to open dir\n");
		return;
	}

	while ((d = readdir(dp)))
		callback(d->d_name, data);

	closedir(dp);
}

inline static void date(void)
{
	time_t t = time(NULL);
	strftime(tmp, sizeof(tmp), "%l:%M %a %d %b", localtime(&t));
	sprintf(status, "%s%s%s", status, SEPERATOR, tmp);
}

inline static void *callback_BAT_capacity(const char *string, void *data)
{
	BAT *b = data;
	b->capacity = strtod(string, NULL);
	return NULL;
}

inline static void *callback_BAT_capacity_level(const char *string, void *data)
{
	BAT *b = data;
	b->capacity_level = string[0];
	return NULL;
}

inline static void *callback_BAT_charge_full(const char *string, void *data)
{
	BAT *b = data;
	b->charge_full = strtol(string, NULL, 10);
	return NULL;
}

inline static void *callback_BAT_charge_now(const char *string, void *data)
{
	BAT *b = data;
	b->charge_now = strtol(string, NULL, 10);
	return NULL;
}

inline static void *callback_BAT_status(const char *string, void *data)
{
	BAT *b = data;
	b->status = string[0];
	return NULL;
}

inline static void *callback_AC(const char *string, void *data)
{
	PS *ps = data;
	ps->AC_online = strtod(string, NULL);
	return NULL;
}

inline static void *callback_ps(const char *unit_dev, void *data)
{
	PS *ps = data;
	sprintf(tmp, "%s%s/", POWER_SUPPLIES, unit_dev);

	if (!strcmp(unit_dev, "AC"))
	{
		strcat(tmp, "online");
		read_file(tmp, callback_AC, ps);

		if (ps->AC_online)
			interval = UPDATE_INTV;

		else interval = UPDATE_INTV_ON_BATTERY;
	}

	else if (!strncmp(unit_dev, "BAT", 3 * sizeof(char)))
	{
		char battery_prop[STRLEN];
		sprintf(battery_prop, "%s%s", tmp, "capacity");
		read_file(battery_prop, callback_BAT_capacity, &ps->b);
		sprintf(battery_prop, "%s%s", tmp, "capacity_level");
		read_file(battery_prop, callback_BAT_capacity_level, &ps->b);
		sprintf(battery_prop, "%s%s", tmp, "charge_full");
		read_file(battery_prop, callback_BAT_charge_full, &ps->b);
		sprintf(battery_prop, "%s%s", tmp, "charge_now");
		read_file(battery_prop, callback_BAT_charge_now, &ps->b);
		sprintf(battery_prop, "%s%s", tmp, "status");
		read_file(battery_prop, callback_BAT_status, &ps->b);

		if (ps->b.status == 'F')
			sprintf(status, "%s%s%c", status, SEPERATOR, ps->b.status);

		else sprintf(status, "%s%s%c %u%%", status, SEPERATOR, ps->b.status, ps->b.capacity);
	}

	return NULL;
}

inline static void ps(void)
{
	PS ps;
	read_dir(POWER_SUPPLIES, callback_ps, &ps);
}

inline static void str_append(char *string, const char *sym, float numeric)
{
	/* Function expects numeric to be in kB units */
	if (numeric * kB < kB)
		sprintf(string, "%s %s%.0fB", string, sym, numeric * kB);

	else if (numeric * kB >= kB && numeric * kB < mB)
		sprintf(string, "%s %s%.0fK", string, sym, numeric);

	else if (numeric * kB >= mB && numeric * kB < gB)
		sprintf(string, "%s %s%.1fM", string, sym, numeric / kB);

	else if (numeric * kB >= gB)
		sprintf(string, "%s %s%.1fG", string, sym, numeric / mB);
}

inline static const char *tail(const char *file, unsigned char n)
{
	FILE *fp;
	static char line[STRLEN];
	unsigned char i = 1;
	signed char err;

	if (!(fp = fopen(file, "a+")))
	{
		fprintf(stderr, "Unable to open file\n");
		return NULL;
	}

	fseek(fp, -sizeof(char), SEEK_END);

	while (i < n && !(err = fseek(fp, -2 * sizeof(char), SEEK_CUR)))
		if (fgetc(fp)  == '\n')
			i++;

	if (err)
		fseek(fp, 0L, SEEK_SET);

	fgets(line, sizeof line, fp);
	fclose(fp);	
	line[strlen(line) - 1] = '\0';
	return line;
}

void *callback_idempotent(const char *line, void *data)
{
	(void) *line;
	(void) data;
	return NULL;
}

inline static void public_ip(IP *ip)
{
	char prev_ip[SIZE];
	unsigned int d[4];
	CURLcode result = curl_easy_perform(ip->handle);

	if (result != CURLE_OK ||
		sscanf(ip->buffer, "%3d.%3d.%3d.%3d", &d[0], &d[1], &d[2], &d[3]) != 4)
		return;

	strcpy(prev_ip, tail(iplist, 2));

	if (strcmp(ip->buffer, prev_ip))
	{
		FILE *fp;

		if (!(fp = fopen(iplist, "a+")))
			return;

		fprintf(fp, "%s\n", ip->buffer);
		fclose(fp);
	}

	else strcpy(prev_ip, tail(iplist, 3));

	if (!strcmp(prev_ip, ip->buffer))
		sprintf(status, "%s%s%s", status, SEPERATOR, ip->buffer);

	else
		sprintf(status, "%s%s%s%s%s", status, SEPERATOR, prev_ip, RIGHT_ARROW, ip->buffer);
}

inline static void *callback_net(const char *line, void *data)
{
	Net *ns = data, tmp;

	sscanf(line, "%s %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ] %*[^ ]",
		tmp.if_name,
		&tmp.RX_bytes,
		&tmp.TX_bytes);

	if (strncmp(tmp.if_name, ns->net_if, strlen(ns->net_if)) == 0)
	{
		tmp.RX_bytes /= kB;
		tmp.TX_bytes /= kB;
		ns->down_kbytes = (tmp.RX_bytes - ns->RX_bytes);
		ns->up_kbytes = (tmp.TX_bytes - ns->TX_bytes);
		ns->RX_bytes = tmp.RX_bytes;
		ns->TX_bytes = tmp.TX_bytes;
	}

	return NULL;
}

inline static void *callback_wireless(const char *line, void *data)
{
	Wireless *w = data, tmp;
	sscanf(line, "%s %*[^ ] %d", tmp.if_name, &tmp.link);

	if (strncmp(tmp.if_name, w->ns->net_if, strlen(w->ns->net_if)) == 0)
	{
		strncpy(w->if_name, tmp.if_name, strlen(w->ns->net_if));
		w->link = tmp.link;
	}

	else w->link = 0;

	return NULL;
}

inline static void net(Net *ns)
{
	register unsigned char i, N = LENGTH(net_if);
	Wireless w[N];

	for (i = 0; i < N; i++)
	{
		ns[i].net_if = net_if[i];
		read_file(NET_ADAPTERS, callback_net, &ns[i]);
		w[i].ns = &ns[i];
		read_file(WIRELESS, callback_wireless, &w[i]);

		if (strcmp(ns[i].net_if, w[i].if_name) == 0 && w[i].link)
			sprintf(status, "%s%s%s %.0f%%", status, SEPERATOR, ns[i].net_if, (w[i].link / 70.) * 100);

		else
			sprintf(status, "%s%s%s", status, SEPERATOR, ns[i].net_if);

		str_append(status, UP_ARROW, ns[i].up_kbytes);
		str_append(status, "", ns[i].TX_bytes);
		str_append(status, DOWN_ARROW, ns[i].down_kbytes);
		str_append(status, "", ns[i].RX_bytes);
	}
}

inline static void du(void)
{
	struct statvfs fs;
	register unsigned char i, N = LENGTH(dir), perc;

	for (i = 0; i < N; i++)
	{
		if (statvfs(dir[i], &fs) < 0)
		{
			printf("Unable to get fs info\n");
			continue;
		}

		perc = (1 - ((float) fs.f_bfree / (float) fs.f_blocks)) * 100;
		sprintf(status, "%s %s %u%%", status, dir[i], perc);
 	}
}

inline static void *callback_io(const char *line, void *data)
{
	Diskstats *ds = data, tmp;
	sscanf(line, " %*[^ ] %*[^ ] %s %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %lu %*[^ ] %*[^ ] %*[^ ] %*[^ ]",
		tmp.dev_name,
		&tmp.rd_sec_or_wr_ios,
		&tmp.wr_sec);

	if (strcmp(tmp.dev_name, ds->disk) == 0)
	{
		ds->read_kBs = (tmp.rd_sec_or_wr_ios - ds->rd_sec_or_wr_ios) / 2.;
		ds->write_kBs = (tmp.wr_sec - ds->wr_sec) / 2.;
		ds->rd_sec_or_wr_ios =  tmp.rd_sec_or_wr_ios;
		ds->wr_sec = tmp.wr_sec;
	}

	return NULL;
}

inline static void io(Diskstats *ds)
{
	register unsigned char i, N = LENGTH(disk);
	sprintf(status, "%s%s", status, SEPERATOR);

	for (i = 0; i < N; i++)
	{
		ds[i].disk = disk[i];
		read_file(DISKSTAT, callback_io, &ds[i]);
		strcat(status, disk[i]);
		str_append(status, UP_ARROW, ds[i].read_kBs);
		str_append(status, DOWN_ARROW, ds[i].write_kBs);
		strcat(status, " ");
	}
}

inline static void *callback_mem(const char *line, void *data)
{
	Mem *m = data;

	sscanf(line, "MemTotal: %lu", &m->MemTotal);
	sscanf(line, "MemFree: %lu", &m->MemFree);
	sscanf(line, "MemAvailable: %lu", &m->MemAvailable);
	sscanf(line, "Buffers: %lu", &m->Buffers);
	sscanf(line, "Cached: %lu", &m->Cached);
	sscanf(line, "SwapCached: %lu", &m->SwapCached);
	sscanf(line, "SwapTotal: %lu", &m->SwapTotal);
	sscanf(line, "SwapFree: %lu", &m->SwapFree);

	return NULL;
}

inline static void mem(void)
{
	Mem m;
	read_file(RAM, callback_mem, &m);
	float perc = (float) (m.MemTotal - m.MemFree - m.Buffers - m.Cached) / m.MemTotal * 100;
	float swap = m.SwapTotal - m.SwapFree - m.SwapCached;
	sprintf(status, "%s%s%.0f%%", status, SEPERATOR, perc);
	str_append(status, "\0", swap);
}

inline static void *callback_cpu(const char *line, void *data)
{
	Cpu *c = data, tmp;

	if (sscanf(line, "cpu%*[^0-9] %lu %lu %lu %lu %lu %lu %lu",
		&tmp.user,
		&tmp.nice,
		&tmp.system,
		&tmp.idle,
		&tmp.iowait,
		&tmp.irq,
		&tmp.softirq) == 7)
	{
		c->perc = (double) (tmp.user + tmp.nice + tmp.system + tmp.irq + tmp.softirq
			- c->user - c->nice - c->system - c->irq - c->softirq) /
			 (double) (tmp.user + tmp.nice + tmp.system + tmp.idle + tmp.iowait + tmp.irq + tmp.softirq
			- c->user - c->nice - c->system - c->idle - c->iowait - c->irq - c->softirq) * 100.;

		c->user = tmp.user;
		c->nice = tmp.nice;
		c->system = tmp.system;
		c->idle = tmp.idle;
		c->iowait = tmp.iowait;
		c->irq = tmp.irq;
		c->softirq = tmp.softirq;
	}

	else if (sscanf(line, "processor : %d", &tmp.processor) == 1)
		c->processor = tmp.processor;

	else if (sscanf(line, "cpu MHz : %f", &tmp.mhz) == 1)
		/* Calculate the rolling average wrt to number of cores */
		c->mhz = (c->mhz * (c->processor) + tmp.mhz) / (c->processor + 1);

	return NULL;
}

inline static void cpu(Cpu *c)
{
	read_file(CPU, callback_cpu, c);
	read_file(STAT, callback_cpu, c);
	sprintf(status, "%.0f%% %.0fMHz", c->perc, c->mhz);
}

inline static size_t writefunc(void *ptr, size_t size, size_t nmemb, void *data)
{
	IP *ip = data;
	size_t S = size * nmemb;

	if (S < sizeof ip->buffer)
	{
		memset(ip->buffer, 0, S);
		memcpy(ip->buffer, ptr, S);
		ip->buffer[S] = '\0';
	}

	else ip->buffer[0] = '\0';

	return S;
}

void deinit_curl(IP *ip)
{
	curl_easy_cleanup(ip->handle);
}

void init_curl(IP *ip)
{
	curl_global_init(CURL_GLOBAL_ALL);
	ip->handle = curl_easy_init();
	curl_easy_setopt(ip->handle, CURLOPT_URL, check_ip_url[0]);
	curl_easy_setopt(ip->handle, CURLOPT_WRITEFUNCTION, writefunc);
	curl_easy_setopt(ip->handle, CURLOPT_WRITEDATA, ip);
	curl_easy_setopt(ip->handle, CURLOPT_TIMEOUT, 1L);
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

	struct timespec start, current, diff, intspec, wait;
	struct sigaction SA;
	memset(&SA, 0, sizeof SA);
	SA.sa_handler = set_quit_flag;
	sigaction(SIGINT,  &SA, NULL);
	sigaction(SIGTERM, &SA, NULL);
	setlocale(LC_ALL, "");
	Display *dpy;

	if (!(dpy = XOpenDisplay(NULL)))
	{
		fprintf(stderr, "Cannot open display\n");
		return 1;
	}

	Cpu c;
	Diskstats ds[LENGTH(disk)];
	Net ns[LENGTH(net_if)];
	IP ip;
	init_curl(&ip);

	while (!quit)
	{
		clock_gettime(CLOCK_MONOTONIC, &start);
		status[0] = '\0';

		// Getter Functions
		cpu(&c);
		mem();
		io(ds);
		du();
		net(ns);
		public_ip(&ip);
		ps();
		date();

		// Hack to space the string from the tray
		strcat(status, FULL_SPACE);

		// Update the root display with the string status
		XStoreName(dpy, DefaultRootWindow(dpy), status);
		XSync(dpy, False);

		// Wait
		clock_gettime(CLOCK_MONOTONIC, &current);
		difftimespec(&diff, &current, &start);
		intspec.tv_sec = interval;
		intspec.tv_nsec = (interval % 1000) * 1000;
		difftimespec(&wait, &intspec, &diff);

		if (wait.tv_sec >= 0)
			nanosleep(&wait, NULL);
	}

	deinit_curl(&ip);
	XStoreName(dpy, DefaultRootWindow(dpy), NULL);
	XCloseDisplay(dpy);
	return 0;
}
