/*
 *  Copyright 2011 Mikko Koppanen <mikko@kuut.io>
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.                 
 */
#include <uuid/uuid.h>
#include <sys/time.h>
#include "store.hpp"
#include <iostream>
#include <exception>
#include <boost/scoped_array.hpp>

void pzq::datastore_t::open (const std::string &path)
{
	std::string p = path;

    srand (time (NULL));

    this->db.tune_comparator (DECIMALCOMP);

    if (this->db.open (p, TreeDB::OWRITER | TreeDB::OCREATE | TreeDB::ONOLOCK) == false)
        throw pzq::datastore_exception (this->db);

	p.append (".inflight");

	this->inflight_db.cap_size (31457280);
	if (this->inflight_db.open (p, CacheDB::OWRITER | CacheDB::OCREATE | CacheDB::ONOLOCK) == false)
		throw pzq::datastore_exception (this->db);
}

bool pzq::datastore_t::save (const std::vector <pzq_message> &message_parts)
{
    pzq_uuid_string_t uuid_str;
    timeval tv;
	uuid_t uu;

    if (::gettimeofday (&tv, NULL)) {
        throw new std::runtime_error ("gettimeofday failed");
    }

    uuid_generate (uu);
    uuid_unparse (uu, uuid_str);

    std::stringstream kval;
    kval << tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
    kval << "|";
    kval << uuid_str;

    this->db.begin_transaction ();

    for (std::vector<pzq_message>::size_type i = 0; i != message_parts.size (); i++)
    {
        int32_t flags;
        size_t size;

        flags = static_cast<int32_t> (message_parts [i].second);
        this->db.append (kval.str ().c_str (), kval.str ().size (), (const char *) &flags, sizeof (int32_t));

        size = message_parts [i].first->size ();
        this->db.append (kval.str ().c_str (), kval.str ().size (), (const char *) &size, sizeof (size_t));

        this->db.append (kval.str ().c_str (), kval.str ().size (),
                         (const char *) message_parts [i].first->data (), message_parts [i].first->size ());
    }
    this->db.end_transaction ();
	sync ();

    return true;
}

void pzq::datastore_t::sync ()
{
	if (m_divisor == 0 || (rand () % m_divisor) == 0)
	{
		std::cerr << "Size of database: " << this->db.size () << std::endl;
		std::cerr << "Number of entries: " << this->db.count () << std::endl;

		std::cerr << "Size of inflight database: " << this->inflight_db.size () << std::endl;
		std::cerr << "Number of inflight entries: " << this->inflight_db.count () << std::endl;

        if (!this->db.synchronize (true))
            throw pzq::datastore_exception (this->db);

		if (!this->inflight_db.synchronize (true))
            throw pzq::datastore_exception (this->db);
	}
}

void pzq::datastore_t::remove (const std::string &key)
{
	this->inflight_db.remove (key.c_str (), key.size ());
	this->db.remove (key);
	sync ();
}

void pzq::datastore_t::close ()
{
    this->db.close ();
}

int64_t pzq::datastore_t::messages ()
{
    return this->db.count ();
}

bool pzq::datastore_t::is_in_flight (const std::string &k)
{
	uint64_t value;
	if (this->inflight_db.get (k.c_str (), k.size (), (char *) &value, sizeof (uint64_t)) == -1)
		return false;

	// TODO: hardcoded timeout
	if (time (NULL) - value > 5)
	{
		this->inflight_db.remove (k.c_str (), k.size ());
		std::cerr << "Message expired, scheduling for resend" << std::endl;
		return false;
	}
	return true;
}

void pzq::datastore_t::mark_in_flight (const std::string &k)
{
	uint64_t value = time (NULL);
	this->inflight_db.add (k.c_str (), k.size (), (const char *) &value, sizeof (uint64_t));
}

void pzq::datastore_t::iterate (DB::Visitor *visitor)
{
    if (!this->db.iterate (visitor, false))
        throw pzq::datastore_exception (this->db);

	sync ();
}
