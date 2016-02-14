/* nag2mqtt - Nagios event broker to MQTT gateway
 *
 * Authors:
 *   Thomas Liske <liske@ibh.de>
 *
 * Copyright Holder:
 *   2016 (C) IBH IT-Service GmbH [https://github.com/DE-IBH/nag2mqtt/]
 *
 * License:
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this package; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <sys/types.h>
#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <wordexp.h>
#include <string.h>
#include <errno.h>
#include <mhash.h>

/* include local copy of nagios 3.5 headers */
#include "include/nagios/nebmodules.h"
#include "include/nagios/nebcallbacks.h"
#include "include/nagios/nebstructs.h"
#include "include/nagios/broker.h"
#include "include/nagios/common.h"
#include "include/nagios/nagios.h"

#include <json-c/json.h>

NEB_API_VERSION(CURRENT_NEB_API_VERSION);

extern int process_performance_data;

void *nag2mqtt_module_handle = NULL;

int nag2mqtt_handle_host_check_data(int, void *);
int nag2mqtt_handle_service_check_data(int, void *);

char *basedir = "/run/nag2mqtt/publish";
char *subprefix = NULL;
char hostname[64];


/* These simple helpers where taken from nagioscore/base/logging.c: */
static char *service_state_name(int state) {
  switch(state) {
  case STATE_OK:
    return "OK";
  case STATE_WARNING:
    return "WARNING";
  case STATE_CRITICAL:
    return "CRITICAL";
  }

  return "UNKNOWN";
}

static char *host_state_name(int state) {
  switch(state) {
  case HOST_UP:
    return "UP";
  case HOST_DOWN:
    return "DOWN";
  case HOST_UNREACHABLE:
    return "UNREACHABLE";
  }
  
  return "(unknown)";
}

static char *state_type_name(int state_type) {
  return state_type == HARD_STATE ? "HARD" : "SOFT";
}


/* Initialize NEB callbacks */
int nebmodule_init(int flags, char *args, nebmodule *handle) {
  char buf[4096];

  nag2mqtt_module_handle = handle;

  /* META */
  neb_set_module_info(nag2mqtt_module_handle, NEBMODULE_MODINFO_TITLE, "nag2mqtt");
  neb_set_module_info(nag2mqtt_module_handle, NEBMODULE_MODINFO_AUTHOR, "Thomas Liske");
  neb_set_module_info(nag2mqtt_module_handle, NEBMODULE_MODINFO_TITLE, "Copyright (c) 2016 Thomas Liske <liske@ibh.de>");
  neb_set_module_info(nag2mqtt_module_handle, NEBMODULE_MODINFO_VERSION, "0.0.1");
  neb_set_module_info(nag2mqtt_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v2");
  neb_set_module_info(nag2mqtt_module_handle, NEBMODULE_MODINFO_DESC, "Event and performance broker for nag2mqtt gateway.");

  write_to_all_logs("nag2mqtt: Copyright (c) 2016 Thomas Liske <liske@ibh.de> - https://github.com/DE-IBH/nag2mqtt/", NSLOG_INFO_MESSAGE);
  if (process_performance_data == FALSE)
    write_to_all_logs("nag2mqtt: Please consider to set 'process_performance_data=1' in nagios.cfg to export performance data to MQTT!", NSLOG_INFO_MESSAGE);

  /* Get hostname for JSON output */
  gethostname(hostname, sizeof(hostname));
  hostname[sizeof(hostname) - 1] = 0;

  /* Parse options */
  if(args) {
    wordexp_t we;
    if(wordexp(args, &we, 0)) {
      write_to_all_logs("nag2mqtt: Failed to expand arguments!", NSLOG_CONFIG_ERROR);
      return 1;
    }

    int i;
    char **p = NULL;
    for(i = 0; i < we.we_wordc; i++) {
      if(p) {
	*p = strdup(we.we_wordv[i]);
      }
      else {
	if(strcmp(we.we_wordv[i], "-basedir") == 0) {
	  p = &basedir;
	  continue;
	}

	if(strcmp(we.we_wordv[i], "-subprefix") == 0) {
	  p = &subprefix;
	  continue;
	}

	snprintf(buf, sizeof(buf), "nag2mqtt: Unknown option #%d '%s'!", (i + 1), we.we_wordv[i]);
	buf[sizeof(buf) - 1] = 0;
	write_to_all_logs(buf, NSLOG_INFO_MESSAGE);
	return 1;
      }
    }
    wordfree(&we);
  }

  /* Dump config */
  snprintf(buf, sizeof(buf), "nag2mqtt:  basedir = '%s'", basedir);
  buf[sizeof(buf) - 1] = 0;
  write_to_all_logs(buf, NSLOG_INFO_MESSAGE);
  snprintf(buf, sizeof(buf), "nag2mqtt:  subprefix = '%s'", (subprefix ? subprefix : ""));
  buf[sizeof(buf) - 1] = 0;
  write_to_all_logs(buf, NSLOG_INFO_MESSAGE);
  
  /* Register callbacks */
  neb_register_callback(NEBCALLBACK_HOST_CHECK_DATA, nag2mqtt_module_handle, 0, nag2mqtt_handle_host_check_data);
  neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA, nag2mqtt_module_handle, 0, nag2mqtt_handle_service_check_data);

  return 0;
}


