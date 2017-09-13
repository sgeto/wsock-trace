/*
 * '/etc/hosts' parsing for wsock_trace.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "init.h"
#include "smartlist.h"
#include "in_addr.h"
#include "hosts.h"

struct host_entry {
       char host_name [MAX_HOST_LEN];  /* name of 'etc/hosts' entry */
       int  addr_size;                 /* size of this adresses (4 or 16) */
       int  addr_type;                 /* type AF_INET or AF_INET6 */
       char addr [IN6ADDRSZ];          /* the actual address */
     };

static smartlist_t *hosts_list = (smartlist_t*) -1;

/*
 * Add an entry to the 'hosts_list'.
 */
static void add_entry (const char *name, const char *ip, const void *addr, size_t size, int af_type)
{
  struct host_entry *he = calloc (1, sizeof(*he));

  if (!he)
     return;

  assert (hosts_list != (smartlist_t*) -1);
  assert (hosts_list != NULL);
  assert (size <= sizeof(struct in6_addr));

  he->addr_type = af_type;
  he->addr_size = size;
  _strlcpy (he->host_name, name, sizeof(he->host_name));
  memcpy (&he->addr, addr, size);
  smartlist_add (hosts_list, he);
}

/*
 * smartlist_sort() helper:
 *   compare on names.
 */
static int hosts_compare_name (const void **_a, const void **_b)
{
  const struct host_entry *a = *_a;
  const struct host_entry *b = *_b;

  if (a->addr_type == b->addr_type)
     return strcmp (a->host_name, b->host_name);

  /* This will cause AF_INET6 addresses to come last.
   */
  return (a->addr_type - b->addr_type);
}

/*
 * smartlist_bsearch() helper; compare on name.
 */
static int hosts_bsearch_name (const void *key, const void **member)
{
  const struct host_entry *he = *member;
  const char              *name = key;
  int   rc = strcmp (name, he->host_name);

  TRACE (3, "name: %-30s, he->host_name: %-30s, rc: %d\n", name, he->host_name, rc);
  return (rc);
}

/*
 * Parse the file for lines matching "ip host".
 * Do not care about aliases.
 *
 * Note: the Windows 'hosts' file support both AF_INET and AF_INET6 addresses.
 *       That's the reason we set 'call_WSASetLastError = FALSE'. Since passing
 *       an IPv6-addresses to 'wsock_trace_inet_pton4()' will call 'WSASetLastError()'.
 *       And vice-versa.
 */
static void parse_hosts (FILE *fil)
{
  while (1)
  {
    struct in_addr  in4;
    struct in6_addr in6;

    char  buf[500];
    char *p, *ip, *name, *tok_buf;

    if (!fgets(buf,sizeof(buf)-1,fil))   /* EOF */
       break;

    strip_nl (buf);
    for (p = buf ; *p && isspace((int)*p); )
        p++;

    if (*p == '\0' || *p == '#' || *p == ';')
       continue;

    ip   = _strtok_r (p, " \t", &tok_buf);
    name = _strtok_r (NULL, " \t", &tok_buf);

    if (!name || !ip)
       continue;

    if (wsock_trace_inet_pton4(ip, (u_char*)&in4) == 1)
         add_entry (name, ip, &in4, sizeof(in4), AF_INET);
    else if (wsock_trace_inet_pton6(ip, (u_char*)&in6) == 1)
         add_entry (name, ip, &in6, sizeof(in6), AF_INET6);
  }
  smartlist_sort (hosts_list, hosts_compare_name);
}

/*
 * Print the 'hosts_list' if 'g_cfg.trace_level >= 3'.
 */
static void hosts_file_dump (void)
{
  int i, max = smartlist_len (hosts_list);

  for (i = 0; i < max; i++)
  {
    const struct host_entry *he = smartlist_get (hosts_list, i);
    char  buf [MAX_IP6_SZ];

    wsock_trace_inet_ntop (he->addr_type, he->addr, buf, sizeof(buf));
    trace_printf ( "%3d: host: %-40s addr: %s\n", i, he->host_name, buf);
  }
}

/*
 * Free the memory in 'hosts_list' and free the list itself.
 */
void hosts_file_exit (void)
{
  int i, max;

  if (!hosts_list || hosts_list == (smartlist_t*)-1)
     return;

  max = smartlist_len (hosts_list);
  for (i = 0; i < max; i++)
      free (smartlist_get(hosts_list, i));

  smartlist_free (hosts_list);
  hosts_list = NULL;
}

void hosts_file_init (void)
{
  FILE *fil;

  if (!g_cfg.hosts_file)
     return;

  fil = fopen (g_cfg.hosts_file, "r");
  if (fil)
  {
    hosts_list = smartlist_new();
    if (!hosts_list)
    {
      fclose (fil);
      return;
    }

    /* Cannot call 'WSASetLastError()' in in_addr.c before we're fully unitialised.
     */
    call_WSASetLastError = FALSE;
    parse_hosts (fil);
    fclose (fil);

    if (g_cfg.trace_level >= 3)
       hosts_file_dump();

    call_WSASetLastError = TRUE;
  }
}

/*
 * Check if one of the addresses for 'name' is from the hosts-file.
 */
int hosts_file_check (const char *name, const struct hostent *host)
{
  const char              **addresses = (const char**) host->h_addr_list;
  const struct host_entry  *he;
  int                       i, num;

  /* This should never happen
   */
  if (!hosts_list || hosts_list == (smartlist_t*)-1)
     return (0);

  /* Do a binary search in the 'hosts_list'.
   */
  he = smartlist_bsearch (hosts_list, name, hosts_bsearch_name);
  if (!he)
     return (0);

  for (i = num = 0; addresses && addresses[i]; i++)
  {
    if (he->addr_type == host->h_addrtype &&
        !memcmp(addresses[i], &he->addr, he->addr_size))
       num++;
  }
  return (num);
}