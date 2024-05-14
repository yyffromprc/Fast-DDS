// Copyright 2024 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

#include <bitset>
#include <codecvt>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include <fastdds/dds/xtypes/utils.hpp>

#include <fastdds/dds/log/Log.hpp>
#include <fastdds/dds/xtypes/dynamic_types/detail/dynamic_language_binding.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeMember.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>
#include <fastdds/dds/xtypes/dynamic_types/Types.hpp>

#include "dynamic_types/DynamicDataImpl.hpp"
#include "dynamic_types/DynamicTypeImpl.hpp"
#include "dynamic_types/DynamicTypeMemberImpl.hpp"
#include "dynamic_types/MemberDescriptorImpl.hpp"
#include "dynamic_types/TypeDescriptorImpl.hpp"

#include "utils/collections/Tree.hpp"

namespace eprosima {
namespace fastdds {
namespace dds {

////////////////////////////////////////
// Dynamic Data to JSON serialization //
////////////////////////////////////////

//// Forward declarations

static ReturnCode_t json_serialize(
        const traits<DynamicDataImpl>::ref_type& data,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept;

static ReturnCode_t json_serialize_member(
        const traits<DynamicDataImpl>::ref_type& data,
        const traits<DynamicTypeMember>::ref_type& type_member,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept;

static ReturnCode_t json_serialize_member(
        const traits<DynamicDataImpl>::ref_type& data,
        MemberId member_id,
        TypeKind member_kind,
        const std::string& member_name,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept;

static ReturnCode_t json_serialize_member(
        const traits<DynamicDataImpl>::ref_type& data,
        MemberId member_id,
        TypeKind member_kind,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept;

static ReturnCode_t json_serialize_basic_member(
        const traits<DynamicDataImpl>::ref_type& data,
        MemberId member_id,
        TypeKind member_kind,
        const std::string& member_name,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept;

static ReturnCode_t json_serialize_collection(
        const traits<DynamicDataImpl>::ref_type& data,
        const std::string& member_name,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept;

static ReturnCode_t json_serialize_array(
        const traits<DynamicDataImpl>::ref_type& data,
        TypeKind member_kind,
        unsigned int& index,
        const std::vector<unsigned int>& bounds,
        nlohmann::json& j_array,
        DynamicDataJsonFormat format) noexcept;

template <typename T>
static void json_insert(
        const std::string& key,
        const T& value,
        nlohmann::json& j);

//// Implementation

ReturnCode_t json_serialize(
        const DynamicData::_ref_type& data,
        std::ostream& output,
        DynamicDataJsonFormat format) noexcept
{
    ReturnCode_t ret;
    nlohmann::json j;
    if (RETCODE_OK == (ret = json_serialize(traits<DynamicData>::narrow<DynamicDataImpl>(data), j, format)))
    {
        output << j;
    }
    else
    {
        EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while performing DynamicData to JSON serialization.");
    }
    return ret;
}

ReturnCode_t json_serialize(
        const DynamicData::_ref_type& data,
        std::string& output,
        DynamicDataJsonFormat format) noexcept
{
    ReturnCode_t ret;
    std::stringstream ss;

    if (RETCODE_OK == (ret = json_serialize(data, ss, format)))
    {
        output = ss.str();
    }
    else
    {
        EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while performing DynamicData to JSON serialization.");
    }
    return ret;
}

ReturnCode_t json_serialize(
        const traits<DynamicDataImpl>::ref_type& data,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept
{
    if (nullptr != data)
    {
        switch (data->type()->get_kind())
        {
            case TK_STRUCTURE:
            {
                ReturnCode_t ret = RETCODE_OK;
                DynamicTypeMembersById members;
                if (RETCODE_OK != (ret = data->type()->get_all_members(members)))
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                            "Error encountered while serializing structure to JSON: get_all_members failed.");
                    return ret;
                }
                for (auto it : members)
                {
                    if (RETCODE_OK != (ret = json_serialize_member(data, it.second, output, format)))
                    {
                        EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                                "Error encountered while serializing structure member to JSON.");
                        break;
                    }
                }
                return ret;
            }
            default:
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Only structs are supported by json_serialize method.");
                return RETCODE_BAD_PARAMETER;
            }
        }
    }
    else
    {
        EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                "Encountered null data value while performing DynamicData to JSON serialization.");
        return RETCODE_BAD_PARAMETER;
    }
}

ReturnCode_t json_serialize_member(
        const traits<DynamicDataImpl>::ref_type& data,
        const traits<DynamicTypeMember>::ref_type& type_member,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept
{
    MemberDescriptorImpl& member_desc =
            traits<DynamicTypeMember>::narrow<DynamicTypeMemberImpl>(type_member)->get_descriptor();

    return json_serialize_member(data, type_member->get_id(),
                   traits<DynamicType>::narrow<DynamicTypeImpl>(
                       member_desc.type())->resolve_alias_enclosed_type()->get_kind(),
                   type_member->get_name().to_string(), output, format);
}

ReturnCode_t json_serialize_member(
        const traits<DynamicDataImpl>::ref_type& data,
        MemberId member_id,
        TypeKind member_kind,
        const std::string& member_name,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept
{
    switch (member_kind)
    {
        case TK_NONE:
        case TK_BOOLEAN:
        case TK_BYTE:
        case TK_INT8:
        case TK_INT16:
        case TK_INT32:
        case TK_INT64:
        case TK_UINT8:
        case TK_UINT16:
        case TK_UINT32:
        case TK_UINT64:
        case TK_FLOAT32:
        case TK_FLOAT64:
        case TK_FLOAT128:
        case TK_CHAR8:
        case TK_CHAR16:
        case TK_STRING8:
        case TK_STRING16:
        case TK_ENUM:
        case TK_BITMASK:
        {
            return json_serialize_basic_member(data, member_id, member_kind, member_name, output, format);
        }
        case TK_STRUCTURE:
        case TK_BITSET:
        {
            traits<DynamicDataImpl>::ref_type st_data =
                    traits<DynamicData>::narrow<DynamicDataImpl>(data->loan_value(member_id));
            if (nullptr != st_data)
            {
                ReturnCode_t ret = RETCODE_OK;
                nlohmann::json j_struct;
                DynamicTypeMembersById members;
                if (RETCODE_OK != (ret = st_data->enclosing_type()->get_all_members(members)))
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                            "Error encountered while serializing structure/bitset member to JSON: get_all_members failed.");
                }
                else
                {
                    for (auto it : members)
                    {
                        if (RETCODE_OK != (ret = json_serialize_member(st_data, it.second, j_struct, format)))
                        {
                            EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                                    "Error encountered while serializing structure/bitset member's member to JSON.");
                            break;
                        }
                    }
                }
                if (RETCODE_OK == ret)
                {
                    json_insert(member_name, j_struct, output);
                }
                // Return loaned value
                ReturnCode_t ret_return_loan;
                if (RETCODE_OK != (ret_return_loan = data->return_loaned_value(st_data)))
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while returning loaned value.");
                }
                // Give priority to prior error if occurred
                if (RETCODE_OK != ret)
                {
                    return ret;
                }
                else
                {
                    return ret_return_loan;
                }
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing structure/bitset member to JSON: loan_value failed.");
                return RETCODE_BAD_PARAMETER;
            }
        }
        case TK_UNION:
        {
            traits<DynamicDataImpl>::ref_type st_data =
                    traits<DynamicData>::narrow<DynamicDataImpl>(data->loan_value(member_id));
            if (nullptr != st_data)
            {
                ReturnCode_t ret = RETCODE_OK;
                nlohmann::json j_union;

                DynamicTypeMember::_ref_type active_type_member;
                if (RETCODE_OK !=
                        (ret =
                        st_data->enclosing_type()->get_member(active_type_member,
                        st_data->selected_union_member())))
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                            "Error encountered while serializing union member to JSON: get_member failed.");
                }
                else
                {
                    if (RETCODE_OK == (ret = json_serialize_member(st_data, active_type_member, j_union, format)))
                    {
                        json_insert(member_name, j_union, output);
                    }
                }
                // Return loaned value
                ReturnCode_t ret_return_loan;
                if (RETCODE_OK != (ret_return_loan = data->return_loaned_value(st_data)))
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while returning loaned value.");
                }
                // Give priority to prior error if occurred
                if (RETCODE_OK != ret)
                {
                    return ret;
                }
                else
                {
                    return ret_return_loan;
                }
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing union member to JSON: loan_value failed.");
                return RETCODE_BAD_PARAMETER;
            }
        }
        case TK_SEQUENCE:
        case TK_ARRAY:
        {
            traits<DynamicDataImpl>::ref_type st_data =
                    traits<DynamicData>::narrow<DynamicDataImpl>(data->loan_value(member_id));
            if (nullptr != st_data)
            {
                ReturnCode_t ret = json_serialize_collection(st_data, member_name, output, format);
                // Return loaned value
                ReturnCode_t ret_return_loan;
                if (RETCODE_OK != (ret_return_loan = data->return_loaned_value(st_data)))
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while returning loaned value.");
                }
                // Give priority to prior error if occurred
                if (RETCODE_OK != ret)
                {
                    return ret;
                }
                else
                {
                    return ret_return_loan;
                }
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing sequence/array member to JSON: loan_value failed.");
                return RETCODE_BAD_PARAMETER;
            }
        }
        case TK_MAP:
        {
            traits<DynamicDataImpl>::ref_type st_data =
                    traits<DynamicData>::narrow<DynamicDataImpl>(data->loan_value(member_id));
            if (nullptr != st_data)
            {
                ReturnCode_t ret = RETCODE_OK;
                nlohmann::json j_map;
                TypeDescriptorImpl& map_desc = st_data->enclosing_type()->get_descriptor();
                traits<DynamicTypeImpl>::ref_type key_type = traits<DynamicType>::narrow<DynamicTypeImpl>(
                    map_desc.key_element_type())->resolve_alias_enclosed_type();
                traits<DynamicTypeImpl>::ref_type value_type = traits<DynamicType>::narrow<DynamicTypeImpl>(
                    map_desc.element_type())->resolve_alias_enclosed_type();
                uint32_t size = st_data->get_item_count();
                for (uint32_t i = 0; i < size; ++i)
                {
                    // TODO: use actual map key as dictionary key once available in API
                    MemberId id = st_data->get_member_id_at_index(i);
                    if (RETCODE_OK !=
                            (ret =
                            json_serialize_member(st_data, id, value_type->get_kind(), std::to_string(id), j_map,
                            format)))
                    {
                        EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                                "Error encountered while serializing map member'S member to JSON.");
                        break;
                    }
                }
                if (RETCODE_OK == ret)
                {
                    json_insert(member_name, j_map, output);
                }
                // Return loaned value
                ReturnCode_t ret_return_loan;
                if (RETCODE_OK != (ret_return_loan = data->return_loaned_value(st_data)))
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while returning loaned value.");
                }
                // Give priority to prior error if occurred
                if (RETCODE_OK != ret)
                {
                    return ret;
                }
                else
                {
                    return ret_return_loan;
                }
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing map member to JSON: loan_value failed.");
                return RETCODE_BAD_PARAMETER;
            }
        }
        case TK_ALIAS:
        {
            EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                    "Error encountered while serializing member to JSON: unexpected TK_ALIAS kind.");
            return RETCODE_BAD_PARAMETER;
        }
        default:
            EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                    "Error encountered while serializing map member to JSON: unexpected kind " << member_kind <<
                    " found.");
            return RETCODE_BAD_PARAMETER;
    }
}