/* Deinitialize NEB callbacks */
int nebmodule_deinit(int flags, int reason) {
  neb_deregister_callback(NEBCALLBACK_HOST_CHECK_DATA, nag2mqtt_handle_host_check_data);
  neb_deregister_callback(NEBCALLBACK_SERVICE_CHECK_DATA, nag2mqtt_handle_service_check_data);

  return 0;
}


#define NULLIFY(buf) \
  buf[sizeof(buf) - 1] = 0;

#define JSON_ADD_INT(json, key, val) \
  json_object_object_add((json), (key), json_object_new_int( (val) ));
#define JSON_ADD_STR(json, key, val) \
  ptr = (val); \
  json_object_object_add((json), (key), json_object_new_string( (ptr ? ptr : "") ));

int nag2mqtt_hashfn(const char *fn, char *hstr) {
  MHASH td;
  unsigned char hash[16];
  int i;

  td = mhash_init(MHASH_TIGER128);

  if (td == MHASH_FAILED)
    return -1;

  mhash(td, fn, strlen(fn)); 
  mhash_deinit(td, hash);

  for (i = 0; i < mhash_get_block_size(MHASH_TIGER128); i++) {
    sprintf(&(hstr[i*2]), "%.2x", hash[i]);
  }
  hstr[i*2 + 1] = 0;

  return 0;
}

/* Process host check data */
int nag2mqtt_handle_host_check_data(int event_type, void *data) {
  nebstruct_host_check_data *hostchkdata = (nebstruct_host_check_data *) data;

  /* Sanity checks */
  if(event_type != NEBCALLBACK_HOST_CHECK_DATA)
    return 0;

  if(!hostchkdata)
    return 0;
  
  if(hostchkdata->type != NEBTYPE_HOSTCHECK_PROCESSED)
    return 0;

  char hkey[PATH_MAX];
  char hfn[PATH_MAX];
  char fn1[PATH_MAX];
  char fn2[PATH_MAX];

  snprintf(hkey, PATH_MAX, "%s:host", hostchkdata->host_name);
  NULLIFY(hkey);
  if(nag2mqtt_hashfn(hkey, hfn))
    return 0;

  snprintf(fn1, PATH_MAX, "%s/%s.new", basedir, hfn);
  NULLIFY(fn1);
  snprintf(fn2, PATH_MAX, "%s/%s", basedir, hfn);
  NULLIFY(fn2);

  FILE *fh = fopen(fn1, "wx");
  if(fh) {
    char *ptr;
    json_object *json = json_object_new_object();
    JSON_ADD_INT(json, "_timestamp", (int)hostchkdata->timestamp.tv_sec);
    JSON_ADD_STR(json, "_hostname", hostname);
    JSON_ADD_STR(json, "_subprefix", subprefix);
    JSON_ADD_STR(json, "_type", "HOST");
    JSON_ADD_STR(json, "hostname", hostchkdata->host_name);
    JSON_ADD_INT(json, "current_attempt", hostchkdata->current_attempt);
    JSON_ADD_INT(json, "max_attempts", hostchkdata->max_attempts);
    JSON_ADD_INT(json, "state_type", hostchkdata->state_type);
    JSON_ADD_STR(json, "state_type_s", state_type_name(hostchkdata->state_type));
    JSON_ADD_INT(json, "state", hostchkdata->state);
    JSON_ADD_STR(json, "state_s", host_state_name(hostchkdata->state));
    JSON_ADD_STR(json, "output", hostchkdata->output);
    JSON_ADD_STR(json, "long_output", hostchkdata->long_output);
    JSON_ADD_STR(json, "perf_data", hostchkdata->perf_data);

    /* Add meta data from host definition */
    host *host = find_host(hostchkdata->host_name);
    if(host) {
      if(host->notes)
	JSON_ADD_STR(json, "notes", host->notes);
      if(host->icon_image)
	JSON_ADD_STR(json, "icon_image", host->icon_image);
    }
    
    fprintf(fh, "%s\n", json_object_to_json_string(json));
    fclose(fh);

    rename(fn1, fn2);

    json_object_put(json);
  }
  else {
    char buf[4096];

    snprintf(buf, sizeof(buf), "nag2mqtt: failed to open %s: %s (%d)", fn1, strerror(errno), errno);
    NULLIFY(buf);
    write_to_all_logs(buf, NSLOG_INFO_MESSAGE);
  }

  return 0;
}


