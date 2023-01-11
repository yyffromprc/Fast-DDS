// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
 * @file McapWriter.cpp
 *
 */

#define MCAP_IMPLEMENTATION  // Define this in exactly one .cpp file

#include "McapWriter.h"
#include "SupremeHelloWorldPubSubTypes.h"

#include <mcap/writer.hpp>

#include <thread>
#include <sstream>
#include <string>

using namespace eprosima::fastdds::dds;

const char* McapWriter::SCHEMA_NAME = "/helloworld";
const char* McapWriter::SCHEMA_TEXT = R"(
HelloWorld hello
Arrays array
string msg
================================================================================
MSG: fastdds/HelloWorld
uint32 index
string message
================================================================================
MSG: fastdds/Arrays
char[10] a
int32[] b
HelloWorld[] h
)";

McapWriter::McapWriter()
    : type_(new SupremeHelloWorldPubSubType())
    , n_channels_(5)
{
}

bool McapWriter::init()
{
    shello_.hello().index(0);
    shello_.hello().message("[Inner] HelloWorld");
    shello_.msg("[Outer] HelloWorld");
    std::vector<int32_t> vec = {0};
    shello_.array().b(vec);

    // Initialize MCAP Writer
    auto status = mcap_writer_.open("output.mcap", mcap::McapWriterOptions("ros2"));
    if (!status.ok()) {
        std::cerr << "Failed to open MCAP file for writing: " << status.message << "\n";
        return false;
    }

    // Register a Schema
    schema_ = mcap::Schema(McapWriter::SCHEMA_NAME, "ros2msg", McapWriter::SCHEMA_TEXT);
    mcap_writer_.addSchema(schema_);

    for (int i = 0; i < n_channels_; i++)
    {
        std::stringstream ss;
        ss << "/helloworld/" << i;
        std::string str = ss.str();
        channels_.push_back(mcap::Channel(str, "cdr", schema_.id));
        mcap_writer_.addChannel(channels_.back());
    }

    return true;
}

McapWriter::~McapWriter()
{
    mcap_writer_.close();
}

void McapWriter::run_thread(
        uint32_t sleep,
        mcap::ChannelId channel_id /* = 0 */)
{
    while (!stop_)
    {
        for (const auto& channel : channels_)
        {
            if (publish(channel.id))
            {
                std::cout << "Message saved [" << shello_.hello().index() << "] in channel " << channel.id << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep));

            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }
}

void McapWriter::run(
        uint32_t sleep)
{
    stop_ = false;
    std::thread thread(&McapWriter::run_thread, this, sleep, 0);
    std::cout << "MCAP writer running... \nPlease press enter to stop the writer at any time." << std::endl;
    std::cin.ignore();
    stop_ = true;
    thread.join();
}

bool McapWriter::publish(
        mcap::ChannelId channel_id)
{
    shello_.hello().index(shello_.hello().index() + 1);

    std::stringstream ss;
    ss << "Channel " << channel_id;
    std::string message = ss.str();
    shello_.hello().message(message);
    shello_.array().b().push_back(shello_.hello().index());
    shello_.array().h().push_back(shello_.hello());

    // Write message in mcap file
    mcap::Message msg;
    eprosima::fastrtps::rtps::SerializedPayload_t* serialized_payload =
            new eprosima::fastrtps::rtps::SerializedPayload_t(5000);
    type_.serialize(&shello_, serialized_payload);
    msg.channelId = channel_id;
    msg.sequence = shello_.hello().index();
    msg.logTime = now();
    msg.publishTime = msg.logTime;
    msg.data = reinterpret_cast<std::byte*>(serialized_payload->data);
    msg.dataSize = serialized_payload->length;
    static_cast<void>(mcap_writer_.write(msg));

    return true;
}

mcap::Timestamp McapWriter::now() {
  return mcap::Timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
}