ReturnCode_t json_serialize_member(
        const traits<DynamicDataImpl>::ref_type& data,
        MemberId member_id,
        TypeKind member_kind,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept
{
    return json_serialize_member(data, member_id, member_kind, "", output, format);
}

ReturnCode_t json_serialize_basic_member(
        const traits<DynamicDataImpl>::ref_type& data,
        MemberId member_id,
        TypeKind member_kind,
        const std::string& member_name,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept
{
    switch (member_kind)
    {
        case TK_NONE:
        {
            EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                    "Error encountered while serializing basic member to JSON: unexpected TK_NONE kind.");
            return RETCODE_BAD_PARAMETER;
        }
        case TK_BOOLEAN:
        {
            ReturnCode_t ret;
            bool value;
            if (RETCODE_OK == (ret = data->get_boolean_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_BOOLEAN member to JSON.");
            }
            return ret;
        }
        case TK_BYTE:
        {
            ReturnCode_t ret;
            eprosima::fastrtps::rtps::octet value;
            if (RETCODE_OK == (ret = data->get_byte_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_BYTE member to JSON.");
            }
            return ret;
        }
        case TK_INT8:
        {
            ReturnCode_t ret;
            int8_t value;
            if (RETCODE_OK == (ret = data->get_int8_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_INT8 member to JSON.");
            }
            return ret;
        }
        case TK_INT16:
        {
            ReturnCode_t ret;
            int16_t value;
            if (RETCODE_OK == (ret = data->get_int16_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_INT16 member to JSON.");
            }
            return ret;
        }
        case TK_INT32:
        {
            ReturnCode_t ret;
            int32_t value;
            if (RETCODE_OK == (ret = data->get_int32_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_INT32 member to JSON.");
            }
            return ret;
        }
        case TK_INT64:
        {
            ReturnCode_t ret;
            int64_t value;
            if (RETCODE_OK == (ret = data->get_int64_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_INT64 member to JSON.");
            }
            return ret;
        }
        case TK_UINT8:
        {
            ReturnCode_t ret;
            uint8_t value;
            if (RETCODE_OK == (ret = data->get_uint8_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_UINT8 member to JSON.");
            }
            return ret;
        }
        case TK_UINT16:
        {
            ReturnCode_t ret;
            uint16_t value;
            if (RETCODE_OK == (ret = data->get_uint16_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_UINT16 member to JSON.");
            }
            return ret;
        }
        case TK_UINT32:
        {
            ReturnCode_t ret;
            uint32_t value;
            if (RETCODE_OK == (ret = data->get_uint32_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_UINT32 member to JSON.");
            }
            return ret;
        }
        case TK_UINT64:
        {
            ReturnCode_t ret;
            uint64_t value;
            if (RETCODE_OK == (ret = data->get_uint64_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_UINT64 member to JSON.");
            }
            return ret;
        }
        case TK_FLOAT32:
        {
            ReturnCode_t ret;
            float value;
            if (RETCODE_OK == (ret = data->get_float32_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_FLOAT32 member to JSON.");
            }
            return ret;
        }
        case TK_FLOAT64:
        {
            ReturnCode_t ret;
            double value;
            if (RETCODE_OK == (ret = data->get_float64_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_FLOAT64 member to JSON.");
            }
            return ret;
        }
        case TK_FLOAT128:
        {
            ReturnCode_t ret;
            long double value;
            if (RETCODE_OK == (ret = data->get_float128_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_FLOAT128 member to JSON.");
            }
            return ret;
        }
        case TK_CHAR8:
        {
            ReturnCode_t ret;
            char value;
            if (RETCODE_OK == (ret = data->get_char8_value(value, member_id)))
            {
                std::string aux_string_value({value});
                json_insert(member_name, aux_string_value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_CHAR8 member to JSON.");
            }
            return ret;
        }
        case TK_CHAR16:
        {
            ReturnCode_t ret;
            wchar_t value;
            if (RETCODE_OK == (ret = data->get_char16_value(value, member_id)))
            {
                // Insert UTF-8 converted value
                std::wstring aux_wstring_value({value});
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                std::string utf8_value = converter.to_bytes(aux_wstring_value);
                json_insert(member_name, utf8_value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_CHAR16 member to JSON.");
            }
            return ret;
        }
        case TK_STRING8:
        {
            ReturnCode_t ret;
            std::string value;
            if (RETCODE_OK == (ret = data->get_string_value(value, member_id)))
            {
                json_insert(member_name, value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_STRING8 member to JSON.");
            }
            return ret;
        }
        case TK_STRING16:
        {
            ReturnCode_t ret;
            std::wstring value;
            if (RETCODE_OK == (ret = data->get_wstring_value(value, member_id)))
            {
                // Insert UTF-8 converted value
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
                std::string utf8_value = converter.to_bytes(value);
                json_insert(member_name, utf8_value, output);
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_STRING16 member to JSON.");
            }
            return ret;
        }
        case TK_ENUM:
        {
            ReturnCode_t ret;
            int32_t value;
            if (RETCODE_OK != (ret = data->get_int32_value(value, member_id)))
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing TK_ENUM member to JSON.");
                return ret;
            }

            MemberDescriptor::_ref_type enum_desc{traits<MemberDescriptor>::make_shared()};
            if (RETCODE_OK != (ret = data->get_descriptor(enum_desc, member_id)))
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing TK_ENUM member to JSON: get_descriptor failed.");
                return ret;
            }

            DynamicTypeMembersByName all_members;
            if (RETCODE_OK !=
                    (ret =
                    traits<DynamicType>::narrow<DynamicTypeImpl>(enum_desc->type())->resolve_alias_enclosed_type()
                            ->
                            get_all_members_by_name(all_members)))
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing TK_ENUM member to JSON: get_all_members_by_name failed.");
                return ret;
            }

            ObjectName name;
            ret = RETCODE_BAD_PARAMETER;
            for (auto it : all_members)
            {
                MemberDescriptorImpl& enum_member_desc = traits<DynamicTypeMember>::narrow<DynamicTypeMemberImpl>(
                    it.second)->get_descriptor();
                if (enum_member_desc.default_value() == std::to_string(value))
                {
                    name = it.first;
                    assert(name == it.second->get_name());
                    ret = RETCODE_OK;
                    break;
                }
            }
            if (RETCODE_OK == ret)
            {
                if (format == DynamicDataJsonFormat::OMG)
                {
                    json_insert(member_name, name, output);
                }
                else if (format == DynamicDataJsonFormat::EPROSIMA)
                {
                    nlohmann::json enum_dict = {{"name", name}, {"value", value}};
                    json_insert(member_name, enum_dict, output);
                }
            }
            else
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing TK_ENUM member to JSON: enum value not found.");
            }
            return ret;
        }
        case TK_BITMASK:
        {
            ReturnCode_t ret;

            MemberDescriptor::_ref_type bitmask_member_desc{traits<MemberDescriptor>::make_shared()};
            if (RETCODE_OK != (ret = data->get_descriptor(bitmask_member_desc, member_id)))
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing TK_BITMASK member to JSON: get_descriptor failed.");
                return ret;
            }

            traits<DynamicTypeImpl>::ref_type bitmask_type = traits<DynamicType>::narrow<DynamicTypeImpl>(
                bitmask_member_desc->type())->resolve_alias_enclosed_type();
            TypeDescriptorImpl& bitmask_desc = bitmask_type->get_descriptor();
            auto bound = bitmask_desc.bound().at(0);

            if (format == DynamicDataJsonFormat::OMG)
            {
                if (9 > bound)
                {
                    uint8_t value;
                    if (RETCODE_OK == (ret = data->get_uint8_value(value, member_id)))
                    {
                        json_insert(member_name, value, output);
                    }
                }
                else if (17 > bound)
                {
                    uint16_t value;
                    if (RETCODE_OK == (ret = data->get_uint16_value(value, member_id)))
                    {
                        json_insert(member_name, value, output);
                    }
                }
                else if (33 > bound)
                {
                    uint32_t value;
                    if (RETCODE_OK == (ret = data->get_uint32_value(value, member_id)))
                    {
                        json_insert(member_name, value, output);
                    }
                }
                else
                {
                    uint64_t value;
                    if (RETCODE_OK == (ret = data->get_uint64_value(value, member_id)))
                    {
                        json_insert(member_name, value, output);
                    }
                }

                if (RETCODE_OK != ret)
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                            "Error encountered while serializing TK_BITMASK member to JSON: failed to get value.");
                }
            }
            else if (format == DynamicDataJsonFormat::EPROSIMA)
            {
                nlohmann::json bitmask_dict;
                uint64_t u64_value; // Auxiliar variable to check active bits afterwards
                if (9 > bound)
                {
                    uint8_t value;
                    if (RETCODE_OK == (ret = data->get_uint8_value(value, member_id)))
                    {
                        bitmask_dict["value"] = value;
                        bitmask_dict["binary"] = std::bitset<8>(value).to_string();
                        u64_value = static_cast<uint64_t>(value);
                    }
                }
                else if (17 > bound)
                {
                    uint16_t value;
                    if (RETCODE_OK == (ret = data->get_uint16_value(value, member_id)))
                    {
                        bitmask_dict["value"] = value;
                        bitmask_dict["binary"] = std::bitset<16>(value).to_string();
                        u64_value = static_cast<uint64_t>(value);
                    }
                }
                else if (33 > bound)
                {
                    uint32_t value;
                    if (RETCODE_OK == (ret = data->get_uint32_value(value, member_id)))
                    {
                        bitmask_dict["value"] = value;
                        bitmask_dict["binary"] = std::bitset<32>(value).to_string();
                        u64_value = static_cast<uint64_t>(value);
                    }
                }
                else
                {
                    uint64_t value;
                    if (RETCODE_OK == (ret = data->get_uint64_value(value, member_id)))
                    {
                        bitmask_dict["value"] = value;
                        bitmask_dict["binary"] = std::bitset<64>(value).to_string();
                        u64_value = value;
                    }
                }

                if (RETCODE_OK != ret)
                {
                    EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                            "Error encountered while serializing TK_BITMASK member to JSON: failed to get value.");
                }
                else
                {
                    // Check active bits
                    DynamicTypeMembersById bitmask_members;
                    if (RETCODE_OK != (ret = bitmask_type->get_all_members(bitmask_members)))
                    {
                        EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                                "Error encountered while serializing TK_BITMASK member to JSON: get_all_members failed.");
                    }
                    else
                    {
                        std::vector<std::string> active_bits;
                        for (auto it : bitmask_members)
                        {
                            if (u64_value & (0x01ull << it.second->get_id()))
                            {
                                active_bits.push_back(it.second->get_name().to_string());
                            }
                        }
                        bitmask_dict["active"] = active_bits;

                        // Insert custom bitmask value
                        json_insert(member_name, bitmask_dict, output);
                    }
                }
            }
            return ret;
        }
        default:
            // logWarning
            return RETCODE_BAD_PARAMETER;
    }
}

ReturnCode_t json_serialize_collection(
        const traits<DynamicDataImpl>::ref_type& data,
        const std::string& member_name,
        nlohmann::json& output,
        DynamicDataJsonFormat format) noexcept
{
    ReturnCode_t ret = RETCODE_OK;
    if (data->enclosing_type()->get_kind() == TK_SEQUENCE)
    {
        TypeDescriptorImpl& descriptor = data->enclosing_type()->get_descriptor();

        auto count = data->get_item_count();
        nlohmann::json j_array = nlohmann::json::array();
        for (uint32_t index = 0; index < count; ++index)
        {
            if (RETCODE_OK !=
                    (ret =
                    json_serialize_member(data, static_cast<MemberId>(index),
                    traits<DynamicType>::narrow<DynamicTypeImpl>(descriptor.element_type())->get_kind(), j_array,
                    format)))
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing sequence collection to JSON.");
                break;
            }
        }
        if (RETCODE_OK == ret)
        {
            json_insert(member_name, j_array, output);
        }
    }
    else
    {
        TypeDescriptorImpl& descriptor = data->enclosing_type()->get_descriptor();

        const BoundSeq& bounds = descriptor.bound();
        nlohmann::json j_array = nlohmann::json::array();
        unsigned int index = 0;
        if (RETCODE_OK != (ret = json_serialize_array(data, traits<DynamicType>::narrow<DynamicTypeImpl>(
                    descriptor.element_type())->get_kind(), index, bounds, j_array, format)))
        {
            EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing array collection to JSON.");
        }
        else
        {
            json_insert(member_name, j_array, output);
        }
    }
    return ret;
}

ReturnCode_t json_serialize_array(
        const traits<DynamicDataImpl>::ref_type& data,
        TypeKind member_kind,
        unsigned int& index,
        const std::vector<unsigned int>& bounds,
        nlohmann::json& j_array,
        DynamicDataJsonFormat format) noexcept
{
    assert(j_array.is_array());
    ReturnCode_t ret = RETCODE_OK;
    if (bounds.size() == 1)
    {
        for (unsigned int i = 0; i < bounds[0]; ++i)
        {
            if (RETCODE_OK !=
                    (ret =
                    json_serialize_member(data, static_cast<MemberId>(index++), member_kind, j_array,
                    format)))
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS, "Error encountered while serializing array element to JSON.");
                break;
            }
        }
    }
    else
    {
        for (unsigned int i = 0; i < bounds[0]; ++i)
        {
            nlohmann::json inner_array = nlohmann::json::array();
            if (RETCODE_OK !=
                    (ret =
                    json_serialize_array(data, member_kind, index,
                    std::vector<unsigned int>(bounds.begin() + 1, bounds.end()), inner_array, format)))
            {
                EPROSIMA_LOG_WARNING(XTYPES_UTILS,
                        "Error encountered while serializing array's array element to JSON.");
                break;
            }
            j_array.push_back(inner_array);
        }
    }
    return ret;
}

template <typename T>
void json_insert(
        const std::string& key,
        const T& value,
        nlohmann::json& j)
{
    if (j.is_array())
    {
        j.push_back(value);
    }
    else
    {
        j[key] = value;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// Dynamic Data to JSON serialization //// END
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////
// Dynamic Type to IDL serialization //
///////////////////////////////////////

//// Forward declarations and constants

constexpr const char* TYPE_OPENING =
        "\n{\n";

constexpr const char* TYPE_CLOSURE =
        "};\n";

constexpr const char* TAB_SEPARATOR =
        "    ";

struct TreeNodeType
{
    TreeNodeType(
            std::string member_name,
            std::string type_kind_name,
            DynamicType::_ref_type dynamic_type)
        : member_name(member_name)
        , type_kind_name(type_kind_name)
        , dynamic_type(dynamic_type)
    {
    }

    std::string member_name;
    std::string type_kind_name;
    DynamicType::_ref_type dynamic_type;
};

std::string type_kind_to_str(
        const DynamicType::_ref_type& type);

//// Implementation

DynamicType::_ref_type container_internal_type(
        const DynamicType::_ref_type& dyn_type)
{
    TypeDescriptor::_ref_type type_descriptor {traits<TypeDescriptor>::make_shared()};
    const auto ret = dyn_type->get_descriptor(type_descriptor);
    if (ret != RETCODE_OK)
    {
        //throw utils::InconsistencyException("No Type Descriptor");
    }
    return type_descriptor->element_type();
}

std::vector<uint32_t> container_size(
        const DynamicType::_ref_type& dyn_type)
{
    TypeDescriptor::_ref_type type_descriptor {traits<TypeDescriptor>::make_shared()};
    const auto ret = dyn_type->get_descriptor(type_descriptor);
    if (ret != RETCODE_OK)
    {
        //throw utils::InconsistencyException("No Type Descriptor");
    }
    return type_descriptor->bound();
}

std::vector<std::pair<std::string, DynamicType::_ref_type>> get_members_sorted(
        const DynamicType::_ref_type& dyn_type)
{
    std::vector<std::pair<std::string, DynamicType::_ref_type>> result;

    std::map<MemberId, DynamicTypeMember::_ref_type> members;
    dyn_type->get_all_members(members);

    for (const auto& member : members)
    {
        ObjectName dyn_name = member.second->get_name();
        MemberDescriptor::_ref_type member_descriptor {traits<MemberDescriptor>::make_shared()};
        const auto ret = member.second->get_descriptor(member_descriptor);
        if (ret != RETCODE_OK)
        {
            //throw utils::InconsistencyException("No Member Descriptor");
        }
        result.emplace_back(
            std::make_pair<std::string, DynamicType::_ref_type>(
                dyn_name.to_string(),
                std::move(member_descriptor->type())));
    }
    return result;
}

std::string array_kind_to_str(
        const DynamicType::_ref_type& dyn_type)
{
    auto internal_type = container_internal_type(dyn_type);
    auto this_array_size = container_size(dyn_type);

    std::stringstream ss;
    ss << type_kind_to_str(internal_type);

    for (const auto& bound : this_array_size)
    {
        ss << "[" << bound << "]";
    }

    return ss.str();
}

std::string sequence_kind_to_str(
        const DynamicType::_ref_type& dyn_type)
{
    auto internal_type = container_internal_type(dyn_type);
    auto this_sequence_size = container_size(dyn_type);

    std::stringstream ss;
    ss << "sequence<" << type_kind_to_str(internal_type);

    for (const auto& bound : this_sequence_size)
    {
        ss << ", " << bound;
    }
    ss << ">";

    return ss.str();
}

std::string map_kind_to_str(
        const DynamicType::_ref_type& dyn_type)
{
    std::stringstream ss;
    TypeDescriptor::_ref_type type_descriptor {traits<TypeDescriptor>::make_shared()};
    const auto ret = dyn_type->get_descriptor(type_descriptor);
    if (ret != RETCODE_OK)
    {
        //throw utils::InconsistencyException("No Type Descriptor");
    }
    auto key_type = type_descriptor->key_element_type();
    auto value_type = type_descriptor->element_type();
    ss << "map<" << type_kind_to_str(key_type) << ", " << type_kind_to_str(value_type) << ">";

    return ss.str();
}

std::string type_kind_to_str(
        const DynamicType::_ref_type& dyn_type)
{
    switch (dyn_type->get_kind())
    {
        case TK_BOOLEAN:
            return "boolean";

        case TK_BYTE:
            return "octet";

        case TK_INT16:
            return "short";

        case TK_INT32:
            return "long";

        case TK_INT64:
            return "long long";

        case TK_UINT16:
            return "unsigned short";

        case TK_UINT32:
            return "unsigned long";

        case TK_UINT64:
            return "unsigned long long";

        case TK_FLOAT32:
            return "float";

        case TK_FLOAT64:
            return "double";

        case TK_FLOAT128:
            return "long double";

        case TK_CHAR8:
            return "char";

        case TK_CHAR16:
            return "wchar";

        case TK_STRING8:
            return "string";

        case TK_STRING16:
            return "wstring";

        case TK_ARRAY:
            return array_kind_to_str(dyn_type);

        case TK_SEQUENCE:
            return sequence_kind_to_str(dyn_type);

        case TK_MAP:
            return map_kind_to_str(dyn_type);

        case TK_STRUCTURE:
        case TK_ENUM:
        case TK_UNION:
            return (dyn_type->get_name()).to_string();

        case TK_BITSET:
        case TK_BITMASK:
        case TK_NONE:
            //throw utils::UnsupportedException(
                    //   STR_ENTRY << "Type " << dyn_type->get_name() << " is not supported.");
            return "";

        default:
            //throw utils::InconsistencyException(
                    //   STR_ENTRY << "Type " << dyn_type->get_name() << " has not correct kind.");
            return ""; 

    }
}

utilities::collections::TreeNode<TreeNodeType> generate_dyn_type_tree(
        const DynamicType::_ref_type& type,
        const std::string& member_name = "PARENT")
{
    // Get kind
    TypeKind kind = type->get_kind();

    switch (kind)
    {
        case TK_STRUCTURE:
        {
            // If is struct, the call is recursive.
            // Create new tree node
            utilities::collections::TreeNode<TreeNodeType> parent(member_name, (type->get_name()).to_string(), type);

            // Get all members of this struct
            std::vector<std::pair<std::string,
                    DynamicType::_ref_type>> members_by_name = get_members_sorted(type);

            for (const auto& member : members_by_name)
            {
                // Add each member with its name as a new node in a branch (recursion)
                parent.add_branch(
                    generate_dyn_type_tree(member.second, member.first));
            }
            return parent;
        }

        case TK_ARRAY:
        case TK_SEQUENCE:
        {
            // If container (array or struct) has exactly one branch
            // Calculate child branch
            auto internal_type = container_internal_type(type);

            // Create this node
            utilities::collections::TreeNode<TreeNodeType> container(member_name, type_kind_to_str(type), type);
            // Add branch
            container.add_branch(generate_dyn_type_tree(internal_type, "CONTAINER_MEMBER"));

            return container;
        }

        default:
            return utilities::collections::TreeNode<TreeNodeType>(member_name, type_kind_to_str(type), type);
    }
}

std::ostream& node_to_str(
        std::ostream& os,
        const utilities::collections::TreeNode<TreeNodeType>& node)
{
    os << TAB_SEPARATOR;

    if (node.info.dynamic_type->get_kind() == TK_ARRAY)
    {
        auto dim_pos = node.info.type_kind_name.find("[");
        auto kind_name_str = node.info.type_kind_name.substr(0, dim_pos);
        auto dim_str = node.info.type_kind_name.substr(dim_pos, std::string::npos);

        os << kind_name_str << " " << node.info.member_name << dim_str;
    }
    else
    {
        os << node.info.type_kind_name << " " << node.info.member_name;
    }

    return os;
}

std::ostream& struct_to_str(
        std::ostream& os,
        const utilities::collections::TreeNode<TreeNodeType>& node)
{
    // Add types name
    os << "struct " << node.info.type_kind_name << TYPE_OPENING;

    // Add struct attributes
    for (auto const& child : node.branches())
    {
        node_to_str(os, child.info);
        os << ";\n";
    }

    // Close definition
    os << TYPE_CLOSURE;

    return os;
}

std::ostream& enum_to_str(
        std::ostream& os,
        const utilities::collections::TreeNode<TreeNodeType>& node)
{
    os << "enum " << node.info.type_kind_name << TYPE_OPENING << TAB_SEPARATOR;

   std::map<MemberId, DynamicTypeMember::_ref_type> members;
    node.info.dynamic_type->get_all_members(members);
    bool first_iter = true;
    for (const auto& member : members)
    {
        if (!first_iter)
        {
            os << ",\n" << TAB_SEPARATOR;
        }
        first_iter = false;

        os << member.second->get_name();
    }

    // Close definition
    os << "\n" << TYPE_CLOSURE;

    return os;
}

std::ostream& union_to_str(
        std::ostream& os,
        const utilities::collections::TreeNode<TreeNodeType>& node)
{
    TypeDescriptor::_ref_type type_descriptor {traits<TypeDescriptor>::make_shared()};
    const auto ret = node.info.dynamic_type->get_descriptor(type_descriptor);
    if (ret != RETCODE_OK)
    {
        //throw utils::InconsistencyException("No Type Descriptor");
    }
    os << "union " << node.info.type_kind_name << " switch (" << type_kind_to_str(
        type_descriptor->discriminator_type()) << ")" << TYPE_OPENING;

    std::map<MemberId, DynamicTypeMember::_ref_type> members;
    node.info.dynamic_type->get_all_members(members);  // WARNING: Default case not included in this collection, and currently not available
    for (const auto& member : members)
    {
        MemberDescriptor::_ref_type member_descriptor {traits<MemberDescriptor>::make_shared()};
        const auto ret = member.second->get_descriptor(member_descriptor);
        if (ret != RETCODE_OK)
        {
            //throw utils::InconsistencyException("No Member Descriptor");
        }
        auto labels = member_descriptor->label();  // WARNING: There might be casting issues as discriminant type is currently not taken into consideration
        bool first_iter = true;
        for (const auto& label : labels)
        {
            if (first_iter)
            {
                os << TAB_SEPARATOR;
            }
            else
            {
                os << " ";
            }
            first_iter = false;

            os << "case " << std::to_string(label) << ":";
        }

        os << "\n" << TAB_SEPARATOR << TAB_SEPARATOR << type_kind_to_str(member_descriptor->type()) <<
            " " << member.second->get_name() << ";\n";


    }

    // Close definition
    os << TYPE_CLOSURE;

    return os;
}

std::string generate_dyn_type_schema_from_tree(
        const utilities::collections::TreeNode<TreeNodeType>& parent_node)
{
    std::set<std::string> types_written;

    std::stringstream ss;

    // For every Node, check if it is of a "writable" type (i.e. struct, enum or union).
    // If it is, check if it is not yet written
    // If it is not, write it down
    for (const auto& node : parent_node.all_nodes())
    {
        auto kind = node.info.dynamic_type->get_kind();
        if (types_written.find(node.info.type_kind_name) == types_written.end())
        {
            switch (kind)
            {
                case TK_STRUCTURE:
                    struct_to_str(ss, node);
                    break;

                case TK_ENUM:
                    enum_to_str(ss, node);
                    break;

                case TK_UNION:
                    union_to_str(ss, node);
                    break;

                default:
                    continue;
            }
            ss << "\n"; // Introduce blank line between type definitions
            types_written.insert(node.info.type_kind_name);
        }
    }

    // Write struct parent node at last, after all its dependencies
    // NOTE: not a requirement for Foxglove IDL Parser, dependencies can be placed after parent
    struct_to_str(ss, parent_node);

    return ss.str();
}

std::string generate_idl_schema(
        const traits<DynamicType>::ref_type& dynamic_type)
{
    // Generate type tree
    utilities::collections::TreeNode<TreeNodeType> parent_type = generate_dyn_type_tree(dynamic_type);

    // From tree, generate string
    return generate_dyn_type_schema_from_tree(parent_type);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// Dynamic Type to IDL serialization //// END
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}         // namespace dds
}     // namespace fastdds
} // namespace eprosima
