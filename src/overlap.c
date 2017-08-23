/*
 * overlap.c - Part of Wsock-Trace
 *
 * Functions for dealing with overlapped operations in WSA functions.
 * All winsock function that this can apply to is:
 *   AcceptEx(), ConnectEx(), DisconnectEx(), TransmitFile(),
 *   TransmitPackets(), WSARecv(), WSARecvFrom(), WSARecvMsg(),
 *   WSASend(), WSASendMsg(), WSASendTo(), and WSAIoctl().
 *
 * Allthough Wsock-Trace does support only a few of these.
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "init.h"
#include "smartlist.h"
#include "overlap.h"

#ifndef OV_TRACE_LEVEL
#define OV_TRACE_LEVEL 2
#endif

#define OV_TRACE(fmt, ...)                                       \
        do {                                                     \
          if (g_cfg.trace_level >= OV_TRACE_LEVEL)               \
             trace_printf ("%*.s" fmt, g_cfg.trace_indent+4, "", \
                           ## __VA_ARGS__);                      \
        } while (0)

func_WSAGetOverlappedResult  p_WSAGetOverlappedResult = NULL;

/*
 * Structure to remember a socket and overlapped structure.
 * Used in overlapped send and receive to update the recv
 * and transmit counters in 'WSAGetOverlappedResult()'.
 */
struct overlapped {
       /*
        * TRUE:  this is an overlapped WSARecv() or WSARecvFrom().
        * FALSE: this an overlapped WSASend() or WSASendTo().
        */
       BOOL is_recv;

       /* Number of bytes expected in WSARecv() / WSARecvFrom() or
        * number of bytes given in WSASend() or WSASendTo().
        */
       DWORD bytes;

       /* The socket, event and overlapped pointer the call was issued with.
        */
       SOCKET         sock;
       WSAEVENT       event;
       WSAOVERLAPPED *ov;
     };

static smartlist_t *ov_list;

void overlap_exit (void)
{
  struct overlapped *ov;
  int    i, max = smartlist_len (ov_list);

  if (g_cfg.trace_level >= 1 && max >= 1)
  {
    trace_printf ("%*.s%d overlapped tranfers not completed:\n",
                  g_cfg.trace_indent+2, "", max);
    for (i = 0; i < max; i++)
    {
      ov = smartlist_get (ov_list, i);
      trace_printf ("%*.sOverlap: 0x%p, hEvent: 0x%p, sock: %u, is_recv: %d, bytes: %lu\n",
                    g_cfg.trace_indent+4, "", ov->ov, ov->event, ov->sock, ov->is_recv, ov->bytes);
    }
  }

  for (i = 0; i < max; i++)
  {
    ov = smartlist_get (ov_list, i);
    free (ov);
  }
  smartlist_free (ov_list);
}

int overlap_init (void)
{
  ov_list = smartlist_new();
  return (1);
}

static void overlap_trace (int i, const struct overlapped *ov)
{
  if (i == -1)
  {
    int max = smartlist_len (ov_list);

    for (i = 0; i < max; i++)
    {
      ov = smartlist_get (ov_list, i);
      OV_TRACE ("overlapped[%d]: is_recv: %d, event: 0x%p, sock: %u\n",
                i, ov->is_recv, ov->event, ov->sock);
    }
  }
  else
    OV_TRACE ("overlapped[%d]: is_recv: %d, event: 0x%p, sock: %u\n",
              i, ov->is_recv, ov->event, ov->sock);
}

void overlap_store (SOCKET s, WSAOVERLAPPED *o, DWORD num_bytes, BOOL is_recv)
{
  struct overlapped *ov;
  int    i, max = smartlist_len (ov_list);
  BOOL   modify = FALSE;

  OV_TRACE ("%s(): Overlap: 0x%p, sock: %u, hEvent: 0x%p\n",
            __FUNCTION__, o, s, o ? o->hEvent : NULL);

  for (i = 0; i < max && !modify; i++)
  {
    ov = smartlist_get (ov_list, i);
    if (ov->ov == o && ov->sock == s && ov->is_recv == is_recv)
       modify = TRUE;
  }

  if (!modify)
  {
    ov = malloc (sizeof(*ov));
    if (!ov)
       return;
    ov->is_recv = is_recv;
    ov->sock    = s;
    ov->ov      = o;
  }

  ov->event = o ? o->hEvent : NULL;
  ov->bytes = num_bytes;
  if (!modify)
     smartlist_add (ov_list, ov);
  overlap_trace (-1, NULL);
}

/*
 * Try to update all overlapped operations matching this event.
 */
void overlap_recall_all (const WSAEVENT *event)
{
  int i;

  for (i = 0; i < smartlist_len(ov_list); i++)
  {
    const struct overlapped *ov = smartlist_get (ov_list, i);
    DWORD bytes = 0;
    BOOL  rc    = FALSE;

    if (p_WSAGetOverlappedResult && ov->event == event)
    {
      ENTER_CRIT();
      rc = (*p_WSAGetOverlappedResult) (ov->sock, ov->ov, &bytes, 0, NULL);
      LEAVE_CRIT();

      if (rc)
         overlap_recall (ov->sock, ov->ov, bytes);
    }
    if (i < smartlist_len(ov_list))
       OV_TRACE ("overlapped[%d]: event: 0x%p, is_recv: %d, rc: %d, got %lu bytes.\n",
                 i, ov->event, ov->is_recv, rc, bytes);
    else
      OV_TRACE ("overlapped[%d]: is_recv: %d, was recalled.\n", i, ov->is_recv);
  }
}

/*
 * Match the socket 's' and 'ov' value against a previous overlapped
 * 'WSARecvXX()' / 'WSASendXX()' and update the 'g_cfg.counts.recv_bytes'
 * or 'g_cfg.counts.send_bytes' statistics with the 'bytes' value.
 */
void overlap_recall (SOCKET s, const WSAOVERLAPPED *o, DWORD bytes)
{
  int i;

  for (i = 0; i < smartlist_len(ov_list); i++)
  {
    struct overlapped *ov = smartlist_get (ov_list, i);

    overlap_trace (i, ov);

    if (ov->ov != o || ov->sock != s)
       continue;

    if (ov->is_recv)
    {
      g_cfg.counts.recv_bytes += bytes;
      OV_TRACE ("overlapped[%d]: room for %lu bytes, got %lu bytes.\n",
                i, ov->bytes, bytes);
    }
    else
    {
      g_cfg.counts.send_bytes += bytes;
      OV_TRACE ("overlapped[%d]: sent %lu bytes, actual sent %lu bytes.\n",
                i, ov->bytes, bytes);
    }
    free (ov);
    smartlist_del (ov_list, i);
    break;
  }
}

/*
 * Remove a overlap entry matching socket 's'.
 */
void overlap_remove (SOCKET s)
{
  int i;

  if (!ov_list)
     return;

  for (i = 0; i < smartlist_len(ov_list); i++)
  {
    struct overlapped *ov = smartlist_get (ov_list, i);

    if (ov->sock == s)
    {
      free (ov);
      smartlist_del (ov_list, i);
    }
  }
  overlap_trace (-1, NULL);
}