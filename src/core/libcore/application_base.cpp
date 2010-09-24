// copyright 2010 t. schneider tes@mit.edu
//
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#include <iostream>

#include "goby/util/time.h"

#include "application_base.h"

using namespace goby::core;
using namespace goby::util;
using boost::interprocess::message_queue;
using boost::shared_ptr;

boost::posix_time::time_duration goby::core::ApplicationBase::CONNECTION_WAIT_INTERVAL =
    boost::posix_time::seconds(1);

goby::core::ApplicationBase::ApplicationBase(
    const std::string& application_name /* = "" */,
    boost::posix_time::time_duration loop_period /*= boost::posix_time::milliseconds(100)*/)
    : application_name_(application_name),
      loop_period_(loop_period),
      connected_(false)
{
    // connect and some other initialization duties
    if(!application_name.empty()) start(); 
}

goby::core::ApplicationBase::~ApplicationBase()
{
    glogger << "ApplicationBase destructing..." << std::endl;
    
    if(connected_) end();
}

void goby::core::ApplicationBase::start()
{
    std::string verbosity = "verbose";
    glogger.add_stream(verbosity, &std::cout);
    glogger.name(application_name());

    connect();
    
    to_server_queue_ = shared_ptr<message_queue>(new message_queue(boost::interprocess::open_only, std::string(goby::core::TO_SERVER_QUEUE_PREFIX + application_name_).c_str()));
    
    from_server_queue_ = shared_ptr<message_queue>(new message_queue(boost::interprocess::open_only, std::string(goby::core::FROM_SERVER_QUEUE_PREFIX + application_name_).c_str()));
    
    t_start_ = goby_time();
    // start on the next even second
    t_next_loop_ = boost::posix_time::second_clock::universal_time() +
        boost::posix_time::seconds(1);
}

void goby::core::ApplicationBase::end()
{
    disconnect();
}

void goby::core::ApplicationBase::connect()
{
    try
    {
        glogger <<  "trying to connect..." <<  std::endl;
        
        // set up queues to wait for response from server

        message_queue::remove(std::string(CONNECT_RESPONSE_QUEUE_PREFIX + application_name_).c_str());

        // TODO(tes): mangle application name to prevent "/" problems in queue name
        message_queue response_queue(boost::interprocess::create_only,
                                     std::string(CONNECT_RESPONSE_QUEUE_PREFIX
                                                 + application_name_).c_str(),
                                     1,
                                     MAX_MSG_BUFFER_SIZE);
        
        // make connection
        message_queue listen_queue(boost::interprocess::open_only, CONNECT_LISTEN_QUEUE.c_str());
        
        // populate server request for connection
        notification_.Clear();
        notification_.set_notification_type(proto::Notification::CONNECT_REQUEST);
        notification_.set_application_name(application_name_);

        // serialize and send the server request
        send(listen_queue, notification_, buffer_, sizeof(buffer_));  
        
        // wait for response
        if(!timed_receive(response_queue,
                          notification_,
                          goby_time() + CONNECTION_WAIT_INTERVAL,
                          buffer_,
                          &buffer_msg_size_))
        {
            glogger << die << "no response to connection request. make sure gobyd is alive."
                    <<  std::endl;        
        }
        

        if(notification_.notification_type() == proto::Notification::CONNECTION_ACCEPTED)
        {
            connected_ = true;
            glogger <<  "connection succeeded." <<  std::endl;
        }
        else
        {
            glogger << die << "server did not connect us: " << "\n"
                    << notification_ << std::endl;
        }

        message_queue::remove(std::string(CONNECT_RESPONSE_QUEUE_PREFIX + application_name_).c_str());

    }
    catch(boost::interprocess::interprocess_exception &ex)
    {
        glogger << die << "could not open message queue(s) with gobyd. check to make sure gobyd is alive. " << ex.what() << std::endl;
    }
}


