/*************************************************************************/
/*  stream_peer_ssl.h                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifndef STREAM_PEER_SSL_H
#define STREAM_PEER_SSL_H

#include "io/stream_peer.h"

class StreamPeerSSL : public StreamPeer {
	OBJ_TYPE(StreamPeerSSL, StreamPeer);

public:
	typedef void (*LoadCertsFromMemory)(const ByteArray &p_certs);

protected:
	static StreamPeerSSL *(*_create)();
	static void _bind_methods();

	static LoadCertsFromMemory load_certs_func;
	static bool available;

	friend class Main;
	static bool initialize_certs;

public:
	enum Status {
		STATUS_DISCONNECTED,
		STATUS_CONNECTED,
		STATUS_ERROR_NO_CERTIFICATE,
		STATUS_ERROR_HOSTNAME_MISMATCH
	};

	virtual Error accept(Ref<StreamPeer> p_base) = 0;
	virtual Error connect(Ref<StreamPeer> p_base, bool p_validate_certs = false, const String &p_for_hostname = String()) = 0;
	virtual Status get_status() const = 0;

	virtual void disconnect() = 0;

	static StreamPeerSSL *create();

	static void load_certs_from_memory(const ByteArray &p_memory);
	static bool is_available();

	StreamPeerSSL();
};

VARIANT_ENUM_CAST(StreamPeerSSL::Status);

#endif // STREAM_PEER_SSL_H
