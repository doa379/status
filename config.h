#define	STRLEN			1024
#define UPDATE_INTV_ON_BATTERY 	2
#define UPDATE_INTV		2
#define MAX_BATTERIES 2

static const char *BLKDEV[] = { "nvme0n1" };
static const char *DIRECTORY[] = { "/", "/tmp" };
static const char *NETIF[] = { "eth0", "wlan0" };
static const char *IPLIST = "/tmp/iplist";
static const char *IPURL[] = {
	"http://whatismyip.akamai.com",
	"http://checkip.amazonaws.com",
	"http://ipinfo.io/ip",
	"http://ipecho.net/plain",
};
