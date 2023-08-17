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
 * @file ImageDataPublisher.cpp
 *
 */

#include "ImageDataPublisher.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/core/status/PublicationMatchedStatus.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.h>
#include <fastrtps/types/TypesBase.h>

#include "ImageDataPubSubTypes.h"

std::atomic<bool> ImageDataPublisher::running_(false);
std::mutex ImageDataPublisher::running_mtx_;
std::condition_variable ImageDataPublisher::running_cv_;

using namespace eprosima::fastdds::dds;

ImageDataPublisher::ImageDataPublisher()
    : participant_(nullptr)
    , publisher_(nullptr)
    , topic_(nullptr)
    , writer_(nullptr)
    , type_(new ImageDataMsgPubSubType())
    , first_connected_(false)
    , frequency_(10)
{
    init_msg();
}

bool ImageDataPublisher::init()
{
    //CREATE THE PARTICIPANT
    DomainParticipantQos pqos;
    pqos.properties().properties().emplace_back("dds.sec.auth.plugin",
            "builtin.PKI-DH");
    pqos.properties().properties().emplace_back("dds.sec.auth.builtin.PKI-DH.identity_ca",
            "file://certs/maincacert.pem");
    pqos.properties().properties().emplace_back("dds.sec.auth.builtin.PKI-DH.identity_certificate",
            "file://certs/mainpubcert.pem");
    pqos.properties().properties().emplace_back("dds.sec.auth.builtin.PKI-DH.private_key",
            "file://certs/mainpubkey.pem");

    /* Proper security configuration */
    pqos.properties().properties().emplace_back("dds.sec.access.plugin",
            "builtin.Access-Permissions");
    pqos.properties().properties().emplace_back("dds.sec.access.builtin.Access-Permissions.permissions_ca",
            "file://certs/maincacert.pem");
    pqos.properties().properties().emplace_back("dds.sec.access.builtin.Access-Permissions.governance",
            "file://certs/governance.smime");
    pqos.properties().properties().emplace_back("dds.sec.access.builtin.Access-Permissions.permissions",
            "file://certs/permissions.smime");

    pqos.properties().properties().emplace_back("dds.sec.crypto.plugin",
            "builtin.AES-GCM-GMAC");

    /* Use of deprecated property */
    // pqos.properties().properties().emplace_back("rtps.participant.rtps_protection_kind", "ENCRYPT");

    auto udp_transport = std::make_shared<eprosima::fastdds::rtps::UDPv4TransportDescriptor>();
    pqos.transport().user_transports.push_back(udp_transport);
    pqos.transport().use_builtin_transports = false;

    participant_ = DomainParticipantFactory::get_instance()->create_participant(0, pqos);

    if (participant_ == nullptr)
    {
        return false;
    }

    //REGISTER THE TYPE
    type_.register_type(participant_);

    //CREATE THE PUBLISHER
    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);

    if (publisher_ == nullptr)
    {
        return false;
    }

    //CREATE THE TOPIC
    topic_ = participant_->create_topic("MetaImageDataTopic",  type_.get_type_name(), TOPIC_QOS_DEFAULT);

    if (topic_ == nullptr)
    {
        return false;
    }

    //CREATE THE DATAWRITER
    DataWriterQos wqos;
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    wqos.durability().kind = VOLATILE_DURABILITY_QOS;
    wqos.history().kind = KEEP_LAST_HISTORY_QOS;
    wqos.history().depth = 5;
    writer_ = publisher_->create_datawriter(topic_, wqos, this);

    if (writer_ == nullptr)
    {
        return false;
    }

    return true;
}

ImageDataPublisher::~ImageDataPublisher()
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
}

void ImageDataPublisher::on_publication_matched(
        eprosima::fastdds::dds::DataWriter*,
        const eprosima::fastdds::dds::PublicationMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        matched_ = info.total_count;
        first_connected_ = true;
        std::cout << "Publisher matched." << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        matched_ = info.total_count;
        std::cout << "Publisher unmatched." << std::endl;
    }
    else
    {
        std::cout << info.current_count_change
                  << " is not a valid value for PublicationMatchedStatus current count change" << std::endl;
    }
    running_cv_.notify_all();
}

void ImageDataPublisher::run(
        uint16_t frequency)
{
    frequency_ = frequency;
    running_.store(true);
    signal(SIGINT, [](int /*signum*/)
            {
                std::cout << "SIGINT received, stopping Publisher execution." << std::endl;
                running_.store(false);
                running_cv_.notify_all();
            });

    std::cout << "Publisher running. Please press CTRL-C to stop the Publisher" << std::endl;

    std::thread working_thread(&ImageDataPublisher::publish, this);

    std::unique_lock<std::mutex> lck(running_mtx_);
    running_cv_.wait(lck, []
            {
                return !running_;
            });

    working_thread.join();

    std::cout << "Sent samples: " << msg_.frameNumber() << std::endl;

}

void ImageDataPublisher::publish()
{
    // Wait for match
    std::unique_lock<std::mutex> lck(matched_mtx_);
    running_cv_.wait(lck, [this]
            {
                return !running_ || matched_ > 0;
            });

    std::cout << "Starting publication at " << frequency_ << " Hz" << std::endl;

    while (running_ && matched_ > 0)
    {
        msg_.frameNumber(msg_.frameNumber() + 1);
        if (writer_->write(&msg_))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / frequency_));
        }
        else
        {
            msg_.frameNumber(msg_.frameNumber() - 1);
            std::cout << "Something went wrong while sending frame " << msg_.frameNumber() << ". Closing down..." <<
                std::endl;
            running_.store(false);
            running_cv_.notify_all();
        }
    }
}

void ImageDataPublisher::init_msg()
{
    msg_.cameraId(1);
    msg_.frameNumber(0);
    msg_.frameTag(1);
    msg_.exposureDuration(0.5);
    msg_.gain(0.7);
    msg_.readoutDurationSeconds(0.01);
    msg_.captureTimestampNs(1);
    msg_.captureTimestampInProcessingClockDomainNs(1);
    msg_.arrivalTimestampNs(1);
    msg_.processingStartTimestampNs(1);
    msg_.temperatureDegC(35.5);
    ImageFormatMsg format;
    format.width(1);
    format.height(1);
    format.stride(1);
    format.format(PixelFormatEnum::FORMAT_1);
    msg_.imageFormat(format);
    msg_.videoCodecName("some_codec_name");
    msg_.imageBufferSize(50);
    msg_.data(std::vector<uint8_t>(200000, 0xAA));
}
