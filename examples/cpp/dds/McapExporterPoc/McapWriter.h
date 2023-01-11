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
 * @file McapWriter.h
 *
 */

#ifndef MCAPWRITER_H_
#define MCAPWRITER_H_

#include "SupremeHelloWorldPubSubTypes.h"

#include <vector>

#include <fastdds/dds/topic/TypeSupport.hpp>

#include <mcap/writer.hpp>

class McapWriter
{
public:

    McapWriter();

    virtual ~McapWriter();

    //!Initialize
    bool init();

    //!Publish a sample
    bool publish(
            mcap::ChannelId channel_id = 0);

    //!Run publisher
    void run(
            uint32_t sleep);

    // Get current timestamp
    mcap::Timestamp now();

private:

    SupremeHelloWorld shello_;
    eprosima::fastdds::dds::TypeSupport type_;

    bool stop_;
    void run_thread(
            uint32_t sleep,
            mcap::ChannelId channel_id = 0);


    mcap::McapWriter mcap_writer_;
    mcap::Schema schema_;
    std::vector<mcap::Channel> channels_;
    uint8_t n_channels_;
    static const char* SCHEMA_NAME;
    static const char* SCHEMA_TEXT;
};



#endif /* MCAPWRITER_H_ */
