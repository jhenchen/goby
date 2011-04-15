// copyright 2010-2011 t. schneider tes@mit.edu
// 
// the file is the goby daemon, part of the core goby autonomy system
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

#include "minimal_application_base.h"
#include "zero_mq_node.h"

namespace goby
{

    namespace core
    {
        
        class ProtobufApplicationBase : public ZeroMQNode, public MinimalApplicationBase
        {
          protected:
            ProtobufApplicationBase(google::protobuf::Message* cfg = 0);
            virtual ~ProtobufApplicationBase();

            virtual void inbox(const std::string& protobuf_type_name,
                               const void* data,
                               int size) = 0;
            
          private:

            void inbox(MarshallingScheme marshalling_scheme,
                       const std::string& identifier,
                       const void* data,
                       int size);

        };
    }    
}