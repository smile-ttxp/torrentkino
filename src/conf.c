/*
Copyright 2006 Aiko Barz

This file is part of torrentkino.

torrentkino is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

torrentkino is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with torrentkino.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>

#include "torrentkino.h"
#include "log.h"

#include "conf.h"

struct obj_conf *conf_init( int argc, char **argv ) {
	struct obj_conf *conf = (struct obj_conf *) myalloc( sizeof(struct obj_conf) );
	BEN *opts = opts_init();
	BEN *value = NULL;

	/* Parse command line */
	opts_load( opts, argc, argv );

	/* Mode */
	if( ben_dict_search_str( opts, "-f" ) != NULL ) {
		conf->mode = CONF_DAEMON;
	} else {
		conf->mode = CONF_CONSOLE;
	}

	/* Verbosity */
	if( ben_dict_search_str( opts, "-v" ) != NULL ) {
		conf->verbosity = CONF_VERBOSE;
	} else if ( ben_dict_search_str( opts, "-q" ) != NULL ) {
		conf->verbosity = CONF_BEQUIET;
	} else {
		/* Be verbose in the console and quiet while running as a daemon. */
		conf->verbosity = ( conf->mode == CONF_CONSOLE ) ?
			CONF_VERBOSE : CONF_BEQUIET;
	}

	/* Port */
	value = ben_dict_search_str( opts, "-p" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		conf->port = str_safe_port( (char *)ben_str_s( value ) );
	} else {
		conf->port = PORT_DHT_DEFAULT;
	}
	if( conf->port == 0 ) {
		fail( "Invalid port number (-p)" );
	}

	/* Cores */
	conf->cores = unix_cpus();
	if( conf->cores < 1 || conf->cores > 128 ) {
		fail( "Invalid number of CPU cores" );
	}

	/* HOME */
	conf_home( conf, opts );

	/* TLD */
	value = ben_dict_search_str( opts, "-d" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		snprintf( conf->domain, BUF_SIZE, "%s", (char *)ben_str_s( value ) );
	} else {
		strncpy( conf->domain, TLD_DEFAULT, BUF_OFF1 );
	}

	/* Hostname */
	conf_hostname( conf, opts );

	/* Group */
	conf_groupname( conf, opts );

	/* Realm */
	value = ben_dict_search_str( opts, "-r" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		snprintf( conf->realm, BUF_SIZE, "%s", (char *)ben_str_s( value ) );
		conf->bool_realm = TRUE;
	} else {
		snprintf( conf->realm, BUF_SIZE, "%s", CONF_REALM );
		conf->bool_realm = FALSE;
	}

	/* Compute host_id. Respect the realm. */
	conf_hostid( conf->host_id, conf->hostname,
		conf->realm, conf->bool_realm );

	/* Compute group_id. Respect the realm. */
	conf_hostid( conf->group_id, conf->groupname,
		conf->realm, conf->bool_realm );

	/* Lookup replies may enter the cache if the announced port matches mine */
	if( ben_dict_search_str( opts, "-s" ) != NULL ) {
		conf->strict = TRUE;
	} else {
		conf->strict = FALSE;
	}

	if( getuid() == 0 ) {
		snprintf( conf->file, BUF_SIZE, "%s/%s", conf->home, CONF_FILE );
	} else {
		snprintf( conf->file, BUF_SIZE, "%s/.%s", conf->home, CONF_FILE );
	}

	/* Node ID */
	value = ben_dict_search_str( opts, "-n" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		sha1_hash( conf->node_id, (char *)ben_str_s( value ), ben_str_i( value ) );
	} else {
		rand_urandom( conf->node_id, SHA1_SIZE );
	}

	memset( conf->null_id, '\0', SHA1_SIZE );

	/* Bootstrap node:
	 * -x overwrites everything
	 * -l sets a default internet bootstrap server
	 * Last resort: Use multicast
	 */
	value = ben_dict_search_str( opts, "-x" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		snprintf( conf->bootstrap_node, BUF_SIZE, "%s",
			(char *)ben_str_s( value ) );
	} else {
		if( ben_dict_search_str( opts, "-l" ) != NULL ) {
			snprintf( conf->bootstrap_node, BUF_SIZE, "%s", BOOTSTRAP_DEFAULT );
		} else {
			snprintf( conf->bootstrap_node, BUF_SIZE, "%s", MULTICAST_DEFAULT );
		}
	}

	/* Bootstrap port */
	value = ben_dict_search_str( opts, "-y" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		conf->bootstrap_port = str_safe_port( (char *)ben_str_s( value ) );
		printf( "%i %s \n", conf->bootstrap_port, (char *)ben_str_s( value ));
		if( conf->bootstrap_port == 0 ) {
			conf->bootstrap_port = PORT_DHT_DEFAULT;
		}
	} else {
		conf->bootstrap_port = PORT_DHT_DEFAULT;
	}

