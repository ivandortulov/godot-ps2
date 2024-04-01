/*************************************************************************/
/*  packet_peer_udp.cpp                                                  */
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
#include "packet_peer_udp.h"
#include "io/ip.h"

PacketPeerUDP *(*PacketPeerUDP::_create)() = NULL;

void PacketPeerUDP::set_blocking_mode(bool p_enable) {

	blocking = p_enable;
}

String PacketPeerUDP::_get_packet_ip() const {

	return get_packet_address();
}

Error PacketPeerUDP::_set_send_address(const String &p_address, int p_port) {

	IP_Address ip;
	if (p_address.is_valid_ip_address()) {
		ip = p_address;
	} else {
		ip = IP::get_singleton()->resolve_hostname(p_address);
		if (!ip.is_valid())
			return ERR_CANT_RESOLVE;
	}

	set_send_address(ip, p_port);
	return OK;
}

void PacketPeerUDP::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("listen:Error", "port", "bind_address", "recv_buf_size"), &PacketPeerUDP::listen, DEFVAL("*"), DEFVAL(65536));
	ObjectTypeDB::bind_method(_MD("close"), &PacketPeerUDP::close);
	ObjectTypeDB::bind_method(_MD("wait:Error"), &PacketPeerUDP::wait);
	ObjectTypeDB::bind_method(_MD("is_listening"), &PacketPeerUDP::is_listening);
	ObjectTypeDB::bind_method(_MD("get_packet_ip"), &PacketPeerUDP::_get_packet_ip);
	//ObjectTypeDB::bind_method(_MD("get_packet_address"),&PacketPeerUDP::_get_packet_address);
	ObjectTypeDB::bind_method(_MD("get_packet_port"), &PacketPeerUDP::get_packet_port);
	ObjectTypeDB::bind_method(_MD("set_send_address", "host", "port"), &PacketPeerUDP::_set_send_address);
}

Ref<PacketPeerUDP> PacketPeerUDP::create_ref() {

	if (!_create)
		return Ref<PacketPeerUDP>();
	return Ref<PacketPeerUDP>(_create());
}

PacketPeerUDP *PacketPeerUDP::create() {

	if (!_create)
		return NULL;
	return _create();
}

PacketPeerUDP::PacketPeerUDP() {

	blocking = true;
}
