/* -- avahi.c -- */

#include "x11vnc.h"

void avahi_initialise(void);
void avahi_advertise(const char *name, const char *host, const uint16_t port);
void avahi_reset(void);
void avahi_cleanup(void);

#if !defined(LIBVNCSERVER_HAVE_AVAHI) || !defined(LIBVNCSERVER_HAVE_LIBPTHREAD)
void avahi_initialise(void) {
	rfbLog("avahi_initialise: no Avahi support at buildtime.\n");
}
void avahi_advertise(const char *name, const char *host, const uint16_t port) {
	if (!name || !host || !port) {}
	rfbLog("avahi_advertise:  no Avahi support at buildtime.\n");
}
void avahi_reset(void) {
	rfbLog("avahi_reset: no Avahi support at buildtime.\n");
}
void avahi_cleanup(void) {
	rfbLog("avahi_cleanup: no Avahi support at buildtime.\n");
}
#else

#include <avahi-common/thread-watch.h>
#include <avahi-common/alternative.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>

static AvahiThreadedPoll *_poll = NULL;
static AvahiClient *_client = NULL;
static AvahiEntryGroup *_group = NULL;

static int db = 0;

void avahi_initialise(void) {
	int ret;

	if (getenv("AVAHI_DEBUG")) {
		db = 1;
	}

if (db) fprintf(stderr, "in  avahi_initialise\n");
	if (_poll) {
if (db) fprintf(stderr, "    avahi_initialise: poll not null\n");
		return;
	}

	if (! (_poll = avahi_threaded_poll_new()) ) {
		rfbLog("warning: unable to open Avahi poll.\n");
		return;
	}

	_client = avahi_client_new(avahi_threaded_poll_get(_poll),
	    0, NULL, NULL, &ret);
	if (! _client) {
		rfbLog("warning: unable to open Avahi client: %s\n",
		    avahi_strerror(ret));

		avahi_threaded_poll_free(_poll);
		_poll = NULL;
		return;
	}

	if (avahi_threaded_poll_start(_poll) < 0) {
		rfbLog("warning: unable to start Avahi poll.\n");
		avahi_client_free(_client);
		_client = NULL;
		avahi_threaded_poll_free(_poll);
		_poll = NULL;
		return;
	}
if (db) fprintf(stderr, "out avahi_initialise\n");
}

typedef struct {
	const char *name;
	const char *host;
	uint16_t port;
} avahi_service_t;

static void _avahi_create_services(const char *name, const char *host,
    const uint16_t port);

static void _avahi_entry_group_callback(AvahiEntryGroup *g,
    AvahiEntryGroupState state, void *userdata) {
	char *new_name;
	avahi_service_t *svc = (avahi_service_t *)userdata;

if (db) fprintf(stderr, "in  _avahi_entry_group_callback %d 0x%p\n", state, svc);
	if (g != _group && _group != NULL) {
		rfbLog("avahi_entry_group_callback fatal error (group).\n");
		clean_up_exit(1);
	}
	if (userdata == NULL) {
		rfbLog("avahi_entry_group_callback fatal error (userdata).\n");
		clean_up_exit(1);
	}

	switch(state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		rfbLog("Avahi group %s established.\n", svc->name);
#if 0		/* is this the segv problem? */
		free(svc);
#endif
		break;
	case AVAHI_ENTRY_GROUP_COLLISION:
		new_name = avahi_alternative_service_name(svc->name);
		_avahi_create_services(new_name, svc->host, svc->port);
		rfbLog("Avahi Entry group collision\n");
		avahi_free(new_name);
		break;
	case AVAHI_ENTRY_GROUP_FAILURE:
		rfbLog("Avahi Entry group failure: %s\n",
		    avahi_strerror(avahi_client_errno(
		    avahi_entry_group_get_client(g))));
		break;
	}
if (db) fprintf(stderr, "out _avahi_entry_group_callback\n");
}

static void _avahi_create_services(const char *name, const char *host,
    const uint16_t port) {
	avahi_service_t *svc = (avahi_service_t *)malloc(sizeof(avahi_service_t));
	int ret = 0;

if (db) fprintf(stderr, "in  _avahi_create_services  %s %s %d\n", name, host, port);
	svc->name = name;
	svc->host = host;
	svc->port = port;

	if (!_group) {
if (db) fprintf(stderr, "    _avahi_create_services create group\n");
		_group = avahi_entry_group_new(_client,
		    _avahi_entry_group_callback, svc);
	}
	if (!_group) {
		rfbLog("avahi_entry_group_new() failed: %s\n",
		    avahi_strerror(avahi_client_errno(_client)));
		return;
	}

	ret = avahi_entry_group_add_service(_group, AVAHI_IF_UNSPEC,
	    AVAHI_PROTO_UNSPEC, 0, name, "_rfb._tcp", NULL, NULL, port, NULL);
	if (ret < 0) {
		rfbLog("Failed to add _rfb._tcp service: %s\n",
		    avahi_strerror(ret));
		return;
	}

	ret = avahi_entry_group_commit(_group);
	if (ret < 0) {
		rfbLog("Failed to commit entry_group:: %s\n",
		    avahi_strerror(ret));
		return;
	}
if (db) fprintf(stderr, "out _avahi_create_services\n");
}

void avahi_advertise(const char *name, const char *host, const uint16_t port) {
if (db) fprintf(stderr, "in  avahi_advertise\n");
	if (!_client) {
if (db) fprintf(stderr, "    avahi_advertise client null\n");
		return;
	}
	if (_poll == NULL) {
		rfbLog("Avahi poll not initialized.\n");
		return;
	}

	avahi_threaded_poll_lock(_poll);
	_avahi_create_services(name, host, port >= 5900 ? port : 5900+port);
	avahi_threaded_poll_unlock(_poll);
if (db) fprintf(stderr, "out avahi_advertise\n");
}

void avahi_reset(void) {
if (db) fprintf(stderr, "in  avahi_reset\n");
	if (!_client || !_group) {
if (db) fprintf(stderr, "    avahi_reset client/group null\n");
		return;
	}
	avahi_entry_group_reset(_group);
	rfbLog("Avahi resetting group.\n");
if (db) fprintf(stderr, "out avahi_reset\n");
}

void avahi_cleanup(void) {
if (db) fprintf(stderr, "in  avahi_cleanup\n");
	if (!_client) {
if (db) fprintf(stderr, "    avahi_cleanup client null\n");
		return;
	}
	avahi_threaded_poll_lock(_poll);
	avahi_threaded_poll_stop(_poll);

	avahi_client_free(_client);
	_client = NULL;

	avahi_threaded_poll_free(_poll);
	_poll = NULL;
if (db) fprintf(stderr, "out avahi_cleanup\n");
}

#endif

