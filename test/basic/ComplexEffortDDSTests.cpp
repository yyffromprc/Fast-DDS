// Copyright 2022 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantFactoryQos.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastrtps/types/TypesBase.h>
#include <fastrtps/xmlparser/XMLProfileManager.h>

#include "types/StringTestPubSubTypes.h"
#include "types/FixedSizedPubSubTypes.h"

using namespace eprosima::fastdds::dds;

constexpr const int TEST_DOMAIN = 14;
constexpr const char* TEST_TOPIC_NAME = "some_random_topic_name";

constexpr const int TIME_ELAPSE_BETWEEN_MESSAGES_MS = 20;
constexpr const int TIME_ELAPSE_BEFORE_CLOSE_WRITER_MS = 20;

using ReturnCode_t = eprosima::fastrtps::types::ReturnCode_t;

template <typename PubSubType>
struct ParticipantPub
{
    ParticipantPub(bool reliable_transient, bool datasharing, bool keep_all)
    {
        // Create participant
        DomainParticipantFactory* factory = DomainParticipantFactory::get_instance();
        participant = factory->create_participant(TEST_DOMAIN, PARTICIPANT_QOS_DEFAULT);

        // Register type
        type_support.reset(new PubSubType());
        type_support.register_type(participant);
        data = type_support.create_data();

        // Create publisher
        publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);

        // Create Topic
        topic = participant->create_topic(TEST_TOPIC_NAME, type_support.get_type_name(), TOPIC_QOS_DEFAULT);

        // Set QoS
        DataWriterQos dw_qos;

        dw_qos.publish_mode().kind = eprosima::fastdds::dds::PublishModeQosPolicyKind::SYNCHRONOUS_PUBLISH_MODE;
        dw_qos.data_sharing().off();
        if (!datasharing)
        {
            dw_qos.data_sharing().off();
        }

        if (reliable_transient)
        {
            dw_qos.reliability().kind = eprosima::fastdds::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS;
            dw_qos.durability().kind = eprosima::fastdds::dds::DurabilityQosPolicyKind::TRANSIENT_LOCAL_DURABILITY_QOS;
        }

        if (keep_all)
        {
            dw_qos.history().kind = eprosima::fastdds::dds::HistoryQosPolicyKind::KEEP_ALL_HISTORY_QOS;
        }
        {
            dw_qos.history().depth = 5000;
        }

        // Create DataWriter
        writer = publisher->create_datawriter(topic, dw_qos);
    };

    ~ParticipantPub()
    {
        type_support.delete_data(data);
        publisher->delete_datawriter(writer);
        participant->delete_publisher(publisher);
        participant->delete_topic(topic);

        DomainParticipantFactory* factory = DomainParticipantFactory::get_instance();
        factory->delete_participant(participant);
    }

    void write()
    {
        writer->write(data);
    }

    void* data{nullptr};

    TypeSupport type_support{};
    DomainParticipant* participant{nullptr};
    Topic* topic{nullptr};
    Publisher* publisher{nullptr};
    DataWriter* writer{nullptr};
};

template <typename PubSubType>
struct ParticipantSub : public DataReaderListener
{
    ParticipantSub(bool reliable_transient, bool datasharing, bool keep_all)
    {
        // Create participant
        DomainParticipantFactory* factory = DomainParticipantFactory::get_instance();
        participant = factory->create_participant(TEST_DOMAIN, PARTICIPANT_QOS_DEFAULT);

        // Register type
        type_support.reset(new PubSubType());
        type_support.register_type(participant);
        data = type_support.create_data();

        // Create subscriber
        subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);

        // Create Topic
        topic = participant->create_topic(TEST_TOPIC_NAME, type_support.get_type_name(), TOPIC_QOS_DEFAULT);

        // Set QoS
        DataReaderQos dr_qos;

        if (!datasharing)
        {
            dr_qos.data_sharing().off();
        }

        if (reliable_transient)
        {
            dr_qos.reliability().kind = eprosima::fastdds::dds::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS;
            dr_qos.durability().kind = eprosima::fastdds::dds::DurabilityQosPolicyKind::TRANSIENT_LOCAL_DURABILITY_QOS;
        }

        if (keep_all)
        {
            dr_qos.history().kind = eprosima::fastdds::dds::HistoryQosPolicyKind::KEEP_ALL_HISTORY_QOS;
        }
        {
            dr_qos.history().depth = 5000;
        }

