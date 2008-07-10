/* Copyright (C) 2008 AbiSource Corporation B.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "RealmConnection.h"

namespace rpv1 = realm::protocolv1;

RealmConnection::RealmConnection(const std::string& ca_file, const std::string& address, int port, 
								 const std::string& cookie, UT_sint64 doc_id, bool master, const std::string& session_id,
								 boost::function<void (RealmConnection&)> sig)
	: m_io_service(),
	m_socket(m_io_service),
	m_ca_file(ca_file),
	m_address(address),
	m_port(port),
	m_thread_ptr(),
	m_cookie(cookie),
	m_doc_id(doc_id),
	m_master(master),
	m_session_id(session_id),
	m_buf(1024), // always have a reasonable block of free memory available to cut back on the memory allocations a bit,
	m_packet_queue(boost::bind(&RealmConnection::_signal, this)),
	m_sig(sig),
	m_buddies(),
	m_pdp_ptr(),
	m_tls_tunnel_ptr()
{
}

bool RealmConnection::connect()
{
	UT_DEBUGMSG(("RealmConnection::connect()\n"));
	UT_return_val_if_fail(!m_thread_ptr, false);
	

	try {
		// setup our local TLS tunnel to the realm
		m_tls_tunnel_ptr.reset(new tls_tunnel::ClientProxy(m_address, m_port, m_ca_file, false));
		asio::thread thread(boost::bind(&tls_tunnel::ClientProxy::run, m_tls_tunnel_ptr.get()));

		// connect to the tunnel
		asio::ip::tcp::resolver::query query(m_tls_tunnel_ptr->local_address(), boost::lexical_cast<std::string>(m_tls_tunnel_ptr->local_port()));
		asio::ip::tcp::resolver resolver(m_io_service);
		asio::ip::tcp::resolver::iterator iterator(resolver.resolve(query));
		m_socket.connect(*iterator);
	}
	catch (tls_tunnel::Exception& e)
	{
		UT_DEBUGMSG(("tls_tunnel exception connecting to realm: %s\n", e.message().c_str()));
		return false;
	}
	catch (asio::system_error& se)
	{
		UT_DEBUGMSG(("Error connecting to realm: %s\n", se.what()));
		return false;
	}
	catch (...)
	{
		UT_DEBUGMSG(("Error connecting to realm!\n"));
		return false;
	}
	
	if (!_login())
	{
		UT_DEBUGMSG(("RealmConnection login failed!\n"));
		disconnect();
		return false;
	}

	UT_DEBUGMSG(("RealmConnection connected\n"));
	
	// start reading realm messages
	_receive();	
	
	m_thread_ptr.reset(new asio::thread(boost::bind(&asio::io_service::run, &m_io_service)));
	return true;
}

void RealmConnection::disconnect()
{
	UT_DEBUGMSG(("RealmConnection::disconnect()\n"));
	
	if (m_socket.is_open())
	{
		m_socket.cancel();
		m_socket.close();
	}
	
	if (m_thread_ptr)
	{
		m_io_service.stop();
		m_thread_ptr->join();
	}
	m_thread_ptr.reset();

	if (m_tls_tunnel_ptr)
	{
		m_tls_tunnel_ptr->stop();
		m_tls_tunnel_ptr.reset();
	}
}

void RealmConnection::addBuddy(boost::shared_ptr<RealmBuddy> buddy_ptr)
{
	m_buddies.push_back(buddy_ptr);
}

void RealmConnection::removeBuddy(UT_uint8 realm_connection_id)
{
	for (std::vector<boost::shared_ptr<RealmBuddy> >::iterator it = m_buddies.begin(); it != m_buddies.end(); it++)
	{
		UT_continue_if_fail(*it);
		if ((*it)->realm_connection_id() == realm_connection_id)
		{
			m_buddies.erase(it);
			return;
		}		
	}
	
	UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
}

boost::shared_ptr<RealmBuddy> RealmConnection::getBuddy(UT_uint8 realm_connection_id)
{
	for (std::vector<boost::shared_ptr<RealmBuddy> >::iterator it = m_buddies.begin(); it != m_buddies.end(); it++)
	{
		UT_continue_if_fail(*it);
		if ((*it)->realm_connection_id() == realm_connection_id)
		{
			return *it;
		}		
	}
	
	UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
	return boost::shared_ptr<RealmBuddy>();
}

void RealmConnection::_signal()
{
	m_sig(*this);
}

bool RealmConnection::_login()
{
	UT_DEBUGMSG(("RealmConnection::_login()\n"));
	
	// FIXME: make this a combined asio buffer
	boost::shared_ptr<std::string> header_ptr(new std::string(2*sizeof(UT_uint32) + m_cookie.size(), '\0'));
	std::string& header = *header_ptr;
	
	UT_uint32 proto_magic = 0x000A0B01;
	UT_uint32 proto_version = 0x01;
	// FIXME: not Big Endian safe!!
	memcpy(&header[0], &proto_magic, sizeof(UT_uint32));
	memcpy(&header[sizeof(UT_uint32)], &proto_version, sizeof(UT_uint32));
	memcpy(&header[2*sizeof(UT_uint32)], m_cookie.data(), m_cookie.size());
	
	// holds the login response information
	std::string response(1, '\0');
	
	try
	{
		// send the login credententials
		// TODO: we should check the number of bytes written
		asio::write(m_socket, asio::buffer(header));
		
		// read the login response
		// TODO: we should check the number of bytes read
		asio::read(m_socket, asio::buffer(&response[0], response.size()));
	}
	catch (asio::system_error e)
	{
		UT_DEBUGMSG(("Error while writing/writing protocol header: %s\n", e.what()));
		return false;
	}

	switch (response[0])
	{
		case realm::protocol::HANDSHAKE_RESERVED:
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			return false;
		case realm::protocol::HANDSHAKE_OK:
			UT_DEBUGMSG(("Login response OK!\n"));
			break;
		case realm::protocol::HANDSHAKE_BAD_IDENTIFIER:
			UT_DEBUGMSG(("realm::protocol::HANDSHAKE_BAD_IDENTIFIER response!\n"));
			return false;
		case realm::protocol::HANDSHAKE_UNSUPPORTED_PROTOCOL:
			UT_DEBUGMSG(("realm::protocol::HANDSHAKE_UNSUPPORTED_PROTOCOL response!\n"));
			return false;
		case realm::protocol::HANDSHAKE_INVALID_COOKIE:
			UT_DEBUGMSG(("realm::protocol::HANDSHAKE_INVALID_COOKIE response!\n"));
			return false;
		default:
			UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
			return false;
	}	
	
	return true;
}

void RealmConnection::_receive()
{
	UT_DEBUGMSG(("RealmConnection::_receive()\n"));
	m_buf.clear();
	boost::shared_ptr<std::string> msg_ptr(new std::string(1, '\0'));
	asio::async_read(m_socket, asio::buffer(&(*msg_ptr)[0], msg_ptr->size()),
		boost::bind(&RealmConnection::_message, shared_from_this(),
			asio::placeholders::error, asio::placeholders::bytes_transferred, msg_ptr));
}

void RealmConnection::_message(const asio::error_code& e, std::size_t bytes_transferred, boost::shared_ptr<std::string> msg_ptr)
{
	UT_DEBUGMSG(("RealmConnection::_message()\n"));
	if (e)
	{
		UT_DEBUGMSG(("Error reading message: %s\n", e.message().c_str()));
		disconnect();		
		return;
	}	
	UT_DEBUGMSG(("Constructing packet of type: 0x%x\n", (*msg_ptr)[0]));
	PacketPtr packet_ptr = realm::protocolv1::Packet::construct(static_cast<rpv1::packet_type>((*msg_ptr)[0]));
	if (!packet_ptr) {
		UT_DEBUGMSG(("Error constructing packet for type 0x%x\n", (*msg_ptr)[0]));
		return;
	}
	_complete_packet(packet_ptr);
}

void RealmConnection::_complete_packet(PacketPtr packet_ptr)
{
	UT_DEBUGMSG(("RealmConnection::_complete_packet()\n"));
	int bytes_needed = packet_ptr->complete(m_buf.data(), m_buf.size());
	switch (bytes_needed)
	{
		case -1:
			UT_DEBUGMSG(("Error determining packet (type: 0x%x) completion state!\n", packet_ptr->type()));
			return;
		case 0:
			{
				UT_DEBUGMSG(("Read full packet\n"));
				UT_return_if_fail(packet_ptr->parse(m_buf.data(), m_buf.size()) != -1);
				m_packet_queue.push(packet_ptr);
				_receive();
			}
			break;
		default:
			UT_DEBUGMSG(("Need more data (%d bytes) for this packet...\n", bytes_needed));
			// read the needed number of bytes
			char* ptr = m_buf.prepare(bytes_needed);
			asio::async_read(m_socket, asio::buffer(ptr, bytes_needed),
							boost::bind(&RealmConnection::_complete, shared_from_this(),
								asio::placeholders::error, asio::placeholders::bytes_transferred, packet_ptr)
							);
			break;
	}
}

void RealmConnection::_complete(const asio::error_code& e, std::size_t bytes_transferred, PacketPtr packet_ptr)
{
	UT_DEBUGMSG(("RealmConnection::_complete()\n"));
	if (e)
	{
		UT_DEBUGMSG(("Error reading message: %s\n", e.message().c_str()));
		disconnect();		
		return;
	}	
	m_buf.commit(bytes_transferred);
	_complete_packet(packet_ptr);
}
