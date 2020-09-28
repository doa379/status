#define	STRLEN			1024
#define UPDATE_INTV_ON_BATTERY 	1
#define UPDATE_INTV		1
#define MAX_BATTERIES 2

static const char *BLKDEV[] = { "nvme0n1" };
static const char *DIRECTORY[] = { "/", "/tmp" };
static const char *NETIF[] = { "eth0", "wlan0", /*"wlan1", "wlan2"*/ };
static const char *IPLIST = "/tmp/iplist";
static const char *IPURL[] = {
	"http://whatismyip.akamai.com",
	"http://checkip.amazonaws.com",
	"http://ipinfo.io/ip",
	"http://ipecho.net/plain",
};
