#include "goby/core/core.h"
#include "goby/util/time.h"
#include "test.pb.h"
#include <ctime>
#include <google/protobuf/text_format.h>
#include <boost/unordered_map.hpp>

using goby::util::goby_time;
using goby::core::proto::Filter;

TestGConfig cfg;

class TestG1 : public goby::core::ApplicationBase
{
public:
    TestG1()
        : goby::core::ApplicationBase("test_app1", boost::posix_time::milliseconds(1000))
        {
            subscribe(&TestG1::handler, this, make_filter("name", Filter::EQUAL, "joe"));
            subscribe(&TestG1::handler2, this, make_filter("name", Filter::EQUAL, "bob"));
        }

private:
    void loop()
        {
            static int i = 0;
            TestMessage a;

            a.set_foo(++i);
//            publish(a);
            // std::string out;
            // google::protobuf::TextFormat::PrintToString(a, &out);
            // glogger << a.GetDescriptor()->full_name() << std::endl;
            // glogger << out << std::endl;
            
//            glogger << "out: " << a << std::endl;
        }

    void handler(const TestMessage& msg)
        {
            double d = msg.foo();
            
            glogger << "in1: " << msg << std::endl;
        }

    void handler2(const TestMessage& msg)
        {
            glogger << "in2: " << msg << std::endl;
        }

};    

int main()
{
    // boost::unordered_multimap<GobyVariable, int> m;
   
    // m.insert(std::make_pair(GobyVariable("Foo", "a"),1));
    // m.insert(std::make_pair(GobyVariable("Foo"),2));
    // m.insert(std::make_pair(GobyVariable("Foo", "b"),3));
    // m.insert(std::make_pair(GobyVariable("Bar"),4));
    // m.insert(std::make_pair(GobyVariable("Bar", "a"),5));
    // m.insert(std::make_pair(GobyVariable("Baz", "c"),6));
    // m.insert(std::make_pair(GobyVariable("Quk", "c"),6));

    // typedef boost::unordered_multimap<GobyVariable,int>::iterator iterator;

    // for (iterator it = m.begin(); it != m.end(); ++it)
    //     std::cout << (*it).first.type_name << " " << (*it).first.var_name << " => " << (*it).second << std::endl;

    // std::cout << std::endl;
    
    // std::pair<iterator,iterator> p_it = m.equal_range(GobyVariable("Foo", ""));
    
    //  for (iterator it = p_it.first; it != p_it.second; ++it)
    //      std::cout << (*it).first.type_name << " " << (*it).first.var_name << " => " << (*it).second << std::endl;
    
    
    TestG1 g1;
    g1.run();
}
