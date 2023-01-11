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
 * @file HelloWorldPublisher.cpp
 *
 */

#define MCAP_IMPLEMENTATION  // Define this in exactly one .cpp file

#include "HelloWorldPublisher.h"
#include "SupremeHelloWorldPubSubTypes.h"
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/attributes/PublisherAttributes.h>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

#include <mcap/writer.hpp>

#include <thread>

using namespace eprosima::fastdds::dds;

const char* HelloWorldPublisher::SCHEMA_NAME = "/helloworld";
const char* HelloWorldPublisher::SCHEMA_TEXT = R"(
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

HelloWorldPublisher::HelloWorldPublisher()
    : participant_(nullptr)
    , publisher_(nullptr)
    , topic_(nullptr)
    , writer_(nullptr)
    , type_(new SupremeHelloWorldPubSubType())
{
}

bool HelloWorldPublisher::init()
{
    hello_.index(0);
    hello_.message("[Inner] HelloWorld");
    shello_.msg("[Outer] HelloWorld");
    // std::array<char, 10> arr = {'a', 'b', 'a', 'b', 'a', 'b', 'a', 'b'};
    // shello_.array().a(arr);
    std::vector<int32_t> vec = {0};
    shello_.array().b(vec);
    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("Participant_pub");
    auto factory = DomainParticipantFactory::get_instance();
    participant_ = factory->create_participant(0, pqos);

    if (participant_ == nullptr)
    {
        return false;
    }

    // Register the type
    type_.register_type(participant_);

    // Create the publisher
    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);

    if (publisher_ == nullptr)
    {
        return false;
    }

    // Create the topic
    topic_ = participant_->create_topic("/helloworld", "SupremeHelloWorld", TOPIC_QOS_DEFAULT);

    if (topic_ == nullptr)
    {
        return false;
    }

    // Create the writer
    writer_ = publisher_->create_datawriter(topic_, DATAWRITER_QOS_DEFAULT);

    if (writer_ == nullptr)
    {
        return false;
    }

    // Initialize MCAP Writer
    auto status = mcap_writer_.open("output.mcap", mcap::McapWriterOptions("ros2"));
    if (!status.ok()) {
        std::cerr << "Failed to open MCAP file for writing: " << status.message << "\n";
        return false;
    }

    // Register a Schema
    helloworld_schema = mcap::Schema(
        HelloWorldPublisher::SCHEMA_NAME,
        "ros2msg",
        HelloWorldPublisher::SCHEMA_TEXT);
    mcap_writer_.addSchema(helloworld_schema);

    // Register a Channel
    helloworld_channel = mcap::Channel("/helloworld", "cdr", helloworld_schema.id);
    mcap_writer_.addChannel(helloworld_channel);

    return true;
}

HelloWorldPublisher::~HelloWorldPublisher()
{
    if (writer_ != nullptr)
    {
        publisher_->delete_datawriter(writer_);
    }
    if (publisher_ != nullptr)
    {
        participant_->delete_publisher(publisher_);
    }
    if (topic_ != nullptr)
    {
        participant_->delete_topic(topic_);
    }
    DomainParticipantFactory::get_instance()->delete_participant(participant_);

    mcap_writer_.close();
}

void HelloWorldPublisher::run_thread(
        uint32_t sleep)
{
    while (!stop_)
    {
        if (publish())
        {
            std::cout << "Message: " << hello_.message() << " with index: " << hello_.index()
                      << " SENT" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }
}

void HelloWorldPublisher::run(
        uint32_t sleep)
{
    stop_ = false;
    std::thread thread(&HelloWorldPublisher::run_thread, this, sleep);
    std::cout << "Publisher running... \nPlease press enter to stop the publisher at any time." << std::endl;
    std::cin.ignore();
    stop_ = true;
    thread.join();
}

bool HelloWorldPublisher::publish()
{
    hello_.index(hello_.index() + 1);
    shello_.array().b().push_back(hello_.index());
    shello_.array().h().push_back(hello_);
    writer_->write(&shello_);

    // Write message in mcap file
    mcap::Message msg;
    eprosima::fastrtps::rtps::SerializedPayload_t* serialized_payload =
            new eprosima::fastrtps::rtps::SerializedPayload_t(5000);
    type_.serialize(&shello_, serialized_payload);
    msg.channelId = helloworld_channel.id;
    msg.sequence = hello_.index();
    msg.logTime = now();
    msg.publishTime = msg.logTime;
    msg.data = reinterpret_cast<std::byte*>(serialized_payload->data);
    msg.dataSize = serialized_payload->length;
    static_cast<void>(mcap_writer_.write(msg));

    return true;
}

mcap::Timestamp HelloWorldPublisher::now() {
  return mcap::Timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
}
