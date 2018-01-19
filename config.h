#define	STRLEN			1024
#define UPDATE_INTV_ON_BATTERY 	15
#define UPDATE_INTV		2

static const char *disk[] = { "sdb" };
static const char *dir[] = { "/", "/tmp" };
static const char *net_if[] = { "eth0", "wlan0" };
static const char *iplist = "/tmp/iplist";
static const char *check_ip_url[] = {
	"http://whatismyip.akamai.com",
	"http://checkip.amazonaws.com",
	"http://ipinfo.io/ip",
	"http://ipecho.net/plain",
};

