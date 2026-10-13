#include <rfb/rfbclient.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void fail(const char *message)
{
  fprintf(stderr, "%s\n", message);
  exit(1);
}

static void assert_u32(uint32_t actual, uint32_t expected, const char *message)
{
  if (actual != expected) {
    fprintf(stderr, "%s: got 0x%08x expected 0x%08x\n", message, actual, expected);
    exit(1);
  }
}

static uint32_t pixel_value(int x, int y)
{
  return (uint32_t)(0x01000000u | ((uint32_t)y << 8) | (uint32_t)x);
}

int main(void)
{
  rfbClient *client = rfbGetClient(8, 3, 4);
  uint32_t raw[6 * 5];
  uint32_t *fb;
  int x, y;

  if (!client)
    fail("rfbGetClient failed");

  client->width = 100;
  client->height = 80;

  if (rfbClientSetFrameBufferViewport(client, -1, 20, 4, 3))
    fail("negative viewport x was accepted");
  if (rfbClientSetFrameBufferViewport(client, 10, 20, 0, 3))
    fail("zero viewport width was accepted");
  if (rfbClientSetFrameBufferViewport(client, 98, 20, 4, 3))
    fail("out-of-bounds viewport was accepted");

  if (!rfbClientSetFrameBufferViewport(client, 10, 20, 4, 3))
    fail("rfbClientSetFrameBufferViewport failed");

  if (!client->useFrameBufferViewport)
    fail("viewport flag was not enabled");
  if (client->updateRect.x != 10 || client->updateRect.y != 20 ||
      client->updateRect.w != 4 || client->updateRect.h != 3)
    fail("viewport did not update updateRect");

  rfbClientClearFrameBufferViewport(client);
  if (client->useFrameBufferViewport)
    fail("pre-allocation clear did not disable viewport");
  if (client->updateRect.x != 0 || client->updateRect.y != 0 ||
      client->updateRect.w != client->width || client->updateRect.h != client->height)
    fail("pre-allocation clear did not restore full updateRect");

  if (!rfbClientSetFrameBufferViewport(client, 10, 20, 4, 3))
    fail("rfbClientSetFrameBufferViewport after clear failed");

  if (!client->MallocFrameBuffer(client))
    fail("viewport framebuffer allocation failed");

  if (rfbClientSetFrameBufferViewport(client, 0, 0, 2, 2))
    fail("post-allocation viewport change was accepted");

  rfbClientClearFrameBufferViewport(client);
  if (!client->useFrameBufferViewport)
    fail("post-allocation clear disabled viewport unsafely");
  if (client->updateRect.x != 10 || client->updateRect.y != 20 ||
      client->updateRect.w != 4 || client->updateRect.h != 3)
    fail("post-allocation clear changed updateRect unsafely");

  fb = (uint32_t *)client->frameBuffer;
  for (y = 0; y < 3; y++)
    for (x = 0; x < 4; x++)
      fb[y * 4 + x] = 0;

  for (y = 0; y < 5; y++)
    for (x = 0; x < 6; x++)
      raw[y * 6 + x] = pixel_value(8 + x, 19 + y);

  client->GotBitmap(client, (const uint8_t *)raw, 8, 19, 6, 5);

  assert_u32(fb[0], pixel_value(10, 20), "top-left clipped raw pixel");
  assert_u32(fb[3], pixel_value(13, 20), "top-right clipped raw pixel");
  assert_u32(fb[8], pixel_value(10, 22), "bottom-left clipped raw pixel");
  assert_u32(fb[11], pixel_value(13, 22), "bottom-right clipped raw pixel");

  client->GotFillRect(client, 12, 21, 10, 10, 0xdeadbeefu);

  assert_u32(fb[1 * 4 + 1], pixel_value(11, 21), "pixel before clipped fill");
  assert_u32(fb[1 * 4 + 2], 0xdeadbeefu, "first clipped fill pixel");
  assert_u32(fb[1 * 4 + 3], 0xdeadbeefu, "second clipped fill pixel");
  assert_u32(fb[2 * 4 + 2], 0xdeadbeefu, "bottom clipped fill pixel");

  free(client->frameBuffer);
  client->frameBuffer = NULL;
  rfbClientCleanup(client);
  return 0;
}