#ifdef POLARSSL
	/* Secret key */
	value = ben_dict_search_str( opts, "-k" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		snprintf( conf->key, BUF_SIZE, "%s", (char *)ben_str_s( value ) );
		conf->bool_encryption = TRUE;
	} else {
		memset( conf->key, '\0', BUF_SIZE );
		conf->bool_encryption = FALSE;
	}
#endif

	opts_free( opts );

	return conf;
}

void conf_free( void ) {
	myfree( _main->conf );
}

void conf_home( struct obj_conf *conf, BEN *opts ) {

	if( getenv( "HOME" ) == NULL || getuid() == 0 ) {
		strncpy( conf->home, "/etc", BUF_OFF1 );
	} else {
		snprintf( conf->home,  BUF_SIZE, "%s", getenv( "HOME") );
	}

	if( !file_isdir( conf->home ) ) {
		fail( "%s does not exist", conf->home );
	}
}

void conf_hostname( struct obj_conf *conf, BEN *opts ) {
	BEN *value = NULL;
	char *f = NULL;
	char *p = NULL;

	/* Init */
	strncpy( conf->hostname, "bulk.p2p", BUF_OFF1 );

	/* Hostname from args */
	value = ben_dict_search_str( opts, "-a" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		snprintf( conf->hostname, BUF_SIZE, "%s.%s",
				(char *)ben_str_s( value ), conf->domain );
		return;
	}

	/* Hostname from file */
	if( ! file_isreg( CONF_HOSTFILE ) ) {
		return;
	}

	f = (char *) file_load( CONF_HOSTFILE, 0, file_size( CONF_HOSTFILE ) );

	if( f == NULL ) {
		return;
	}

	if( ( p = strchr( f, '\n')) != NULL ) {
		*p = '\0';
	}

	snprintf( conf->hostname, BUF_SIZE, "%s.%s", f, conf->domain );

	myfree( f );
}

void conf_groupname( struct obj_conf *conf, BEN *opts ) {
	BEN *value = NULL;

	/* Init */
	strncpy( conf->groupname, "None", BUF_OFF1 );

	value = ben_dict_search_str( opts, "-g" );
	if( ben_is_str( value ) && ben_str_i( value ) >= 1 ) {
		snprintf( conf->groupname, BUF_SIZE, "%s.%s",
				(char *)ben_str_s( value ), conf->domain );
		conf->bool_group = TRUE;
		return;
	} else {
		conf->bool_group = FALSE;
	}
}

void conf_hostid( UCHAR *host_id, char *hostname, char *realm, int bool ) {
	UCHAR sha1_buf1[SHA1_SIZE];
	UCHAR sha1_buf2[SHA1_SIZE];
	int j = 0;

	/* The realm influences the way, the lookup hash gets computed */
	if( bool == TRUE ) {
		sha1_hash( sha1_buf1, hostname, strlen( hostname ) );
		sha1_hash( sha1_buf2, realm, strlen( realm ) );

		for( j = 0; j < SHA1_SIZE; j++ ) {
			host_id[j] = sha1_buf1[j] ^ sha1_buf2[j];
		}
	} else {
		sha1_hash( host_id, hostname, strlen( hostname ) );
	}
}

