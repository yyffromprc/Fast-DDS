# Copyright 2022 Proyectos y Sistemas de Mantenimiento SL (eProsima).
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#################################
# BASIC
#################################
find_package(GTest CONFIG REQUIRED)

add_executable(basic_dds_tests
    BasicDDSTests.cpp
    types/FixedSized.cxx
    types/FixedSizedPubSubTypes.cxx
    types/StringTest.cxx
    types/StringTestPubSubTypes.cxx
)

target_link_libraries(
    basic_dds_tests
    GTest::gtest
    fastcdr
    fastrtps
    foonathan_memory
)

set(TEST_LIST
    test_fixed_intraprocess_reliable_datasharing
    test_string_intraprocess_reliable_datasharing
    test_fixed_intraprocess_reliable
    test_string_intraprocess_reliable
    test_fixed_reliable
    test_strings_reliable
    test_fixed_datasharing
    test_string_datasharing
)

foreach(test_name ${TEST_LIST})
    add_test(NAME basic_dds_tests.${test_name}
        COMMAND
            basic_dds_tests
            --gtest_filter=basic_dds_tests.${test_name}:**/basic_dds_tests.${test_name}/**)
endforeach()

#################################
# EFFORT
#################################
find_package(GTest CONFIG REQUIRED)

add_executable(effort_dds_tests
    EffortDDSTests.cpp
    types/FixedSized.cxx
    types/FixedSizedPubSubTypes.cxx
    types/StringTest.cxx
    types/StringTestPubSubTypes.cxx
)

target_link_libraries(
    effort_dds_tests
    GTest::gtest
    fastcdr
    fastrtps
    foonathan_memory
)

set(TEST_LIST
    test_2w_2r_reliable
)

foreach(test_name ${TEST_LIST})
    add_test(NAME effort_dds_tests.${test_name}
        COMMAND
            effort_dds_tests
            --gtest_filter=effort_dds_tests.${test_name}:**/effort_dds_tests.${test_name}/**)
endforeach()

#################################
# COMPLEX
#################################
find_package(GTest CONFIG REQUIRED)

add_executable(complex_dds_tests
    ComplexEffortDDSTests.cpp
    types/FixedSized.cxx
    types/FixedSizedPubSubTypes.cxx
    types/StringTest.cxx
    types/StringTestPubSubTypes.cxx
)

target_link_libraries(
    complex_dds_tests
    GTest::gtest
    fastcdr
    fastrtps
    foonathan_memory
)

set(TEST_LIST
    test_2w_2r_reliable
)

foreach(test_name ${TEST_LIST})
    add_test(NAME complex_dds_tests.${test_name}
        COMMAND
            complex_dds_tests
            --gtest_filter=complex_dds_tests.${test_name}:**/complex_dds_tests.${test_name}/**)
endforeach()
