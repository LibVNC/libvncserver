#include <rfb/rfb.h>
#include <rfb/rfbregion.h>

#include "private.h"

#include <stdio.h>
#include <string.h>

static void
init_cursor_client(rfbScreenInfoPtr screen, rfbClientPtr client, rfbCursorPtr cursor)
{
  memset(screen, 0, sizeof(*screen));
  memset(client, 0, sizeof(*client));
  memset(cursor, 0, sizeof(*cursor));

  screen->width = 100;
  screen->height = 100;
  screen->cursor = cursor;

  cursor->width = 10;
  cursor->height = 10;
  cursor->xhot = 0;
  cursor->yhot = 0;

  client->screen = screen;
  client->cursorX = 0;
  client->cursorY = 0;
}

static int
expect_single_rect(sraRegionPtr region, int x1, int y1, int x2, int y2)
{
  sraRectangleIterator *iterator;
  sraRect rect;
  int failed = 0;

  if(sraRgnCountRects(region) != 1) {
    fprintf(stderr, "expected one rectangle, got %lu\n", sraRgnCountRects(region));
    return 1;
  }

  iterator = sraRgnGetIterator(region);
  if(!iterator || !sraRgnIteratorNext(iterator, &rect)) {
    fprintf(stderr, "failed to read region rectangle\n");
    if(iterator)
      sraRgnReleaseIterator(iterator);
    return 1;
  }

  if(rect.x1 != x1 || rect.y1 != y1 || rect.x2 != x2 || rect.y2 != y2) {
    fprintf(stderr, "expected rectangle %d,%d-%d,%d, got %d,%d-%d,%d\n",
            x1, y1, x2, y2, rect.x1, rect.y1, rect.x2, rect.y2);
    failed = 1;
  }

  sraRgnReleaseIterator(iterator);
  return failed;
}

static int
check_unrestricted_cursor_redraw(void)
{
  rfbScreenInfo screen;
  rfbClientRec client;
  rfbCursor cursor;
  sraRegionPtr update;
  int failed;

  init_cursor_client(&screen, &client, &cursor);

  update = sraRgnCreate();
  rfbRedrawAfterHideCursor(&client, update, NULL);
  failed = expect_single_rect(update, 0, 0, 10, 10);
  sraRgnDestroy(update);

  return failed;
}

static int
check_empty_requested_region(void)
{
  rfbScreenInfo screen;
  rfbClientRec client;
  rfbCursor cursor;
  sraRegionPtr update;
  sraRegionPtr requested;
  int failed = 0;

  init_cursor_client(&screen, &client, &cursor);

  update = sraRgnCreate();
  requested = sraRgnCreate();
  rfbRedrawAfterHideCursor(&client, update, requested);

  if(!sraRgnEmpty(update)) {
    fprintf(stderr, "empty requested region must not add cursor redraw rectangles\n");
    failed = 1;
  }

  sraRgnDestroy(requested);
  sraRgnDestroy(update);

  return failed;
}

static int
check_partially_requested_region(void)
{
  rfbScreenInfo screen;
  rfbClientRec client;
  rfbCursor cursor;
  sraRegionPtr update;
  sraRegionPtr requested;
  int failed;

  init_cursor_client(&screen, &client, &cursor);

  update = sraRgnCreate();
  requested = sraRgnCreateRect(5, 5, 15, 15);
  rfbRedrawAfterHideCursor(&client, update, requested);
  failed = expect_single_rect(update, 5, 5, 10, 10);

  sraRgnDestroy(requested);
  sraRgnDestroy(update);

  return failed;
}

int
main(void)
{
  int failed = 0;

  failed |= check_unrestricted_cursor_redraw();
  failed |= check_empty_requested_region();
  failed |= check_partially_requested_region();

  return failed;
}
