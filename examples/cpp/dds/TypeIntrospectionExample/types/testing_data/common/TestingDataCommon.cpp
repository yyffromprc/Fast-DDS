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

/**
 * @file ComplexCode.h
 *
 */

#include <fastrtps/types/DynamicDataPtr.h>
#include <fastrtps/types/DynamicDataFactory.h>
#include <fastrtps/types/DynamicTypeBuilderFactory.h>
#include <fastrtps/types/DynamicTypeBuilderPtr.h>

#include "../../types.hpp"

using namespace eprosima::fastrtps;

template <>
eprosima::fastrtps::types::DynamicData_ptr get_data_by_type<DataTypeKind::TESTING_DATA>(
        const unsigned int& index,
        eprosima::fastrtps::types::DynamicType_ptr dyn_type)
{
    // Create and initialize new data
    eprosima::fastrtps::types::DynamicData_ptr new_data;
    new_data = eprosima::fastrtps::types::DynamicDataFactory::get_instance()->create_data(dyn_type);

    // Set index
    new_data->set_uint32_value(index, 0);
    // Set time
    new_data->set_uint64_value(index, 1);
    // Set message (it requires to loan the array)
    eprosima::fastrtps::types::DynamicData* char_array = new_data->loan_value(2);

    std::string msg_string = "Hello World " + std::to_string(index % 100000);

    unsigned int i = 0;
    for (const char& c : msg_string)
    {
        char_array->set_char8_value(c, i++);
    }

    for (;i < 63; i++)
    {
        char_array->set_char8_value('_', i);
    }
    char_array->set_char8_value('\0', 63);

    new_data->return_loaned_value(char_array);

    // Initialize array
    eprosima::fastrtps::types::DynamicData* a_array = new_data->loan_value(3);
    eprosima::fastrtps::types::DynamicData* elem;
    // for (i=0; i<30; i++)
    {
        elem = a_array->loan_value(0);
        a_array->return_loaned_value(elem);
        elem = a_array->loan_value(1);
        a_array->return_loaned_value(elem);

    }
    new_data->return_loaned_value(a_array);

    return new_data;
}