void conf_print( void ) {
	char hex[HEX_LEN];

	if ( getenv( "PWD" ) == NULL || getenv( "HOME" ) == NULL ) {
		info( NULL, "# Hint: Reading environment variables failed. sudo?");
		info( NULL, "# This is not a problem. But in some cases it might be useful" );
		info( NULL, "# to use 'sudo -E' to export some variables like $HOME or $PWD." );
	}

	info( NULL, "Cores: %i", _main->conf->cores );

	if( _main->conf->mode == CONF_CONSOLE ) {
		info( NULL, "Mode: Console (-f)" );
	} else {
		info( NULL, "Mode: Daemon (-f)" );
	}

	if( _main->conf->verbosity == CONF_BEQUIET ) {
		info( NULL, "Verbosity: Quiet (-q/-v)" );
	} else {
		info( NULL, "Verbosity: Verbose (-q/-v)" );
	}

	info( NULL, "Workdir: %s", _main->conf->home );

	info( NULL, "Config file: %s", _main->conf->file );

	info( NULL, "Listen to UDP/%i (-p)", _main->conf->port );

	info( NULL, "Domain: %s (-d)", _main->conf->domain );
	info( NULL, "Hostname: %s (-a)", _main->conf->hostname );
	info( NULL, "Group: %s (-g)", _main->conf->groupname );

	hex_hash_encode( hex, _main->conf->node_id );
	info( NULL, "Node ID: %s", hex );

	hex_hash_encode( hex, _main->conf->host_id );
	info( NULL, "Host ID: %s", hex );

	if( _main->conf->bool_group ) {
		hex_hash_encode( hex, _main->conf->group_id );
		info( NULL, "Group ID: %s", hex );
	}

	info( NULL, "Bootstrap node: %s (-x/-l)", _main->conf->bootstrap_node );
	info( NULL, "Bootstrap port: UDP/%i (-y)", _main->conf->bootstrap_port );
	if( _main->conf->strict ) {
		info( NULL, "Strict mode: Yes (-s)" );
	} else {
		info( NULL, "Strict mode: No (-s)" );
	}

	/* Realm */
	if( _main->conf->bool_realm == 1 ) {
		info( NULL, "Realm: %s (-r)", _main->conf->realm );
	} else {
		info( NULL, "Realm: None (-r)" );
	}

	/* Encryption */
#ifdef POLARSSL
	if( _main->conf->bool_encryption == 1 ) {
		info( NULL, "Encryption key: %s (-k)", _main->conf->key );
	} else {
		info( NULL, "Encryption key: None (-k)" );
	}
#endif
}

void conf_write( void ) {
	BEN *dict = ben_init( BEN_DICT );
	BEN *key = NULL;
	BEN *val = NULL;
	RAW *raw = NULL;

	/* Port */
	key = ben_init( BEN_STR );
	val = ben_init( BEN_INT );
	ben_str( key, (UCHAR *)"port", 4 );
	ben_int( val, _main->conf->port );
	ben_dict( dict, key, val );

	/* IP mode */
	key = ben_init( BEN_STR );
	val = ben_init( BEN_INT );
	ben_str( key, (UCHAR *)"ip_version", 10 );
#ifdef IPV6
	ben_int( val, 6 );
#elif IPV4
	ben_int( val, 4 );
#endif
	ben_dict( dict, key, val );

	/* Domain */
	key = ben_init( BEN_STR );
	val = ben_init( BEN_STR );
	ben_str( key, (UCHAR *)"domain", 6 );
	ben_str( val, (UCHAR *)_main->conf->domain, strlen( _main->conf->domain ) );
	ben_dict( dict, key, val );

	/* Encode */
	raw = ben_enc( dict );

	/* Write */
	if( !file_write( _main->conf->file, (char *)raw->code, raw->size ) ) {
		fail( "Writing %s failed", _main->conf->file );
	}

	raw_free( raw );
	ben_free( dict );
}

int conf_verbosity( void ) {
	return _main->conf->verbosity;
}

int conf_mode( void ) {
	return _main->conf->mode;
}
