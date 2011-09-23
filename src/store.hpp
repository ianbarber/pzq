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

#ifndef PZQ_STORE_HPP
# define PZQ_STORE_HPP

#include "pzq.hpp"

using namespace kyotocabinet;

namespace pzq {

    typedef char pzq_uuid_string_t [37];

    class i_datastore_t
    {
    public:
        // Called when the data store is opened
        virtual void open (const std::string &path) = 0;

        // Save a message and flags to datastore
        virtual bool save (const std::vector <pzq_message> &message_parts) = 0;

		// Delete message
		virtual void remove (const std::string &) = 0;

        // Close the store
        virtual void close () = 0;

		// sync to disk
		virtual void sync () = 0;

		// Whether message has been sent but no ACK yet
		virtual bool is_in_flight (const std::string &k) = 0;

		// Mark message sent (no ack yet)
		virtual void mark_in_flight (const std::string &k) = 0;

        // How many messages in total in store
        virtual int64_t messages () = 0;

        // Iterate using a visitor
        virtual void iterate (DB::Visitor *visitor) = 0;

        // virtual destructor
        virtual ~i_datastore_t () {}
    };

    class datastore_t : public i_datastore_t
    {
    protected:
        TreeDB db;
		CacheDB inflight_db;
        int m_divisor;

    public:
        datastore_t () : m_divisor (0)
        {}

        void open (const std::string &path);

        bool save (const std::vector <pzq_message> &message_parts);

		void remove (const std::string &key);

		void sync ();

        void close ();

        int64_t messages ();

		bool is_in_flight (const std::string &k);

		void mark_in_flight (const std::string &k);

        void set_sync_divisor (int divisor)
        {
            m_divisor = divisor;
        }

        void iterate (DB::Visitor *visitor);
    };

    class datastore_exception : public std::exception
    {
    private:
        std::string db_err;

    public:

        datastore_exception (const char *message)
        {
            db_err.append (message);
        }

        datastore_exception (const char *message, const TreeDB& db)
        {
            db_err.append (message);
            db_err.append (": ");
            db_err.append (db.error ().message ());
        }

        datastore_exception (const TreeDB& db)
        {
            db_err.append (db.error ().message ());
        }

        virtual const char* what() const throw()
        {
            return db_err.c_str ();
        }

        virtual ~datastore_exception() throw()
        {}
    };
}

#endif