/* Process service check data */
int nag2mqtt_handle_service_check_data(int event_type, void *data) {
  nebstruct_service_check_data *srvchkdata = (nebstruct_service_check_data *) data;

  /* Sanity checks */
  if(event_type != NEBCALLBACK_SERVICE_CHECK_DATA)
    return 0;

  if(!srvchkdata)
    return 0;
  
  if(srvchkdata->type != NEBTYPE_SERVICECHECK_PROCESSED)
    return 0;

  char hkey[PATH_MAX];
  char hfn[PATH_MAX];
  char fn1[PATH_MAX];
  char fn2[PATH_MAX];

  snprintf(hkey, PATH_MAX, "%s:%s:service", srvchkdata->host_name, srvchkdata->service_description);
  NULLIFY(hkey);
  if(nag2mqtt_hashfn(hkey, hfn))
    return 0;

  snprintf(fn1, PATH_MAX, "%s/%s.new", basedir, hfn);
  NULLIFY(fn1);
  snprintf(fn2, PATH_MAX, "%s/%s", basedir, hfn);
  NULLIFY(fn2);

  FILE *fh = fopen(fn1, "wx");
  if(fh) {
    char *ptr;
    json_object *json = json_object_new_object();
    JSON_ADD_INT(json, "_timestamp", (int)srvchkdata->timestamp.tv_sec);
    JSON_ADD_STR(json, "_hostname", hostname);
    JSON_ADD_STR(json, "_subprefix", subprefix);
    JSON_ADD_STR(json, "_type", "SERVICE");
    JSON_ADD_STR(json, "hostname", srvchkdata->host_name);
    JSON_ADD_STR(json, "service_description", srvchkdata->service_description);
    JSON_ADD_INT(json, "current_attempt", srvchkdata->current_attempt);
    JSON_ADD_INT(json, "max_attempts", srvchkdata->max_attempts);
    JSON_ADD_INT(json, "state_type", srvchkdata->state_type);
    JSON_ADD_STR(json, "state_type_s", state_type_name(srvchkdata->state_type));
    JSON_ADD_INT(json, "state", srvchkdata->state);
    JSON_ADD_STR(json, "state_s", service_state_name(srvchkdata->state));
    JSON_ADD_STR(json, "output", srvchkdata->output);
    JSON_ADD_STR(json, "long_output", srvchkdata->long_output);
    JSON_ADD_STR(json, "perf_data", srvchkdata->perf_data);

    /* Add meta data from service definition */
    service *service = find_service(srvchkdata->host_name, srvchkdata->service_description);
    if(service) {
      if(service->notes)
	JSON_ADD_STR(json, "notes", service->notes);
      if(service->icon_image)
	JSON_ADD_STR(json, "icon_image", service->icon_image);
    }

    fprintf(fh, "%s\n", json_object_to_json_string(json));
    fclose(fh);

    rename(fn1, fn2);

    json_object_put(json);
  }
  else {
    char buf[4096];

    snprintf(buf, sizeof(buf), "nag2mqtt: failed to open %s: %s (%d)", fn1, strerror(errno), errno);
    NULLIFY(buf);
    write_to_all_logs(buf, NSLOG_INFO_MESSAGE);
  }

  return 0;
}
