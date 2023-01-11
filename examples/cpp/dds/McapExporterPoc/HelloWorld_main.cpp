// Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file HelloWorld_main.cpp
 *
 */

#include <limits>
#include <sstream>

#include "McapWriter.h"

#include <fastrtps/log/Log.h>

using eprosima::fastdds::dds::Log;

int main(
        int argc,
        char** argv)
{
    std::cout << "Starting..."<< std::endl;
    long sleep = 100;
    if (argc >= 2)
    {
        sleep = atoi(argv[2]);
    }

    McapWriter mcap_writer;
    if (mcap_writer.init())
    {
        mcap_writer.run(sleep);
    }

    Log::Reset();
    return 0;
}
