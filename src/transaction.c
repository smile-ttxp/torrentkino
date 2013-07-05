/*
Copyright 2011 Aiko Barz

This file is part of masala.

masala is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

masala is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with masala.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <netdb.h>
#include <sys/epoll.h>

#include "malloc.h"
#include "thrd.h"
#include "main.h"
#include "str.h"
#include "list.h"
#include "hash.h"
#include "log.h"
#include "conf.h"
#include "file.h"
#include "unix.h"
#include "udp.h"
#include "ben.h"
#include "token.h"
#include "neighbourhood.h"
#include "lookup.h"
#include "transaction.h"
#include "p2p.h"
#include "bucket.h"
#include "time.h"
#include "send_p2p.h"
#include "random.h"

struct obj_transaction *tdb_init( void ) {
	struct obj_transaction *transaction = (struct obj_transaction *)
		myalloc( sizeof(struct obj_transaction), "tdb_init" );
	transaction->list = list_init();
	transaction->hash = hash_init( 1000 );
	return transaction;
}

void tdb_free( void ) {
	tdb_expire(); /* Game Over Mode */
	list_clear( _main->transaction->list );
	list_free( _main->transaction->list );
	hash_free( _main->transaction->hash );
	myfree( _main->transaction, "tdb_free" );
}

ITEM *tdb_put( int type, UCHAR *target, IP *from ) {
	ITEM *item = NULL;
	TID *tid = NULL;

	tid = (TID *) myalloc( sizeof(TID), "tdb_put" );

	/* ID */
	tdb_create_random_id( tid->id );

	/* PING, ANNOUNCE_PEER, FIND_NODE, GET_PEERS */
	tid->type = type;

	/* Availability */
	time_add_1_min( &tid->time );

	/* More details */
	switch( type ) {
		case P2P_GET_PEERS:
		case P2P_ANNOUNCE_GET_PEERS:
			tid->lookup = ldb_init( target, from );
			break;
		default:
			tid->lookup = NULL;
	}

	item = list_put( _main->transaction->list, tid );
	hash_put( _main->transaction->hash, tid->id, TID_SIZE, item );

	return item;
}

void tdb_del( ITEM *i ) {
	TID *tid = NULL;

	if( i == NULL ) {
		return;
	} else {
		tid = list_value( i );
	}

	ldb_free( tid->lookup );
	hash_del( _main->transaction->hash, tdb_tid( i ), TID_SIZE );
	list_del( _main->transaction->list, i );
	myfree( tid, "tdb_del" );
}

void tdb_expire( void ) {
	ITEM *item = NULL;
	ITEM *next = NULL;
	TID *tid = NULL;
	long int i = 0;

	item = _main->transaction->list->start;
	for( i=0; i<_main->transaction->list->counter; i++ ) {
		tid = list_value( item );
		next = list_next( item );

		/* Too OLD or GAME OVER */
		if( _main->p2p->time_now.tv_sec > tid->time || _main->status == GAMEOVER ) {

			switch( tid->type ) {
				case P2P_ANNOUNCE_GET_PEERS:
					if( p2p_cron_announce_now( item ) ) {
						tdb_del( item );
					}
					break;
				default:
					tdb_del( item );
			}
		}

		item = next;
	}
}

ITEM *tdb_item( UCHAR *id ) {
	return hash_get( _main->transaction->hash, id, TID_SIZE);
}

int tdb_type( ITEM *i ) {
	TID *tid = NULL;

	if( i == NULL ) {
		return P2P_TYPE_UNKNOWN;
	} else {
		tid = list_value( i );
	}

	return tid->type;
}

LOOKUP *tdb_ldb( ITEM *i ) {
	TID *tid = list_value( i );
	return tid->lookup;
}

UCHAR *tdb_tid( ITEM *i ) {
	TID *tid = list_value( i );
	return tid->id;
}

void tdb_create_random_id( UCHAR *id ) {
	int i = 0;
	int max = 1000;

	/* Create unique ID but do not try this forever */
	do {
		rand_urandom( id, TID_SIZE );
		i++;
	} while( hash_exists( _main->transaction->hash, id, TID_SIZE) && i < max );

	if( i >= max ) {
		log_info( NULL, 0, "Transaction IDs exhausted. Giving up." );
	}
}