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
 * @file ImageDataSubscriber.cpp
 *
 */

#include "ImageDataSubscriber.h"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>

#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/core/status/BaseStatus.hpp>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/InstanceState.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastrtps/types/TypesBase.h>

#include "ImageDataPubSubTypes.h"

std::atomic<bool> ImageDataSubscriber::running_(false);
std::mutex ImageDataSubscriber::mtx_;
std::condition_variable ImageDataSubscriber::cv_;

using namespace eprosima::fastdds::dds;

ImageDataSubscriber::ImageDataSubscriber()
    : participant_(nullptr)
    , subscriber_(nullptr)
    , topic_(nullptr)
    , reader_(nullptr)
    , type_(new ImageDataMsgPubSubType())
    , matched_(0)
    , received_samples_(0)
    , lost_samples_(0)
{
}

bool ImageDataSubscriber::init()
{
    //CREATE THE PARTICIPANT
    DomainParticipantQos pqos;

    pqos.properties().properties().emplace_back("dds.sec.auth.plugin",
            "builtin.PKI-DH");
    pqos.properties().properties().emplace_back("dds.sec.auth.builtin.PKI-DH.identity_ca",
            "file://certs/maincacert.pem");
    pqos.properties().properties().emplace_back("dds.sec.auth.builtin.PKI-DH.identity_certificate",
            "file://certs/mainsubcert.pem");
    pqos.properties().properties().emplace_back("dds.sec.auth.builtin.PKI-DH.private_key",
            "file://certs/mainsubkey.pem");

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

    participant_ = DomainParticipantFactory::get_instance()->create_participant(11, pqos);

    if (participant_ == nullptr)
    {
        return false;
    }

    //REGISTER THE TYPE
    type_.register_type(participant_);

    //CREATE THE SUBSCRIBER
    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);

    if (subscriber_ == nullptr)
    {
        return false;
    }

    //CREATE THE TOPIC
    topic_ = participant_->create_topic("MetaImageDataTopic", type_.get_type_name(), TOPIC_QOS_DEFAULT);
    //CREATE THE DATAREADER
    DataReaderQos rqos;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    rqos.durability().kind = VOLATILE_DURABILITY_QOS;
    rqos.history().kind = KEEP_LAST_HISTORY_QOS;
    rqos.history().depth = 100;

    reader_ = subscriber_->create_datareader(topic_, rqos, this);

    if (reader_ == nullptr)
    {
        return false;
    }

    return true;
}

ImageDataSubscriber::~ImageDataSubscriber()
{
    if (reader_ != nullptr)
    {
        subscriber_->delete_datareader(reader_);
    }
    if (topic_ != nullptr)
    {
        participant_->delete_topic(topic_);
    }
    if (subscriber_ != nullptr)
    {
        participant_->delete_subscriber(subscriber_);
    }
    DomainParticipantFactory::get_instance()->delete_participant(participant_);
}

void ImageDataSubscriber::on_subscription_matched(
        DataReader*,
        const SubscriptionMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        matched_ = info.total_count;
        std::cout << "Subscriber matched" << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        matched_ = info.total_count;
        std::cout << "Subscriber unmatched" << std::endl;
    }
    else
    {
        std::cout << info.current_count_change
                  << " is not a valid value for SubscriptionMatchedStatus current count change" << std::endl;
    }
}

void ImageDataSubscriber::on_data_available(
        DataReader* reader)
{
    SampleInfo info;
    if (reader->take_next_sample(&msg_, &info) == ReturnCode_t::RETCODE_OK)
    {
        if (info.instance_state == ALIVE_INSTANCE_STATE)
        {
            received_samples_++;
        }
    }
}

void ImageDataSubscriber::on_sample_lost(
        eprosima::fastdds::dds::DataReader* /*reader*/,
        const eprosima::fastdds::dds::SampleLostStatus& status)
{
    lost_samples_ = status.total_count;
}

void ImageDataSubscriber::run()
{
    running_.store(true);
    signal(SIGINT, [](int /*signum*/)
            {
                std::cout << "SIGINT received, stopping Subscriber execution." << std::endl;
                running_.store(false);
                cv_.notify_all();
            });

    std::cout << "Subscriber running. Please press CTRL-C to stop the Subscriber" << std::endl;

    std::unique_lock<std::mutex> lck(mtx_);
    cv_.wait(lck, []
            {
                return !running_;
            });

    std::cout << "Received samples: " << received_samples_ << std::endl;
    std::cout << "Lost samples:     " << lost_samples_ << std::endl;
}