        // Create Datareader
        reader = subscriber->create_datareader(topic, dr_qos, this);
    };

    ~ParticipantSub()
    {
        type_support.delete_data(data);
        subscriber->delete_datareader(reader);
        participant->delete_subscriber(subscriber);
        participant->delete_topic(topic);

        DomainParticipantFactory* factory = DomainParticipantFactory::get_instance();
        factory->delete_participant(participant);
    }

    void on_data_available(DataReader*) override
    {
        {
            std::unique_lock<std::mutex> lock(cv_mutex);
            data_available.store(true);
            std::cout << "---------- OPEN ----------" << std::endl;
        }
        cv.notify_all();
    }

    void wait_for_data()
    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        std::cout << "---------- WAIT ----------" << std::endl;
        cv.wait(lock, [this](){ return data_available.load(); });
        std::cout << "---------- WAKE ----------" << std::endl;
    }

    void read()
    {
        std::cout << "---------- READ ----------" << std::endl;
        auto result = reader->take_next_sample(data, &info);
        ASSERT_EQ(result, ReturnCode_t::RETCODE_OK);

        bool n = reader->get_unread_count() > 0;
        std::unique_lock<std::mutex> lock(cv_mutex);
        data_available.store(n);
        if (n)
        {
            std::cout << "---------- CLOS ----------" << std::endl;
        }
        std::cout << "---------- FINI ----------" << std::endl;
    }

    TypeSupport type_support{};
    DomainParticipant* participant{nullptr};
    Topic* topic{nullptr};
    Subscriber* subscriber{nullptr};
    DataReader* reader{nullptr};

    void* data{nullptr};
    SampleInfo info;

    std::atomic<bool> data_available{false};
    std::condition_variable cv;
    std::mutex cv_mutex;
};

template <typename PubSubType>
void execute_test_complex(
    int messages,
    int writers, int readers,
    bool intraprocess, bool reliable_transient, bool datasharing, bool keep_all)
{
    // Disable intraprocess
    if (!intraprocess)
    {
        eprosima::fastrtps::LibrarySettingsAttributes att;
        att.intraprocess_delivery = eprosima::fastrtps::IntraprocessDeliveryType::INTRAPROCESS_OFF;
        eprosima::fastrtps::xmlparser::XMLProfileManager::library_settings(att);
    }

    // Reader routine
    auto reader_routine =
        [reliable_transient, datasharing, keep_all]
        (int index, int messages_to_receive)
        {
            std::cout << "Reader " << index << " starting." << std::endl;
            ParticipantSub<PubSubType> participant(reliable_transient, datasharing, keep_all);
            for (int i=0; i<messages_to_receive; i++)
            {
                participant.wait_for_data();
                std::cout << "Reader " << index << " reading " << i << std::endl;
                participant.read();
                std::this_thread::sleep_for(std::chrono::milliseconds(TIME_ELAPSE_BETWEEN_MESSAGES_MS/2));
            }
            std::cout << "Reader " << index << " finishing." << std::endl;
            static_cast<void>(index);
        };

    // Writer routine
    auto writer_routine =
        [reliable_transient, datasharing, keep_all]
        (int index, int messages_to_send)
        {
            std::cout << "Writer " << index << " starting." << std::endl;
            ParticipantPub<PubSubType> participant(reliable_transient, datasharing, keep_all);
            for (int i=0; i<messages_to_send; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(TIME_ELAPSE_BETWEEN_MESSAGES_MS));
                std::cout << "Writer " << index << " writting " << i << std::endl;
                participant.write();

                if (i % 10)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(TIME_ELAPSE_BETWEEN_MESSAGES_MS*5));
                }
            }
                std::this_thread::sleep_for(std::chrono::milliseconds(TIME_ELAPSE_BEFORE_CLOSE_WRITER_MS));
            std::cout << "Writer " << index << " finishing." << std::endl;
            static_cast<void>(index);
        };

    std::vector<std::thread> threads;

    // Reader threads
    for(int i=0; i<readers; i++)
    {
        threads.emplace_back(reader_routine, i, messages * (writers + writers/2));
    }

    // Writer threads
    for(int i=0; i<writers/2; i++)
    {
        threads.emplace_back(writer_routine, i, messages*2);
    }
    for(int i=writers/2; i<writers; i++)
    {
        threads.emplace_back(writer_routine, i, messages);
    }

    for (auto& t : threads)
    {
        t.join();
    }
}

TEST(complex_dds_tests, test_2w_2r_reliable)
{
    std::cout << "test_2w_2r_reliable" << std::endl;
    execute_test_complex<FixedSizedPubSubType>(
        20,    // messages
        2,     // writers
        2,     // readers
        true,  // intraprocess
        true,  // reliable
        false, // datasharing
        true   // keep all
    );
}

int main(
        int argc,
        char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
