
#include "visitor.hpp"

bool pzq::visitor_t::can_write ()
{
    int events = 0;
    size_t optsiz = sizeof (int);

    m_socket.get ()->getsockopt (ZMQ_EVENTS, &events, &optsiz);

    if (events & ZMQ_POLLOUT)
        return true;

    return false;
}

const char *pzq::visitor_t::visit_full (const char *kbuf, size_t ksiz, const char *vbuf, size_t vsiz, size_t *sp) 
{
    size_t pos = 0;
    int32_t flags;
    size_t msg_size;
    bool more = true;

	std::string key (kbuf, ksiz);

	if (m_store.get ()->is_in_flight (key))
	{
		return NOP;
	}

    if (!can_write ())
    	throw std::runtime_error ("The socket is in blocking state");

    zmq::message_t header (ksiz);
    memcpy (header.data (), kbuf, ksiz);

    if (!m_socket.get ()->send (header, ZMQ_SNDMORE))
        throw std::runtime_error ("Failed to send the message header");

    while (more)
    {
        memcpy (&flags, vbuf + pos, sizeof (int32_t));
        pos += sizeof (int32_t);

        int tmp_flags = static_cast <int> (flags);

        memcpy (&msg_size, vbuf + pos, sizeof (size_t));
        pos += sizeof (size_t);

        zmq::message_t msg (msg_size);
        memcpy (msg.data (), vbuf + pos, msg_size);
        pos += msg_size;

        if (!m_socket.get ()->send (msg, tmp_flags))
            throw std::runtime_error ("Failed to send the message part");

        more = (flags & ZMQ_SNDMORE);
    }
	m_store->mark_in_flight (key);
    return NOP;
}