void goby::core::ApplicationBase::disconnect()
{
    try
    {
        // make disconnection
        message_queue listen_queue(boost::interprocess::open_only,
                                   CONNECT_LISTEN_QUEUE.c_str());
    
        // populate server request for connection
        notification_.Clear();
        notification_.set_notification_type(proto::Notification::DISCONNECT_REQUEST);
        notification_.set_application_name(application_name_);

        glogger << notification_ << std::endl;
        
        // serialize and send the server request
        send(listen_queue, notification_, buffer_, sizeof(buffer_));
        connected_ = false;
    }
    catch(boost::interprocess::interprocess_exception &ex)
    {
        glogger << warn << ex.what() << std::endl;
    }
}


void goby::core::ApplicationBase::run()
{

    while(connected_)
    {
        if(timed_receive(*from_server_queue_,
                         notification_,
                         t_next_loop_,
                         buffer_,
                         &buffer_msg_size_))
        {
            glogger << "> " << notification_ << std::endl;
            
            switch(notification_.notification_type())
            {
                case proto::Notification::HEARTBEAT:
                {
                    // reply with same message
                    send(*to_server_queue_, buffer_, buffer_msg_size_);
                }
                break;

                // someone else's publish
                case proto::Notification::PUBLISH_REQUEST:
                {
                    typedef boost::unordered_multimap<std::string, shared_ptr<SubscriptionBase> >::const_iterator const_iterator;
                    std::pair<const_iterator, const_iterator> equal_it_pair =
                        subscriptions_.equal_range(notification_.embedded_msg().type());
    
                    for(const_iterator it = equal_it_pair.first; it != equal_it_pair.second; ++it)
                        it->second->post(notification_.embedded_msg().body());
                }
                break;
                        
                default: break;
            }
        }
        else
        {
            loop();
            t_next_loop_ += loop_period_;
        }            
    }
}


void goby::core::ApplicationBase::server_notify_subscribe(const google::protobuf::Descriptor* descriptor, const proto::Filter& filter)
{
    const std::string& type_name = descriptor->full_name();

    notification_.Clear();
    
    if(!registered_protobuf_types_.count(type_name))
    {
        // copy descriptor for the new subscription type to the notification message
        descriptor->file()->CopyTo(notification_.mutable_file_descriptor_proto());
        registered_protobuf_types_.insert(type_name);
    }
    notification_.mutable_embedded_msg()->set_type(type_name);
    
    if(filter.IsInitialized() && is_valid_filter(descriptor, filter))
        notification_.mutable_embedded_msg()->mutable_filter()->CopyFrom(filter);
    
    notification_.set_notification_type(proto::Notification::SUBSCRIBE_REQUEST);

    glogger << notification_ << std::endl;
    
    
    send(*to_server_queue_, notification_, buffer_, sizeof(buffer_));
}

bool goby::core::ApplicationBase::is_valid_filter(const google::protobuf::Descriptor* descriptor, const proto::Filter& filter)
{
    using namespace google::protobuf;
    // check filter for exclusions
    const FieldDescriptor* field_descriptor = descriptor->FindFieldByName(filter.key());
    if(!field_descriptor // make sure it exists
       || (field_descriptor->cpp_type() == FieldDescriptor::CPPTYPE_ENUM || // exclude enums
           field_descriptor->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) // exclude embedded
       || field_descriptor->is_repeated()) // no repeated fields for filter
    {
        glogger << die << "bad filter: " << filter << "for message descriptor: " << descriptor->DebugString() << std::endl;
        return false;
    }
    return true;
}

void goby::core::ApplicationBase::server_notify_publish(
    const google::protobuf::Descriptor* descriptor,
    const std::string& serialized_message)
{
    const std::string& type_name = descriptor->full_name();

    notification_.Clear();
    if(!registered_protobuf_types_.count(type_name))
    {
        // copy descriptor for the new subscription type to the notification message
        descriptor->file()->CopyTo(notification_.mutable_file_descriptor_proto());
        registered_protobuf_types_.insert(type_name);
    }

    notification_.mutable_embedded_msg()->set_type(type_name);
    
    notification_.set_notification_type(proto::Notification::PUBLISH_REQUEST);
    notification_.mutable_embedded_msg()->set_body(serialized_message);

    glogger << "< " << notification_ << std::endl;    

    send(*to_server_queue_, notification_, buffer_, sizeof(buffer_));
}